#include <cstdint>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <SDL2/SDL.h>
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/frame.h>
}

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>

#include "app_state.hpp"
#include "main.hpp"
#include "media_info.hpp"
#include "video_player.hpp"
#include "audio_player.hpp"

int video_stream_index = -1;
AVFormatContext* fmt_ctx = nullptr;
AVCodecContext* video_codec_ctx = nullptr;
SwrContext* swr_ctx = nullptr;
AVPacket* pkt = nullptr;
AVFrame* frame = nullptr;
AVRational framerate;

frame_info* current_frame_info = nullptr;

std::atomic<int32_t> start_time = 0;

std::queue<AVFrame*> video_frame_queue;
std::mutex video_frame_mutex;
std::condition_variable video_frame_cv;
std::atomic<bool> video_thread_running = true;
std::thread video_thread;
std::mutex playback_mutex;
std::condition_variable playback_cv;
int64_t pause_start_time = 0;
int64_t total_paused_duration = 0;

std::mutex info_mutex;

int64_t clamp_seek_time(int64_t target_time) {
    return std::clamp(target_time, int64_t(0), fmt_ctx->duration - AV_TIME_BASE);
}

AVCodecContext* video_player_create_codec_context(AVFormatContext* fmt_ctx, int stream_index) {
    AVCodecParameters* codecpar = fmt_ctx->streams[stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);

    if (!codec) {
        printf("[Video Player] No decoder found for codec ID: %d\n", codecpar->codec_id);
        return nullptr;
    }

	#ifdef DEBUG
    printf("[Video Player] Using decoder: %s\n", codec->name);
	#endif

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        printf("[Video Player] Failed to allocate codec context\n");
        return nullptr;
    }

    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_parameters_to_context(codec_ctx, codecpar) < 0) {
        printf("[Video Player] Failed to copy codec parameters to codec context\n");
        avcodec_free_context(&codec_ctx);
        return nullptr;
    }

    codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codec_ctx->skip_frame = AVDISCARD_NONREF;

    int ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        printf("[Video Player] Failed to open codec: %s\n", errbuf);
        avcodec_free_context(&codec_ctx);
        return nullptr;
    }

	#ifdef DEBUG
    printf("[Video Player] Codec opened successfully\n");
	#endif

    return codec_ctx;
}

void clear_video_frames() {
    std::lock_guard<std::mutex> lock(video_frame_mutex);
    while (!video_frame_queue.empty()) {
        AVFrame* f = video_frame_queue.front();
        av_frame_free(&f);
        video_frame_queue.pop();
    }
}

void video_player_seek(float delta_seconds) {
    if (!fmt_ctx || video_stream_index < 0) return;

    video_player_play(false);
    {
        std::unique_lock<std::mutex> lock(playback_mutex);

        int64_t current_time = static_cast<int64_t>(media_info_get()->current_video_playback_time * AV_TIME_BASE);
        int64_t target_time = clamp_seek_time(current_time + static_cast<int64_t>(delta_seconds * AV_TIME_BASE));

        if (av_seek_frame(fmt_ctx, video_stream_index, target_time, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY) < 0) {
            printf("[Video Player] Seek failed!\n");
            return;
        }

        avcodec_flush_buffers(video_codec_ctx);
        clear_video_frames();

        if (current_frame_info) {
            SDL_DestroyTexture(current_frame_info->texture);
            delete current_frame_info;
            current_frame_info = nullptr;
        }

        start_time = av_gettime_relative() - target_time;
        media_info_get()->current_video_playback_time = target_time / static_cast<double>(AV_TIME_BASE);

        audio_player_seek(media_info_get()->current_video_playback_time);
        printf("[Video Player] Seek done! New time: %.2lld seconds\n", media_info_get()->current_video_playback_time);
    }

    video_player_play(true);
}

