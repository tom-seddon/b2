#ifndef HEADER_5AC78014CEAA456EAEAF1C0EAEFC7180 // -*- mode:c++ -*-
#define HEADER_5AC78014CEAA456EAEAF1C0EAEFC7180

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SDL_AudioSpec;
class FileDialog;

#include "Messages.h"
#include <string>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Base class for video writer.
//
// 1. Create. Specify file name and output format index.
//
// 2. Call BeginWrite to set things in motion.
//
// 3. Call GetAudioFormat and GetVideoFormat to find the audio and
// video formats actually chosen.
//
// 4. Call WriteSound and WriteVideo to write sound and video data.
// For WriteSound, supply any amount of sound data; for WriteVideo,
// supply one frame's-worth of data.
//
// (There's further work to do on this, since there's no output
// format/bitrate selection, etc.)
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VideoWriter {
  public:
    VideoWriter(std::shared_ptr<MessageList> message_list,
                std::string file_name,
                size_t format_index);
    virtual ~VideoWriter() = 0;

    VideoWriter(VideoWriter &&) = delete;
    VideoWriter &operator=(VideoWriter &&) = delete;
    VideoWriter(const VideoWriter &) = delete;
    VideoWriter &operator=(const VideoWriter &) = delete;

    // Only intended for UI purposes.
    const std::string &GetFileName() const;

    virtual bool BeginWrite() = 0;
    virtual bool EndWrite() = 0;

    // Only freq, format and channels need be filled out.
    virtual bool GetAudioFormat(SDL_AudioSpec *spec) const = 0;

    virtual bool GetVideoFormat(uint32_t *format_ptr, int *width_ptr, int *height_ptr) const = 0;

    virtual bool WriteSound(const void *data, size_t data_size_bytes) = 0;
    virtual bool WriteVideo(const void *data) = 0;

    std::shared_ptr<MessageList> GetMessageList() const;

  protected:
    const size_t m_format_index;
    const std::string m_file_name;
    Messages m_msg;
    void *m_window = nullptr; //HWND, NSWindow *, whatever
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct VideoWriterFormat {
    std::string extension;
    std::string description;
};

size_t GetNumVideoWriterFormats();
const VideoWriterFormat *GetVideoWriterFormatByIndex(size_t index);
std::unique_ptr<VideoWriter> CreateVideoWriter(std::shared_ptr<MessageList> message_list,
                                               std::string file_name,
                                               size_t format_index);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
