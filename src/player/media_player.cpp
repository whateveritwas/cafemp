// Architecture loosely based on FFmpeg's ffplay.c (Copyright (c) 2003 Fabrice Bellard, LGPL 2.1+).

#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <condition_variable>
#include <chrono>
#include <unistd.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <SDL2/SDL.h>

#include "utils/utils.hpp"
#include "utils/media_info.hpp"
#include "utils/sdl.hpp"
#include "logger/logger.hpp"
#include "player/media_player.hpp"

#define MP "MediaPlayer"


#define VIDEO_FRAME_QUEUE_SIZE 4
#define AUDIO_FRAME_QUEUE_SIZE 9
#define MIN_FRAMES 8

#define AV_SYNC_THRESHOLD_MIN 0.04
#define AV_SYNC_THRESHOLD_MAX 0.10
#define AV_SYNC_FRAMEDUP_THR 0.10
#define AV_NOSYNC_THRESHOLD 10.0

#define PIX_FMT_TARGET AV_PIX_FMT_YUV420P
#define AUDIO_OUT_CHANNELS 2
#define AUDIO_OUT_RATE 48000
#define AUDIO_BUF_MAX_BYTES (768 * 1024)


static double wall_now() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}


struct PktNode {
    AVPacket* pkt;
    int serial;
    PktNode*  next;
};

struct PacketQueue {
    PktNode* head = nullptr;
    PktNode* tail = nullptr;
    int nb_packets = 0;
    int size = 0;
    int64_t dur = 0;
    int serial = 0;
    bool abort = false;
    std::mutex mtx;
    std::condition_variable cond;
};

static AVPacket* g_flush_pkt = nullptr;

static void pq_init(PacketQueue* q) {
    q->head = q->tail = nullptr;
    q->nb_packets = 0;
    q->size = 0;
    q->dur = 0;
    q->serial = 0;
    q->abort = true;
}

static void pq_put_private(PacketQueue* q, AVPacket* pkt) {
    if (q->abort) { av_packet_free(&pkt); return; }
    PktNode* n = new PktNode{};
    n->pkt    = pkt;
    n->serial = (pkt->data == g_flush_pkt->data) ? (++q->serial) : q->serial;
    n->next   = nullptr;
    if (q->tail) q->tail->next = n; else q->head = n;
    q->tail = n;
    q->nb_packets++;
    q->size += pkt->size + (int)sizeof(*n);
    q->dur  += pkt->duration;
    q->cond.notify_one();
}

static bool pq_put(PacketQueue* q, AVPacket* pkt) {
    AVPacket* p = av_packet_alloc();
    if (!p) { av_packet_unref(pkt); return false; }
    av_packet_move_ref(p, pkt);
    std::lock_guard<std::mutex> lk(q->mtx);
    if (q->abort) { av_packet_free(&p); return false; }
    pq_put_private(q, p);
    return true;
}

