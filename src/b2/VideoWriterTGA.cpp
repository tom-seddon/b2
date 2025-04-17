#include <shared/system.h>
#include "VideoWriterTGA.h"
#include "VideoWriter.h"
#include "misc.h"
#include <SDL.h>
#include <shared/file_io.h>
#include <shared/path.h>
#include <inttypes.h>
#include <shared/debug.h>
#include <shared/load_store.h>
#include <beeb/sound.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// ~1.5 GBytes
//
// WAV chunks are limited to 4 GBytes, but long is 32 bits on VC++ and there's
// no wrapper for _ftelli64/off_t/etc. - yet. So don't let the file get too
// large.
static const uint32_t MAX_WAV_FILE_DATA_SIZE_BYTES = 1u << 31;

static const int AUDIO_HZ = 48000;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct VideoWriterFormatTGA {
    VideoWriterFormat fmt;
    int audio_hz;
    SDL_AudioFormat sdl_audio_format;
    uint16_t wFormatTag;
};

static VideoWriterFormatTGA GetVideoWriterFormatTGA(int hz, SDL_AudioFormat sdl_audio_format, uint16_t wFormatTag, const char *audio_format_description) {
    VideoWriterFormatTGA f;
    f.fmt.extension = ".tga";
    f.fmt.description = strprintf("%dx%d uncompressed (TGA; %.1f KHz %s mono WAV)", TV_TEXTURE_WIDTH, TV_TEXTURE_HEIGHT, hz / 1000.f, audio_format_description);
    f.audio_hz = hz;
    f.sdl_audio_format = sdl_audio_format;
    f.wFormatTag = wFormatTag;

    return f;
}

static const uint16_t PCM_WAVE_FORMAT = 1;        //WAVE_FORMAT_PCM
static const uint16_t IEEE_FLOAT_WAVE_FORMAT = 3; //WAVE_FORMAT_IEEE_FLOAT