void video_player_play(bool new_state) {
    std::lock_guard<std::mutex> lock(playback_mutex);
    if (!media_info_get()->playback_status && new_state) {
        int64_t now = av_gettime_relative();
        int64_t pause_duration = now - pause_start_time;
        start_time += pause_duration;
        playback_cv.notify_one();
    } else if (media_info_get()->playback_status && !new_state) {
        pause_start_time = av_gettime_relative();
    }

	#ifdef DEBUG_VIDEO
    printf("[Video Player] Changing playback_status to %s\n", new_state ? "true" : "false");
	#endif

    media_info_get()->playback_status = new_state;
}

frame_info* video_player_get_current_frame_info() {
    if (!current_frame_info) {
        printf("[Video Player] No current_frame_info\n");
        return nullptr;
    }
    return current_frame_info;
}

int video_player_init(const char* filepath, SDL_Renderer* renderer, SDL_Texture*& texture) {
	#ifdef DEBUG_VIDEO
    printf("[Video player] Starting Video Player...\n");
    printf("[Video player] Opening file %s\n", filepath);
	#endif

    AVDictionary* options = nullptr;
    av_dict_set(&options, "probesize", "10000", 0);
    av_dict_set(&options, "fpsprobesize", "10000", 0);
    av_dict_set(&options, "formatprobesize", "10000", 0);

    if (avformat_open_input(&fmt_ctx, filepath, nullptr, &options) != 0) {
	#ifdef DEBUG_VIDEO
        printf("[Video player] Could not open file: %s\n", filepath);
	#endif
        return -1;
    }

    avformat_find_stream_info(fmt_ctx, nullptr);

    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0) {
	#ifdef DEBUG_VIDEO
        printf("[Video player] Could not find video stream.\n");
	#endif
        return -1;
    }

    video_codec_ctx = video_player_create_codec_context(fmt_ctx, video_stream_index);
    if (!video_codec_ctx) return -1;

    SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                                video_codec_ctx->width, video_codec_ctx->height);

    framerate = fmt_ctx->streams[video_stream_index]->r_frame_rate;
    media_info_get()->framerate = av_q2d(framerate);
    media_info_get()->total_video_playback_time = video_player_get_total_play_time();

	#ifdef DEBUG_VIDEO
    printf("[Video player] FPS: %f\n", media_info_get()->framerate);
	#endif

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    return 0;
}

void video_player_start(const char* path, SDL_Renderer& renderer, SDL_Texture*& texture) {
    video_thread_running = true;

    if (current_frame_info) {
        SDL_DestroyTexture(current_frame_info->texture);
        delete current_frame_info;
        current_frame_info = nullptr;
    }

    video_player_init(path, &renderer, texture);
    audio_player_init(path);

    start_video_decoding_thread();
    app_state_set(STATE_PLAYING_VIDEO);
}

void start_video_decoding_thread() {
	#ifdef DEBUG_VIDEO
    printf("[Video player] Starting video decoding thread...\n");
	#endif
    video_thread = std::thread(process_video_frame_thread);
}

void process_video_frame_thread() {
    AVFrame* local_frame = av_frame_alloc();
    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
    start_time = av_gettime_relative();
    total_paused_duration = 0;
    pause_start_time = 0;

    while (video_thread_running) {
        {
            std::unique_lock<std::mutex> lock(playback_mutex);
            playback_cv.wait(lock, [] {
                std::lock_guard<std::mutex> info_lock(info_mutex);
                return media_info_get()->playback_status || !video_thread_running;
            });
        }

        AVPacket pkt;
        if (av_read_frame(fmt_ctx, &pkt) >= 0) {
            if (pkt.stream_index == video_stream_index) {
                if (!avcodec_send_packet(video_codec_ctx, &pkt)) {
                    while (!avcodec_receive_frame(video_codec_ctx, local_frame)) {
                        int64_t pts_us = local_frame->pts * av_q2d(time_base) * 1e6;
                        int64_t now_us = av_gettime_relative() - start_time;
                        int64_t delay = pts_us - now_us;

                        if (delay > 0) av_usleep(delay);

                        {
                            std::lock_guard<std::mutex> info_lock(info_mutex);
                            media_info_get()->current_video_playback_time = local_frame->pts * av_q2d(time_base);
                        }

                        {
                            std::lock_guard<std::mutex> lock(video_frame_mutex);
                            if (video_frame_queue.size() < 10) {
                                video_frame_queue.push(local_frame);
                                local_frame = av_frame_alloc();
                            } else {
								#ifdef DEBUG_VIDEO
                                printf("[Video Player] Dropping frame due to full queue\n");
								#endif
                                av_frame_unref(local_frame);
                            }
                        }
                    }
                }
            }
            av_packet_unref(&pkt);
        } else {
            break;
        }
    }

    av_frame_free(&local_frame);
    clear_video_frames();
}

