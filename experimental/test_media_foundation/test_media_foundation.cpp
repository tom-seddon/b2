#define INITGUID
#include <shared/system.h>
#include <shared/system_specific.h>
#include <shared/testing.h>
#include <cguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <atlbase.h>
#include <string>
#include <vector>
#include <shared/debug.h>
#include <shared/CommandLineParser.h>
#include <shared/log.h>
#include <shared/path.h>
#include <algorithm>
#include "testing_windows.h"
#include <inttypes.h>
#include <mfobjects.h>
#include <shared/file_io.h>

#include <shared/enum_decl.h>
#include "test_media_foundation.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "test_media_foundation.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define STBI_ASSERT ASSERT
#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4244) //OPERATOR: conversion from TYPE to TYPE, possible loss of data
#pragma warning(disable : 4456) //declaration of IDENTIFIER hides previous local declaration
#include "stb_image.h"
#pragma warning(pop)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// if defined, write all the audio in one big lump at the end, rather
// than writing it bit by bit. (This was purely a sanity check... I've
// no idea what how this might affect the output file.)
#define ONE_AUDIO_CHUNK 0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#undef OUT //stupid Windows headers

LOG_DEFINE(OUT, "", &log_printer_stdout_and_debugger);
LOG_DEFINE(ERR, "", &log_printer_stderr_and_debugger);

std::wstring GetWideStringFromUTF8String(const char *str) {
    size_t strLen = strlen(str);

    int n = MultiByteToWideChar(CP_UTF8, 0, str, (int)strLen, nullptr, 0);
    if (n == 0)
        return L"";

    std::vector<wchar_t> buffer;
    buffer.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, str, (int)strLen, buffer.data(), (int)buffer.size());

    return std::wstring(buffer.begin(), buffer.end());
}