static int pq_get(PacketQueue* q, AVPacket* pkt, bool block, int* serial_out) {
    std::unique_lock<std::mutex> lk(q->mtx);
    for (;;) {
        if (q->abort) return -1;
        if (q->head) {
            PktNode* n = q->head;
            q->head = n->next;
            if (!q->head) q->tail = nullptr;
            q->nb_packets--;
            q->size -= n->pkt->size + (int)sizeof(*n);
            q->dur  -= n->pkt->duration;
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

static void pq_flush_locked(PacketQueue* q) {
    std::lock_guard<std::mutex> lk(q->mtx);
    while (q->head) {
        PktNode* n = q->head; q->head = n->next;
        av_packet_free(&n->pkt); delete n;
    }
    q->tail = nullptr; q->nb_packets = 0; q->size = 0; q->dur = 0;
    q->serial++;
}

static void pq_abort(PacketQueue* q) {
    std::lock_guard<std::mutex> lk(q->mtx);
    q->abort = true;
    q->cond.notify_all();
}

static void pq_start(PacketQueue* q) {
    std::lock_guard<std::mutex> lk(q->mtx);
    q->abort = false;
    AVPacket* fp = av_packet_alloc();
    if (fp) {
        fp->data = g_flush_pkt->data;
        fp->size = 0;
        pq_put_private(q, fp);
    }
}

static void pq_destroy(PacketQueue* q) { pq_flush_locked(q); }


struct Frame {
    AVFrame* frame = nullptr;
    double pts = 0.0;
    double duration = 0.0;
    int serial = 0;
    int width = 0;
    int height = 0;
    bool uploaded = false;
};

struct FrameQueue {
    Frame buf[16];
    int rindex = 0;
    int windex = 0;
    int size = 0;
    int max_size = 0;
    int keep_last = 0;
    int rindex_shown = 0;
    std::mutex mtx;
    std::condition_variable cond;
    PacketQueue* pktq = nullptr;
};

static int fq_init(FrameQueue* f, PacketQueue* pktq, int max_size, int keep_last) {
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

static void fq_destroy(FrameQueue* f) {
    for (int i = 0; i < f->max_size; ++i) {
        av_frame_unref(f->buf[i].frame);
        av_frame_free(&f->buf[i].frame);
    }
}

static void fq_signal(FrameQueue* f) {
    std::lock_guard<std::mutex> lk(f->mtx);
    f->cond.notify_all();
}

static Frame* fq_peek(FrameQueue* f) {
    return &f->buf[(f->rindex + f->rindex_shown) % f->max_size];
}
static Frame* fq_peek_next(FrameQueue* f) {
    return &f->buf[(f->rindex + f->rindex_shown + 1) % f->max_size];
}
static Frame* fq_peek_last(FrameQueue* f) {
    return &f->buf[f->rindex];
}

static Frame* fq_peek_writable(FrameQueue* f) {
    std::unique_lock<std::mutex> lk(f->mtx);
    f->cond.wait(lk, [f]{ return f->size < f->max_size || f->pktq->abort; });
    if (f->pktq->abort) return nullptr;
    return &f->buf[f->windex];
}

static void fq_push(FrameQueue* f) {
    if (++f->windex == f->max_size) f->windex = 0;
    std::lock_guard<std::mutex> lk(f->mtx);
    f->size++;
    f->cond.notify_one();
}


static void fq_next(FrameQueue* f) {
    if (f->keep_last && !f->rindex_shown) { f->rindex_shown = 1; return; }
    av_frame_unref(f->buf[f->rindex].frame);
    f->buf[f->rindex].uploaded = false;
    if (++f->rindex == f->max_size) f->rindex = 0;
    std::lock_guard<std::mutex> lk(f->mtx);
    f->size--;
    f->cond.notify_one();
}

static int fq_nb_remaining(FrameQueue* f) {
    return f->size - f->rindex_shown;
}

struct Clock {
    double pts = NAN;
    double pts_drift = 0.0;
    double last_upd = 0.0;
    double speed = 1.0;
    int serial = -1;
    bool paused = false;
    int* q_serial = nullptr;
};

static double clock_get(const Clock* c) {
    if (c->q_serial && *c->q_serial != c->serial) return NAN;
    if (c->paused) return c->pts;
    double t = wall_now();
    return c->pts_drift + t - (t - c->last_upd) * (1.0 - c->speed);
}

static void clock_set_at(Clock* c, double pts, int serial, double t) {
    c->pts = pts; c->last_upd = t; c->pts_drift = pts - t; c->serial = serial;
}
static void clock_set(Clock* c, double pts, int serial) {
    clock_set_at(c, pts, serial, wall_now());
}

static void clock_init(Clock* c, int* q_serial) {
    c->speed = 1.0;
    c->paused = true;
    c->q_serial = q_serial;
    int s = q_serial ? *q_serial : 0;
    clock_set_at(c, 0.0, s, wall_now());
}

static void clock_sync_to_slave(Clock* c, const Clock* slave) {
    double mt = clock_get(c), st = clock_get(slave);
    if (!std::isnan(st) && (std::isnan(mt) || std::fabs(mt - st) > AV_NOSYNC_THRESHOLD)) clock_set(c, st, slave->serial);
}


struct Decoder {
    AVPacket* pkt = nullptr;
    PacketQueue* queue = nullptr;
    AVCodecContext* avctx = nullptr;
    int pkt_serial = -1;
    int finished = 0;
    int packet_pending = 0;
    int64_t start_pts = AV_NOPTS_VALUE;
    AVRational start_pts_tb = {0, 1};
    int64_t next_pts = AV_NOPTS_VALUE;
    AVRational next_pts_tb = {0, 1};
};

static int decoder_init(Decoder* d, AVCodecContext* avctx, PacketQueue* queue) {
    d->pkt = av_packet_alloc();
    if (!d->pkt) return AVERROR(ENOMEM);
    d->avctx          = avctx;
    d->queue          = queue;
    d->pkt_serial     = -1;
    d->finished       = 0;
    d->packet_pending = 0;
    d->start_pts      = AV_NOPTS_VALUE;
    d->start_pts_tb   = {0, 1};
    d->next_pts       = AV_NOPTS_VALUE;
    d->next_pts_tb    = {0, 1};
    return 0;
}

static void decoder_free_pkt(Decoder* d) { av_packet_free(&d->pkt); }

static void decoder_abort(Decoder* d, FrameQueue* fq) {
    pq_abort(d->queue);
    fq_signal(fq);
}

static int decoder_decode_frame(Decoder* d, AVFrame* frame) {
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
                        if (frame->pts != AV_NOPTS_VALUE) frame->pts = av_rescale_q(frame->pts, d->avctx->pkt_timebase, tb);
                        else if (d->next_pts != AV_NOPTS_VALUE) frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
                        if (frame->pts != AV_NOPTS_VALUE) {
                            d->next_pts = frame->pts + frame->nb_samples;
                            d->next_pts_tb = tb;
                        }
                    }
                    break;
                default: break;
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
                if (pq_get(d->queue, d->pkt, true, &d->pkt_serial) < 0)
                    return -1;
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

        if (avcodec_send_packet(d->avctx, d->pkt) == AVERROR(EAGAIN)) d->packet_pending = 1;
        else av_packet_unref(d->pkt);
    }
}


struct PlayerState {
    AVFormatContext* fmt_ctx = nullptr;
    int video_idx = -1;
    int audio_idx = -1;
    AVRational video_tb = {0, 1};

    AVCodecContext* video_avctx = nullptr;
    AVCodecContext* audio_avctx = nullptr;

    PacketQueue videoq, audioq;
    FrameQueue  pictq, sampq;
    Decoder viddec, auddec;

    Clock audclk, vidclk, extclk;

    double wall_play_origin = 0.0;
    double wall_play_offset = 0.0;

    SwsContext* sws_ctx = nullptr;
    int sws_src_w = 0, sws_src_h = 0;
    SwrContext* swr_ctx = nullptr;

    SDL_AudioDeviceID audio_dev = 0;
    SDL_AudioSpec audio_spec = {};
    bool audio_enabled= false;

    frame_info* cur_frame_info = nullptr;
    SDL_Rect dest_rect = {0,0,0,0};
    bool dest_rect_init = false;
    int out_w = 0, out_h = 0;
    bool hw_decoder = false;

    double frame_timer = 0.0;
    double max_frame_dur = 3600.0;

    std::atomic<bool> running {false};
    std::atomic<bool> playing {false};
    std::atomic<bool> paused {true};
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

    // Stats for periodic debug logging
    int frames_decoded = 0;
    int frames_dropped = 0;
    double last_log_time = 0.0;
};

static PlayerState* S = nullptr;


static void rebuild_swr();


static bool stream_has_enough_packets(AVStream* st, int stream_id, const PacketQueue& q) {
    return stream_id < 0 || q.abort || (st->disposition & AV_DISPOSITION_ATTACHED_PIC) || (q.nb_packets > MIN_FRAMES && (!q.dur || av_q2d(st->time_base) * q.dur > 1.0));
}


static double get_master_clock() {
    if (!S) return 0.0;
    if (S->audio_enabled && S->audio_dev) {
        double t = clock_get(&S->audclk);
        if (!std::isnan(t) && t > 0.0) return t;
    }
    if (S->paused.load())
        return S->wall_play_offset;
    return S->wall_play_offset + (wall_now() - S->wall_play_origin);
}


static AVFrame* to_yuv420p(AVFrame* src) {
    int dst_w = (S->hw_decoder && S->out_w) ? S->out_w : src->width;
    int dst_h = (S->hw_decoder && S->out_h) ? S->out_h : src->height;

    if (!S->sws_ctx || S->sws_src_w != src->width || S->sws_src_h != src->height) {
        sws_freeContext(S->sws_ctx);
        S->sws_ctx = sws_getContext(src->width, src->height, (AVPixelFormat)src->format, dst_w, dst_h, PIX_FMT_TARGET, SWS_BILINEAR, nullptr, nullptr, nullptr);
        S->sws_src_w = src->width;
        S->sws_src_h = src->height;
        if (!S->sws_ctx) {
            log_message(LOG_ERROR, MP, "sws_getContext failed for %dx%d fmt=%d", src->width, src->height, src->format);
            return nullptr;
        }
        log_message(LOG_DEBUG, MP, "SwsContext created: %dx%d -> %dx%d", src->width, src->height, dst_w, dst_h);
    }
    AVFrame* dst = av_frame_alloc();
    dst->format = PIX_FMT_TARGET; dst->width = dst_w; dst->height = dst_h;
    if (av_frame_get_buffer(dst, 32) < 0) {
        log_message(LOG_ERROR, MP, "av_frame_get_buffer failed for converted frame");
        av_frame_free(&dst); return nullptr;
    }
    sws_scale(S->sws_ctx, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);
    dst->pts = src->pts;
    dst->best_effort_timestamp = src->best_effort_timestamp;
    return dst;
}


static void video_decode_thread() {
    log_message(LOG_DEBUG, MP, "Video decode thread started");

    PlayerState* ps = S;
    AVRational   tb = ps->video_tb;
    AVRational   fr = av_guess_frame_rate(ps->fmt_ctx,
					  ps->fmt_ctx->streams[ps->video_idx], nullptr);
    AVFrame* raw = av_frame_alloc();
    if (!raw) {
        log_message(LOG_ERROR, MP, "Video decode thread: av_frame_alloc failed");
        return;
    }

    int total = 0, dropped = 0;

    for (;;) {
        int got = decoder_decode_frame(&ps->viddec, raw);
        if (got < 0) {
            log_message(LOG_DEBUG, MP, "Video decode thread aborted (decoded=%d dropped=%d)", total, dropped);
            break;
        }
        if (got == 0) {
            log_message(LOG_DEBUG, MP, "Video decode thread EOF (decoded=%d dropped=%d)", total, dropped);
            break;
        }

        bool needs_conv = ps->hw_decoder || (raw->format != PIX_FMT_TARGET);
        AVFrame* f = needs_conv ? to_yuv420p(raw) : av_frame_clone(raw);
        av_frame_unref(raw);
        if (!f) {
            log_message(LOG_WARNING, MP, "Video frame conversion failed, skipping");
            dropped++;
            continue;
        }

        double pts = (f->pts == AV_NOPTS_VALUE) ? NAN : f->pts * av_q2d(tb);
        double duration = (fr.num && fr.den) ? av_q2d(AVRational{fr.den, fr.num}) : 0.0;

        Frame* vp = fq_peek_writable(&ps->pictq);
        if (!vp) { av_frame_free(&f); break; }

        vp->pts = pts;
        vp->duration = duration;
        vp->serial = ps->viddec.pkt_serial;
        vp->width = f->width;
        vp->height = f->height;
        vp->uploaded = false;
        av_frame_move_ref(vp->frame, f);
        av_frame_free(&f);
        fq_push(&ps->pictq);
        total++;
    }

    log_message(LOG_DEBUG, MP, "Video decode thread exiting");
    av_frame_free(&raw);
}


static void audio_decode_thread() {
    log_message(LOG_DEBUG, MP, "Audio decode thread started");

    PlayerState* ps = S;
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        log_message(LOG_ERROR, MP, "Audio decode thread: av_frame_alloc failed");
        return;
    }

    int total = 0;

    for (;;) {
        int got = decoder_decode_frame(&ps->auddec, frame);
        if (got < 0) {
            log_message(LOG_DEBUG, MP, "Audio decode thread aborted (decoded=%d)", total);
            break;
        }
        if (got == 0) {
            log_message(LOG_DEBUG, MP, "Audio decode thread EOF (decoded=%d)", total);
            break;
        }

        Frame* af = fq_peek_writable(&ps->sampq);
        if (!af) { av_frame_unref(frame); break; }

        AVRational tb = {1, frame->sample_rate};
        af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
        af->serial = ps->auddec.pkt_serial;
        af->duration = av_q2d(AVRational{frame->nb_samples, frame->sample_rate});
        av_frame_move_ref(af->frame, frame);
        fq_push(&ps->sampq);
        total++;
    }

    log_message(LOG_DEBUG, MP, "Audio decode thread exiting");
    av_frame_free(&frame);
}


static void rebuild_swr() {
    if (S->swr_ctx) { swr_free(&S->swr_ctx); S->swr_ctx = nullptr; }
    AVCodecContext* ac = S->audio_avctx;
    if (!ac) return;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59,37,100)
    int64_t in_ch = ac->ch_layout.nb_channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    int in_chc = ac->ch_layout.nb_channels;
#else
    int64_t in_ch  = av_get_default_channel_layout(ac->channels);
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
    av_opt_set_sample_fmt(S->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16,0);
    if (swr_init(S->swr_ctx) < 0) {
        log_message(LOG_ERROR, MP, "swr_init failed (in: %d Hz %dch fmt=%d -> out: %d Hz %dch s16)", ac->sample_rate, in_chc, (int)ac->sample_fmt, AUDIO_OUT_RATE, AUDIO_OUT_CHANNELS);
        swr_free(&S->swr_ctx); S->swr_ctx = nullptr;
        return;
    }
    log_message(LOG_DEBUG, MP, "SwrContext ready: %d Hz %dch fmt=%d -> %d Hz %dch s16", ac->sample_rate, in_chc, (int)ac->sample_fmt, AUDIO_OUT_RATE, AUDIO_OUT_CHANNELS);
}


