// Architecture loosely based on FFmpeg's ffplay.c (Copyright (c) 2003 Fabrice Bellard, LGPL 2.1+).

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <malloc.h>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
}

#include "logger/logger.hpp"
#include "nv12_shader.h"
#include "player/media_player.hpp"
#include "utils/display.hpp"
#include "utils/media_info.hpp"
#include "yuv420p_shader.h"

#include <SDL2/SDL.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/texture.h>
#include <gx2/utils.h>
#include <gx2r/surface.h>
#include <whb/gfx.h>

#define MP "MediaPlayer"

#define VIDEO_FRAME_QUEUE_SIZE 16
#define AUDIO_FRAME_QUEUE_SIZE 16
#define MIN_FRAMES 8

#define AV_SYNC_THRESHOLD_MIN 0.04
#define AV_SYNC_THRESHOLD_MAX 0.10
#define AV_SYNC_FRAMEDUP_THR 0.10
#define AV_NOSYNC_THRESHOLD 10.0

#define AUDIO_OUT_CHANNELS 2
#define AUDIO_OUT_RATE 48000
#define AUDIO_BUF_MAX_BYTES (768 * 1024)

static double wall_now() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

struct VideoPlane {
    GX2Texture tex[2]{};
    GX2Sampler smp{};
    int coded_w = 0;
    int coded_h = 0;
    bool valid = false;
};

static inline float px_to_ndc_x(int x) { return 2.0f * (float)x / display_get().width - 1.0f; }
static inline float px_to_ndc_y(int y) { return 1.0f - 2.0f * (float)y / display_get().height; }

struct VideoVertex {
    float x, y, u, v;
};

static bool alloc_plane(VideoPlane &p, GX2SurfaceFormat fmt, uint32_t comp_map, int w, int h) {
    for (int b = 0; b < 2; ++b) {
        memset(&p.tex[b], 0, sizeof(p.tex[b]));
        GX2Surface &surf = p.tex[b].surface;
        surf.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        surf.use = GX2_SURFACE_USE_TEXTURE;
        surf.width = (uint32_t)w;
        surf.height = (uint32_t)h;
        surf.depth = 1;
        surf.mipLevels = 1;
        surf.format = fmt;
        surf.aa = GX2_AA_MODE1X;

        surf.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;

        p.tex[b].viewNumSlices = 1;
        p.tex[b].viewNumMips = 1;
        p.tex[b].compMap = comp_map;

        if (!GX2RCreateSurface(&surf, GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ)) {
            log_message(LOG_ERROR, MP, "alloc_plane: GX2RCreateSurface failed buf=%d fmt=%d %dx%d", b, (int)fmt, w, h);
            if (b == 1) GX2RDestroySurfaceEx(&p.tex[0].surface, GX2R_RESOURCE_BIND_NONE);
            return false;
        }
        GX2InitTextureRegs(&p.tex[b]);

        void *px = GX2RLockSurfaceEx(&surf, 0, GX2R_RESOURCE_BIND_NONE);
        if (px) {
            memset(px, 0x10, surf.imageSize);
            GX2RUnlockSurfaceEx(&surf, 0, GX2R_RESOURCE_BIND_NONE);
        }
    }

    GX2InitSampler(&p.smp, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    p.coded_w = w;
    p.coded_h = h;
    p.valid = true;

    log_message(LOG_DEBUG, MP, "alloc_plane: %dx%d fmt=%d pitch=%u imageSize=%u (double-buffered)", w, h, (int)fmt, p.tex[0].surface.pitch, p.tex[0].surface.imageSize);
    return true;
}

static void free_plane(VideoPlane &p) {
    if (!p.valid) return;

    for (int b = 0; b < 2; ++b)
        GX2RDestroySurfaceEx(&p.tex[b].surface, GX2R_RESOURCE_BIND_NONE);

    p.valid = false;
    p.coded_w = 0;
    p.coded_h = 0;
}

static void upload_plane(VideoPlane &p, int write_idx, const uint8_t *src, int src_linesize, int copy_bytes_per_row, int rows) {
    GX2Surface &surf = p.tex[write_idx].surface;

    uint8_t *dst = (uint8_t *)GX2RLockSurfaceEx(&surf, 0, GX2R_RESOURCE_BIND_NONE);
    if (!dst) {
        log_message(LOG_ERROR, MP, "upload_plane: GX2RLockSurfaceEx returned null");
        return;
    }

    const uint32_t bytes_per_texel = (surf.format == GX2_SURFACE_FORMAT_UNORM_R8_G8) ? 2u : 1u;
    const uint32_t dst_row_bytes = surf.pitch * bytes_per_texel;

    for (int y = 0; y < rows; ++y) {
        memcpy(dst + (size_t)y * dst_row_bytes, src + (size_t)y * src_linesize, (size_t)copy_bytes_per_row);
    }

    GX2RUnlockSurfaceEx(&surf, 0, GX2R_RESOURCE_BIND_NONE);
}

struct PktNode {
    AVPacket *pkt;
    int serial;
    PktNode *next;
};

struct PacketQueue {
    PktNode *head = nullptr;
    PktNode *tail = nullptr;
    int nb_packets = 0;
    int size = 0;
    int64_t dur = 0;
    int serial = 0;
    bool abort = false;
    std::mutex mtx;
    std::condition_variable cond;
};

static AVPacket *g_flush_pkt = nullptr;

static void pq_init(PacketQueue *q) {
    q->head = q->tail = nullptr;
    q->nb_packets = q->size = q->serial = 0;
    q->dur = 0;
    q->abort = true;
}

static void pq_put_private(PacketQueue *q, AVPacket *pkt) {
    if (q->abort) {
        av_packet_free(&pkt);
        return;
    }

    PktNode *n = new PktNode{};

    n->pkt = pkt;
    n->serial = (pkt->data == g_flush_pkt->data) ? (++q->serial) : q->serial;
    n->next = nullptr;

    if (q->tail)
        q->tail->next = n;
    else
        q->head = n;

    q->tail = n;
    q->nb_packets++;
    q->size += pkt->size + (int)sizeof(*n);
    q->dur += pkt->duration;
    q->cond.notify_one();
}

static bool pq_put(PacketQueue *q, AVPacket *pkt) {
    AVPacket *p = av_packet_alloc();

    if (!p) {
        av_packet_unref(pkt);
        return false;
    }

    av_packet_move_ref(p, pkt);
    std::lock_guard<std::mutex> lk(q->mtx);

    if (q->abort) {
        av_packet_free(&p);
        return false;
    }

    pq_put_private(q, p);

    return true;
}

static int pq_get(PacketQueue *q, AVPacket *pkt, bool block, int *serial_out) {
    std::unique_lock<std::mutex> lk(q->mtx);
    for (;;) {
        if (q->abort) return -1;
        if (q->head) {
            PktNode *n = q->head;
            q->head = n->next;

            if (!q->head) q->tail = nullptr;

            q->nb_packets--;
            q->size -= n->pkt->size + (int)sizeof(*n);
            q->dur -= n->pkt->duration;
            av_packet_move_ref(pkt, n->pkt);
            if (serial_out) *serial_out = n->serial;
            av_packet_free(&n->pkt);
            delete n;

            return 1;
        }
        if (!block) return 0;
        q->cond.wait(lk);
    }
}

static void pq_flush_locked(PacketQueue *q) {
    std::lock_guard<std::mutex> lk(q->mtx);

    while (q->head) {
        PktNode *n = q->head;
        q->head = n->next;
        av_packet_free(&n->pkt);
        delete n;
    }

    q->tail = nullptr;
    q->nb_packets = q->size = 0;
    q->dur = 0;
    q->serial++;
}

static void pq_abort(PacketQueue *q) {
    std::lock_guard<std::mutex> lk(q->mtx);
    q->abort = true;
    q->cond.notify_all();
}

static void pq_start(PacketQueue *q) {
    std::lock_guard<std::mutex> lk(q->mtx);
    q->abort = false;
    AVPacket *fp = av_packet_alloc();
    if (fp) {
        fp->data = g_flush_pkt->data;
        fp->size = 0;
        pq_put_private(q, fp);
    }
}

static void pq_destroy(PacketQueue *q) { pq_flush_locked(q); }

struct Frame {
    AVFrame *frame = nullptr;
    double pts = 0.0;
    double duration = 0.0;
    int serial = 0;
    int width = 0;
    int height = 0;
    bool uploaded = false;
};

struct FrameQueue {
    Frame buf[16];
    int rindex = 0, windex = 0, size = 0;
    int max_size = 0, keep_last = 0, rindex_shown = 0;
    std::mutex mtx;
    std::condition_variable cond;
    PacketQueue *pktq = nullptr;
};

static int fq_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last) {
    f->rindex = f->windex = f->size = f->rindex_shown = 0;
    f->max_size = max_size;
    f->keep_last = !!keep_last;
    f->pktq = pktq;
    for (int i = 0; i < max_size; ++i) {
        f->buf[i] = Frame{};
        f->buf[i].frame = av_frame_alloc();
        if (!f->buf[i].frame) return -1;
    }
    return 0;
}