//static const UINT32 WIDTH=512;
//static const UINT32 HEIGHT=512;
//static const UINT32 FPS=30;

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
    if (!LoadFile(&data, file_name, nullptr)) {
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

extern void msdn_main();
HRESULT InitializeSinkWriter(
    IMFSinkWriter **ppWriter,
    DWORD *pStreamIndex,
    const WCHAR *url,
    DWORD width,
    DWORD height,
    DWORD fps);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<CComPtr<IMFActivate>> GetMFTEnum(GUID category, UINT32 flags, const MFT_REGISTER_TYPE_INFO *input_type, const MFT_REGISTER_TYPE_INFO *output_type, CComPtr<IMFAttributes> attributes) {
    (void)category, (void)flags, (void)input_type, (void)output_type;

    IMFActivate **activaters;
    UINT32 num_activaters;
    TEST_HR(MFTEnumEx(MFT_CATEGORY_AUDIO_ENCODER, MFT_ENUM_FLAG_SORTANDFILTER, nullptr, nullptr, &activaters, &num_activaters));

    std::vector<CComPtr<IMFActivate>> result;
    result.resize(num_activaters);
    for (UINT32 i = 0; i < num_activaters; ++i) {
        result[i].p = activaters[i]; //steal ref.
    }

    CoTaskMemFree(activaters);
    activaters = nullptr;

    return result;
}

static void WriteSample(const CComPtr<IMFSinkWriter> &pSinkWriter, DWORD stream_index, LONGLONG timestamp, LONGLONG duration, const void *data, size_t num_bytes) {
    CComPtr<IMFMediaBuffer> buffer;
    ASSERT(num_bytes <= MAXDWORD);
    TEST_HR(MFCreateMemoryBuffer((DWORD)num_bytes, &buffer));

    BYTE *buffer_data;
    TEST_HR(buffer->Lock(&buffer_data, nullptr, nullptr));
    memcpy(buffer_data, data, num_bytes);
    TEST_HR(buffer->Unlock());
    TEST_HR(buffer->SetCurrentLength((DWORD)num_bytes));

    CComPtr<IMFSample> sample;
    TEST_HR(MFCreateSample(&sample));
    TEST_HR(sample->AddBuffer(buffer));
    TEST_HR(sample->SetSampleTime(timestamp));
    TEST_HR(sample->SetSampleDuration(duration));

    TEST_HR(pSinkWriter->WriteSample(stream_index, sample));
}

int main(int argc, char *argv[]) {
    AddTestFailFn(&PrintLastHRESULT, nullptr);

    if (false) {
        msdn_main();
        return 0;
    }

    std::vector<CComPtr<IMFActivate>> audio_activaters = GetMFTEnum(MFT_CATEGORY_AUDIO_ENCODER, MFT_ENUM_FLAG_SORTANDFILTER, nullptr, nullptr, nullptr);
    std::vector<CComPtr<IMFActivate>> video_activaters = GetMFTEnum(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_SORTANDFILTER, nullptr, nullptr, nullptr);

    if (argc < 2) {
        LOGF(ERR, "specify folder with JPEGs in and maybe a WAV\n");
        return 1;
    }

    std::vector<std::string> jpeg_file_names, wav_file_names;
    {
        std::vector<std::string> file_names;
        PathGlob(argv[1], [&](const std::string &path, bool is_folder) {
            if (is_folder) {
                file_names.push_back(path);
            }
        });

        jpeg_file_names = FindFileNamesWithExtension(file_names, ".jpg");
        wav_file_names = FindFileNamesWithExtension(file_names, ".wav");

        if (jpeg_file_names.empty()) {
            LOGF(ERR, "no JPEGs found in: %s\n", argv[1]);
            return 1;
        }

        if (wav_file_names.size() > 1) {
            LOGF(ERR, "multiple WAVs found in: %s\n", argv[1]);
            return 1;
        }

        std::sort(jpeg_file_names.begin(), jpeg_file_names.end(), &StringPathLessThanStringPath);
    }

    LOGF(OUT, "%zu JPEG files\n", jpeg_file_names.size());

    WAVFile wav_file;
    const WAVEFORMATEX *wav_fmt = nullptr;
    size_t wav_length_samples = 0, bytes_per_sample = 0;
    LONGLONG frame_duration;
    {
        double fps;

        if (!wav_file_names.empty()) {
            if (!LoadWAVFile(&wav_file, wav_file_names[0].c_str())) {
                LOGF(ERR, "failed to load WAV file: %s\n", wav_file_names[0].c_str());
                return 1;
            }

            wav_fmt = (const WAVEFORMATEX *)wav_file.fmt_buf.data();

            if (wav_fmt->wFormatTag != WAVE_FORMAT_PCM) {
                LOGF(ERR, "not PCM WAV file: %s\n", wav_file_names[0].c_str());
                return 1;
            }

            size_t bytes_per_second = wav_fmt->nSamplesPerSec * wav_fmt->nChannels * wav_fmt->wBitsPerSample / 8;
            double num_seconds = (double)wav_file.data.size() / bytes_per_second;
            bytes_per_sample = wav_fmt->nChannels * wav_fmt->wBitsPerSample / 8;
            TEST_EQ_UU(wav_file.data.size() % bytes_per_sample, 0);
            wav_length_samples = wav_file.data.size() / bytes_per_sample;
            fps = jpeg_file_names.size() / num_seconds;

            LOGF(OUT, "%zu bytes WAV data\n", wav_file.data.size());
            LOGF(OUT, "%zu samples WAV data\n", wav_length_samples);
            LOGF(OUT, "%zu bytes/second\n", bytes_per_second);
            LOGF(OUT, "%f seconds WAV data\n", num_seconds);
            LOGF(OUT, "implying %f frames/second\n", fps);
        } else {
            fps = 30.;
        }

        // the calculation can be (# jpegs)*(bytes/sec)/(bytes data),
        // which could be used as the MF_MT_FRAME_RATE_FPS ratio. But
        // they're only UINT32s and that doesn't leave much overhead.
        frame_duration = (LONGLONG)(10000000. / fps);
        LOGF(OUT, "Frame duration period: %" PRId64 "\n", frame_duration);
    }

    int width, height;
    if (!stbi_info(jpeg_file_names[0].c_str(), &width, &height, nullptr)) {
        LOGF(ERR, "failed to get info from first file: %s\n", jpeg_file_names[0].c_str());
        return 1;
    }

    LOGF(OUT, "%d x %d\n", width, height);

    TEST_TRUE(width > 0);
    TEST_TRUE(height > 0);

    width = 704;
    height = 576;

    TEST_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));
    TEST_HR(MFStartup(MF_VERSION));

    //IMFSinkWriter *tmp_sw=nullptr;
    //DWORD tmp_si;
    //TEST_HR(InitializeSinkWriter(&tmp_sw,&tmp_si));
    //return 0;

    // http://stackoverflow.com/a/26068214/1618406
    std::wstring output_file_name = GetWideStringFromUTF8String("C:\\temp\\MFtest.wmv");

    CComPtr<IMFAttributes> sink_writer_attributes;
    TEST_HR(MFCreateAttributes(&sink_writer_attributes, 10));
    TEST_HR(sink_writer_attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE));
    TEST_HR(sink_writer_attributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE));

    CComPtr<IMFSinkWriter> pSinkWriter;
    DWORD video_stream_index = MAXDWORD;
    DWORD audio_stream_index = MAXDWORD;
    //TEST_HR(InitializeSinkWriter(&pSinkWriter,&video_stream_index,output_file_name.c_str(),(DWORD)width,(DWORD)height,FPS));

    TEST_HR(MFCreateSinkWriterFromURL(output_file_name.c_str(), nullptr, sink_writer_attributes, &pSinkWriter));

    static const int FPS = 30;

    CComPtr<IMFMediaType> video_output_type;
    TEST_HR(MFCreateMediaType(&video_output_type));
    TEST_HR(video_output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    TEST_HR(video_output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));
    TEST_HR(video_output_type->SetUINT32(MF_MT_AVG_BITRATE, 800000));
    TEST_HR(video_output_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
    TEST_HR(MFSetAttributeSize(video_output_type, MF_MT_FRAME_SIZE, (UINT32)width, (UINT32)height));
    TEST_HR(MFSetAttributeRatio(video_output_type, MF_MT_FRAME_RATE, FPS, 1));
    TEST_HR(MFSetAttributeRatio(video_output_type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1));
    TEST_HR(pSinkWriter->AddStream(video_output_type, &video_stream_index));

    CComPtr<IMFMediaType> video_input_type;
    TEST_HR(MFCreateMediaType(&video_input_type));
    TEST_HR(video_input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    TEST_HR(video_input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32));
    TEST_HR(video_input_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
    TEST_HR(MFSetAttributeSize(video_input_type, MF_MT_FRAME_SIZE, (UINT32)width, (UINT32)height));
    TEST_HR(video_input_type->SetUINT32(MF_MT_DEFAULT_STRIDE, width * 4));
    TEST_HR(MFSetAttributeRatio(video_input_type, MF_MT_FRAME_RATE, FPS, 1));
    TEST_HR(MFSetAttributeRatio(video_input_type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1));
    TEST_HR(pSinkWriter->SetInputMediaType(video_stream_index, video_input_type, nullptr));

    //UINT32 default_stride;
    //TEST_HR(video_input_type->GetUINT32(MF_MT_DEFAULT_STRIDE,&default_stride));
    //LOGF(OUT,"Default Stride: %" PRId32 "\n",(INT32)default_stride);

    CComPtr<IMFMediaType> audio_output_type, audio_input_type;
    if (wav_fmt) {
        // https://msdn.microsoft.com/en-us/library/windows/desktop/dd742785(v=vs.85).aspx - AAC

        TEST_HR(MFCreateMediaType(&audio_output_type));
        TEST_HR(audio_output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
        TEST_HR(audio_output_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC));
        TEST_HR(audio_output_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, wav_fmt->nChannels));
        TEST_HR(audio_output_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16));
        TEST_HR(audio_output_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000));
        TEST_HR(audio_output_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, wav_fmt->nSamplesPerSec));
        TEST_HR(audio_output_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, wav_fmt->nChannels));
        TEST_HR(pSinkWriter->AddStream(audio_output_type, &audio_stream_index));

        TEST_HR(MFCreateMediaType(&audio_input_type));
        TEST_HR(audio_input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
        TEST_HR(audio_input_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
        TEST_HR(audio_input_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, wav_fmt->nChannels));
        TEST_HR(audio_input_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, wav_fmt->nSamplesPerSec));
        TEST_HR(audio_input_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, wav_fmt->nAvgBytesPerSec));
        TEST_HR(audio_input_type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, wav_fmt->nBlockAlign));
        TEST_HR(audio_input_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, wav_fmt->wBitsPerSample));
        TEST_HR(audio_input_type->SetUINT32(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE, wav_fmt->wBitsPerSample));
        TEST_HR(audio_input_type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE));
        TEST_HR(pSinkWriter->SetInputMediaType(audio_stream_index, audio_input_type, nullptr));
    }

    TEST_HR(pSinkWriter->BeginWriting());

    LONGLONG frame_timestamp = 0;

    uint64_t error = 0, step = 0, limit = 0;
    size_t audio_index = 0;
    if (wav_fmt) {
        step = jpeg_file_names.size();
        limit = wav_length_samples;
    }

    uint64_t loop_total_ticks = 0, load_total_ticks = 0, conv_total_ticks = 0, video_total_ticks = 0, audio_total_ticks = 0;

    for (size_t frame = 0; frame < jpeg_file_names.size(); ++frame) {
        const std::string &file_name = jpeg_file_names[frame];

        if (frame > 500) {
            break;
        }

        LOGF(OUT, "%s\n", file_name.c_str());

        uint64_t start_ticks, load_ticks, conv_ticks, video_ticks, audio_ticks, end_ticks;

        start_ticks = GetCurrentTickCount();

        int w, h, ncomp;
        stbi_uc *image_data = stbi_load(file_name.c_str(), &w, &h, &ncomp, 4);
        if (!image_data) {
            LOGF(ERR, "failed to load file: %s\n", file_name.c_str());
            return 1;
        }

        //if(w>=width||h>=height) {
        //    LOGF(ERR,"file is %dx%d, %dx%d: %s\n",w,h,width,height,file_name.c_str());
        //    return 1;
        //}

        load_ticks = GetCurrentTickCount();

        // convert RGB->BGR
        {
            stbi_uc *pixel = image_data;
            for (int i = 0; i < w * h; ++i) {
                std::swap(pixel[0], pixel[2]);
                pixel += 4;
            }
        }

        // fiddle with size if necessary.
        if (w != width || h != height) {
            auto new_image_data = (uint32_t *)calloc(width * height, 4);

            int nx = (std::min)(width, w);
            int ny = (std::min)(height, h);

            for (int y = 0; y < ny; ++y) {
                memcpy((char *)new_image_data + y * width * 4, image_data + y * w * 4, nx * 4);
            }

            free(image_data);
            image_data = (stbi_uc *)new_image_data;
        }

        conv_ticks = GetCurrentTickCount();

        {
            CComPtr<IMFMediaBuffer> buffer;
            TEST_HR(MFCreateMemoryBuffer(width * height * 4, &buffer));

            BYTE *buffer_data;
            TEST_HR(buffer->Lock(&buffer_data, nullptr, nullptr));
            TEST_HR(MFCopyImage(buffer_data, width * 4, image_data, width * 4, width * 4, height));
            TEST_HR(buffer->Unlock());
            TEST_HR(buffer->SetCurrentLength(width * height * 4));

            CComPtr<IMFSample> sample;
            TEST_HR(MFCreateSample(&sample));
            TEST_HR(sample->AddBuffer(buffer));
            TEST_HR(sample->SetSampleTime(frame_timestamp));
            TEST_HR(sample->SetSampleDuration(frame_duration));

            TEST_HR(pSinkWriter->WriteSample(video_stream_index, sample));
        }

        video_ticks = GetCurrentTickCount();

        if (wav_fmt) {
            size_t num_samples = (limit - error) / step;
            size_t num_bytes = num_samples * bytes_per_sample;

            ASSERT(audio_index + num_bytes <= wav_file.data.size());
#if !ONE_AUDIO_CHUNK
            WriteSample(pSinkWriter, audio_stream_index, frame_timestamp, frame_duration, &wav_file.data[audio_index], num_bytes);
#endif

            audio_index += num_bytes;

            error += num_samples * step;
            ASSERT(error / (num_samples * step) <= 1);
            error %= num_samples * step;
        }

        audio_ticks = GetCurrentTickCount();

        frame_timestamp += frame_duration;

        free(image_data);

        end_ticks = GetCurrentTickCount();

        loop_total_ticks += end_ticks - start_ticks;
        load_total_ticks += load_ticks - start_ticks;
        conv_total_ticks += conv_ticks - load_ticks;
        video_total_ticks += video_ticks - conv_ticks;
        audio_total_ticks += audio_ticks - video_ticks;
    }

#if ONE_AUDIO_CHUNK
    WriteSample(pSinkWriter, audio_stream_index, 0, frame_timestamp, wav_file.data.data(), audio_index);
#endif

    LOGF(OUT, "Total: %f\n", GetSecondsFromTicks(loop_total_ticks));
    LOGF(OUT, "Load: %f\n", GetSecondsFromTicks(load_total_ticks));
    LOGF(OUT, "Conv: %f\n", GetSecondsFromTicks(conv_total_ticks));
    LOGF(OUT, "Video: %f\n", GetSecondsFromTicks(video_total_ticks));
    LOGF(OUT, "Audio: %f\n", GetSecondsFromTicks(audio_total_ticks));

    TEST_HR(pSinkWriter->Finalize());
}