static void pump_audio() {
    if (!S->audio_enabled || !S->audio_dev || !S->swr_ctx) return;
    if (S->paused.load()) return;
    if (SDL_GetQueuedAudioSize(S->audio_dev) > AUDIO_BUF_MAX_BYTES) return;

    static std::vector<uint8_t> pcm_buf;
    const int bps = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

    while (fq_nb_remaining(&S->sampq) > 0) {
        Frame* af = fq_peek(&S->sampq);
        if (!af || !af->frame) break;
        if (af->serial != S->audioq.serial) {
            log_message(LOG_DEBUG, MP, "pump_audio: discarding stale audio frame (serial %d != queue %d)", af->serial, S->audioq.serial);
            fq_next(&S->sampq); continue;
        }

        AVFrame* f  = af->frame;
        int delay   = swr_get_delay(S->swr_ctx, S->audio_avctx->sample_rate);
        int max_out = (int)av_rescale_rnd((int64_t)f->nb_samples + delay, AUDIO_OUT_RATE, S->audio_avctx->sample_rate, AV_ROUND_UP);
        size_t need = (size_t)max_out * AUDIO_OUT_CHANNELS * bps;
        if (pcm_buf.size() < need) pcm_buf.resize(need);

        uint8_t* out = pcm_buf.data();
        int n = swr_convert(S->swr_ctx, &out, max_out, (const uint8_t**)f->data, f->nb_samples);
        if (n < 0) {
            log_message(LOG_ERROR, MP, "swr_convert failed: %d", n);
            fq_next(&S->sampq); continue;
        }
        if (n > 0) {
            SDL_QueueAudio(S->audio_dev, pcm_buf.data(), n * AUDIO_OUT_CHANNELS * bps);
            if (!std::isnan(af->pts)) {
                double pts_end = af->pts + (double)n / AUDIO_OUT_RATE;
                double queued  = SDL_GetQueuedAudioSize(S->audio_dev) / (double)(AUDIO_OUT_RATE * AUDIO_OUT_CHANNELS * bps);
                double clk = pts_end - queued;
                clock_set(&S->audclk, clk, af->serial);
                clock_sync_to_slave(&S->extclk, &S->audclk);
            }
        }
        fq_next(&S->sampq);
    }
}