static void fq_destroy(FrameQueue *f) {
    for (int i = 0; i < f->max_size; ++i) {
        av_frame_unref(f->buf[i].frame);
        av_frame_free(&f->buf[i].frame);
    }
}

static void fq_signal(FrameQueue *f) {
    std::lock_guard<std::mutex> lk(f->mtx);
    f->cond.notify_all();
}

static Frame *fq_peek(FrameQueue *f) { return &f->buf[(f->rindex + f->rindex_shown) % f->max_size]; }
static Frame *fq_peek_next(FrameQueue *f) { return &f->buf[(f->rindex + f->rindex_shown + 1) % f->max_size]; }
static Frame *fq_peek_last(FrameQueue *f) { return &f->buf[f->rindex]; }

static Frame *fq_peek_writable(FrameQueue *f) {
    std::unique_lock<std::mutex> lk(f->mtx);
    f->cond.wait(lk, [f] { return f->size < f->max_size || f->pktq->abort; });
    return f->pktq->abort ? nullptr : &f->buf[f->windex];
}

static void fq_push(FrameQueue *f) {
    if (++f->windex == f->max_size) f->windex = 0;
    std::lock_guard<std::mutex> lk(f->mtx);
    f->size++;
    f->cond.notify_one();
}

static void fq_next(FrameQueue *f) {
    if (f->keep_last && !f->rindex_shown) {
        f->rindex_shown = 1;
        return;
    }
    av_frame_unref(f->buf[f->rindex].frame);
    f->buf[f->rindex].uploaded = false;
    if (++f->rindex == f->max_size) f->rindex = 0;
    std::lock_guard<std::mutex> lk(f->mtx);
    f->size--;
    f->cond.notify_one();
}

static int fq_nb_remaining(FrameQueue *f) { return f->size - f->rindex_shown; }

struct Clock {
    double pts = NAN, pts_drift = 0, last_upd = 0, speed = 1.0;
    int serial = -1;
    bool paused = false;
    int *q_serial = nullptr;
};

static double clock_get(const Clock *c) {
    if (c->q_serial && *c->q_serial != c->serial) return NAN;
    if (c->paused) return c->pts;
    double t = wall_now();
    return c->pts_drift + t - (t - c->last_upd) * (1.0 - c->speed);
}

static void clock_set_at(Clock *c, double pts, int serial, double t) {
    c->pts = pts;
    c->last_upd = t;
    c->pts_drift = pts - t;
    c->serial = serial;
}

static void clock_set(Clock *c, double pts, int serial) { clock_set_at(c, pts, serial, wall_now()); }

static void clock_init(Clock *c, int *q_serial) {
    c->speed = 1.0;
    c->paused = true;
    c->q_serial = q_serial;
    clock_set_at(c, 0.0, q_serial ? *q_serial : 0, wall_now());
}

static void clock_sync_to_slave(Clock *c, const Clock *slave) {
    double mt = clock_get(c), st = clock_get(slave);
    if (!std::isnan(st) && (std::isnan(mt) || std::fabs(mt - st) > AV_NOSYNC_THRESHOLD)) clock_set(c, st, slave->serial);
}

struct Decoder {
    AVPacket *pkt = nullptr;
    PacketQueue *queue = nullptr;
    AVCodecContext *avctx = nullptr;
    int pkt_serial = -1, finished = 0, packet_pending = 0;
    int64_t start_pts = AV_NOPTS_VALUE;
    AVRational start_pts_tb = {0, 1};
    int64_t next_pts = AV_NOPTS_VALUE;
    AVRational next_pts_tb = {0, 1};
};

static int decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue) {
    d->pkt = av_packet_alloc();
    if (!d->pkt) return AVERROR(ENOMEM);
    d->avctx = avctx;
    d->queue = queue;
    d->pkt_serial = -1;
    d->finished = d->packet_pending = 0;
    d->start_pts = d->next_pts = AV_NOPTS_VALUE;
    d->start_pts_tb = d->next_pts_tb = {0, 1};
    return 0;
}

static void decoder_free_pkt(Decoder *d) { av_packet_free(&d->pkt); }

static void decoder_abort(Decoder *d, FrameQueue *fq) {
    pq_abort(d->queue);
    fq_signal(fq);
}