int64_t video_player_get_total_play_time() {
    if (!fmt_ctx || video_stream_index < 0) return 0;

    AVStream* stream = fmt_ctx->streams[video_stream_index];
    if (!stream) return 0;

    int64_t duration = stream->duration;
    if (duration <= 0) {
        duration = fmt_ctx->duration * stream->time_base.den / (stream->time_base.num * AV_TIME_BASE);
    } else {
        duration = duration * av_q2d(stream->time_base);
    }

    return static_cast<int64_t>(duration);
}

void render_video_frame(SDL_Renderer* renderer) {
    AVFrame* frame = nullptr;

    {
        std::lock_guard<std::mutex> lock(video_frame_mutex);
        if (!video_frame_queue.empty()) {
            frame = video_frame_queue.front();
            video_frame_queue.pop();
        }
    }

    if (!frame) return;

    if (!current_frame_info || !current_frame_info->texture) {
        if (current_frame_info) delete current_frame_info;
        current_frame_info = new frame_info;
        current_frame_info->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV,
            SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height);
        current_frame_info->frame_width = frame->width;
        current_frame_info->frame_height = frame->height;
    }

    SDL_UpdateYUVTexture(current_frame_info->texture, nullptr,
        frame->data[0], frame->linesize[0],
        frame->data[1], frame->linesize[1],
        frame->data[2], frame->linesize[2]);

    av_frame_free(&frame);
}

void video_player_update(SDL_Renderer* renderer) {
    if (!media_info_get_copy().playback_status) return;
    render_video_frame(renderer);
}

void stop_video_decoding_thread() {
	#ifdef DEBUG_VIDEO
    printf("[Video player] Stopping video decoding thread...\n");
	#endif
    video_thread_running = false;
    playback_cv.notify_all();
    if (video_thread.joinable()) {
        video_thread.join();
	#ifdef DEBUG_VIDEO
        printf("[Video player] Video decoding thread joined\n");
	#endif
    }
}

int video_player_cleanup() {
	#ifdef DEBUG_VIDEO
    printf("[Video player] Stopping Video Player...\n");
	#endif

    audio_player_cleanup();
    stop_video_decoding_thread();

    clear_video_frames();

    if (current_frame_info) {
        if (current_frame_info->texture) {
            SDL_DestroyTexture(current_frame_info->texture);
        }
        delete current_frame_info;
        current_frame_info = nullptr;
		#ifdef DEBUG_VIDEO
        printf("[Video player] Freed current_frame_info\n");
		#endif
    }

    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
		#ifdef DEBUG_VIDEO
        printf("[Video player] Freed last video frame\n");
		#endif
    }

    if (pkt) {
        av_packet_free(&pkt);
        pkt = nullptr;
		#ifdef DEBUG_VIDEO
        printf("[Video player] Freed packet\n");
		#endif
    }

    if (video_codec_ctx) {
        avcodec_free_context(&video_codec_ctx);
        video_codec_ctx = nullptr;
		#ifdef DEBUG_VIDEO
        printf("[Video player] Freed codec context\n");
		#endif
    }

    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
		#ifdef DEBUG_VIDEO
        printf("[Video player] Closed input file\n");
		#endif
    }

    video_stream_index = -1;
    media_info_get()->current_video_playback_time = 0;
    total_paused_duration = 0;
    pause_start_time = 0;

    video_player_play(false);
    return 0;
}