static double vp_duration(Frame* vp, Frame* nextvp) {
    if (vp->serial != nextvp->serial) return 0.0;
    double dur = nextvp->pts - vp->pts;
    if (std::isnan(dur) || dur <= 0 || dur > S->max_frame_dur) return vp->duration;
    return dur;
}

static double compute_target_delay(double delay) {
    double master = get_master_clock();
    double diff = clock_get(&S->vidclk) - master;
    double sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
    if (!std::isnan(diff) && std::fabs(diff) < S->max_frame_dur) {
        if (diff <= -sync_threshold) delay = FFMAX(0.0, delay + diff);
        else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THR) delay = delay + diff;
        else if (diff >= sync_threshold) delay = 2.0 * delay;
    }
    return delay;
}


static void read_thread() {
    log_message(LOG_DEBUG, MP, "Read thread started");

    PlayerState* ps = S;
    AVPacket* pkt = av_packet_alloc();
    int pkts_read = 0;

    for (;;) {
        if (!ps->running.load()) break;

        if (ps->paused.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Seek request
        {
            std::unique_lock<std::mutex> lk(ps->seek_mtx);
            if (ps->seek_req) {
                int64_t pos  = ps->seek_pos;
                ps->seek_req = false;
                lk.unlock();

                log_message(LOG_DEBUG, MP, "Seeking to %.3f s (pts=%" PRId64 ")", pos / (double)AV_TIME_BASE, pos);

                if (av_seek_frame(ps->fmt_ctx, -1, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY) >= 0) {
                    avformat_flush(ps->fmt_ctx);
                    log_message(LOG_OK, MP, "Seek successful");
                } else {
                    log_message(LOG_WARNING, MP, "av_seek_frame failed");
                }

                if (ps->video_idx >= 0) { pq_flush_locked(&ps->videoq); pq_start(&ps->videoq); }
                if (ps->audio_idx >= 0) { pq_flush_locked(&ps->audioq); pq_start(&ps->audioq); }
                ps->eof = false;
                ps->force_refresh.store(true);
                ps->seek_cv.notify_all();
            }
        }

        // Guard: only index streams[] when idx is valid
        bool ev = (ps->video_idx < 0) ||
	    stream_has_enough_packets(ps->fmt_ctx->streams[ps->video_idx], ps->video_idx, ps->videoq);
        bool ea = (ps->audio_idx < 0) ||
	    stream_has_enough_packets(ps->fmt_ctx->streams[ps->audio_idx], ps->audio_idx, ps->audioq);
        if (ev && ea) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        int ret = av_read_frame(ps->fmt_ctx, pkt);
        if (ret == AVERROR_EOF || avio_feof(ps->fmt_ctx->pb)) {
            if (!ps->eof) {
                log_message(LOG_DEBUG, MP, "EOF reached after %d packets; sending drain signals", pkts_read);
                if (ps->video_idx >= 0) {
                    AVPacket* ep = av_packet_alloc();
                    if (ep) pq_put(&ps->videoq, ep);
                }
                if (ps->audio_idx >= 0) {
                    AVPacket* ep = av_packet_alloc();
                    if (ep) pq_put(&ps->audioq, ep);
                }
                ps->eof = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        if (ret < 0) {
            log_message(LOG_ERROR, MP, "av_read_frame error: %d", ret);
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

    log_message(LOG_DEBUG, MP, "Read thread exiting (read %d packets)", pkts_read);
    av_packet_free(&pkt);
}


static bool init_video_stream() {
    S->video_idx = av_find_best_stream(S->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (S->video_idx < 0) {
        log_message(LOG_WARNING, MP, "No video stream found");
        return false;
    }

    AVStream* st = S->fmt_ctx->streams[S->video_idx];
    S->video_tb = st->time_base;
    S->out_w = st->codecpar->width;
    S->out_h = st->codecpar->height;

    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (st->codecpar->codec_id == AV_CODEC_ID_H264 && st->codecpar->height <= 720) {
        const AVCodec* hw = avcodec_find_decoder_by_name("h264_wiiu");
        if (hw) {
            codec = hw;
            S->hw_decoder = true;
            log_message(LOG_OK, MP, "Using h264_wiiu HW decoder for %dx%d H.264", S->out_w, S->out_h);
        } else {
            log_message(LOG_WARNING, MP, "h264_wiiu decoder not found, falling back to SW");
        }
    }
    if (!codec) {
        log_message(LOG_ERROR, MP, "No decoder found for video codec id=%d", (int)st->codecpar->codec_id);
        return false;
    }

    AVCodecContext* avctx = avcodec_alloc_context3(codec);
    if (!avctx) { log_message(LOG_ERROR, MP, "avcodec_alloc_context3 failed (video)"); return false; }
    if (avcodec_parameters_to_context(avctx, st->codecpar) < 0) {
        log_message(LOG_ERROR, MP, "avcodec_parameters_to_context failed (video)");
        avcodec_free_context(&avctx); return false;
    }
    avctx->pkt_timebase = st->time_base;

    if (!S->hw_decoder) {
        avctx->thread_count = 2;
        avctx->thread_type = FF_THREAD_SLICE;
        avctx->flags2 |= AV_CODEC_FLAG2_FAST;
        avctx->skip_loop_filter = AVDISCARD_NONREF;
        log_message(LOG_DEBUG, MP, "SW video decoder: 2 slice threads, skip_loop_filter=NONREF, FAST");
    }

    if (avcodec_open2(avctx, codec, nullptr) < 0) {
        log_message(LOG_ERROR, MP, "avcodec_open2 failed for video codec '%s'", codec->name);
        avcodec_free_context(&avctx); return false;
    }
    S->video_avctx = avctx;

    log_message(LOG_OK, MP, "Video stream opened: stream=%d codec=%s %dx%d tb=%d/%d", S->video_idx, codec->name, S->out_w, S->out_h, st->time_base.num, st->time_base.den);

    if (fq_init(&S->pictq, &S->videoq, VIDEO_FRAME_QUEUE_SIZE, 1) < 0) {
        log_message(LOG_ERROR, MP, "fq_init failed for pictq");
        return false;
    }
    if (decoder_init(&S->viddec, avctx, &S->videoq) < 0) {
        log_message(LOG_ERROR, MP, "decoder_init failed (video)");
        return false;
    }

    S->cur_frame_info = new frame_info{};
    S->cur_frame_info->width = S->out_w;
    S->cur_frame_info->height = S->out_h;
    S->cur_frame_info->texture = SDL_CreateTexture(sdl_get()->sdl_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, S->out_w, S->out_h);

    if (!S->cur_frame_info->texture) {
        log_message(LOG_ERROR, MP, "SDL_CreateTexture failed: %s", SDL_GetError());
        return false;
    }
    log_message(LOG_DEBUG, MP, "Video texture created (%dx%d IYUV)", S->out_w, S->out_h);
    return true;
}

static bool init_audio_stream() {
    S->audio_idx = av_find_best_stream(S->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (S->audio_idx < 0) {
        log_message(LOG_WARNING, MP, "No audio stream found (continuing without audio)");
        return true;
    }

    AVStream* st = S->fmt_ctx->streams[S->audio_idx];
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) {
        log_message(LOG_ERROR, MP, "No decoder for audio codec id=%d", (int)st->codecpar->codec_id);
        return false;
    }

    AVCodecContext* avctx = avcodec_alloc_context3(codec);
    if (!avctx) { log_message(LOG_ERROR, MP, "avcodec_alloc_context3 failed (audio)"); return false; }
    if (avcodec_parameters_to_context(avctx, st->codecpar) < 0) {
        log_message(LOG_ERROR, MP, "avcodec_parameters_to_context failed (audio)");
        avcodec_free_context(&avctx); return false;
    }
    avctx->pkt_timebase = st->time_base;
    if (avcodec_open2(avctx, codec, nullptr) < 0) {
        log_message(LOG_ERROR, MP, "avcodec_open2 failed for audio codec '%s'", codec->name);
        avcodec_free_context(&avctx); return false;
    }
    S->audio_avctx = avctx;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59,37,100)
    int nch = avctx->ch_layout.nb_channels;
#else
    int nch = avctx->channels;
#endif
    log_message(LOG_OK, MP, "Audio stream opened: stream=%d codec=%s %d Hz %dch fmt=%d", S->audio_idx, codec->name, avctx->sample_rate, nch, (int)avctx->sample_fmt);

    if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_AudioSpec want{};
    want.freq = AUDIO_OUT_RATE; want.format = AUDIO_S16SYS;
    want.channels = AUDIO_OUT_CHANNELS; want.samples = 4096;
    S->audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &S->audio_spec, 0);
    if (!S->audio_dev) {
        log_message(LOG_ERROR, MP, "SDL_OpenAudioDevice failed: %s", SDL_GetError());
        avcodec_free_context(&avctx); return false;
    }
    log_message(LOG_OK, MP, "SDL audio device opened: %d Hz %dch (got %d Hz %dch)", AUDIO_OUT_RATE, AUDIO_OUT_CHANNELS, S->audio_spec.freq, S->audio_spec.channels);

    rebuild_swr();
    if (!S->swr_ctx) {
        log_message(LOG_ERROR, MP, "Failed to build SwrContext");
        avcodec_free_context(&avctx); return false;
    }

    if (fq_init(&S->sampq, &S->audioq, AUDIO_FRAME_QUEUE_SIZE, 1) < 0) {
        log_message(LOG_ERROR, MP, "fq_init failed for sampq");
        return false;
    }
    if (decoder_init(&S->auddec, avctx, &S->audioq) < 0) {
        log_message(LOG_ERROR, MP, "decoder_init failed (audio)");
        return false;
    }

    S->cur_audio_track = S->audio_idx;
    S->audio_enabled   = true;
    SDL_PauseAudioDevice(S->audio_dev, 1);

    {
        std::lock_guard<std::mutex> lk(S->audio_tracks_mtx);
        S->audio_tracks.clear();
        for (unsigned i = 0; i < S->fmt_ctx->nb_streams; ++i) {
            AVStream* s = S->fmt_ctx->streams[i];
            if (s->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) continue;
            AVDictionaryEntry* lang = av_dict_get(s->metadata, "language", nullptr, 0);
            S->audio_tracks.push_back({
		    (int)i,
		    avcodec_get_name(s->codecpar->codec_id),
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59,37,100)
		    s->codecpar->ch_layout.nb_channels,
#else
		    s->codecpar->channels,
#endif
		    s->codecpar->sample_rate,
		    lang ? lang->value : "und"
		});
            log_message(LOG_DEBUG, MP, "  Audio track %d: codec=%s rate=%d lang=%s", (int)i, avcodec_get_name(s->codecpar->codec_id), s->codecpar->sample_rate, lang ? lang->value : "und");
        }
        media_info_get()->total_audio_track_count = (int)S->audio_tracks.size();
    }
    log_message(LOG_OK, MP, "Found %d audio track(s)", (int)S->audio_tracks.size());
    return true;
}


int media_player_init(const char* path) {
    log_message(LOG_DEBUG, MP, "media_player_init: %s", path);

    if (S) {
        log_message(LOG_WARNING, MP, "Re-initializing; cleaning up previous instance");
        media_player_cleanup();
    }

    if (!g_flush_pkt) {
        g_flush_pkt = av_packet_alloc();
        if (!g_flush_pkt) { log_message(LOG_ERROR, MP, "Failed to alloc flush sentinel"); return -1; }
        static uint8_t sentinel_byte = 0;
        g_flush_pkt->data = &sentinel_byte;
        g_flush_pkt->size = 0;
    }

    S = new PlayerState{};
    avformat_network_init();

    S->fmt_ctx = avformat_alloc_context();
    if (!S->fmt_ctx) {
        log_message(LOG_ERROR, MP, "avformat_alloc_context failed");
        delete S; S = nullptr; return -1;
    }
    {
        int open_err = avformat_open_input(&S->fmt_ctx, path, nullptr, nullptr);
        if (open_err < 0) {
            char errbuf[256] = {};
            av_strerror(open_err, errbuf, sizeof(errbuf));
            log_message(LOG_ERROR, MP, "avformat_open_input failed: [%d] %s  (path='%s')", open_err, errbuf, path);
            char cwd[512] = {};
            if (getcwd(cwd, sizeof(cwd)))
                log_message(LOG_DEBUG, MP, "CWD: %s", cwd);
            avformat_free_context(S->fmt_ctx); S->fmt_ctx = nullptr;
            delete S; S = nullptr; return -1;
        }
    }
    S->fmt_ctx->flags |= AVFMT_FLAG_NOBUFFER;
    S->fmt_ctx->probesize = 32 * 1024;
    S->fmt_ctx->max_analyze_duration = AV_TIME_BASE / 2;

    if (avformat_find_stream_info(S->fmt_ctx, nullptr) < 0) {
        log_message(LOG_ERROR, MP, "avformat_find_stream_info failed");
        avformat_close_input(&S->fmt_ctx); delete S; S = nullptr; return -1;
    }
    log_message(LOG_OK, MP, "Container opened: fmt=%s nb_streams=%u dur=%.2f s", S->fmt_ctx->iformat->name, S->fmt_ctx->nb_streams, S->fmt_ctx->duration / (double)AV_TIME_BASE);

    S->max_frame_dur = (S->fmt_ctx->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    pq_init(&S->videoq);
    pq_init(&S->audioq);

    bool has_v = init_video_stream();
    bool has_a = init_audio_stream();
    if (!has_v && !has_a) {
        log_message(LOG_ERROR, MP, "No usable streams found — aborting");
        media_player_cleanup(); return -1;
    }

    if (has_v) pq_start(&S->videoq);
    if (has_a) pq_start(&S->audioq);
    log_message(LOG_DEBUG, MP, "Packet queues armed (video serial=%d audio serial=%d)", S->videoq.serial, S->audioq.serial);

    clock_init(&S->audclk, &S->audioq.serial);
    clock_init(&S->vidclk, &S->videoq.serial);
    clock_init(&S->extclk, nullptr);
    log_message(LOG_DEBUG, MP, "Clocks initialised (aud serial=%d vid serial=%d)", S->audclk.serial, S->vidclk.serial);

    S->frame_timer = wall_now();
    S->wall_play_origin = wall_now();
    S->wall_play_offset = 0.0;

    S->running.store(true);
    S->paused.store(true);
    S->playing.store(false);

    S->read_tid  = std::thread(read_thread);
    if (has_v) S->video_tid = std::thread(video_decode_thread);
    if (has_a) S->audio_tid = std::thread(audio_decode_thread);
    log_message(LOG_OK, MP, "Threads started (read + %s%s)", has_v ? "video " : "", has_a ? "audio" : "");

    media_info_get()->playback_status = false;
    if (has_v && S->video_idx >= 0) {
        AVStream* vs = S->fmt_ctx->streams[S->video_idx];
        double dur = (vs->duration != AV_NOPTS_VALUE) ? vs->duration * av_q2d(vs->time_base) : 0.0;
        media_info_get()->total_video_playback_time = dur;
        log_message(LOG_DEBUG, MP, "Video duration: %.2f s", dur);
    }
    if (has_a && S->audio_idx >= 0) {
        AVStream* as = S->fmt_ctx->streams[S->audio_idx];
        int64_t dur = (as->duration != AV_NOPTS_VALUE) ? (int64_t)(as->duration * av_q2d(as->time_base)) : 0;
        media_info_get()->total_audio_playback_time = dur;
        log_message(LOG_DEBUG, MP, "Audio duration: %" PRId64 " s", dur);
    }

    log_message(LOG_OK, MP, "media_player_init complete");
    return 0;
}

void media_player_play(bool play) {
    if (!S) return;

    log_message(LOG_DEBUG, MP, "media_player_play(%s) [was %s, clock=%.3f s]", play ? "true" : "false", S->playing.load() ? "playing" : "paused", get_master_clock());

    if (play && S->paused.load()) {
        S->frame_timer += wall_now() - S->vidclk.last_upd;
        S->vidclk.paused = false;
        S->wall_play_origin = wall_now();
        clock_set(&S->vidclk, clock_get(&S->vidclk), S->vidclk.serial);
    } else if (!play && !S->paused.load()) {
        S->wall_play_offset = get_master_clock();
        log_message(LOG_DEBUG, MP, "Pause: wall_play_offset snapped to %.3f s", S->wall_play_offset);
    }

    clock_set(&S->extclk, clock_get(&S->extclk), S->extclk.serial);
    S->audclk.paused = S->vidclk.paused = S->extclk.paused = !play;

    S->paused.store(!play);
    S->playing.store(play);
    media_info_get()->playback_status = play;

    if (S->audio_dev) SDL_PauseAudioDevice(S->audio_dev, play ? 0 : 1);
}

void media_player_seek(double seconds) {
    if (!S || !S->fmt_ctx) return;

    log_message(LOG_OK, MP, "Seek requested: %.3f s (master clock was %.3f s)", seconds, get_master_clock());

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

    double master = get_master_clock();
    media_info_get()->current_video_playback_time = master;

    pump_audio();

    if (!S->video_avctx) return;

    // Periodic stats log every ~5 s
    if (S->playing.load()) {
        double now = wall_now();
        if (now - S->last_log_time >= 5.0) {
            log_message(LOG_DEBUG, MP, "Status: clock=%.2f s | vq=%d pkt | aq=%d pkt | " "pictq=%d | sampq=%d | frames decoded=%d dropped=%d", master, S->videoq.nb_packets, S->audioq.nb_packets, fq_nb_remaining(&S->pictq), fq_nb_remaining(&S->sampq), S->frames_decoded, S->frames_dropped);
            S->last_log_time = now;
        }
    }

retry:
    if (fq_nb_remaining(&S->pictq) == 0) {
        // nothing new - fall through to display keep_last frame
    } else {
        Frame* lastvp = fq_peek_last(&S->pictq);
        Frame* vp = fq_peek(&S->pictq);

        if (vp->serial != S->videoq.serial) {
            log_message(LOG_DEBUG, MP, "Discarding stale video frame (serial %d != queue %d)", vp->serial, S->videoq.serial);
            fq_next(&S->pictq); goto retry;
        }
        if (lastvp->serial != vp->serial) S->frame_timer = wall_now();

        if (!S->playing.load()) goto display;

        double last_duration = vp_duration(lastvp, vp);
        double delay = compute_target_delay(last_duration);
        double now = wall_now();

        if (now < S->frame_timer + delay) goto display;

        S->frame_timer += delay;
        if (delay > 0 && (now - S->frame_timer) > AV_SYNC_THRESHOLD_MAX)
            S->frame_timer = now;

        {
            std::lock_guard<std::mutex> lk(S->pictq.mtx);
            if (!std::isnan(vp->pts)) {
                clock_set(&S->vidclk, vp->pts, vp->serial);
                clock_sync_to_slave(&S->extclk, &S->vidclk);
            }
        }

        if (fq_nb_remaining(&S->pictq) > 1) {
            Frame* nextvp = fq_peek_next(&S->pictq);
            double dur    = vp_duration(vp, nextvp);
            if (now > S->frame_timer + dur) {
                S->frames_dropped++;
                log_message(LOG_WARNING, MP, "Late frame dropped (pts=%.3f master=%.3f diff=%.3f ms)", vp->pts, master, (now - S->frame_timer - dur) * 1000.0);
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

    Frame* vp = fq_peek_last(&S->pictq);
    if (!vp || !vp->frame || !vp->frame->data[0]) return;

    if (!vp->uploaded) {
        if (S->cur_frame_info && S->cur_frame_info->texture) {
            int tw, th;
            SDL_QueryTexture(S->cur_frame_info->texture, nullptr, nullptr, &tw, &th);
            if (tw != vp->width || th != vp->height) {
                log_message(LOG_DEBUG, MP, "Texture resolution changed: %dx%d -> %dx%d", tw, th, vp->width, vp->height);
                SDL_DestroyTexture(S->cur_frame_info->texture);
                S->cur_frame_info->texture = SDL_CreateTexture(sdl_get()->sdl_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, vp->width, vp->height);
                S->dest_rect_init = false;
            }
        }
        if (S->cur_frame_info && S->cur_frame_info->texture)
            SDL_UpdateYUVTexture(S->cur_frame_info->texture, nullptr, vp->frame->data[0], vp->frame->linesize[0], vp->frame->data[1], vp->frame->linesize[1], vp->frame->data[2], vp->frame->linesize[2]);
        vp->uploaded = true;
    }

    if (!S->dest_rect_init) {
        S->dest_rect = calculate_aspect_fit_rect(vp->width, vp->height);
        S->dest_rect_init = true;
        log_message(LOG_DEBUG, MP, "Display rect: x=%d y=%d w=%d h=%d", S->dest_rect.x, S->dest_rect.y, S->dest_rect.w, S->dest_rect.h);
    }

    if (S->cur_frame_info && S->cur_frame_info->texture)
        SDL_RenderCopy(sdl_get()->sdl_renderer, S->cur_frame_info->texture, nullptr, &S->dest_rect);
}

bool media_player_switch_audio_track(int new_idx) {
    if (!S || !S->audio_enabled) return false;
    if (new_idx < 0 || new_idx >= (int)S->fmt_ctx->nb_streams) return false;
    AVStream* st = S->fmt_ctx->streams[new_idx];
    if (st->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) return false;
    if (new_idx == S->audio_idx) {
        log_message(LOG_DEBUG, MP, "Audio track %d already active", new_idx);
        return true;
    }

    log_message(LOG_OK, MP, "Switching audio track %d -> %d", S->audio_idx, new_idx);

    double t = get_master_clock();
    bool was_playing = S->playing.load();
    media_player_play(false);
    if (S->audio_dev) { SDL_PauseAudioDevice(S->audio_dev, 1); SDL_ClearQueuedAudio(S->audio_dev); }

    log_message(LOG_DEBUG, MP, "Stopping audio decode thread...");
    decoder_abort(&S->auddec, &S->sampq);
    if (S->audio_tid.joinable()) S->audio_tid.join();
    log_message(LOG_DEBUG, MP, "Audio decode thread joined");

    decoder_free_pkt(&S->auddec);

    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) {
        log_message(LOG_ERROR, MP, "No decoder for new audio track codec id=%d", (int)st->codecpar->codec_id);
        return false;
    }

    AVCodecContext* avctx = avcodec_alloc_context3(codec);
    if (!avctx || avcodec_parameters_to_context(avctx, st->codecpar) < 0 || avcodec_open2(avctx, codec, nullptr) < 0) {
        log_message(LOG_ERROR, MP, "Failed to open codec for new audio track");
        avcodec_free_context(&avctx); return false;
    }
    avctx->pkt_timebase = st->time_base;

    avcodec_free_context(&S->audio_avctx);
    S->audio_avctx = avctx;

    rebuild_swr();
    if (!S->swr_ctx) {
        log_message(LOG_ERROR, MP, "rebuild_swr failed after track switch");
        avcodec_free_context(&S->audio_avctx);
        return false;
    }

    S->audio_idx = new_idx;
    S->cur_audio_track = new_idx;

    pq_flush_locked(&S->audioq); pq_start(&S->audioq);
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
    log_message(LOG_OK, MP, "Audio track switched to %d, decode thread restarted", new_idx);

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
    auto dur = [](int i) -> double {
        AVStream* s = S->fmt_ctx->streams[i];
        return (s->duration != AV_NOPTS_VALUE) ? s->duration * av_q2d(s->time_base) : -1.0;
    };
    if (S->video_idx >= 0) { double d = dur(S->video_idx); if (d >= 0) return d; }
    if (S->audio_idx >= 0) { double d = dur(S->audio_idx); if (d >= 0) return d; }
    return (S->fmt_ctx->duration != AV_NOPTS_VALUE) ? S->fmt_ctx->duration / (double)AV_TIME_BASE : 0.0;
}

void media_player_cleanup() {
    if (!S) return;

    log_message(LOG_DEBUG, MP, "media_player_cleanup: stopping threads (decoded=%d dropped=%d clock=%.2f s)", S->frames_decoded, S->frames_dropped, get_master_clock());

    S->running.store(false);
    S->playing.store(false);
    S->paused.store(true);

    pq_abort(&S->videoq); fq_signal(&S->pictq);
    pq_abort(&S->audioq); fq_signal(&S->sampq);
    {
        std::lock_guard<std::mutex> lk(S->seek_mtx);
        S->seek_req = false;
    }
    S->seek_cv.notify_all();

    if (S->read_tid.joinable())  { S->read_tid.join();  log_message(LOG_DEBUG, MP, "Read thread joined"); }
    if (S->video_tid.joinable()) { S->video_tid.join(); log_message(LOG_DEBUG, MP, "Video thread joined"); }
    if (S->audio_tid.joinable()) { S->audio_tid.join(); log_message(LOG_DEBUG, MP, "Audio thread joined"); }

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
        log_message(LOG_DEBUG, MP, "SDL audio device closed");
    }
    if (S->sws_ctx) sws_freeContext(S->sws_ctx);
    if (S->swr_ctx) swr_free(&S->swr_ctx);
    if (S->cur_frame_info) {
        if (S->cur_frame_info->texture) SDL_DestroyTexture(S->cur_frame_info->texture);
        delete S->cur_frame_info;
    }
    if (S->fmt_ctx) avformat_close_input(&S->fmt_ctx);
    if (SDL_WasInit(SDL_INIT_AUDIO)) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    avformat_network_deinit();

    delete S;
    S = nullptr;

    log_message(LOG_OK, MP, "media_player_cleanup complete");
}
