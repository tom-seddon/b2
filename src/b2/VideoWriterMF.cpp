#include <shared/system.h>
#include <shared/system_specific.h>
#include "VideoWriterMF.h"
#include "VideoWriter.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <atlbase.h>
#include <mfobjects.h>
#include "native_ui.h"
#include <shared/debug.h>
#include <SDL.h>
#include "conf.h"
#include "misc.h"

#include <shared/enum_decl.h>
#include "VideoWriterMF_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "VideoWriterMF_private.inl"
#include <shared/enum_end.h>

struct MFAttribute {
    MFAttributeType type = MFAttributeType_None;
    GUID guid{};
    UINT32 uvalue0 = 0, uvalue1 = 0;
    GUID gvalue{};

    MFAttribute(MFAttributeType type, const GUID &guid, UINT32 uvalue0, UINT32 uvalue1 = 0);
    MFAttribute(MFAttributeType type, const GUID &guid, const GUID &gvalue);
};

MFAttribute::MFAttribute(MFAttributeType type_, const GUID &guid_, UINT32 uvalue0_, UINT32 uvalue1_)
    : type(type_)
    , guid(guid_)
    , uvalue0(uvalue0_)
    , uvalue1(uvalue1_) {
}

MFAttribute::MFAttribute(MFAttributeType type_, const GUID &guid_, const GUID &gvalue_)
    : type(type_)
    , guid(guid_)
    , gvalue(gvalue_) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static MFAttribute MFAttributeUINT32(const GUID &guid, UINT32 value) {
    return MFAttribute(MFAttributeType_Uint32, guid, value);
}

static MFAttribute MFAttributeGUID(const GUID &guid, const GUID &value) {
    return MFAttribute(MFAttributeType_Guid, guid, value);
}

static MFAttribute MFAttributeSize(const GUID &guid, UINT32 a, UINT32 b) {
    return MFAttribute(MFAttributeType_Size, guid, a, b);
}

static MFAttribute MFAttributeRatio(const GUID &guid, UINT32 a, UINT32 b) {
    return MFAttribute(MFAttributeType_Ratio, guid, a, b);
}