static int decoder_decode_frame(Decoder *d, AVFrame *frame) {
    int ret = AVERROR(EAGAIN);
    for (;;) {
        if (d->queue->serial == d->pkt_serial) {
            do {
                if (d->queue->abort) return -1;
                switch (d->avctx->codec_type) {
                    case AVMEDIA_TYPE_VIDEO:
                        ret = avcodec_receive_frame(d->avctx, frame);
                        if (ret >= 0) frame->pts = frame->best_effort_timestamp;
                        break;
                    case AVMEDIA_TYPE_AUDIO:
                        ret = avcodec_receive_frame(d->avctx, frame);
                        if (ret >= 0) {
                            AVRational tb = {1, frame->sample_rate};
                            if (frame->pts != AV_NOPTS_VALUE)
                                frame->pts = av_rescale_q(frame->pts, d->avctx->pkt_timebase, tb);
                            else if (d->next_pts != AV_NOPTS_VALUE)
                                frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
                            if (frame->pts != AV_NOPTS_VALUE) {
                                d->next_pts = frame->pts + frame->nb_samples;
                                d->next_pts_tb = tb;
                            }
                        }
                        break;
                    default:
                        break;
                }
                if (ret == AVERROR_EOF) {
                    d->finished = d->pkt_serial;
                    avcodec_flush_buffers(d->avctx);
                    return 0;
                }
                if (ret >= 0) return 1;
            } while (ret != AVERROR(EAGAIN));
        }
        do {
            if (d->packet_pending) {
                d->packet_pending = 0;
            } else {
                int old_serial = d->pkt_serial;
                if (pq_get(d->queue, d->pkt, true, &d->pkt_serial) < 0) return -1;
                if (old_serial != d->pkt_serial) {
                    avcodec_flush_buffers(d->avctx);
                    d->finished = 0;
                    d->next_pts = d->start_pts;
                    d->next_pts_tb = d->start_pts_tb;
                }
            }
            if (d->queue->serial == d->pkt_serial) break;
            av_packet_unref(d->pkt);
        } while (true);
        if (d->pkt->data == g_flush_pkt->data) {
            avcodec_flush_buffers(d->avctx);
            d->finished = 0;
            av_packet_unref(d->pkt);
            continue;
        }
        if (!d->pkt->data) {
            avcodec_send_packet(d->avctx, nullptr);
            av_packet_unref(d->pkt);
            continue;
        }
        if (avcodec_send_packet(d->avctx, d->pkt) == AVERROR(EAGAIN))
            d->packet_pending = 1;
        else
            av_packet_unref(d->pkt);
    }
}

enum class VideoFmt { Unknown, YUV420P, NV12 };

struct PlayerState {
    AVFormatContext *fmt_ctx = nullptr;
    int video_idx = -1;
    int audio_idx = -1;
    AVRational video_tb = {0, 1};

    AVCodecContext *video_avctx = nullptr;
    AVCodecContext *audio_avctx = nullptr;

    PacketQueue videoq, audioq;
    FrameQueue pictq, sampq;
    Decoder viddec, auddec;

    Clock audclk, vidclk, extclk;

    double wall_play_origin = 0.0;
    double wall_play_offset = 0.0;

    SwrContext *swr_ctx = nullptr;
    SDL_AudioDeviceID audio_dev = 0;
    SDL_AudioSpec audio_spec = {};
    bool audio_enabled = false;

    std::atomic<VideoFmt> video_fmt{VideoFmt::Unknown};

    VideoPlane plane_y{}; // Y,  full coded resolution
    VideoPlane plane_u{}; // Cb, half resolution
    VideoPlane plane_v{}; // Cr, half resolution

    VideoPlane plane_uv{}; // UV interleaved, RG8, half resolution

    int plane_write_idx = 0;

    WHBGfxShaderGroup *shader_yuv420p = nullptr;
    WHBGfxShaderGroup *shader_nv12 = nullptr;

    void *quad_vtx = nullptr;
    uint32_t quad_vtx_size = 0;

    frame_info *cur_frame_info = nullptr;
    rect dest_rect = {0, 0, 0, 0};
    bool dest_rect_init = false;
    int out_w = 0;
    int out_h = 0;
    bool hw_decoder = false;

    double frame_timer = 0.0;
    double max_frame_dur = 3600.0;

    std::atomic<bool> running{false};
    std::atomic<bool> playing{false};
    std::atomic<bool> paused{true};
    std::atomic<bool> force_refresh{false};
    bool eof = false;

    std::thread read_tid, video_tid, audio_tid;

    std::mutex seek_mtx;
    std::condition_variable seek_cv;
    bool seek_req = false;
    int64_t seek_pos = 0;

    std::vector<AudioTrackInfo> audio_tracks;
    std::mutex audio_tracks_mtx;
    int cur_audio_track = -1;

    int frames_decoded = 0;
    int frames_dropped = 0;
    double last_log_time = 0.0;
};

static PlayerState *S = nullptr;
static void rebuild_swr();

static WHBGfxShaderGroup *load_shader(const uint8_t *gsh_data, const char *name) {
    WHBGfxShaderGroup *g = new WHBGfxShaderGroup{};

    if (!WHBGfxLoadGFDShaderGroup(g, 0, gsh_data)) {
        log_message(LOG_ERROR, MP, "load_shader: failed for '%s'", name);
        delete g;
        return nullptr;
    }

    WHBGfxInitShaderAttribute(g, "in_pos", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);

    WHBGfxInitShaderAttribute(g, "in_uv", 0, 8, GX2_ATTRIB_FORMAT_FLOAT_32_32);

    if (!WHBGfxInitFetchShader(g)) {
        log_message(LOG_ERROR, MP, "load_shader: fetch shader failed for '%s'", name);

        WHBGfxFreeShaderGroup(g);
        delete g;
        return nullptr;
    }

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, g->fetchShader.program, g->fetchShader.size);

    log_message(LOG_OK, MP, "load_shader: '%s' loaded", name);

    return g;
}

static void free_shader(WHBGfxShaderGroup *&g) {
    if (!g) return;
    WHBGfxFreeShaderGroup(g);
    delete g;
    g = nullptr;
}

static bool init_video_planes(VideoFmt fmt, int coded_w, int coded_h) {
    const uint32_t cm_r8 = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
    const uint32_t cm_rg8 = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_0, GX2_SQ_SEL_1);

    if (!alloc_plane(S->plane_y, GX2_SURFACE_FORMAT_UNORM_R8, cm_r8, coded_w, coded_h)) return false;

    if (fmt == VideoFmt::YUV420P) {
        if (!alloc_plane(S->plane_u, GX2_SURFACE_FORMAT_UNORM_R8, cm_r8, coded_w / 2, coded_h / 2)) return false;
        if (!alloc_plane(S->plane_v, GX2_SURFACE_FORMAT_UNORM_R8, cm_r8, coded_w / 2, coded_h / 2)) return false;
        log_message(LOG_OK, MP, "init_video_planes: YUV420P %dx%d (Y-pitch=%u U-pitch=%u)", coded_w, coded_h, S->plane_y.tex[0].surface.pitch, S->plane_u.tex[0].surface.pitch);
    } else {
        if (!alloc_plane(S->plane_uv, GX2_SURFACE_FORMAT_UNORM_R8_G8, cm_rg8, coded_w / 2, coded_h / 2)) return false;
        log_message(LOG_OK, MP, "init_video_planes: NV12 %dx%d (Y-pitch=%u UV-pitch=%u)", coded_w, coded_h, S->plane_y.tex[0].surface.pitch, S->plane_uv.tex[0].surface.pitch);
    }
    return true;
}

static void free_video_planes() {
    free_plane(S->plane_y);
    free_plane(S->plane_u);
    free_plane(S->plane_v);
    free_plane(S->plane_uv);
}

