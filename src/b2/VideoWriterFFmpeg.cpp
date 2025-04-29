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
#include <libavcodec/avcodec.h>
}

#ifdef __clang__

#pragma GCC diagnostic pop

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 19, 0)
#define USE_AVCODEC_GET_SUPPORTED_CONFIG 1
#else
#define USE_AVCODEC_GET_SUPPORTED_CONFIG 0
#endif

#if LIBAVUTIL_VERSION_MAJOR >= 57
#define AVCHANNELLAYOUT_STRUCT 1
#else
#define AVCHANNELLAYOUT_STRUCT 0
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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
static AVSampleFormat g_aformat = AV_SAMPLE_FMT_NONE;

static const AVCodec *g_vcodec;

static SDL_AudioSpec g_audio_spec;

#if AVCHANNELLAYOUT_STRUCT
static const AVChannelLayout CHANNEL_LAYOUT_MONO = AV_CHANNEL_LAYOUT_MONO;
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(FFMPEG, "ffmpeg", "FFMPEG", &log_printer_stdout_and_debugger, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if USE_AVCODEC_GET_SUPPORTED_CONFIG
template <class T>
static const T *GetCodecConfigs(const AVCodec *codec, AVCodecConfig config) {
    const void *data;
    if (avcodec_get_supported_config(nullptr, codec, config, 0, &data, nullptr) < 0) {
        data = nullptr;
    }

    return (const T *)data;
}
#endif

// terminated by 0
static const int *GetCodecSampleRates(const AVCodec *codec) {
#if USE_AVCODEC_GET_SUPPORTED_CONFIG
    return GetCodecConfigs<int>(codec, AV_CODEC_CONFIG_SAMPLE_RATE);
#else
    return codec->supported_samplerates;
#endif
}

#if AVCHANNELLAYOUT_STRUCT
// terminated by {0}
static const AVChannelLayout *GetCodecChannelLayouts(const AVCodec *codec) {
#if USE_AVCODEC_GET_SUPPORTED_CONFIG
    return GetCodecConfigs<AVChannelLayout>(codec, AV_CODEC_CONFIG_CHANNEL_LAYOUT);
#else
    return codec->channel_layouts;
#endif
}
#endif

// terminated by AV_SAMPLE_FMT_NONE
static const AVSampleFormat *GetCodecSampleFormats(const AVCodec *codec) {
#if USE_AVCODEC_GET_SUPPORTED_CONFIG
    return GetCodecConfigs<AVSampleFormat>(codec, AV_CODEC_CONFIG_SAMPLE_FORMAT);
#else
    return codec->sample_fmts;
#endif
}

// terminated by AV_PIX_FMT_NONE
static const AVPixelFormat *GetCodecPixelFormats(const AVCodec *codec) {
#if USE_AVCODEC_GET_SUPPORTED_CONFIG
    return GetCodecConfigs<AVPixelFormat>(codec, AV_CODEC_CONFIG_PIX_FORMAT);
#else
    return codec->pix_fmts;
#endif
}

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

        m_vstream->time_base = av_make_q(1, (int)1e7);

        m_vcontext = avcodec_alloc_context3(g_vcodec);
        if (!m_vcontext) {
            return this->Error(0, "avcodec_alloc_context3 (video)");
        }

        m_vcontext->bit_rate = format->vbitrate;
        m_vcontext->width = format->vwidth;
        m_vcontext->height = format->vheight;
        m_vcontext->time_base = m_vstream->time_base;
        m_vcontext->gop_size = 12; //???
        {
            const AVPixelFormat *pix_fmts = GetCodecPixelFormats(g_vcodec);
            if (!pix_fmts || pix_fmts[0] == AV_PIX_FMT_NONE) {
                return this->Error(0, "codec appears to have no pixel formats (video)");
            }

            m_vcontext->pix_fmt = pix_fmts[0];
        }
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
#if !AVCHANNELLAYOUT_STRUCT
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
#if !AVCHANNELLAYOUT_STRUCT
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
#if !AVCHANNELLAYOUT_STRUCT
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

        return true;
    }

    bool EndWrite() override {
        int rc;

        if (!this->EndWrite(m_vcontext, m_vstream, "video")) {
            return false;
        }

        // Pad any partial frame with silence, and submit it.
        if (m_aframe_index > 0) {
            uint8_t *data = m_aframe->data[0];
            ASSERT(SDL_AUDIO_BITSIZE(g_audio_spec.format) % 8 == 0);
            int sample_size_bytes = SDL_AUDIO_BITSIZE(g_audio_spec.format) / 8;

            int num_bytes = (m_aframe->nb_samples - m_aframe_index) * sample_size_bytes;
            ASSERT(num_bytes >= 0);
            memset(data + m_aframe_index * sample_size_bytes, 0, (size_t)num_bytes);

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

    bool WriteSound(const void *data, size_t data_size_bytes) override {
        size_t audio_bitsize = SDL_AUDIO_BITSIZE(g_audio_spec.format);
        if (audio_bitsize == 8 || audio_bitsize == 16 || audio_bitsize == 32) {
            size_t sample_size_bytes = audio_bitsize / 8;
            auto src = (const uint8_t *)data;
            uint8_t *dest = m_aframe->data[0];
            size_t num_src_samples_left = data_size_bytes / sample_size_bytes;
            while (num_src_samples_left > 0) {
                if (m_aframe_index == 0) {
                    int rc = av_frame_make_writable(m_aframe);
                    if (rc < 0) {
                        return this->Error(rc, "av_frame_make_writable (audio)");
                    }

                    m_aframe->pts = m_apts;
                    m_apts += m_aframe->nb_samples;
                }

                ASSERT(m_aframe->nb_samples > m_aframe_index);
                size_t num_dest_samples = (size_t)(m_aframe->nb_samples - m_aframe_index);

                size_t n = num_src_samples_left;
                if (n > num_dest_samples) {
                    n = num_dest_samples;
                }

                ASSERT(m_aframe_index >= 0);
                memcpy(dest + (size_t)m_aframe_index * sample_size_bytes, src, n * sample_size_bytes);

                src += n * sample_size_bytes;
                num_src_samples_left -= n;

                m_aframe_index += n;
                ASSERT(m_aframe_index <= m_aframe->nb_samples);
                if (m_aframe_index == m_aframe->nb_samples) {
                    if (!this->Write(m_acontext, m_astream, m_aframe, "audio")) {
                        return false;
                    }

                    m_aframe_index = 0;
                }
            }

            return true;
        } else {
            return this->Error(0, "unsupported audio bit depth: %d", SDL_AUDIO_BITSIZE(g_audio_spec.format));
        }
    }

    bool WriteVideo(const void *data,int64_t timestamp_ns) override {
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

        m_vframe->pts = timestamp_ns;

        if (!this->Write(m_vcontext, m_vstream, m_vframe, "video")) {
            return false;
        }

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

#if !AVCHANNELLAYOUT_STRUCT
static bool IsChannelLayoutSupported(const AVCodec *codec,
                                     uint64_t requested_layout) {
    if (!codec->channel_layouts) {
        return true;
    }

    for (const uint64_t *codec_layout = codec->channel_layouts; *codec_layout != 0; ++codec_layout) {
        if (codec_layout == requested_layout) {
            return true;
        }
    }

    return false;
}
#else
static bool IsChannelLayoutSupported(const AVCodec *codec,
                                     const AVChannelLayout *requested_layout) {
    const AVChannelLayout *codec_layouts = GetCodecChannelLayouts(codec);

    if (!codec_layouts) {
        // Judging by the encode_audio example, if ch_layouts is NULL, anything goes?
        return true;
    }

    for (const AVChannelLayout *codec_layout = codec_layouts; codec_layout->order != 0; ++codec_layout) {
        if (av_channel_layout_compare(requested_layout, codec_layout) == 0) {
            return true;
        }
    }

    return false;
}
#endif

static bool IsSampleFormatSupported(const AVCodec *codec,
                                    AVSampleFormat requested_fmt) {
    //AV_CODEC_CONFIG_SAMPLE_FORMAT,  ///< AVSampleFormat, terminated by AV_SAMPLE_FMT_NONE
    //return IsSupported(fmt, codec->sample_fmts, (AVSampleFormat)-1);

    const AVSampleFormat *codec_fmts = GetCodecSampleFormats(codec);

    if (!codec_fmts) {
        return true;
    }

    for (const AVSampleFormat *codec_fmt = codec_fmts; *codec_fmt != AV_SAMPLE_FMT_NONE; ++codec_fmt) {
        if (*codec_fmt == requested_fmt) {
            return true;
        }
    }

    return false;
}

static int FindBestSampleRate(const AVCodec *codec,
                              int requested_rate) {
    int best_rate = requested_rate;

    if (const int *codec_rates = GetCodecSampleRates(codec)) {
        int64_t best_error = INT64_MAX;
        for (const int *codec_rate = codec_rates; *codec_rate != 0; ++codec_rate) {
            int64_t error = *codec_rate - requested_rate;
            error *= error;

            if (error < best_error) {
                best_error = error;
                best_rate = *codec_rate;
            }
        }
    }

    return best_rate;
}

static void CheckSampleFormatSupported(AVSampleFormat av_format, SDL_AudioFormat sdl_format) {
    if (g_aformat == AV_SAMPLE_FMT_NONE) {
        if (IsSampleFormatSupported(g_acodec, av_format)) {
            g_aformat = av_format;
            g_audio_spec.format = sdl_format;
        }
    }
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

    const AVCodec *flac_codec = avcodec_find_encoder(AV_CODEC_ID_FLAC);
    if (flac_codec) {
        g_acodec = flac_codec;
    }

    if (output_format->flags & AVFMT_NOFILE) {
        messages->e.f("FFmpeg: %s format doesn't support writing to file\n",
                      output_format->long_name);
        goto bad;
    }

    CheckSampleFormatSupported(AV_SAMPLE_FMT_S16, AUDIO_S16SYS);
    CheckSampleFormatSupported(AV_SAMPLE_FMT_S16P, AUDIO_S16SYS);
    CheckSampleFormatSupported(AV_SAMPLE_FMT_FLT, AUDIO_S16SYS);
    CheckSampleFormatSupported(AV_SAMPLE_FMT_FLTP, AUDIO_S16SYS);
    if (g_aformat == AV_SAMPLE_FMT_NONE) {
        messages->e.f("FFmpeg: %s codec doesn't support either float or 16-bit PCM audio\n", g_acodec->name);
        goto bad;
    }

#if !AVCHANNELLAYOUT_STRUCT
    const bool is_mono_supported = IsChannelLayoutSupported(g_acodec, AV_CH_LAYOUT_MONO);
#else
    const bool is_mono_supported = IsChannelLayoutSupported(g_acodec, &CHANNEL_LAYOUT_MONO);
#endif
    if (!is_mono_supported) {
        messages->e.f("FFmpeg: %s codec doesn't support mono output\n", g_acodec->name);
        goto bad;
    }

    g_audio_spec.freq = FindBestSampleRate(g_acodec, 48000);
    g_audio_spec.channels = 1;

    LOGF(FFMPEG, "Audio format: %dHz\n", g_audio_spec.freq);

    LOGF(FFMPEG, "Video output formats:\n");

    for (int vscale = 1; vscale <= 2; ++vscale) {
        VideoWriterFFmpegFormat f;

        f.vwidth = TV_TEXTURE_WIDTH * vscale;
        f.vheight = TV_TEXTURE_HEIGHT * vscale;
        f.vbitrate = 4000000;
        f.abitrate = 256000;

        f.vwf.extension = std::string(".") + FORMAT;
        f.vwf.description = strprintf("%dx%d %s (", f.vwidth, f.vheight, output_format->name);

        f.vwf.description += strprintf("%s %.1f Mb/sec; ", g_vcodec->name, f.vbitrate / 1e6);

        const AVCodecDescriptor *acodec_desc = avcodec_descriptor_get(g_acodec->id);
        if (acodec_desc && acodec_desc->props & AV_CODEC_PROP_LOSSY) {
            f.vwf.description += strprintf("%s %.1f Kb/sec", g_acodec->name, f.abitrate / 1e3);
        } else {
            f.vwf.description += strprintf("%.1f KHz %s", g_audio_spec.freq / 1e3, g_acodec->name);
        }

        f.vwf.description += ")";

        LOGF(FFMPEG, "    %zu. %s\n", g_formats.size() + 1, f.vwf.description.c_str());

        g_formats.push_back(std::move(f));
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