static const VideoWriterFormatTGA TGA_FORMATS[] = {
    GetVideoWriterFormatTGA(AUDIO_HZ, AUDIO_S16LSB, PCM_WAVE_FORMAT, "16-bit"),
    GetVideoWriterFormatTGA(AUDIO_HZ, AUDIO_F32LSB, IEEE_FLOAT_WAVE_FORMAT, "float"),
    GetVideoWriterFormatTGA(SOUND_CLOCK_HZ, AUDIO_F32LSB, IEEE_FLOAT_WAVE_FORMAT, "float"),
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void AddFOURCC(std::vector<unsigned char> *dest, const char *fourcc) {
    dest->push_back(fourcc[0]);
    dest->push_back(fourcc[1]);
    dest->push_back(fourcc[2]);
    dest->push_back(fourcc[3]);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VideoWriterTGA : public VideoWriter {
  public:
    VideoWriterTGA(std::shared_ptr<MessageList> message_list,
                   std::string file_name_stem,
                   size_t format_index)
        : VideoWriter(std::move(message_list),
                      file_name_stem,
                      format_index) {
        m_stem = PathWithoutExtension(file_name_stem);
    }

    bool BeginWrite() override {
        std::string folder = PathGetFolder(m_stem);
        if (!PathCreateFolder(folder)) {
            m_msg.e.f("Failed to create folder: %s\n", folder.c_str());
            return false;
        }

        return true;
    }

    bool EndWrite() override {
        if (!this->CloseWAVFile()) {
            return false;
        }

        return true;
    }

    bool GetAudioFormat(SDL_AudioSpec *spec) const override {
        ASSERT(m_format_index < sizeof TGA_FORMATS / sizeof TGA_FORMATS[0]);
        const VideoWriterFormatTGA *fmt = &TGA_FORMATS[m_format_index];
        spec->freq = fmt->audio_hz;
        spec->format = fmt->sdl_audio_format;
        spec->channels = 1;
        return true;
    }

    bool GetVideoFormat(uint32_t *format_ptr, int *width_ptr, int *height_ptr) const override {
        *format_ptr = SDL_PIXELFORMAT_ARGB8888;
        *width_ptr = TV_TEXTURE_WIDTH;
        *height_ptr = TV_TEXTURE_HEIGHT;

        return true;
    }

    bool WriteSound(const void *data, size_t data_size_bytes) override {
        if (m_wav_data_size_bytes + data_size_bytes > MAX_WAV_FILE_DATA_SIZE_BYTES) {
            if (!this->CloseWAVFile()) {
                return false;
            }
        }

        if (!m_wav_f) {
            m_wav_path = strprintf("%s.%03" PRIu64 ".wav", m_stem.c_str(), m_wav_counter++);

            m_wav_f = fopenUTF8(m_wav_path.c_str(), "wb");
            if (!m_wav_f) {
                m_msg.e.f("Failed to open wav file: %s\n", m_wav_path.c_str());
                return false;
            }

            ASSERT(m_format_index < sizeof TGA_FORMATS / sizeof TGA_FORMATS[0]);
            const VideoWriterFormatTGA *fmt = &TGA_FORMATS[m_format_index];

            std::vector<unsigned char> h;
            size_t i;

            AddFOURCC(&h, "RIFF");
            h.resize(h.size() + 4);

            AddFOURCC(&h, "WAVE");

            AddFOURCC(&h, "fmt ");
            i = h.size();
            h.resize(i + 4 + 18);
            Store32LE(&h[i], 18);
            i += 4;

            uint16_t nChannels = 1;
            uint32_t nSamplesPerSec = fmt->audio_hz;
            uint16_t wBitsPerSample = SDL_AUDIO_BITSIZE(fmt->sdl_audio_format);
            uint16_t nBlockAlign = nChannels * wBitsPerSample / 8;
            uint32_t nAvgBytesPerSec = nSamplesPerSec * nBlockAlign;

            Store16LE(&h[i + 0], fmt->wFormatTag);
            Store16LE(&h[i + 2], nChannels);
            Store32LE(&h[i + 4], nSamplesPerSec);
            Store32LE(&h[i + 8], nAvgBytesPerSec);
            Store16LE(&h[i + 12], nBlockAlign);
            Store16LE(&h[i + 14], wBitsPerSample);
            Store16LE(&h[i + 16], 0);

            AddFOURCC(&h, "data");
            m_wav_data_size_offset = (long)h.size();
            h.resize(h.size() + 4);

            size_t n = fwrite(h.data(), 1, h.size(), m_wav_f);
            if (n != h.size()) {
                m_msg.e.f("Failed to write WAV header: %s\n", m_wav_path.c_str());
                return false;
            }

            // -8 because the RIFF chunk header isn't included.
            m_wav_header_size_bytes = (uint32_t)(h.size() - 8);
            Store32LE(&h[4], m_wav_header_size_bytes);
        }

        size_t n = fwrite(data, 1, data_size_bytes, m_wav_f);
        m_wav_data_size_bytes += (uint32_t)n;

        if (n != data_size_bytes) {
            m_msg.e.f("Wrote only %zu/%zu bytes to file: %s\n", n, data_size_bytes, m_wav_path.c_str());
            return false;
        }

        return true;
    }

    bool WriteVideo(const void *data) override {
        // 18 byte header.
        m_tga_data.clear();
        m_tga_data.resize(18);

        // +0 - No image ID field is included.
        // +1 - No colour map data is included.
        // +2 - Uncompressed true colour.
        m_tga_data[2] = 2;
        // +3, +4, +5, +6, +7 - Irrelevant due to lack of colour map.
        // +8, +9 - X origin
        // +10, +11 - Y origin
        // +12, +13 - Width
        Store16LE(&m_tga_data[12], TV_TEXTURE_WIDTH);
        // +14, +15 - Height
        Store16LE(&m_tga_data[14], TV_TEXTURE_HEIGHT);
        // +16 - Pixel depth
        m_tga_data[16] = 24;
        // +17 - Image description
        m_tga_data[17] = 2 << 4; //origin is top left

        size_t index = m_tga_data.size();
        m_tga_data.resize(m_tga_data.size() + TV_TEXTURE_HEIGHT * TV_TEXTURE_WIDTH * 3);
        {
            auto src = (const unsigned char *)data;
            unsigned char *dest = &m_tga_data[index];
            for (size_t i = 0; i < TV_TEXTURE_HEIGHT * TV_TEXTURE_WIDTH; ++i) {
                *dest++ = *src++;
                *dest++ = *src++;
                *dest++ = *src++;
                ++src; //skip alpha
            }
            ASSERT(dest == &m_tga_data[m_tga_data.size()]);
        }

        // 1e7 frames = ~55 hours
        std::string path = strprintf("%s.%07" PRIu64 ".tga", m_stem.c_str(), m_frame_counter++);

        FILE *f = fopenUTF8(path.c_str(), "wb");
        if (!f) {
            m_msg.e.f("Failed to open file: %s\n", path.c_str());
            return false;
        }

        size_t n = fwrite(m_tga_data.data(), 1, m_tga_data.size(), f);

        fclose(f), f = nullptr;

        if (n != m_tga_data.size()) {
            m_msg.e.f("Wrote only %zu/%zu bytes to file: %s\n", n, m_tga_data.size(), path.c_str());
            return false;
        }

        return true;
    }

  protected:
  private:
    // Buffer for one frame's TGA data. Will eventually end up the right size.
    std::vector<uint8_t> m_tga_data;
    uint64_t m_frame_counter = 0;
    std::string m_stem;

    uint64_t m_wav_counter = 0;
    std::string m_wav_path;
    FILE *m_wav_f = nullptr;
    uint32_t m_wav_header_size_bytes = 0;
    uint32_t m_wav_data_size_bytes = 0;
    long m_wav_data_size_offset = 0;

    bool CloseWAVFile() {
        bool good = true;
        if (m_wav_f) {
            if (!this->WriteSize(4, m_wav_header_size_bytes + m_wav_data_size_bytes, "WAV size")) {
                good = false;
            } else if (!this->WriteSize(m_wav_data_size_offset, m_wav_data_size_bytes, "data size")) {
                good = false;
            }

            fclose(m_wav_f), m_wav_f = nullptr;
        }

        return true;
    }

    bool WriteSize(long offset, uint32_t value, const char *what) {
        unsigned char data[4];
        Store32LE(data, value);

        if (fseek(m_wav_f, offset, SEEK_SET) != 0) {
            m_msg.e.f("Failed to update %s (1): %s\n", what, m_wav_path.c_str());
            return false;
        }

        if (fwrite(data, 1, 4, m_wav_f) != 4) {
            m_msg.e.f("Failed to update %s (2): %s\n", what, m_wav_path.c_str());
            return false;
        }

        return true;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<VideoWriter> CreateVideoWriterTGA(std::shared_ptr<MessageList> message_list,
                                                  std::string file_name,
                                                  size_t format_index) {
    return std::make_unique<VideoWriterTGA>(std::move(message_list),
                                            std::move(file_name),
                                            format_index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumVideoWriterTGAFormats() {
    return sizeof TGA_FORMATS / sizeof TGA_FORMATS[0];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const VideoWriterFormat *GetVideoWriterTGAFormatByIndex(size_t index) {
    if (index < GetNumVideoWriterFormats()) {
        return &TGA_FORMATS[index].fmt;
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