static void update_quad(const rect &r) {
    constexpr uint32_t needed = 4 * sizeof(VideoVertex);
    if (!S->quad_vtx || S->quad_vtx_size < needed) {
        free(S->quad_vtx);
        S->quad_vtx = memalign(GX2_VERTEX_BUFFER_ALIGNMENT, needed);
        S->quad_vtx_size = needed;
    }

    float x0 = px_to_ndc_x(r.x), y0 = px_to_ndc_y(r.y);
    float x1 = px_to_ndc_x(r.x + r.w), y1 = px_to_ndc_y(r.y + r.h);

    VideoVertex *v = static_cast<VideoVertex *>(S->quad_vtx);
    v[0] = {x0, y0, 0.0f, 0.0f}; // top-left
    v[1] = {x0, y1, 0.0f, 1.0f}; // bottom-left
    v[2] = {x1, y0, 1.0f, 0.0f}; // top-right
    v[3] = {x1, y1, 1.0f, 1.0f}; // bottom-right

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, S->quad_vtx, S->quad_vtx_size);
}

static void video_upload_frame(const AVFrame *f) {
    const int wi = S->plane_write_idx;

    if (f->format == AV_PIX_FMT_YUV420P) {
        // Y: full resolution, 1 byte/sample
        upload_plane(S->plane_y, wi, f->data[0], f->linesize[0], f->width, f->height);
        // U: half resolution, 1 byte/sample
        upload_plane(S->plane_u, wi, f->data[1], f->linesize[1], f->width / 2, f->height / 2);
        // V: half resolution, 1 byte/sample
        upload_plane(S->plane_v, wi, f->data[2], f->linesize[2], f->width / 2, f->height / 2);
    } else if (f->format == AV_PIX_FMT_NV12) {
        // Y: full resolution, 1 byte/sample
        upload_plane(S->plane_y, wi, f->data[0], f->linesize[0], f->width, f->height);
        // UV: interleaved, half resolution.
        upload_plane(S->plane_uv, wi, f->data[1], f->linesize[1], f->width, f->height / 2);
    } else {
        log_message(LOG_WARNING, MP, "video_upload_frame: unsupported fmt=%d — frame skipped", f->format);
        return;
    }

    S->plane_write_idx ^= 1;
}

static void video_render_common(WHBGfxShaderGroup *grp, const rect &dest) {
    if (!grp || !S->quad_vtx) return;

    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
    GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO, GX2_BLEND_COMBINE_MODE_ADD, FALSE, GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO, GX2_BLEND_COMBINE_MODE_ADD);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetViewport(0, 0, display_get().width, display_get().height, 0, 1);
    GX2SetScissor(0, 0, display_get().width, display_get().height);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, S->quad_vtx, S->quad_vtx_size);
    GX2SetFetchShader(&grp->fetchShader);
    GX2SetVertexShader(grp->vertexShader);
    GX2SetPixelShader(grp->pixelShader);

    const float mvp[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

    GX2SetVertexUniformReg(0, 16, &mvp[0][0]);
    GX2SetAttribBuffer(0, S->quad_vtx_size, sizeof(VideoVertex), S->quad_vtx);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, 0, 1);
}

static void video_render_yuv420p(const rect &dest) {
    const int ri = S->plane_write_idx ^ 1;

    GX2SetPixelTexture(&S->plane_y.tex[ri], 0);
    GX2SetPixelSampler(&S->plane_y.smp, 0);
    GX2SetPixelTexture(&S->plane_u.tex[ri], 1);
    GX2SetPixelSampler(&S->plane_u.smp, 1);
    GX2SetPixelTexture(&S->plane_v.tex[ri], 2);
    GX2SetPixelSampler(&S->plane_v.smp, 2);

    video_render_common(S->shader_yuv420p, dest);
}

static void video_render_nv12(const rect &dest) {
    const int ri = S->plane_write_idx ^ 1;

    GX2SetPixelTexture(&S->plane_y.tex[ri], 0);
    GX2SetPixelSampler(&S->plane_y.smp, 0);
    GX2SetPixelTexture(&S->plane_uv.tex[ri], 1);
    GX2SetPixelSampler(&S->plane_uv.smp, 1);

    video_render_common(S->shader_nv12, dest);
}

