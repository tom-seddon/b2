#include <shared/system.h>
#include <stdio.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <vector>
#include <string.h>
#include <string>
#include <shared/path.h>
#include <algorithm>
#include <shared/testing.h>
#include <shared/CommandLineParser.h>
#include <map>
#include <shared/file_io.h>

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

static int g_last_av = 0;

#define TEST_AV(X) TEST_TRUE((g_last_av = (X)) >= 0)

static void PrintLastAVResult(const TestFailArgs *tfa) {
    (void)tfa;

    char tmp[AV_ERROR_MAX_STRING_SIZE];

    av_strerror(g_last_av, tmp, sizeof tmp);

    LOGF(ERR, "Last FFmpeg result: %d (%s)\n", g_last_av, tmp);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static std::string av_strerror(int result) {
//    char tmp[AV_ERROR_MAX_STRING_SIZE];
//
//    av_strerror(result,tmp,sizeof tmp);
//
//    return tmp;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "test_ffmpeg.inl"
#include <shared/enum_end.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <shared/enum_def.h>
#include "test_ffmpeg.inl"
#include <shared/enum_end.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define STBI_ASSERT ASSERT
#define STB_IMAGE_IMPLEMENTATION
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "stb_image.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(VOUT, "", &log_printer_stdout_and_debugger, false);
LOG_DEFINE(OUT, "", &log_printer_stdout_and_debugger);
LOG_DEFINE(ERR, "", &log_printer_stderr_and_debugger);

static const LogSet g_log_set = {LOG(VOUT), LOG(OUT), LOG(ERR)};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
struct WAVEFORMAT {
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
};
#include <shared/poppack.h>
CHECK_SIZEOF(WAVEFORMAT, 14);

#include <shared/pshpack1.h>
struct WAVEFORMATEX : WAVEFORMAT {
    uint16_t wBitsPerSample;
    uint16_t cbSize;
};
#include <shared/poppack.h>
CHECK_SIZEOF(WAVEFORMATEX, 18);

static const uint16_t WAVE_FORMAT_PCM = 1;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool StringPathLessThanStringPath(const std::string &a, const std::string &b) {
    return PathCompare(a.c_str(), b.c_str()) < 0;
}

static std::vector<std::string> FindFileNamesWithExtension(const std::vector<std::string> &names, const char *ext) {
    std::vector<std::string> result;

    for (const std::string &name : names) {
        std::string e = PathGetExtension(name);
        if (PathCompare(e, ext) == 0) {
            result.push_back(name);
        }
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool ForEachWavFileChunk(
    const void *wav_data_arg,
    size_t wav_data_size,
    bool (*chunk_fn)(const char *id, uint32_t size, const char *data, void *context),
    void *context,
    char *err_text,
    size_t err_text_size) {
    const char *wav_data = (const char *)wav_data_arg;

    bool good = false;

    if (wav_data_size >= 12 && strncmp(wav_data, "RIFF", 4) == 0 && strncmp(wav_data + 8, "WAVE", 4) == 0) {
        uint32_t riff_size = *(const uint32_t *)(wav_data + 4);
        if (riff_size <= wav_data_size - 8) {
            uint32_t chunk_offset = 4;

            good = true;

            while (chunk_offset < riff_size) {
                const char *header = wav_data + 8 + chunk_offset;

                char id[5];
                memcpy(id, header, 4);
                id[4] = 0;

                uint32_t size = *(const uint32_t *)(header + 4);

                if (!(*chunk_fn)(id, size, header + 8, context)) {
                    good = false;
                    break;
                }

                if (size % 2 != 0) {
                    // odd-sized chunks are followed by a pad byte
                    ++size;
                }

                chunk_offset += 8 + size;
            }

            ASSERT(chunk_offset == riff_size);
        }
    } else {
        snprintf(err_text, err_text_size, "not a WAV file");
    }

    return good;
}

struct WAVFile {
    std::vector<char> fmt_buf, data;
};

static bool StoreWAVFile(const char *id, uint32_t size, const char *data, void *context) {
    auto file = (WAVFile *)context;
    std::vector<char> *buf = nullptr;

    if (strcmp(id, "fmt ") == 0) {
        if (size < sizeof(WAVEFORMAT)) {
            return false;
        }

        buf = &file->fmt_buf;
    } else if (strcmp(id, "data") == 0) {
        buf = &file->data;
    }

    if (buf) {
        if (size > 0) {
            buf->resize(size);
            memcpy(buf->data(), data, size);
        }
    }

    return true;
}

static bool LoadWAVFile(WAVFile *file, const std::string &file_name) {
    std::vector<uint8_t> data;
    if (!LoadFile(&data, file_name, &g_log_set)) {
        return false;
    }

    char error[100];
    bool good = ForEachWavFileChunk(data.data(), data.size(), &StoreWAVFile, file, error, sizeof error);

    if (!good) {
        return false;
    }

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
static void PrintFlagsOneLine(T value, const char *(*get_name_fn)(T)) {
    bool any = false;

    for (T mask = 0; mask != 0; mask <<= 1) {
        const char *name = (*get_name_fn)(mask);
        if (name[0] == '?') {
            continue;
        }

        if (value & mask) {
            if (any) {
                LOGF(OUT, " ");
            }
            LOG_STR(OUT, name);
            any = true;
        }
    }

    if (!any) {
        LOG_STR(OUT, "(none)");
    }
}

template <class T>
static void PrintFlags(T value, const char *(*get_name_fn)(T)) {
    bool any = false;

    {
        LOGI(OUT);
        for (T mask = 0; mask != 0; mask <<= 1) {
            const char *name = (*get_name_fn)(mask);
            if (name[0] == '?') {
                continue;
            }

            if (value & mask) {
                LOGF(OUT, "%s\n", name);
                any = true;
            }
        }
    }

    // Checking for value==0 is no good - some codecs have bits set
    // that appear to have no name.
    if (!any) {
        LOGF(OUT, "(none)\n");
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DumpCodecsBrief(const char *type_name, AVMediaType type) {
    std::vector<const char *> names;

    {
        void *opaque = nullptr;
        while (const AVCodec *c = av_codec_iterate(&opaque)) {
            if (c->type == type) {
                names.push_back(c->name);
            }
        }
    }

    std::sort(names.begin(), names.end(), [](auto a, auto b) {
        return strcmp(a, b) < 0;
    });

    LOGF(OUT, "%s codecs:", type_name);

    if (names.empty()) {
        LOGF(OUT, " (none)\n");
    } else {
        for (const char *name : names) {
            LOGF(OUT, " %s", name);
        }
        LOGF(OUT, "\n");
    }
}

static void DumpCodecsBrief() {
    DumpCodecsBrief("Video", AVMEDIA_TYPE_VIDEO);
    DumpCodecsBrief("Audio", AVMEDIA_TYPE_AUDIO);
}

template <class T>
static void DumpCodecFormats(const T *fmts,
                             const char *label,
                             char *(*get_string_fn)(char *, int, T),
                             bool header,
                             T terminator) {
    if (!fmts) {
        return;
    }

    LOGF(OUT, "%s: ", label);
    LOGI(OUT);

    char desc[1000];

    if (header) {
        (*get_string_fn)(desc, sizeof desc, (T)-1);
        LOGF(OUT, "%s\n", desc);
    }

    for (const T *p = fmts; *p != terminator; ++p) {
        (*get_string_fn)(desc, sizeof desc, *p);
        LOGF(OUT, "%s\n", desc);
    }
}

#if LIBAVCODEC_VERSION_MAJOR < 60
static char *GetChannelLayoutString(char *buf, int n, uint64_t layout) {
    av_get_channel_layout_string(buf, n, -1, layout);
    return nullptr;
}
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 60
static void DumpChLayouts(const AVCodec *c) {
    LOGF(OUT, "Channel Layouts: ");
    LOGI(OUT);

    if (!c->ch_layouts) {
        LOGF(OUT, "NULL\n");
    } else {
        for (const AVChannelLayout *l = c->ch_layouts; l->order != 0; ++l) {
            LOGF(OUT, "%zd. ", l - c->ch_layouts);
            LOGI(OUT);
            LOGF(OUT, "order=%d (%s)\n", (int)l->order, GetAVChannelOrderEnumName(l->order));
            LOGF(OUT, "nb_channels=%d\n", l->nb_channels);
        }
    }
}
#endif

static char *GetSampleRateString(char *buf, int n, int hz) {
    ASSERT(n >= 0);
    snprintf(buf, (size_t)n, "%dHz", hz);
    return nullptr;
}

static void DumpCodecVerbose(const AVCodec *c) {
    if (!c) {
        LOGF(OUT, "(none)\n");
    } else {
        LOGF(OUT, "%s: ", c->name);
        LOGI(OUT);
        LOGF(OUT, "%s\n", c->long_name);

        LOGF(OUT, "Type: %s\n", GetAVMediaTypeEnumName(c->type));

        //LOGF(OUT,"Codec ID: %s\n",GetAVCodecName(c->id));

        DumpCodecFormats(c->pix_fmts,
                         "Pixel Formats",
                         &av_get_pix_fmt_string,
                         true,
                         (AVPixelFormat)-1);

        DumpCodecFormats(c->sample_fmts,
                         "Sample Formats",
                         &av_get_sample_fmt_string,
                         true,
                         (AVSampleFormat)-1);

#if LIBAVCODEC_VERSION_MAJOR >= 60

        DumpChLayouts(c);

#else

        DumpCodecFormats(c->channel_layouts,
                         "Channel Layouts",
                         &GetChannelLayoutString,
                         false,
                         (uint64_t)0);

#endif

        DumpCodecFormats(c->supported_samplerates,
                         "Sample Rates",
                         &GetSampleRateString,
                         false,
                         0);

        LOGF(OUT, "Capabilities: ");
        PrintFlags((uint32_t)c->capabilities, &GetAVCodecCapEnumName);
    }
}

static void DumpCodecsVerbose() {
    void *opaque = nullptr;
    while (const AVCodec *c = av_codec_iterate(&opaque)) {
        DumpCodecVerbose(c);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *GetAVCodecNameById(AVCodecID id) {
    void *opaque = nullptr;
    while (const AVCodec *c = av_codec_iterate(&opaque)) {
        if (c->id == id) {
            return c->name;
        }
    }

    return "(none)";
}

static void DumpFormatsOrg() {
    void *opaque = nullptr;
    LOGF(OUT, "|name|long_name|mime_type|extensions|acodec|vcodec|scodec|flags|\n");
    LOGF(OUT, "|---\n");
    while (const AVOutputFormat *f = av_muxer_iterate(&opaque)) {
        LOGF(OUT, "|%s ", f->name);
        LOGF(OUT, "|%s", f->long_name);
        LOGF(OUT, "|%s", f->mime_type);
        LOGF(OUT, "|%s", f->extensions);
        LOGF(OUT, "|%s", GetAVCodecNameById(f->audio_codec));
        LOGF(OUT, "|%s", GetAVCodecNameById(f->video_codec));
        LOGF(OUT, "|%s", GetAVCodecNameById(f->subtitle_codec));
        LOGF(OUT, "|");
        PrintFlagsOneLine(f->flags, &GetAVOutputFormatFlagEnumName);
        LOGF(OUT, "\n");
    }
}

static void DumpFormatsVerbose() {
    void *opaque = nullptr;
    while (const AVOutputFormat *f = av_muxer_iterate(&opaque)) {
        LOGF(OUT, "%s: ", f->name);
        LOGI(OUT);
        LOGF(OUT, "%s\n", f->long_name);
        LOGF(OUT, "MIME Type: %s\n", f->mime_type);
        LOGF(OUT, "Extensions: %s\n", f->extensions);
        LOGF(OUT, "Default Audio Codec: %s\n", GetAVCodecNameById(f->audio_codec));
        LOGF(OUT, "Default Video Codec: %s\n", GetAVCodecNameById(f->video_codec));
        LOGF(OUT, "Default Subtitle Codec: %s\n", GetAVCodecNameById(f->subtitle_codec));

        LOGF(OUT, "Flags: ");
        PrintFlags(f->flags, &GetAVOutputFormatFlagEnumName);
    }
}

static void DumpFormatsBrief() {
    std::vector<const char *> names;

    void *opaque = nullptr;
    while (const AVOutputFormat *f = av_muxer_iterate(&opaque)) {
        names.push_back(f->name);
    }

    std::sort(names.begin(), names.end(), [](auto a, auto b) {
        return strcmp(a, b) < 0;
    });

    LOGF(OUT, "Formats:");

    if (names.empty()) {
        LOGF(OUT, " (none)\n");
    } else {
        for (const char *name : names) {
            LOGF(OUT, " %s", name);
        }
        LOGF(OUT, "\n");
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    bool version = false;
    bool dump_codecs = false;
    bool dump_formats = false;
    bool verbose = false;
    std::string folder_name;
    std::string output_file_name;
    std::string acodec;
    std::string vcodec;
    std::string format = "mp4";
    std::map<std::string, std::string> options;
    int num_frames = 0;
    std::string audio_output_file_name;
    bool org = false;
};

static bool DoOptions(Options *o, int argc, char *argv[]) {
    CommandLineParser p;

    // there used to be a way to fill this! - maybe it will come back
    // one day.
    std::vector<std::string> options;
    bool help;

    p.AddOption("version").Help("show version numbers").SetIfPresent(&o->version);
    p.AddOption("codecs").Help("dump codecs list").SetIfPresent(&o->dump_codecs);
    p.AddOption("formats").Help("dump output formats").SetIfPresent(&o->dump_formats);
    p.AddOption('V', "verbose").Help("be extra verbose").SetIfPresent(&o->verbose);
    p.AddOption('f', "format").Meta("FORMAT").Help("set output format").Arg(&o->format).ShowDefault();
    p.AddOption('a', "audio-codec").Meta("CODEC").Help("set audio codec").Arg(&o->acodec);
    p.AddOption('v', "video-codec").Meta("CODEC").Help("set video codec").Arg(&o->vcodec);
    p.AddOption('o').Meta("FILE").Help("write output to FILE").Arg(&o->output_file_name);
    p.AddOption("org").Help("produce org-mode-friendly output when possible").SetIfPresent(&o->org);

    p.AddOption('n', "num-frames").Meta("COUNT").Help("write out max COUNT frames").Arg(&o->num_frames);
    p.AddOption('A', "audio-output").Meta("FILE").Help("write raw audio data to FILE").Arg(&o->audio_output_file_name);

    p.AddHelpOption(&help);

    std::vector<std::string> other_args;
    if (!p.Parse(argc, argv, &other_args) || help) {
        p.Help(argv[0]);

        // not great to return exit code 1 on -h, but it's only test code
        return false;
    }

    if (other_args.size() > 1) {
        LOGF(ERR, "Must specify exactly one folder\n");
        return false;
    } else if (other_args.size() == 1) {
        o->folder_name = other_args[0];
    }

    if (o->num_frames < 0) {
        LOGF(ERR, "invalid --num-frames: %d\n", o->num_frames);
        return false;
    }

    for (const std::string &option : options) {
        std::string::size_type eq = option.find_first_of("=");
        if (eq == std::string::npos) {
            LOGF(ERR, "invalid option syntax: %s\n", option.c_str());
            return false;
        } else {
            std::string k = option.substr(0, eq), v = option.substr(eq + 1);
            if (o->options.count(k) > 0) {
                LOGF(ERR, "duplicated option: %s\n", k.c_str());
                return false;
            } else {
                o->options[k] = v;
            }
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static double GetSecondsFromPTS(int64_t pts, const AVRational &time_base) {
    return pts * av_q2d(time_base);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void WriteFrames(AVFormatContext *f_context,
                        AVCodecContext *c_context,
                        int stream_index,
                        int finish_error,
                        const char *label) {
    ASSERT(stream_index >= 0 && (unsigned)stream_index < f_context->nb_streams);
    AVStream *stream = f_context->streams[stream_index];

    for (;;) {
        AVPacket *packet = av_packet_alloc();

        g_last_av = avcodec_receive_packet(c_context, packet);
        if (g_last_av == finish_error) {
            av_packet_free(&packet);
            ASSERT(!packet);
            return;
        } else if (g_last_av < 0) {
            TEST_TRUE(false);
        }

        int64_t old_pts = packet->pts;
        (void)old_pts;

        av_packet_rescale_ts(packet, c_context->time_base, stream->time_base);
        packet->stream_index = stream_index;

        LOGF(VOUT, "RECV: %s: pts=%" PRId64 " (%f sec)\n", label, packet->pts, GetSecondsFromPTS(packet->pts, stream->time_base));

        // %s packet: stream_index=%d: old pts=%" PRId64 " (%f sec); new pts=%" PRId64 " (%f sec)\n",
        //                label,
        //                packet->stream_index,
        //                old_pts,
        //                GetSecondsFromPTS(old_pts,c_context->time_base),
        //                packet->pts,
        //                GetSecondsFromPTS(packet->pts,stream->time_base));

        TEST_AV(av_interleaved_write_frame(f_context, packet));

        av_packet_free(&packet);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindCodec(const AVCodec **codec_ptr,
                      const AVOutputFormat *oformat,
                      AVCodecID default_id,
                      const std::string &name,
                      AVMediaType type) {
    if (!name.empty()) {
        if (name == "none") {
            *codec_ptr = nullptr;
            return true;
        }

        const AVCodec *codec = avcodec_find_encoder_by_name(name.c_str());
        if (!codec) {
            LOGF(ERR, "unknown codec: %s\n", name.c_str());
            return false;
        }

        if (codec->type != type) {
            LOGF(ERR, "wrong codec type: %s\n", name.c_str());
            LOGF(OUT, "(expected %s; got %s)\n",
                 GetAVMediaTypeEnumName(type),
                 GetAVMediaTypeEnumName(codec->type));
            return false;
        }

        *codec_ptr = codec;
        return true;
    } else {
        if (default_id == AV_CODEC_ID_NONE) {
            LOGF(ERR, "no %s codec for output format: %s\n",
                 GetAVMediaTypeEnumName(type),
                 oformat->name);
            return false;
        }

        const AVCodec *codec = avcodec_find_encoder(default_id);
        ASSERT(codec);
        ASSERT(codec->type == type);

        *codec_ptr = codec;
        return true;
    }
}

static AVDictionary *CreateAVDictionary(const std::map<std::string, std::string> &kvs) {
    AVDictionary *dict = nullptr;

    for (auto &&kv : kvs) {
        av_dict_set(&dict, kv.first.c_str(), kv.second.c_str(), 0);
    }

    return dict;
}

int main(int argc, char *argv[]) {
    Options options;
    if (!DoOptions(&options, argc, argv)) {
        return 1;
    }

    if (options.verbose) {
        LOG(VOUT).Enable();
    }

    if (options.version) {
        LOGF(OUT, "libavcodec: ");
        {
            LOGI(OUT);
            LOGF(OUT, "version: %u.%u.%u\n", AV_VERSION_MAJOR(avcodec_version()), AV_VERSION_MINOR(avcodec_version()), AV_VERSION_MICRO(avcodec_version()));
            LOGF(OUT, "configuration: %s\n", avcodec_configuration());
            LOGF(OUT, "licence: %s\n", avcodec_license());
        }

        LOGF(OUT, "libavformat: ");
        {
            LOGI(OUT);
            LOGF(OUT, "version: %u.%u.%u\n", AV_VERSION_MAJOR(avformat_version()), AV_VERSION_MINOR(avformat_version()), AV_VERSION_MICRO(avformat_version()));
            LOGF(OUT, "configuration: %s\n", avformat_configuration());
            LOGF(OUT, "licence: %s\n", avformat_license());
        }
    }

    if (options.dump_codecs) {
        if (options.verbose) {
            DumpCodecsVerbose();
        } else {
            DumpCodecsBrief();
        }
    }

    if (options.dump_formats) {
        if (options.org) {
            DumpFormatsOrg();
        } else if (options.verbose) {
            DumpFormatsVerbose();
        } else {
            DumpFormatsBrief();
        }
    }

    if (options.folder_name.empty()) {
        LOGF(ERR, "no folder specified.\n");
        return 1;
    }

    std::vector<std::string> jpeg_file_names, wav_file_names;
    {
        std::vector<std::string> file_names;
        PathGlob(options.folder_name, [&](const std::string &path, bool is_folder) {
            if (!is_folder) {
                file_names.push_back(path);
            }
        });

        jpeg_file_names = FindFileNamesWithExtension(file_names, ".jpg");
        wav_file_names = FindFileNamesWithExtension(file_names, ".wav");

        if (jpeg_file_names.empty()) {
            LOGF(ERR, "no JPEGs found in: %s\n", options.folder_name.c_str());
            return 1;
        }

        if (wav_file_names.size() > 1) {
            LOGF(ERR, "multiple WAVs found in: %s\n", options.folder_name.c_str());
            return 1;
        }

        std::sort(jpeg_file_names.begin(), jpeg_file_names.end(), &StringPathLessThanStringPath);
    }

    LOGF(VOUT, "%zu JPEG files\n", jpeg_file_names.size());

    WAVFile wav_file;
    const WAVEFORMATEX *wav_fmt = nullptr;
    size_t wav_length_samples = 0, bytes_per_sample = 0;
    AVRational frame_duration;
    {
        double fps;

        if (!wav_file_names.empty()) {
            if (!LoadWAVFile(&wav_file, wav_file_names[0].c_str())) {
                LOGF(ERR, "failed to load WAV file: %s\n", wav_file_names[0].c_str());
                return 1;
            }

            wav_fmt = (const WAVEFORMATEX *)wav_file.fmt_buf.data();

            if (wav_fmt->wFormatTag != WAVE_FORMAT_PCM ||
                (wav_fmt->nChannels != 1 && wav_fmt->nChannels != 2) ||
                (wav_fmt->wBitsPerSample != 8 && wav_fmt->wBitsPerSample != 16)) {
                LOGF(ERR, "not PCM 8-/16-bit mono/stereo WAV file: %s\n", wav_file_names[0].c_str());
                return 1;
            }

            size_t bytes_per_second = wav_fmt->nSamplesPerSec * wav_fmt->nChannels * wav_fmt->wBitsPerSample / 8;
            double num_seconds = (double)wav_file.data.size() / bytes_per_second;
            bytes_per_sample = (uint32_t)wav_fmt->nChannels * wav_fmt->wBitsPerSample / 8u;
            TEST_EQ_UU(wav_file.data.size() % bytes_per_sample, 0);
            wav_length_samples = wav_file.data.size() / bytes_per_sample;
            fps = jpeg_file_names.size() / num_seconds;

            LOGF(VOUT, "%zu bytes WAV data\n", wav_file.data.size());
            LOGF(VOUT, "%zu samples WAV data\n", wav_length_samples);
            LOGF(VOUT, "%zu bytes/second\n", bytes_per_second);
            LOGF(VOUT, "%f seconds WAV data\n", num_seconds);
            LOGF(VOUT, "implying %f frames/second or %f seconds/frame\n",
                 fps,
                 num_seconds / jpeg_file_names.size());
        } else {
            fps = 30.;
        }

        // Ensure denominator is max 65535. See mpegvideo_enc.c.
        // frame_duration.num = 100000;
        // frame_duration.den = (int)(fps * 100000.);
        frame_duration.num=(int)(65535.0/fps);
        frame_duration.den=65535;
        
        // the calculation can be (# jpegs)*(bytes/sec)/(bytes data),
        // which could be used as the MF_MT_FRAME_RATE_FPS ratio. But
        // they're only UINT32s and that doesn't leave much overhead.
        //frame_duration=(uint64_t)(10000000./fps);
        LOGF(OUT, "Frame duration period: %d/%d (%f) seconds\n",
             frame_duration.num,
             frame_duration.den,
             (double)frame_duration.num / frame_duration.den);
    }

    int width, height;
    if (!stbi_info(jpeg_file_names[0].c_str(), &width, &height, nullptr)) {
        LOGF(ERR, "failed to get info from first file: %s\n", jpeg_file_names[0].c_str());
        return 1;
    }

    LOGF(OUT, "%d x %d\n", width, height);

    TEST_TRUE(width > 0);
    TEST_TRUE(height > 0);

    if (options.output_file_name.empty()) {
        LOGF(ERR, "no output file specified.\n");
        return 1;
    }

    FILE *audio_output_file = nullptr;
    if (!options.audio_output_file_name.empty()) {
        audio_output_file = fopen(options.audio_output_file_name.c_str(), "wb");
        if (!audio_output_file) {
            int e = errno;

            LOGF(ERR, "failed to open audio output file: %s\n", options.audio_output_file_name.c_str());
            LOGF(OUT, "    (fopen failed: %s)\n", strerror(e));
            return 1;
        }
    }

    AddTestFailFn(&PrintLastAVResult, nullptr);

    AVFormatContext *avf_context;
    TEST_AV(avformat_alloc_output_context2(&avf_context,
                                           nullptr,
                                           options.format.c_str(),
                                           options.output_file_name.c_str()));
    TEST_FALSE(avf_context->oformat->flags & AVFMT_NOFILE);

    TEST_AV(avio_open(&avf_context->pb,
                      options.output_file_name.c_str(),
                      AVIO_FLAG_WRITE));

    const AVCodec *acodec, *vcodec;

    if (!FindCodec(&acodec,
                   avf_context->oformat,
                   avf_context->oformat->audio_codec,
                   options.acodec,
                   AVMEDIA_TYPE_AUDIO)) {
        return 1;
    }

    if (!FindCodec(&vcodec,
                   avf_context->oformat,
                   avf_context->oformat->video_codec,
                   options.vcodec,
                   AVMEDIA_TYPE_VIDEO)) {
        return 1;
    }

    AVPixelFormat stb_pix_fmt = AV_PIX_FMT_RGBA;

    // Pick video codec's favourite pixel format.
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

    if (vcodec) {
        pix_fmt = vcodec->pix_fmts[0];

        // But if the codec supports the stb_image pixel format, do use
        // that.
        for (const AVPixelFormat *p = vcodec->pix_fmts; *p != -1; ++p) {
            if (*p == stb_pix_fmt) {
                pix_fmt = stb_pix_fmt;
                break;
            }
        }
    }

    if (acodec) {
        // Try to avoid resampling hell.
        bool found;

        found = false;
        for (const AVSampleFormat *p = acodec->sample_fmts; *p != -1; ++p) {
            if (*p == AV_SAMPLE_FMT_FLTP) {
                found = true;
                break;
            }
        }

        if (!found) {
            LOGF(OUT, "audio codec doesn't support fltp\n");
            return 1;
        }

        if (acodec->supported_samplerates) {
            found = false;

            for (const int *p = acodec->supported_samplerates; *p != 0; ++p) {
                if (*p == (int)wav_fmt->nSamplesPerSec) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                LOGF(OUT, "audio codec doesn't support WAV sample rate: %uHz\n", wav_fmt->nSamplesPerSec);
                return 1;
            }
        } else {
            // Presumably this means anything goes...??
        }
    }

    LOGF(OUT, "Output format: %s\n", avf_context->oformat->name);
    LOGF(OUT, "Video codec: ");
    DumpCodecVerbose(vcodec);

    LOGF(OUT, "Audio codec: ");
    DumpCodecVerbose(acodec);

    AVStream *vstream = nullptr;
    AVCodecContext *vc_context = nullptr;
    AVFrame *vframe = nullptr, *tmp_vframe = nullptr;
    if (vcodec) {
        vstream = avformat_new_stream(avf_context, vcodec);
        TEST_NON_NULL(vstream);

        vc_context = avcodec_alloc_context3(vcodec);
        TEST_NON_NULL(vc_context);

        vc_context->bit_rate = 800000;
        vc_context->width = width;
        vc_context->height = height;
        vc_context->time_base = vstream->time_base = frame_duration;
        vc_context->gop_size = 12; //???
        vc_context->pix_fmt = pix_fmt;

        // -strict -2
        vc_context->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
        if (avf_context->oformat->flags & AVFMT_GLOBALHEADER) {
            vc_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        AVDictionary *vopts = CreateAVDictionary(options.options);
        TEST_AV(avcodec_open2(vc_context, vcodec, &vopts));
        av_dict_free(&vopts);

        TEST_AV(avcodec_parameters_from_context(vstream->codecpar,
                                                vc_context));

        vframe = av_frame_alloc();
        TEST_NON_NULL(vframe);

        vframe->format = vc_context->pix_fmt;
        vframe->width = width;
        vframe->height = height;

        // 32??? - that's what the muxing example does, and presumably
        // there's a reason for it.
        TEST_AV(av_frame_get_buffer(vframe, 32));

        tmp_vframe = av_frame_alloc();
        TEST_NON_NULL(tmp_vframe);

        tmp_vframe->format = stb_pix_fmt;
        tmp_vframe->width = width;
        tmp_vframe->height = height;

        TEST_AV(av_frame_get_buffer(tmp_vframe, 32));
    }

    AVStream *astream = nullptr;
    AVCodecContext *ac_context = nullptr;
    AVFrame *aframe = nullptr;
    if (acodec) {
        astream = avformat_new_stream(avf_context, acodec);
        TEST_NON_NULL(astream);

        ac_context = avcodec_alloc_context3(acodec);
        TEST_NON_NULL(ac_context);

        ac_context->bit_rate = 128000;
        ac_context->sample_rate = (int)wav_fmt->nSamplesPerSec;
        ac_context->sample_fmt = AV_SAMPLE_FMT_FLTP;
#if LIBAVUTIL_VERSION_MAJOR < 58
        ac_context->channel_layout = wav_fmt->nChannels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
        ac_context->channels = av_get_channel_layout_nb_channels(ac_context->channel_layout);
#else
        const AVChannelLayout LAYOUT_MONO = AV_CHANNEL_LAYOUT_MONO;
        const AVChannelLayout LAYOUT_STEREO = AV_CHANNEL_LAYOUT_STEREO;
        TEST_AV(av_channel_layout_copy(&ac_context->ch_layout, wav_fmt->nChannels == 1 ? &LAYOUT_MONO : &LAYOUT_STEREO));
#endif
        ac_context->time_base = av_make_q(1, ac_context->sample_rate);

        // -strict -2
        ac_context->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

        if (avf_context->oformat->flags & AVFMT_GLOBALHEADER) {
            ac_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        AVDictionary *aopts = CreateAVDictionary(options.options);
        TEST_AV(avcodec_open2(ac_context, acodec, &aopts));
        av_dict_free(&aopts);

        TEST_AV(avcodec_parameters_from_context(astream->codecpar,
                                                ac_context));

        aframe = av_frame_alloc();
        TEST_NON_NULL(aframe);

        aframe->format = ac_context->sample_fmt;
        aframe->nb_samples = ac_context->frame_size;
        aframe->sample_rate = ac_context->sample_rate;

#if LIBAVUTIL_VERSION_MAJOR < 58
        aframe->channel_layout = ac_context->channel_layout;
#else
        TEST_AV(av_channel_layout_copy(&aframe->ch_layout, &ac_context->ch_layout));
#endif

        TEST_AV(av_frame_get_buffer(aframe, 0));

        LOGF(OUT, "Audio: (context) frame size: %d\n", ac_context->frame_size);
        LOGF(OUT, "Audio: (frame) frame size: %d\n", aframe->nb_samples);
    }

    // init scaler context.
    SwsContext *sws_context = sws_getContext(width,
                                             height,
                                             (AVPixelFormat)tmp_vframe->format,
                                             width,
                                             height,
                                             (AVPixelFormat)vframe->format,
                                             SWS_BICUBIC,
                                             nullptr,
                                             nullptr,
                                             nullptr);
    TEST_NON_NULL(sws_context);

    // https://blogs.gentoo.org/lu_zero/2016/03/29/new-avcodec-api/

    //TEST_TRUE(vcodec&&!acodec);

    {
        AVDictionary *opts = CreateAVDictionary(options.options);
        TEST_AV(avformat_write_header(avf_context,
                                      &opts));
        av_dict_free(&opts);
    }

    int max_file_name_len = 0;
    for (const std::string &f : jpeg_file_names) {
        ASSERT(f.size() <= INT_MAX);
        if ((int)f.size() > max_file_name_len) {
            max_file_name_len = (int)f.size();
        }
    }

    TEST_NON_NULL(vcodec);
    TEST_NON_NULL(acodec);

    size_t aframe_index = 0;
    uint64_t error = 0, step = 0, limit = 0;
    if (wav_fmt) {
        step = jpeg_file_names.size();
        limit = wav_length_samples;
    }

    size_t wav_file_data_index = 0;

    LOGF(VOUT, "Audio context timebase: %d/%d\n",
         ac_context->time_base.num,
         ac_context->time_base.den);
    LOGF(VOUT, "Audio stream timebase: %d/%d\n",
         astream->time_base.num,
         astream->time_base.den);
    LOGF(VOUT, "Video context timebase: %d/%d\n",
         vc_context->time_base.num,
         vc_context->time_base.den);
    LOGF(VOUT, "Video stream timebase: %d/%d\n",
         vstream->time_base.num,
         vstream->time_base.den);
    LOGF(VOUT, "Audio frame stuff: nb_samples=%d, linesize=%u\n",
         aframe->nb_samples, aframe->linesize[0]);

    size_t num_frames = jpeg_file_names.size();
    if (options.num_frames > 0 && num_frames > (size_t)options.num_frames) {
        num_frames = (size_t)options.num_frames;
    }

    for (size_t frame = 0; frame < num_frames; ++frame) {
        const std::string &file_name = jpeg_file_names[frame];

        // LOGF(OUT,"%p %p\n",(void *)aframe,(void *)vframe);

        LOGF(OUT, "File: %-*s (%03d%%)\r",
             max_file_name_len,
             file_name.c_str(),
             (int)((frame + 1.) / num_frames * 100.));
        LOG(OUT).Flush();
        LOGF(VOUT, "\n");
        fflush(stdout);

        int w, h, ncomp;
        stbi_uc *image_data = stbi_load(file_name.c_str(), &w, &h, &ncomp, 4);
        if (!image_data) {
            LOGF(ERR, "failed to load file: %s\n", file_name.c_str());
            return 1;
        }

        // fiddle with size if necessary.
        if (w != width || h != height) {
            auto new_image_data = (uint32_t *)calloc((size_t)(width * height), 4);

            int nx = (std::min)(width, w);
            int ny = (std::min)(height, h);

            for (int y = 0; y < ny; ++y) {
                memcpy((char *)new_image_data + y * width * 4,
                       image_data + y * w * 4,
                       (size_t)nx * 4);
            }

            free(image_data);
            image_data = (stbi_uc *)new_image_data;
        }

        // copy RGBA data into temp frame.
        TEST_AV(av_frame_make_writable(tmp_vframe));

        for (int y = 0; y < height; ++y) {
            memcpy(tmp_vframe->data[0] + y * tmp_vframe->linesize[0],
                   image_data + y * width * 4,
                   (size_t)width * 4);
        }

        // convert temp frame into output frame.
        TEST_AV(av_frame_make_writable(vframe));

        sws_scale(sws_context,
                  tmp_vframe->data, tmp_vframe->linesize, 0, height,
                  vframe->data, vframe->linesize);

        ASSERT(frame <= INT64_MAX);
        vframe->pts = (int64_t)frame;
        // LOGF(OUT,"New video frame pts: %" PRId64 " (%f sec)\n",
        //        vframe->pts,
        //        GetSecondsFromPTS(vframe->pts,vc_context->time_base));

        LOGF(VOUT, "SEND: video: pts=%" PRId64 " (%f sec)\n", vframe->pts, GetSecondsFromPTS(vframe->pts, vc_context->time_base));
        TEST_AV(avcodec_send_frame(vc_context, vframe));
        WriteFrames(avf_context,
                    vc_context,
                    vstream->index,
                    AVERROR(EAGAIN),
                    "video");

        if (acodec) {
            size_t num_samples = (limit - error) / step;
            ASSERT(ac_context->frame_size >= 0);

            //TEST_EQ_UU(wav_fmt->nChannels, 2u);
            TEST_EQ_UU(wav_fmt->wBitsPerSample, 16u);
            for (size_t sample_idx = 0;
                 sample_idx < num_samples;
                 ++sample_idx) {
                if (aframe_index == 0) {
                    TEST_AV(av_frame_make_writable(aframe));
                    TEST_NON_NULL(aframe->data);

                    //aframe->nb_samples=ac_context->frame_size;
                    aframe->pts = (int64_t)wav_file_data_index / 4; //(wav_fmt->wBitsPerSample*wav_fmt->nChannels/8);
                    //LOGF(OUT,"New audio frame pts: %" PRId64 " (%f sec)\n",aframe->pts,GetSecondsFromPTS(aframe->pts,ac_context->time_base));
                }

                for (size_t channel_idx = 0;
                     channel_idx < wav_fmt->nChannels;
                     ++channel_idx) {
                    ASSERT(wav_file_data_index < wav_file.data.size());
                    ASSERT(wav_file_data_index + 1 < wav_file.data.size());

                    int16_t tmp;
                    memcpy(&tmp, &wav_file.data[wav_file_data_index], 2);
                    //int16_t tmp=(uint8_t)wav_file.data[wav_file_data_index+0]|wav_file.data[wav_file_data_index+1]<<8;
                    wav_file_data_index += 2;

                    // if(audio_output_file) {
                    //     fwrite(&tmp,2,1,audio_output_file);
                    // }

                    float f = tmp / 32767.f;

                    // if(audio_output_file) {
                    //     fwrite(&f,4,1,audio_output_file);
                    // }

                    ASSERT(aframe_index < (size_t)aframe->nb_samples);
                    ASSERT(aframe->linesize[0] >= 0);
                    ASSERT(aframe_index * 4 < (size_t)aframe->linesize[0]);
                    float *dest = (float *)aframe->data[channel_idx] + aframe_index;
                    *dest = f;

                    error += step;
                    ASSERT(error / step <= 1);
                    error %= step;
                }

                ++aframe_index;

                ASSERT(aframe_index <= (size_t)aframe->nb_samples);
                if (aframe_index == (size_t)aframe->nb_samples) {
                    if (audio_output_file) {
                        for (int i = 0; i < aframe->nb_samples; ++i) {
#if LIBAVCODEC_VERSION_MAJOR < 60
                            int num_channels = aframe->channels;
#else
                            int num_channels = aframe->ch_layout.nb_channels;
#endif
                            for (int j = 0; j < num_channels; ++j) {
                                const float *ch = (float *)aframe->data[j];

                                fwrite(&ch[i], 4, 1, audio_output_file);
                            }
                        }
                    }

                    LOGF(VOUT, "SEND: audio: pts=%" PRId64 " (%f sec)\n", aframe->pts, GetSecondsFromPTS(aframe->pts, ac_context->time_base));
                    TEST_AV(avcodec_send_frame(ac_context, aframe));
                    WriteFrames(avf_context,
                                ac_context,
                                astream->index,
                                AVERROR(EAGAIN),
                                "audio");
                    aframe_index = 0;
                }
            }
        }

        free(image_data);
        image_data = nullptr;
    }

    LOGF(OUT, "\n");

    if (vc_context) {
        // Done.
        TEST_AV(avcodec_send_frame(vc_context, nullptr));

        WriteFrames(avf_context,
                    vc_context,
                    vstream->index,
                    AVERROR_EOF,
                    "video");
    }

    if (ac_context) {
        TEST_AV(avcodec_send_frame(ac_context, nullptr));

        WriteFrames(avf_context,
                    ac_context,
                    astream->index,
                    AVERROR_EOF,
                    "audio");
    }

    TEST_AV(av_write_trailer(avf_context));

    TEST_AV(avio_closep(&avf_context->pb));

    // for(unsigned i=0;i<avf_context->nb_streams;++i) {
    //     LOGF(OUT,"%u: %p\n",i,(void *)avf_context->streams[i]->codec->priv_data);
    // }

    sws_freeContext(sws_context);
    sws_context = nullptr;

    av_frame_free(&vframe);
    av_frame_free(&tmp_vframe);
    av_frame_free(&aframe);

    avcodec_free_context(&ac_context);
    avcodec_free_context(&vc_context);

#if LIBAVFORMAT_VERSION_MAJOR == 58
    // http://stackoverflow.com/questions/43389411/
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    for (unsigned i = 0; i < avf_context->nb_streams; ++i) {
        AVStream *st = avf_context->streams[i];

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

    avformat_free_context(avf_context);
    avf_context = nullptr;

    if (audio_output_file) {
        fclose(audio_output_file);
        audio_output_file = nullptr;
    }
}