static HRESULT SetAttribute(IMFAttributes *attributes, const MFAttribute &attr) {
    switch (attr.type) {
    default:
        ASSERT(false);
        return E_FAIL;

    case MFAttributeType_Guid:
        return attributes->SetGUID(attr.guid, attr.gvalue);

    case MFAttributeType_Uint32:
        return attributes->SetUINT32(attr.guid, attr.uvalue0);

    case MFAttributeType_Size:
        return MFSetAttributeSize(attributes, attr.guid, attr.uvalue0, attr.uvalue1);

    case MFAttributeType_Ratio:
        return MFSetAttributeRatio(attributes, attr.guid, attr.uvalue0, attr.uvalue1);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::wstring GetWideStringFromUTF8String(const char *str) {
    size_t strLen = strlen(str);

    int n = MultiByteToWideChar(CP_UTF8, 0, str, (int)strLen, nullptr, 0);
    if (n == 0)
        return L"";

    std::vector<wchar_t> buffer;
    buffer.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, str, (int)strLen, buffer.data(), (int)buffer.size());

    return std::wstring(buffer.begin(), buffer.end());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const UINT32 FPS = 50;
static const double TICKS_PER_SEC = 1e7;
static const LONGLONG TICKS_PER_FRAME = (LONGLONG)(TICKS_PER_SEC / FPS);
static const UINT32 VIDEO_AVG_BITRATE = 4000000;
static const UINT32 AUDIO_AVG_BYTES_PER_SECOND = 24000;

static const std::string FORMAT_DESCRIPTION = strprintf("MPEG-4 (H264 %.1fMb/sec; AAC %.1fKb/sec)", VIDEO_AVG_BITRATE / 1.e6, AUDIO_AVG_BYTES_PER_SECOND * 8 / 1.e3);

static const VideoWriterFormat FORMATS[] = {
    {".mp4", strprintf("%dx%d ", TV_TEXTURE_WIDTH, TV_TEXTURE_HEIGHT) + FORMAT_DESCRIPTION},
    {".mp4", strprintf("%dx%d ", TV_TEXTURE_WIDTH * 2, TV_TEXTURE_HEIGHT * 2) + FORMAT_DESCRIPTION},
};

class VideoWriterMF : public VideoWriter {
  public:
    VideoWriterMF(std::shared_ptr<MessageList> message_list,
                  std::string file_name,
                  size_t format_index)
        : VideoWriter(std::move(message_list),
                      std::move(file_name),
                      format_index) {
        m_afmt.wFormatTag = WAVE_FORMAT_PCM;
        m_afmt.nChannels = 1;
        m_afmt.nSamplesPerSec = 48000;
        m_afmt.wBitsPerSample = 16;
        m_afmt.nBlockAlign = m_afmt.nChannels * m_afmt.wBitsPerSample / 8;
        m_afmt.nAvgBytesPerSec = m_afmt.nSamplesPerSec * m_afmt.nBlockAlign;
    }

    bool BeginWrite() override {
        HRESULT hr;

        UINT width, height;
        if (m_format_index == 1) {
            // 2x
            width = 2 * TV_TEXTURE_WIDTH;
            height = 2 * TV_TEXTURE_HEIGHT;
        } else {
            width = TV_TEXTURE_WIDTH;
            height = TV_TEXTURE_HEIGHT;
        }

        std::vector<MFAttribute> sink_writer_attributes_list = {
            MFAttributeUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE),
            MFAttributeUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE),
        };
        CComPtr<IMFAttributes> sink_writer_attributes;
        ASSERT(sink_writer_attributes_list.size() < UINT32_MAX);
        hr = MFCreateAttributes(&sink_writer_attributes, (UINT32)sink_writer_attributes_list.size());
        if (FAILED(hr)) {
            return this->Error(hr, "MFCreateAttributes");
        }

        if (!this->SetAttributes(sink_writer_attributes, sink_writer_attributes_list, "sink writer")) {
            return false;
        }

        std::wstring file_name = GetWideStringFromUTF8String(m_file_name.c_str());
        hr = MFCreateSinkWriterFromURL(file_name.c_str(), nullptr, sink_writer_attributes, &m_sink_writer);
        if (FAILED(hr)) {
            return this->Error(hr, "MFCreateSinkWriterFromURL");
        }

        std::vector<MFAttribute> video_output_attributes = {
            MFAttributeGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            MFAttributeGUID(MF_MT_SUBTYPE, MFVideoFormat_H264),
            MFAttributeUINT32(MF_MT_AVG_BITRATE, VIDEO_AVG_BITRATE),
            MFAttributeUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive),
            MFAttributeSize(MF_MT_FRAME_SIZE, width, height),
            MFAttributeRatio(MF_MT_FRAME_RATE, FPS, 1),
            MFAttributeRatio(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
        };
        CComPtr<IMFMediaType> video_output_type = this->CreateMediaType(video_output_attributes, "video output");

        std::vector<MFAttribute> video_input_attributes = {
            MFAttributeGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            MFAttributeGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32),
            MFAttributeUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive),
            MFAttributeSize(MF_MT_FRAME_SIZE, TV_TEXTURE_WIDTH, TV_TEXTURE_HEIGHT),
            MFAttributeUINT32(MF_MT_DEFAULT_STRIDE, TV_TEXTURE_WIDTH * 4),
            MFAttributeRatio(MF_MT_FRAME_RATE, FPS, 1),
            MFAttributeRatio(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
        };
        CComPtr<IMFMediaType> video_input_type = this->CreateMediaType(video_input_attributes, "video input");

        std::vector<MFAttribute> audio_output_attributes = {
            MFAttributeGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            MFAttributeGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC),
            MFAttributeUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1),
            MFAttributeUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
            MFAttributeUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, AUDIO_AVG_BYTES_PER_SECOND),
            MFAttributeUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, m_afmt.nSamplesPerSec),
            MFAttributeUINT32(MF_MT_AUDIO_NUM_CHANNELS, m_afmt.nChannels),
        };
        CComPtr<IMFMediaType> audio_output_type = this->CreateMediaType(audio_output_attributes, "audio output");

        std::vector<MFAttribute> audio_input_attributes = {
            MFAttributeGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            MFAttributeGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
            MFAttributeUINT32(MF_MT_AUDIO_NUM_CHANNELS, m_afmt.nChannels),
            MFAttributeUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, m_afmt.nSamplesPerSec),
            MFAttributeUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, m_afmt.nAvgBytesPerSec),
            MFAttributeUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, m_afmt.nBlockAlign),
            MFAttributeUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, m_afmt.wBitsPerSample),
            MFAttributeUINT32(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE, m_afmt.wBitsPerSample),
            MFAttributeUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE),
        };
        CComPtr<IMFMediaType> audio_input_type = this->CreateMediaType(audio_input_attributes, "audio input");

        if (!video_output_type || !video_input_type || !audio_output_type || !audio_input_type) {
            return false;
        }

        hr = m_sink_writer->AddStream(video_output_type, &m_video_stream_index);
        if (FAILED(hr)) {
            return this->Error(hr, "SinkWriter AddStream (video)");
        }

        hr = m_sink_writer->SetInputMediaType(m_video_stream_index, video_input_type, nullptr);
        if (FAILED(hr)) {
            return this->Error(hr, "SinkWriter SetInputMediaType (video)");
        }

        hr = m_sink_writer->AddStream(audio_output_type, &m_audio_stream_index);
        if (FAILED(hr)) {
            return this->Error(hr, "SinkWriter AddStream (audio)");
        }

        hr = m_sink_writer->SetInputMediaType(m_audio_stream_index, audio_input_type, nullptr);
        if (FAILED(hr)) {
            return this->Error(hr, "SinkWriter SetInputMediaType (audio)");
        }

        hr = m_sink_writer->BeginWriting();
        if (FAILED(hr)) {
            return this->Error(hr, "SinkWriter BeginWriting");
        }

        return true;
    }

    bool EndWrite() {
        HRESULT hr;

        hr = m_sink_writer->Finalize();
        if (FAILED(hr)) {
            return this->Error(hr, "SinkWriter Finalize");
        }

        return true;
    }

    bool GetAudioFormat(SDL_AudioSpec *spec) const override {
        memset(spec, 0, sizeof *spec);

        spec->freq = m_afmt.nSamplesPerSec;
        spec->format = AUDIO_S16SYS;
        ASSERT(m_afmt.nChannels < 256);
        spec->channels = (uint8_t)m_afmt.nChannels;

        return true;
    }

    bool GetVideoFormat(uint32_t *format_ptr, int *width_ptr, int *height_ptr) const override {
        *format_ptr = SDL_PIXELFORMAT_ARGB8888;
        *width_ptr = TV_TEXTURE_WIDTH;
        *height_ptr = TV_TEXTURE_HEIGHT;

        return true;
    }

    bool WriteSound(const void *data, size_t data_size_bytes) override {
        HRESULT hr;

        ASSERT(m_audio_stream_index != MAXDWORD);

        CComPtr<IMFMediaBuffer> buffer = this->CreateBuffer(data_size_bytes, "audio");
        if (!buffer) {
            return false;
        }

        ASSERT(data_size_bytes % m_afmt.nBlockAlign == 0);
        double num_seconds = (data_size_bytes / m_afmt.nBlockAlign) / (double)m_afmt.nSamplesPerSec;
        LONGLONG num_ticks = (LONGLONG)(num_seconds * TICKS_PER_SEC);

        BYTE *buffer_data;
        hr = buffer->Lock(&buffer_data, nullptr, nullptr);
        if (FAILED(hr)) {
            return this->Error(hr, "Buffer Lock (audio)");
        }

        memcpy(buffer_data, data, data_size_bytes);

        if (!this->UnlockBuffer(buffer, data_size_bytes, "audio")) {
            return false;
        }

        if (!this->AddSample(buffer, m_audio_stream_index, m_audio_ticks, num_ticks, "audio")) {
            return false;
        }

        m_audio_ticks += num_ticks;

        return true;
    }

    bool WriteVideo(const void *data) override {
        HRESULT hr;

        ASSERT(m_video_stream_index != MAXDWORD);

        CComPtr<IMFMediaBuffer> buffer = this->CreateBuffer(TV_TEXTURE_WIDTH * TV_TEXTURE_HEIGHT * 4, "video");
        if (!buffer) {
            return false;
        }

        BYTE *buffer_data;
        hr = buffer->Lock(&buffer_data, nullptr, nullptr);
        if (FAILED(hr)) {
            return this->Error(hr, "Buffer Unlock (video)");
        }

        hr = MFCopyImage(buffer_data, TV_TEXTURE_WIDTH * 4, (const BYTE *)data, TV_TEXTURE_WIDTH * 4, TV_TEXTURE_WIDTH * 4, TV_TEXTURE_HEIGHT);
        if (FAILED(hr)) {
            return this->Error(hr, "MFCopyImage");
        }

        if (!this->UnlockBuffer(buffer, TV_TEXTURE_WIDTH * 4 * TV_TEXTURE_HEIGHT, "video")) {
            return false;
        }

        if (!this->AddSample(buffer, m_video_stream_index, m_video_ticks, (LONGLONG)TICKS_PER_FRAME, "video")) {
            return false;
        }

        m_video_ticks += TICKS_PER_FRAME;

        return true;
    }

  protected:
  private:
    CComPtr<IMFSinkWriter> m_sink_writer;
    WAVEFORMATEX m_afmt{};
    LONGLONG m_audio_ticks = 0;
    LONGLONG m_video_ticks = 0;
    DWORD m_audio_stream_index = MAXDWORD;
    DWORD m_video_stream_index = MAXDWORD;

    CComPtr<IMFMediaType> CreateMediaType(const std::vector<MFAttribute> &attributes, const char *what) {
        HRESULT hr;

        CComPtr<IMFMediaType> type;

        hr = MFCreateMediaType(&type);
        if (FAILED(hr)) {
            this->Error(hr, "MFCreateMediaType");
            return nullptr;
        }

        if (!this->SetAttributes(type, attributes, what)) {
            return nullptr;
        }

        return type;
    }

    bool SetAttributes(IMFAttributes *imf, const std::vector<MFAttribute> &attributes, const char *what) {
        HRESULT hr;

        for (const MFAttribute &attribute : attributes) {
            hr = SetAttribute(imf, attribute);
            if (FAILED(hr)) {
                return this->Error(hr, "SetAttribute for %s (%s)", this->GetAttributeName(attribute.guid).c_str(), what);
            }
        }

        return true;
    }

    CComPtr<IMFMediaBuffer> CreateBuffer(size_t num_bytes, const char *what) {
        HRESULT hr;

        ASSERT(num_bytes <= MAXDWORD);

        CComPtr<IMFMediaBuffer> buffer;
        hr = MFCreateMemoryBuffer((DWORD)num_bytes, &buffer);
        if (FAILED(hr)) {
            this->Error(hr, "MFCreateMemoryBuffer (%s)", what);
            return nullptr;
        }

        return buffer;
    }

    bool UnlockBuffer(const CComPtr<IMFMediaBuffer> &buffer, size_t size, const char *what) {
        HRESULT hr;

        hr = buffer->Unlock();
        if (FAILED(hr)) {
            return this->Error(hr, "Buffer Unlock (%s)", what);
        }

        ASSERT(size <= MAXDWORD);
        hr = buffer->SetCurrentLength((DWORD)size);
        if (FAILED(hr)) {
            return this->Error(hr, "Buffer SetCurrentLength (%s)", what);
        }

        return true;
    }

    bool AddSample(const CComPtr<IMFMediaBuffer> &buffer, DWORD stream_index, LONGLONG time, LONGLONG duration, const char *what) {
        HRESULT hr;

        CComPtr<IMFSample> sample;
        hr = MFCreateSample(&sample);
        if (FAILED(hr)) {
            return this->Error(hr, "MFCreateSample (%s)", what);
        }

        hr = sample->AddBuffer(buffer);
        if (FAILED(hr)) {
            return this->Error(hr, "MFSample AddBuffer (%s)", what);
        }

        hr = sample->SetSampleTime(time);
        if (FAILED(hr)) {
            return this->Error(hr, "MFSample SetSampleTime (%s)", what);
        }

        hr = sample->SetSampleDuration(duration);
        if (FAILED(hr)) {
            return this->Error(hr, "MFSample SetSampleDuration (%s)", what);
        }

        hr = m_sink_writer->WriteSample(stream_index, sample);
        if (FAILED(hr)) {
            return this->Error(hr, "SinkWriter WriteSample (%s)", what);
        }

        return true;
    }

    bool Error(HRESULT hr, const char *fmt, ...) PRINTF_LIKE(3, 4) {
        char *msg;
        {
            va_list v;
            va_start(v, fmt);
            vasprintf(&msg, fmt, v);
            va_end(v);
        }

        m_msg.e.f("failed to save video to: %s\n", m_file_name.c_str());
        m_msg.i.f("(%s failed: %s)\n", msg, GetErrorDescription(hr));

        free(msg);
        msg = nullptr;

        return false;
    }

    std::string GetAttributeName(const GUID &guid) {
        uint8_t tmp[16];
        memcpy(tmp, &guid, 16);

        return strprintf("{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                         tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6], tmp[7], tmp[8], tmp[9], tmp[10], tmp[11], tmp[12], tmp[13], tmp[14], tmp[15]);
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<VideoWriter> CreateVideoWriterMF(std::shared_ptr<MessageList> message_list,
                                                 std::string file_name,
                                                 size_t format_index) {
    return std::make_unique<VideoWriterMF>(std::move(message_list),
                                           std::move(file_name),
                                           format_index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool InitMF(Messages *messages) {
    HRESULT hr;

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        messages->e.f("Failed to initialise Windows Media Foundation\n");
        messages->i.f("(MFStartup failed: %s)\n", GetErrorDescription(hr));
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumVideoWriterMFFormats() {
    return sizeof FORMATS / sizeof FORMATS[0];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const VideoWriterFormat *GetVideoWriterMFFormatByIndex(size_t index) {
    return &FORMATS[index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