static void rebuild_swr() {
    if (S->swr_ctx) {
        swr_free(&S->swr_ctx);
        S->swr_ctx = nullptr;
    }
    AVCodecContext *ac = S->audio_avctx;
    if (!ac) return;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
    int64_t in_ch = ac->ch_layout.nb_channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    int in_chc = ac->ch_layout.nb_channels;
#else
    int64_t in_ch = av_get_default_channel_layout(ac->channels);
    int in_chc = ac->channels;
#endif
    int64_t out_ch = av_get_default_channel_layout(AUDIO_OUT_CHANNELS);

    S->swr_ctx = swr_alloc();
    if (!S->swr_ctx) {
        log_message(LOG_ERROR, MP, "swr_alloc failed");
        return;
    }
    av_opt_set_int(S->swr_ctx, "in_channel_layout", in_ch, 0);
    av_opt_set_int(S->swr_ctx, "out_channel_layout", out_ch, 0);
    av_opt_set_int(S->swr_ctx, "in_sample_rate", ac->sample_rate, 0);
    av_opt_set_int(S->swr_ctx, "out_sample_rate", AUDIO_OUT_RATE, 0);
    av_opt_set_sample_fmt(S->swr_ctx, "in_sample_fmt", ac->sample_fmt, 0);
    av_opt_set_sample_fmt(S->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    if (swr_init(S->swr_ctx) < 0) {
        log_message(LOG_ERROR, MP, "swr_init failed");
        swr_free(&S->swr_ctx);
        S->swr_ctx = nullptr;
    }
}

static void pump_audio() {
    if (!S->audio_enabled || !S->audio_dev || !S->swr_ctx) return;
    if (S->paused.load()) return;
    if (SDL_GetQueuedAudioSize(S->audio_dev) > AUDIO_BUF_MAX_BYTES) return;

    static std::vector<uint8_t> pcm_buf;
    const int bps = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

    while (fq_nb_remaining(&S->sampq) > 0) {
        Frame *af = fq_peek(&S->sampq);
        if (!af || !af->frame) break;
        if (af->serial != S->audioq.serial) {
            fq_next(&S->sampq);
            continue;
        }

        AVFrame *f = af->frame;
        int delay = swr_get_delay(S->swr_ctx, S->audio_avctx->sample_rate);
        int max_out = (int)av_rescale_rnd((int64_t)f->nb_samples + delay, AUDIO_OUT_RATE, S->audio_avctx->sample_rate, AV_ROUND_UP);
        size_t need = (size_t)max_out * AUDIO_OUT_CHANNELS * bps;
        if (pcm_buf.size() < need) pcm_buf.resize(need);

        uint8_t *out = pcm_buf.data();
        int n = swr_convert(S->swr_ctx, &out, max_out, (const uint8_t **)f->data, f->nb_samples);
        if (n < 0) {
            fq_next(&S->sampq);
            continue;
        }
        if (n > 0) {
            SDL_QueueAudio(S->audio_dev, pcm_buf.data(), n * AUDIO_OUT_CHANNELS * bps);
            if (!std::isnan(af->pts)) {
                double pts_end = af->pts + (double)n / AUDIO_OUT_RATE;
                double queued = SDL_GetQueuedAudioSize(S->audio_dev) / (double)(AUDIO_OUT_RATE * AUDIO_OUT_CHANNELS * bps);
                clock_set(&S->audclk, pts_end - queued, af->serial);
                clock_sync_to_slave(&S->extclk, &S->audclk);
            }
        }
        fq_next(&S->sampq);
    }
}

static bool stream_has_enough_packets(AVStream *st, int id, const PacketQueue &q) { return id < 0 || q.abort || (st->disposition & AV_DISPOSITION_ATTACHED_PIC) || (q.nb_packets > MIN_FRAMES && (!q.dur || av_q2d(st->time_base) * q.dur > 1.0)); }

static double get_master_clock() {
    if (!S) return 0.0;
    if (S->audio_enabled && S->audio_dev) {
        double t = clock_get(&S->audclk);
        if (!std::isnan(t) && t > 0.0) return t;
    }
    return S->paused.load() ? S->wall_play_offset : S->wall_play_offset + (wall_now() - S->wall_play_origin);
}

static double vp_duration(Frame *vp, Frame *nextvp) {
    if (vp->serial != nextvp->serial) return 0.0;
    double d = nextvp->pts - vp->pts;
    return (std::isnan(d) || d <= 0 || d > S->max_frame_dur) ? vp->duration : d;
}

static double compute_target_delay(double delay) {
    double diff = clock_get(&S->vidclk) - get_master_clock();
    double thr = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
    if (!std::isnan(diff) && std::fabs(diff) < S->max_frame_dur) {
        if (diff <= -thr)
            delay = FFMAX(0.0, delay + diff);
        else if (diff >= thr && delay > AV_SYNC_FRAMEDUP_THR)
            delay = delay + diff;
        else if (diff >= thr)
            delay = 2.0 * delay;
    }
    return delay;
}

static void video_decode_thread() {
    log_message(LOG_DEBUG, MP, "Video decode thread started");

    PlayerState *ps = S;
    AVRational tb = ps->video_tb;
    AVRational fr = av_guess_frame_rate(ps->fmt_ctx, ps->fmt_ctx->streams[ps->video_idx], nullptr);
    AVFrame *raw = av_frame_alloc();
    if (!raw) {
        log_message(LOG_ERROR, MP, "Video decode thread: av_frame_alloc failed");
        return;
    }

    int total = 0, dropped = 0;

    for (;;) {
        int got = decoder_decode_frame(&ps->viddec, raw);
        if (got < 0) {
            log_message(LOG_DEBUG, MP, "Video decode aborted (dec=%d drp=%d)", total, dropped);
            break;
        }
        if (got == 0) {
            log_message(LOG_DEBUG, MP, "Video decode EOF (dec=%d drp=%d)", total, dropped);
            break;
        }

        VideoFmt expected = ps->hw_decoder ? VideoFmt::NV12 : VideoFmt::YUV420P;
        if (ps->video_fmt.load() == VideoFmt::Unknown) {
            AVPixelFormat fmt = (AVPixelFormat)raw->format;
            if (fmt == AV_PIX_FMT_YUV420P) {
                ps->video_fmt.store(VideoFmt::YUV420P);
                log_message(LOG_OK, MP, "Video fmt: YUV420P (SW decoder)");
            } else if (fmt == AV_PIX_FMT_NV12) {
                ps->video_fmt.store(VideoFmt::NV12);
                log_message(LOG_OK, MP, "Video fmt: NV12 (HW decoder)");
            } else {
                log_message(LOG_ERROR, MP, "Video fmt %d unsupported — only YUV420P and NV12 are handled", (int)fmt);
                av_frame_unref(raw);
                continue;
            }
            (void)expected;
        }

        VideoFmt cur_fmt = ps->video_fmt.load();
        if ((cur_fmt == VideoFmt::YUV420P && raw->format != AV_PIX_FMT_YUV420P) || (cur_fmt == VideoFmt::NV12 && raw->format != AV_PIX_FMT_NV12)) {
            log_message(LOG_WARNING, MP, "Frame fmt %d doesn't match active pipeline %d — skipping", raw->format, (int)cur_fmt);
            av_frame_unref(raw);
            dropped++;
            continue;
        }

        double pts = (raw->pts == AV_NOPTS_VALUE) ? NAN : raw->pts * av_q2d(tb);
        double duration = (fr.num && fr.den) ? av_q2d(AVRational{fr.den, fr.num}) : 0.0;

        Frame *vp = fq_peek_writable(&ps->pictq);
        if (!vp) {
            av_frame_unref(raw);
            break;
        }

        vp->pts = pts;
        vp->duration = duration;
        vp->serial = ps->viddec.pkt_serial;
        vp->width = raw->width;
        vp->height = raw->height;
        vp->uploaded = false;

        av_frame_move_ref(vp->frame, raw);
        fq_push(&ps->pictq);
        total++;
    }

    log_message(LOG_DEBUG, MP, "Video decode thread exiting");
    av_frame_free(&raw);
}

static void audio_decode_thread() {
    log_message(LOG_DEBUG, MP, "Audio decode thread started");
    PlayerState *ps = S;
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        log_message(LOG_ERROR, MP, "Audio decode: av_frame_alloc failed");
        return;
    }
    int total = 0;
    for (;;) {
        int got = decoder_decode_frame(&ps->auddec, frame);
        if (got < 0) {
            log_message(LOG_DEBUG, MP, "Audio decode aborted (dec=%d)", total);
            break;
        }
        if (got == 0) {
            log_message(LOG_DEBUG, MP, "Audio decode EOF (dec=%d)", total);
            break;
        }
        Frame *af = fq_peek_writable(&ps->sampq);
        if (!af) {
            av_frame_unref(frame);
            break;
        }
        AVRational tb = {1, frame->sample_rate};
        af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
        af->serial = ps->auddec.pkt_serial;
        af->duration = av_q2d(AVRational{frame->nb_samples, frame->sample_rate});
        av_frame_move_ref(af->frame, frame);
        fq_push(&ps->sampq);
        total++;
    }
    av_frame_free(&frame);
    log_message(LOG_DEBUG, MP, "Audio decode thread exiting");
}

