#include <shared/system.h>
#include "VideoWriterFFmpeg.h"
#include "VideoWriter.h"
#include <SDL.h>
#include "native_ui.h"
#include "conf.h"
#include <mutex>
#include <shared/debug.h>
#include "Remapper.h"
#include <shared/log.h>
#include "misc.h"

#ifdef __clang__

#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wsign-conversion"

#endif

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#ifdef __clang__

#pragma GCC diagnostic pop

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const int FPS = 50;

static const char FORMAT[] = "mp4";

// Should really copy this arrangement for the Windows version...
struct VideoWriterFFmpegFormat {
    VideoWriterFormat vwf;
    int vwidth;
    int vheight;
    int64_t vbitrate;
    int64_t abitrate;
};

static std::vector<VideoWriterFFmpegFormat> g_formats;

static bool g_can_write_video = false;

static const AVCodec *g_acodec;
static AVSampleFormat g_aformat;

static const AVCodec *g_vcodec;

static SDL_AudioSpec g_audio_spec;

#if LIBAVUTIL_VERSION_MAJOR >= 57
static const AVChannelLayout CHANNEL_LAYOUT_MONO = AV_CHANNEL_LAYOUT_MONO;
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(FFMPEG, "ffmpeg", "FFMPEG", &log_printer_stdout_and_debugger, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VideoWriterFFmpeg : public VideoWriter {
  public:
    VideoWriterFFmpeg(std::shared_ptr<MessageList> message_list,
                      std::string file_name,
                      size_t format_index)
        : VideoWriter(std::move(message_list),
                      std::move(file_name),
                      format_index) {
    }

    ~VideoWriterFFmpeg() {
        av_frame_free(&m_vframe);
        // av_frame_free(&m_vframe_rgba);
        av_frame_free(&m_aframe);

        avcodec_free_context(&m_acontext);
        avcodec_free_context(&m_vcontext);

        if (m_ofcontext) {
            this->CloseFile();

#if LIBAVFORMAT_VERSION_MAJOR == 58
            // http://stackoverflow.com/questions/43389411/
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
            for (unsigned i = 0; i < m_ofcontext->nb_streams; ++i) {
                AVStream *st = m_ofcontext->streams[i];

                if (st->codec->priv_data &&
                    st->codec->codec &&
                    st->codec->codec->priv_class) {
                    av_opt_free(st->codec->priv_data);
                }

                av_freep(&st->codec->priv_data);
            }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#endif
            avformat_free_context(m_ofcontext);
            m_ofcontext = nullptr;
        }
    }

    bool BeginWrite() override {
        int rc;
        const VideoWriterFFmpegFormat *format = &g_formats[m_format_index];

        rc = avformat_alloc_output_context2(&m_ofcontext,
                                            nullptr,
                                            FORMAT,
                                            m_file_name.c_str());
        if (rc < 0) {
            return this->Error(rc, "avformat_alloc_output_context2");
        }

        ASSERT(!(m_ofcontext->oformat->flags & AVFMT_NOFILE));

        int context_flags = 0;
        if (m_ofcontext->oformat->flags & AVFMT_GLOBALHEADER) {
            context_flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        LOGF(FFMPEG, "Output File: ``%s''\n", m_file_name.c_str());
        rc = avio_open(&m_ofcontext->pb, m_file_name.c_str(), AVIO_FLAG_WRITE);
        if (rc < 0) {
            return this->Error(rc, "avio_open");
        }

        m_vstream = avformat_new_stream(m_ofcontext, g_vcodec);
        if (!m_vstream) {
            return this->Error(0, "avformat_new_stream (video)");
        }

        m_vstream->time_base = av_make_q(1, FPS);

        m_vcontext = avcodec_alloc_context3(g_vcodec);
        if (!m_vcontext) {
            return this->Error(0, "avcodec_alloc_context3 (video)");
        }

        m_vcontext->bit_rate = format->vbitrate;
        m_vcontext->width = format->vwidth;
        m_vcontext->height = format->vheight;
        m_vcontext->time_base = m_vstream->time_base;
        m_vcontext->gop_size = 12; //???
        m_vcontext->pix_fmt = g_vcodec->pix_fmts[0];

        m_vcontext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
        m_vcontext->flags |= context_flags;

        rc = avcodec_open2(m_vcontext, g_vcodec, nullptr);
        if (rc < 0) {
            return this->Error(rc, "avcodec_open2 (video)");
        }

        rc = avcodec_parameters_from_context(m_vstream->codecpar, m_vcontext);
        if (rc < 0) {
            return this->Error(rc, "avcodec_parameters_from_context (video)");
        }

        m_vframe = this->CreateVideoFrame(m_vcontext->pix_fmt, "final");
        if (!m_vframe) {
            return false;
        }

        // m_vframe_rgba=this->CreateVideoFrame(AV_PIX_FMT_BGRA,"BGRA");
        // if(!m_vframe_rgba) {
        //     return false;
        // }

        m_astream = avformat_new_stream(m_ofcontext, g_acodec);
        if (!m_astream) {
            return this->Error(0, "avformat_new_stream (audio)");
        }

        m_acontext = avcodec_alloc_context3(g_acodec);
        if (!m_acontext) {
            return this->Error(0, "avcodec_alloc_context3 (audio)");
        }

        m_acontext->bit_rate = format->abitrate;
        m_acontext->sample_rate = g_audio_spec.freq;
        m_acontext->sample_fmt = g_aformat;
#if LIBAVUTIL_VERSION_MAJOR < 57
        m_acontext->channel_layout = AV_CH_LAYOUT_MONO;
        m_acontext->channels = 1;
#else
        rc = av_channel_layout_copy(&m_acontext->ch_layout, &CHANNEL_LAYOUT_MONO);
        if (rc < 0) {
            return this->Error(rc, "av_channel_layout_copy (acontext)");
        }
#endif
        m_acontext->time_base = av_make_q(1, m_acontext->sample_rate);

        {
            char channel_desc[1000];
#if LIBAVUTIL_VERSION_MAJOR < 57
            av_get_channel_layout_string(channel_desc, sizeof channel_desc, m_acontext->channels, m_acontext->channel_layout);
#else
            av_channel_layout_describe(&m_acontext->ch_layout, channel_desc, sizeof channel_desc);
#endif

            LOGF(FFMPEG, "Audio context: ");
            LOGI(FFMPEG);
            LOGF(FFMPEG, "Bit Rate: %" PRId64 "\n", m_acontext->bit_rate);
            LOGF(FFMPEG, "Sample Rate: %d\n", m_acontext->sample_rate);
            LOGF(FFMPEG, "Sample Fmt: %s\n", av_get_sample_fmt_name(m_acontext->sample_fmt));
            LOGF(FFMPEG, "Channel Layout: %s\n", channel_desc);
            LOGF(FFMPEG, "Time Base: %d/%d\n", m_acontext->time_base.num, m_acontext->time_base.den);
        }

        m_acontext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
        m_acontext->flags |= context_flags;

        rc = avcodec_open2(m_acontext, g_acodec, nullptr);
        if (rc < 0) {
            return this->Error(rc, "avcodec_open2 (audio)");
        }

        rc = avcodec_parameters_from_context(m_astream->codecpar, m_acontext);
        if (rc < 0) {
            return this->Error(rc, "avcodec_parameters_from_context (audio)");
        }

        m_aframe = av_frame_alloc();

        m_aframe->format = m_acontext->sample_fmt;
        m_aframe->nb_samples = m_acontext->frame_size;
        m_aframe->sample_rate = m_acontext->sample_rate;
#if LIBAVUTIL_VERSION_MAJOR < 57
        m_aframe->channel_layout = m_acontext->channel_layout;
#else
        rc = av_channel_layout_copy(&m_aframe->ch_layout, &m_acontext->ch_layout);
        if (rc != 0) {
            return this->Error(rc, "av_channel_layout_copy (aframe)");
        }
#endif

        rc = av_frame_get_buffer(m_aframe, 0);
        if (rc < 0) {
            return this->Error(rc, "av_frame_get_buffer (audio)");
        }

        m_swscontext = sws_getContext(TV_TEXTURE_WIDTH,
                                      TV_TEXTURE_HEIGHT,
                                      AV_PIX_FMT_BGRA,
                                      m_vframe->width,
                                      m_vframe->height,
                                      (AVPixelFormat)m_vframe->format,
                                      SWS_POINT,
                                      nullptr,
                                      nullptr,
                                      nullptr);
        if (!m_swscontext) {
            return this->Error(0, "sws_getContext");
        }

        rc = avformat_write_header(m_ofcontext, nullptr);
        if (rc < 0) {
            return this->Error(0, "avformat_write_header");
        }

        //m_remapper=Remapper(FPS,m_acontext->sample_rate);

        return true;
    }

    bool EndWrite() override {
        int rc;

        if (!this->EndWrite(m_vcontext, m_vstream, "video")) {
            return false;
        }

        // Pad any partial frame with silence, and submit it.
        if (m_aframe_index > 0) {
            auto dest = (float *)m_aframe->data[0];

            while (m_aframe_index < m_aframe->nb_samples) {
                dest[m_aframe_index++] = 0.f;
            }

            if (!this->Write(m_acontext, m_astream, m_aframe, "audio (final)")) {
                return false;
            }
        }

        if (!this->EndWrite(m_acontext, m_astream, "audio")) {
            return false;
        }

        rc = av_write_trailer(m_ofcontext);
        if (rc < 0) {
            return this->Error(rc, "av_write_trailer");
        }

        this->CloseFile();

        return false;
    }

    bool GetAudioFormat(SDL_AudioSpec *spec) const override {
        *spec = g_audio_spec;

        return true;
    }

    bool GetVideoFormat(uint32_t *format_ptr, int *width_ptr, int *height_ptr) const override {
        *format_ptr = SDL_PIXELFORMAT_ARGB8888;
        *width_ptr = TV_TEXTURE_WIDTH;
        *height_ptr = TV_TEXTURE_HEIGHT;

        return true;
    }

    bool WriteSound(const void *data_, size_t data_size_bytes) override {
        int rc;

        auto src = (const float *)data_;

        ASSERT(data_size_bytes % 4 == 0);
        size_t num_src_samples = data_size_bytes / 4;

        auto dest = (float *)m_aframe->data[0];

        for (size_t i = 0; i < num_src_samples; ++i) {
            if (m_aframe_index == 0) {
                rc = av_frame_make_writable(m_aframe);
                if (rc < 0) {
                    return this->Error(rc, "av_frame_make_writable (audio)");
                }

                m_aframe->pts = m_apts;
                m_apts += m_aframe->nb_samples;
            }

            dest[m_aframe_index++] = src[i];

            if (m_aframe_index == m_aframe->nb_samples) {
                if (!this->Write(m_acontext, m_astream, m_aframe, "audio")) {
                    return false;
                }

                m_aframe_index = 0;
            }
        }

        return true;
    }

    bool WriteVideo(const void *data) override {
        int rc;

        rc = av_frame_make_writable(m_vframe);
        if (rc < 0) {
            return this->Error(rc, "av_frame_make_writable (video)");
        }

        const uint8_t *src_slices[] = {
            (const uint8_t *)data,
        };

        const int src_strides[] = {
            TV_TEXTURE_WIDTH * 4,
        };

        sws_scale(m_swscontext,
                  src_slices, src_strides, 0, TV_TEXTURE_HEIGHT,
                  m_vframe->data, m_vframe->linesize);

        m_vframe->pts = m_vpts;

        if (!this->Write(m_vcontext, m_vstream, m_vframe, "video")) {
            return false;
        }

        ++m_vpts;

        return true;
    }

  protected:
  private:
    AVFormatContext *m_ofcontext = nullptr;
    AVStream *m_vstream = nullptr;
    AVCodecContext *m_vcontext = nullptr;
    AVFrame *m_vframe = nullptr;

    AVStream *m_astream = nullptr;
    AVCodecContext *m_acontext = nullptr;
    AVFrame *m_aframe = nullptr;
    int m_aframe_index = 0;

    SwsContext *m_swscontext = nullptr;

    //Remapper m_remapper;

    int64_t m_vpts = 0;
    int64_t m_apts = 0;

    bool Write(AVCodecContext *ccontext,
               AVStream *stream,
               AVFrame *frame,
               const char *what) {
        return this->DoWrite(ccontext,
                             stream,
                             frame,
                             AVERROR(EAGAIN),
                             what);
    }

    bool EndWrite(AVCodecContext *ccontext,
                  AVStream *stream,
                  const char *what) {
        return this->DoWrite(ccontext,
                             stream,
                             nullptr,
                             AVERROR_EOF,
                             what);
    }

    void CloseFile() {
        avio_closep(&m_ofcontext->pb);
    }

    bool DoWrite(AVCodecContext *ccontext,
                 AVStream *stream,
                 AVFrame *frame,
                 int finish_error,
                 const char *what) {
        int rc;

        ASSERT(m_ofcontext->streams[stream->index] == stream);

        rc = avcodec_send_frame(ccontext, frame);
        if (rc < 0) {
            return this->Error(rc, "avcodec_send_frame (%s)", what);
        }

        for (;;) {
            AVPacket *packet = av_packet_alloc();

            rc = avcodec_receive_packet(ccontext, packet);
            if (rc == finish_error) {
                av_packet_free(&packet);
                return true;
            } else if (rc < 0) {
                av_packet_free(&packet);
                return this->Error(rc, "avcodec_receive_packet (%s)", what);
                return false;
            }

            av_packet_rescale_ts(packet,
                                 ccontext->time_base,
                                 stream->time_base);
            packet->stream_index = stream->index;

            rc = av_interleaved_write_frame(m_ofcontext, packet);

            av_packet_free(&packet);

            if (rc < 0) {
                return this->Error(rc, "av_interleaved_write_frame (%s)", what);
            }
        }
    }

    AVFrame *CreateVideoFrame(AVPixelFormat fmt, const char *desc) {
        AVFrame *f = av_frame_alloc();
        if (!f) {
            this->Error(0, "av_frame_alloc (video; %s)", desc);
            return nullptr;
        }

        f->format = fmt;
        f->width = m_vcontext->width;
        f->height = m_vcontext->height;

        int rc = av_frame_get_buffer(f, 32);
        if (rc < 0) {
            this->Error(0, "av_frame_get_buffer (video; %s)", desc);
            av_frame_free(&f);
            return nullptr;
        }

        return f;
    }

    bool Error(int rc, const char *fmt, ...) PRINTF_LIKE(3, 4) {
        char *msg;
        {
            va_list v;
            va_start(v, fmt);
            if (vasprintf(&msg, fmt, v) == -1) {
                msg = nullptr;
            }
            va_end(v);
        }

        char desc[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(rc, desc, sizeof desc);

        m_msg.e.f("failed to save video to: %s\n", this->GetFileName().c_str());
        m_msg.i.f("(%s failed: %s)\n", msg, desc);

        free(msg);
        msg = nullptr;

        return false;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CanCreateVideoWriterFFmpeg() {
    if (!g_can_write_video) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<VideoWriter> CreateVideoWriterFFmpeg(std::shared_ptr<MessageList> message_list,
                                                     std::string file_name,
                                                     size_t format_index) {
    if (!CanCreateVideoWriterFFmpeg()) {
        return nullptr;
    }

    return std::make_unique<VideoWriterFFmpeg>(std::move(message_list),
                                               std::move(file_name),
                                               format_index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
static bool IsSupported(T value,
                        const T *values,
                        T terminator) {
    if (values) {
        for (const T *p = values; *p != terminator; ++p) {
            if (*p == value) {
                return true;
            }
        }

        return false;
    } else {
        return true;
    }
}

#if LIBAVUTIL_VERSION_MAJOR < 57
static bool IsChannelLayoutSupported(const AVCodec *codec,
                                     uint64_t channel_layout) {
    return IsSupported(channel_layout, codec->channel_layouts, (uint64_t)0);
}
#else
static bool IsChannelLayoutSupported(const AVCodec *codec,
                                     const AVChannelLayout *ch_layout) {
    if (!codec->ch_layouts) {
        // Judging by the encode_audio example, if ch_layouts is NULL, anything goes?
        return true;
    }

    for (const AVChannelLayout *l = codec->ch_layouts; l->order != 0; ++l) {
        if (av_channel_layout_compare(l, ch_layout) == 0) {
            return true;
        }
    }

    return false;
}
#endif

static bool IsSampleFormatSupported(const AVCodec *codec,
                                    AVSampleFormat fmt) {
    return IsSupported(fmt, codec->sample_fmts, (AVSampleFormat)-1);
}

static int FindBestSampleRate(const AVCodec *codec,
                              int rate) {
    int best = rate;
    int64_t best_error = INT64_MAX;

    for (const int *f = codec->supported_samplerates; *f != -1; ++f) {
        int64_t error = *f - rate;
        error *= error;

        if (error < best_error) {
            best_error = error;
            best = *f;
        }
    }

    return best;
}

bool InitFFmpeg(Messages *messages) {
    const AVOutputFormat *output_format = av_guess_format(FORMAT, nullptr, nullptr);

    g_acodec = avcodec_find_encoder(output_format->audio_codec);
    g_vcodec = avcodec_find_encoder(output_format->video_codec);

    if (!output_format || !g_acodec || !g_vcodec) {
        // The repercussions of this: the Video button just does
        // nothing. Which is a bit lame. Need a better mechanism for
        // handling this...
        messages->e.f("FFmpeg: no %s support\n", FORMAT);
    bad:;
        messages->i.f("(video saving won't work)\n");
        return true;
    }

    if (output_format->flags & AVFMT_NOFILE) {
        messages->e.f("FFmpeg: %s format doesn't support writing to file\n",
                      output_format->long_name);
        goto bad;
    }

    if (IsSampleFormatSupported(g_acodec, AV_SAMPLE_FMT_FLTP)) {
        g_aformat = AV_SAMPLE_FMT_FLTP;
    } else if (IsSampleFormatSupported(g_acodec, AV_SAMPLE_FMT_FLT)) {
        g_aformat = AV_SAMPLE_FMT_FLT;
    } else {
        messages->e.f("FFmpeg: %s codec doesn't support float audio\n", g_acodec->name);
        goto bad;
    }

#if LIBAVUTIL_VERSION_MAJOR < 57
    const bool is_mono_supported = IsChannelLayoutSupported(g_acodec, AV_CH_LAYOUT_MONO);
#else
    const bool is_mono_supported = IsChannelLayoutSupported(g_acodec, &CHANNEL_LAYOUT_MONO);
#endif
    if (!is_mono_supported) {
        messages->e.f("FFmpeg: %s codec doesn't support mono output\n", g_acodec->name);
        goto bad;
    }

    g_audio_spec.freq = FindBestSampleRate(g_acodec, 48000);
    g_audio_spec.format = AUDIO_F32SYS;
    g_audio_spec.channels = 1;

    LOGF(FFMPEG, "Audio format: %dHz\n", g_audio_spec.freq);

    LOGF(FFMPEG, "Video output formats:\n");

    for (int vscale = 1; vscale <= 2; ++vscale) {
        for (int ascale = 1; ascale <= 2; ++ascale) {
            VideoWriterFFmpegFormat f;

            f.vwidth = TV_TEXTURE_WIDTH * vscale;
            f.vheight = TV_TEXTURE_HEIGHT * vscale;
            f.vbitrate = 4000000;
            f.abitrate = 128000 * ascale;

            f.vwf.extension = std::string(".") + FORMAT;
            f.vwf.description = strprintf("%dx%d %s (%s %.1fMb/sec; %s %.1fKb/sec)",
                                          f.vwidth,
                                          f.vheight,
                                          output_format->name,
                                          g_vcodec->name,
                                          f.vbitrate / 1e6,
                                          g_acodec->name,
                                          f.abitrate / 1e3);

            LOGF(FFMPEG, "    %zu. %s\n", g_formats.size() + 1, f.vwf.description.c_str());

            g_formats.push_back(std::move(f));
        }
    }

    g_can_write_video = true;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumVideoWriterFFmpegFormats() {
    return g_formats.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const VideoWriterFormat *GetVideoWriterFFmpegFormatByIndex(size_t index) {
    return &g_formats[index].vwf;
}