static void read_thread() {
    log_message(LOG_DEBUG, MP, "Read thread started");
    PlayerState *ps = S;
    AVPacket *pkt = av_packet_alloc();
    int pkts_read = 0;
    for (;;) {
        if (!ps->running.load()) break;
        if (ps->paused.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        {
            std::unique_lock<std::mutex> lk(ps->seek_mtx);
            if (ps->seek_req) {
                int64_t pos = ps->seek_pos;
                ps->seek_req = false;
                lk.unlock();
                if (av_seek_frame(ps->fmt_ctx, -1, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY) >= 0) avformat_flush(ps->fmt_ctx);
                if (ps->video_idx >= 0) {
                    pq_flush_locked(&ps->videoq);
                    pq_start(&ps->videoq);
                }
                if (ps->audio_idx >= 0) {
                    pq_flush_locked(&ps->audioq);
                    pq_start(&ps->audioq);
                }
                ps->eof = false;
                ps->force_refresh.store(true);
                ps->seek_cv.notify_all();
            }
        }

        bool ev = (ps->video_idx < 0) || stream_has_enough_packets(ps->fmt_ctx->streams[ps->video_idx], ps->video_idx, ps->videoq);
        bool ea = (ps->audio_idx < 0) || stream_has_enough_packets(ps->fmt_ctx->streams[ps->audio_idx], ps->audio_idx, ps->audioq);
        if (ev && ea) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        int ret = av_read_frame(ps->fmt_ctx, pkt);
        if (ret == AVERROR_EOF || avio_feof(ps->fmt_ctx->pb)) {
            if (!ps->eof) {
                if (ps->video_idx >= 0) {
                    AVPacket *ep = av_packet_alloc();
                    if (ep) pq_put(&ps->videoq, ep);
                }
                if (ps->audio_idx >= 0) {
                    AVPacket *ep = av_packet_alloc();
                    if (ep) pq_put(&ps->audioq, ep);
                }
                ps->eof = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        if (ret < 0) {
            log_message(LOG_ERROR, MP, "av_read_frame: %d", ret);
            break;
        }

        ps->eof = false;
        pkts_read++;
        if (pkt->stream_index == ps->video_idx)
            pq_put(&ps->videoq, pkt);
        else if (pkt->stream_index == ps->audio_idx)
            pq_put(&ps->audioq, pkt);
        else
            av_packet_unref(pkt);
    }
    log_message(LOG_DEBUG, MP, "Read thread exiting (%d packets)", pkts_read);
    av_packet_free(&pkt);
}

static bool init_video_stream() {
    S->video_idx = av_find_best_stream(S->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (S->video_idx < 0) {
        log_message(LOG_WARNING, MP, "No video stream");
        return false;
    }

    AVStream *st = S->fmt_ctx->streams[S->video_idx];
    S->video_tb = st->time_base;
    S->out_w = st->codecpar->width;
    S->out_h = st->codecpar->height;

    const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);

    if (st->codecpar->codec_id == AV_CODEC_ID_H264) {
        const AVCodec *hw = avcodec_find_decoder_by_name("h264_wiiu");
        if (hw) {
            codec = hw;
            S->hw_decoder = true;
            log_message(LOG_OK, MP, "h264_wiiu HW decoder (%dx%d H.264)", S->out_w, S->out_h);
        }
    }

    if (!codec) {
        log_message(LOG_ERROR, MP, "No decoder for video codec %d", st->codecpar->codec_id);
        return false;
    }

    AVCodecContext *avctx = avcodec_alloc_context3(codec);
    if (!avctx || avcodec_parameters_to_context(avctx, st->codecpar) < 0) {
        log_message(LOG_ERROR, MP, "Video codec context setup failed");
        avcodec_free_context(&avctx);
        return false;
    }
    avctx->pkt_timebase = st->time_base;
    if (!S->hw_decoder) {
        avctx->thread_count = 2;
        avctx->thread_type = FF_THREAD_SLICE;
        avctx->flags2 |= AV_CODEC_FLAG2_FAST;
        avctx->skip_loop_filter = AVDISCARD_NONREF;
    }
    if (avcodec_open2(avctx, codec, nullptr) < 0) {
        log_message(LOG_ERROR, MP, "avcodec_open2 failed for '%s'", codec->name);
        avcodec_free_context(&avctx);
        return false;
    }
    S->video_avctx = avctx;

    log_message(LOG_OK, MP, "Video: stream=%d codec=%s %dx%d tb=%d/%d", S->video_idx, codec->name, S->out_w, S->out_h, st->time_base.num, st->time_base.den);

    S->shader_yuv420p = load_shader(yuv420p_shader, "yuv420p");
    S->shader_nv12 = load_shader(nv12_shader, "nv12");
    if (!S->shader_yuv420p || !S->shader_nv12) {
        log_message(LOG_ERROR, MP, "Shader load failed");
        return false;
    }

    VideoFmt expected_fmt = S->hw_decoder ? VideoFmt::NV12 : VideoFmt::YUV420P;
    if (!init_video_planes(expected_fmt, S->out_w, S->out_h)) {
        log_message(LOG_ERROR, MP, "init_video_planes failed");
        return false;
    }

    S->dest_rect = display_calculate_aspect_fit(S->out_w, S->out_h);
    update_quad(S->dest_rect);
    S->dest_rect_init = true;

    if (fq_init(&S->pictq, &S->videoq, VIDEO_FRAME_QUEUE_SIZE, 1) < 0) return false;
    if (decoder_init(&S->viddec, avctx, &S->videoq) < 0) return false;

    S->cur_frame_info = new frame_info{};
    S->cur_frame_info->width = S->out_w;
    S->cur_frame_info->height = S->out_h;
    return true;
}

static bool init_audio_stream() {
    S->audio_idx = av_find_best_stream(S->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (S->audio_idx < 0) {
        log_message(LOG_WARNING, MP, "No audio stream (continuing without audio)");
        return true;
    }
    AVStream *st = S->fmt_ctx->streams[S->audio_idx];
    const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) {
        log_message(LOG_ERROR, MP, "No audio decoder");
        return false;
    }

    AVCodecContext *avctx = avcodec_alloc_context3(codec);
    if (!avctx || avcodec_parameters_to_context(avctx, st->codecpar) < 0 || (avctx->pkt_timebase = st->time_base, false) || avcodec_open2(avctx, codec, nullptr) < 0) {
        log_message(LOG_ERROR, MP, "Audio codec setup failed");
        avcodec_free_context(&avctx);
        return false;
    }
    S->audio_avctx = avctx;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
    int nch = avctx->ch_layout.nb_channels;
#else
    int nch = avctx->channels;
#endif
    log_message(LOG_OK, MP, "Audio: stream=%d codec=%s %dHz %dch", S->audio_idx, codec->name, avctx->sample_rate, nch);

    if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_AudioSpec want{};
    want.freq = AUDIO_OUT_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = AUDIO_OUT_CHANNELS;
    want.samples = 4096;
    S->audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &S->audio_spec, 0);
    if (!S->audio_dev) {
        log_message(LOG_ERROR, MP, "SDL_OpenAudioDevice: %s", SDL_GetError());
        avcodec_free_context(&avctx);
        return false;
    }
    rebuild_swr();
    if (!S->swr_ctx) {
        avcodec_free_context(&avctx);
        return false;
    }
    if (fq_init(&S->sampq, &S->audioq, AUDIO_FRAME_QUEUE_SIZE, 1) < 0) return false;
    if (decoder_init(&S->auddec, avctx, &S->audioq) < 0) return false;

    S->cur_audio_track = S->audio_idx;
    S->audio_enabled = true;
    SDL_PauseAudioDevice(S->audio_dev, 1);

    {
        std::lock_guard<std::mutex> lk(S->audio_tracks_mtx);
        S->audio_tracks.clear();
        for (unsigned i = 0; i < S->fmt_ctx->nb_streams; ++i) {
            AVStream *s = S->fmt_ctx->streams[i];
            if (s->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) continue;
            AVDictionaryEntry *lang = av_dict_get(s->metadata, "language", nullptr, 0);
            S->audio_tracks.push_back({(int)i, avcodec_get_name(s->codecpar->codec_id),
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
                                       s->codecpar->ch_layout.nb_channels,
#else
                                       s->codecpar->channels,
#endif
                                       s->codecpar->sample_rate, lang ? lang->value : "und"});
        }
        media_info_get()->total_audio_track_count = (int)S->audio_tracks.size();
    }
    log_message(LOG_OK, MP, "%d audio track(s)", (int)S->audio_tracks.size());
    return true;
}

int media_player_init(const char *path) {
    log_message(LOG_DEBUG, MP, "media_player_init: %s", path);
    if (S) {
        log_message(LOG_WARNING, MP, "Re-init: cleaning up");
        media_player_cleanup();
    }

    display_get() = display_get();

    if (!g_flush_pkt) {
        g_flush_pkt = av_packet_alloc();
        if (!g_flush_pkt) {
            log_message(LOG_ERROR, MP, "flush sentinel alloc failed");
            return -1;
        }
        static uint8_t sentinel = 0;
        g_flush_pkt->data = &sentinel;
        g_flush_pkt->size = 0;
    }

    S = new PlayerState{};
    avformat_network_init();

    S->fmt_ctx = avformat_alloc_context();
    if (!S->fmt_ctx) {
        delete S;
        S = nullptr;
        return -1;
    }

    {
        int err = avformat_open_input(&S->fmt_ctx, path, nullptr, nullptr);
        if (err < 0) {
            char buf[256]{};
            av_strerror(err, buf, sizeof(buf));
            log_message(LOG_ERROR, MP, "avformat_open_input: [%d] %s", err, buf);
            avformat_free_context(S->fmt_ctx);
            S->fmt_ctx = nullptr;
            delete S;
            S = nullptr;
            return -1;
        }
    }
    S->fmt_ctx->flags |= AVFMT_FLAG_NOBUFFER;
    S->fmt_ctx->probesize = 32 * 1024;
    S->fmt_ctx->max_analyze_duration = AV_TIME_BASE / 2;

    if (avformat_find_stream_info(S->fmt_ctx, nullptr) < 0) {
        log_message(LOG_ERROR, MP, "avformat_find_stream_info failed");
        avformat_close_input(&S->fmt_ctx);
        delete S;
        S = nullptr;
        return -1;
    }
    log_message(LOG_OK, MP, "Container: fmt=%s streams=%u dur=%.2f s", S->fmt_ctx->iformat->name, S->fmt_ctx->nb_streams, S->fmt_ctx->duration / (double)AV_TIME_BASE);

    S->max_frame_dur = (S->fmt_ctx->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;
    pq_init(&S->videoq);
    pq_init(&S->audioq);

    bool has_v = init_video_stream();
    bool has_a = init_audio_stream();
    if (!has_v && !has_a) {
        media_player_cleanup();
        return -1;
    }

    if (has_v) pq_start(&S->videoq);
    if (has_a) pq_start(&S->audioq);

    clock_init(&S->audclk, &S->audioq.serial);
    clock_init(&S->vidclk, &S->videoq.serial);
    clock_init(&S->extclk, nullptr);

    S->frame_timer = S->wall_play_origin = wall_now();
    S->wall_play_offset = 0.0;
    S->running.store(true);
    S->paused.store(true);
    S->playing.store(false);

    S->read_tid = std::thread(read_thread);
    if (has_v) S->video_tid = std::thread(video_decode_thread);
    if (has_a) S->audio_tid = std::thread(audio_decode_thread);

    media_info_get()->playback_status = false;
    if (has_v && S->video_idx >= 0) {
        AVStream *vs = S->fmt_ctx->streams[S->video_idx];
        double dur = vs->duration != AV_NOPTS_VALUE ? vs->duration * av_q2d(vs->time_base) : 0.0;
        media_info_get()->total_playback_time = dur;
    }
    if (has_a && S->audio_idx >= 0) {
        AVStream *as = S->fmt_ctx->streams[S->audio_idx];
        int64_t dur = as->duration != AV_NOPTS_VALUE ? (int64_t)(as->duration * av_q2d(as->time_base)) : 0;
        media_info_get()->total_playback_time = dur;
    }
    log_message(LOG_OK, MP, "media_player_init complete");
    return 0;
}

void media_player_play(bool play) {
    if (!S) return;
    if (play && S->paused.load()) {
        S->frame_timer += wall_now() - S->vidclk.last_upd;
        S->vidclk.paused = false;
        S->wall_play_origin = wall_now();
        clock_set(&S->vidclk, clock_get(&S->vidclk), S->vidclk.serial);
    } else if (!play && !S->paused.load()) {
        S->wall_play_offset = get_master_clock();
    }
    clock_set(&S->extclk, clock_get(&S->extclk), S->extclk.serial);
    S->audclk.paused = S->vidclk.paused = S->extclk.paused = !play;
    S->paused.store(!play);
    S->playing.store(play);
    media_info_get()->playback_status = play;
    if (S->audio_dev) SDL_PauseAudioDevice(S->audio_dev, play ? 0 : 1);
    log_message(LOG_DEBUG, MP, "media_player_play(%s) clock=%.3f s", play ? "true" : "false", get_master_clock());
}

void media_player_seek(double seconds) {
    if (!S || !S->fmt_ctx) return;
    bool was_playing = S->playing.load();
    if (was_playing) media_player_play(false);
    S->wall_play_offset = seconds;
    S->wall_play_origin = wall_now();
    {
        std::lock_guard<std::mutex> lk(S->seek_mtx);
        S->seek_pos = (int64_t)(seconds * AV_TIME_BASE);
        S->seek_req = true;
        if (S->audio_dev) SDL_ClearQueuedAudio(S->audio_dev);
        clock_set(&S->audclk, seconds, S->audioq.serial);
        clock_set(&S->vidclk, seconds, S->videoq.serial);
        clock_set(&S->extclk, seconds, S->extclk.serial);
        S->frame_timer = wall_now();
    }
    S->seek_cv.notify_all();
    if (was_playing) media_player_play(true);
}

void media_player_update() {
    if (!S) return;

    media_info_get()->current_playback_time = get_master_clock();
    pump_audio();
    if (!S->video_avctx) return;

    if (S->playing.load()) {
        double now = wall_now();
        if (now - S->last_log_time >= 5.0) {
            log_message(LOG_DEBUG, MP, "clock=%.2f vq=%d aq=%d pictq=%d sampq=%d dec=%d drp=%d fmt=%d", get_master_clock(), S->videoq.nb_packets, S->audioq.nb_packets, fq_nb_remaining(&S->pictq), fq_nb_remaining(&S->sampq), S->frames_decoded, S->frames_dropped, (int)S->video_fmt.load());
            S->last_log_time = now;
        }
    }

    VideoFmt fmt = S->video_fmt.load();
    if (fmt == VideoFmt::Unknown) return;

retry:
    if (fq_nb_remaining(&S->pictq) > 0) {
        Frame *lastvp = fq_peek_last(&S->pictq);
        Frame *vp = fq_peek(&S->pictq);

        if (vp->serial != S->videoq.serial) {
            fq_next(&S->pictq);
            goto retry;
        }
        if (lastvp->serial != vp->serial) S->frame_timer = wall_now();
        if (!S->playing.load()) goto display;

        double delay = compute_target_delay(vp_duration(lastvp, vp));
        double now = wall_now();
        if (now < S->frame_timer + delay) goto display;

        S->frame_timer += delay;
        if (delay > 0 && now - S->frame_timer > AV_SYNC_THRESHOLD_MAX) S->frame_timer = now;

        {
            std::lock_guard<std::mutex> lk(S->pictq.mtx);
            if (!std::isnan(vp->pts)) {
                clock_set(&S->vidclk, vp->pts, vp->serial);
                clock_sync_to_slave(&S->extclk, &S->vidclk);
            }
        }

        if (fq_nb_remaining(&S->pictq) > 1) {
            Frame *nextvp = fq_peek_next(&S->pictq);
            if (now > S->frame_timer + vp_duration(vp, nextvp)) {
                S->frames_dropped++;
                fq_next(&S->pictq);
                S->force_refresh.store(false);
                goto retry;
            }
        }
        fq_next(&S->pictq);
        S->force_refresh.store(true);
        S->frames_decoded++;
    }

display:
    if (!S->force_refresh.load()) return;
    S->force_refresh.store(false);
    if (S->pictq.rindex_shown == 0) return;

    Frame *vp = fq_peek_last(&S->pictq);
    if (!vp || !vp->frame || !vp->frame->data[0]) return;

    if (!vp->uploaded) {
        video_upload_frame(vp->frame);
        vp->uploaded = true;
    }

    rect dest = display_calculate_aspect_fit(vp->width, vp->height);
    if (fmt == VideoFmt::YUV420P)
        video_render_yuv420p(dest);
    else
        video_render_nv12(dest);
}

bool media_player_switch_audio_track(int new_idx) {
    if (!S || !S->audio_enabled) return false;
    if (new_idx < 0 || new_idx >= (int)S->fmt_ctx->nb_streams) return false;
    if (S->fmt_ctx->streams[new_idx]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) return false;
    if (new_idx == S->audio_idx) return true;

    double t = get_master_clock();
    bool was_playing = S->playing.load();
    media_player_play(false);
    if (S->audio_dev) {
        SDL_PauseAudioDevice(S->audio_dev, 1);
        SDL_ClearQueuedAudio(S->audio_dev);
    }

    decoder_abort(&S->auddec, &S->sampq);
    if (S->audio_tid.joinable()) S->audio_tid.join();
    decoder_free_pkt(&S->auddec);

    AVStream *st = S->fmt_ctx->streams[new_idx];
    const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) return false;

    AVCodecContext *avctx = avcodec_alloc_context3(codec);
    if (!avctx || avcodec_parameters_to_context(avctx, st->codecpar) < 0 || avcodec_open2(avctx, codec, nullptr) < 0) {
        avcodec_free_context(&avctx);
        return false;
    }
    avctx->pkt_timebase = st->time_base;
    avcodec_free_context(&S->audio_avctx);
    S->audio_avctx = avctx;

    rebuild_swr();
    if (!S->swr_ctx) {
        avcodec_free_context(&S->audio_avctx);
        return false;
    }

    S->audio_idx = S->cur_audio_track = new_idx;
    pq_flush_locked(&S->audioq);
    pq_start(&S->audioq);
    fq_destroy(&S->sampq);
    fq_init(&S->sampq, &S->audioq, AUDIO_FRAME_QUEUE_SIZE, 1);
    decoder_init(&S->auddec, avctx, &S->audioq);
    {
        std::lock_guard<std::mutex> lk(S->seek_mtx);
        S->seek_pos = (int64_t)(t * AV_TIME_BASE);
        S->seek_req = true;
    }
    S->seek_cv.notify_all();
    S->audio_tid = std::thread(audio_decode_thread);
    if (was_playing) {
        if (S->audio_dev) SDL_PauseAudioDevice(S->audio_dev, 0);
        media_player_play(true);
    }
    return true;
}

std::vector<AudioTrackInfo> media_player_get_audio_tracks() {
    if (!S) return {};
    std::lock_guard<std::mutex> lk(S->audio_tracks_mtx);
    return S->audio_tracks;
}

double media_player_get_current_time() { return S ? get_master_clock() : 0.0; }
bool media_player_is_playing() { return S && S->playing.load(); }
int media_player_get_current_audio_track() { return S ? S->cur_audio_track : -1; }

double media_player_get_total_time() {
    if (!S || !S->fmt_ctx) return 0.0;
    auto dur = [](int i) {
        AVStream *s = S->fmt_ctx->streams[i];
        return s->duration != AV_NOPTS_VALUE ? s->duration * av_q2d(s->time_base) : -1.0;
    };
    if (S->video_idx >= 0) {
        double d = dur(S->video_idx);
        if (d >= 0) return d;
    }
    if (S->audio_idx >= 0) {
        double d = dur(S->audio_idx);
        if (d >= 0) return d;
    }
    return S->fmt_ctx->duration != AV_NOPTS_VALUE ? S->fmt_ctx->duration / (double)AV_TIME_BASE : 0.0;
}

void media_player_cleanup() {
    if (!S) return;
    log_message(LOG_DEBUG, MP, "cleanup: dec=%d drp=%d clock=%.2f s", S->frames_decoded, S->frames_dropped, get_master_clock());

    S->running.store(false);
    S->playing.store(false);
    S->paused.store(true);
    pq_abort(&S->videoq);
    fq_signal(&S->pictq);
    pq_abort(&S->audioq);
    fq_signal(&S->sampq);
    {
        std::lock_guard<std::mutex> lk(S->seek_mtx);
        S->seek_req = false;
    }
    S->seek_cv.notify_all();

    if (S->read_tid.joinable()) S->read_tid.join();
    if (S->video_tid.joinable()) S->video_tid.join();
    if (S->audio_tid.joinable()) S->audio_tid.join();

    fq_destroy(&S->pictq);
    fq_destroy(&S->sampq);
    pq_destroy(&S->videoq);
    pq_destroy(&S->audioq);
    decoder_free_pkt(&S->viddec);
    decoder_free_pkt(&S->auddec);
    avcodec_free_context(&S->video_avctx);
    avcodec_free_context(&S->audio_avctx);

    if (S->audio_dev) {
        SDL_PauseAudioDevice(S->audio_dev, 1);
        SDL_ClearQueuedAudio(S->audio_dev);
        SDL_CloseAudioDevice(S->audio_dev);
    }
    if (S->swr_ctx) swr_free(&S->swr_ctx);

    free_video_planes();
    free_shader(S->shader_yuv420p);
    free_shader(S->shader_nv12);
    free(S->quad_vtx);
    S->quad_vtx = nullptr;

    if (S->cur_frame_info) delete S->cur_frame_info;
    if (S->fmt_ctx) avformat_close_input(&S->fmt_ctx);
    if (SDL_WasInit(SDL_INIT_AUDIO)) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    avformat_network_deinit();

    delete S;
    S = nullptr;
    log_message(LOG_OK, MP, "media_player_cleanup complete");
}
