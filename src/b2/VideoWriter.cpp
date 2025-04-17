#include <shared/system.h>
#include <shared/debug.h>
#include "VideoWriter.h"
#include "VideoWriterMF.h"
#include "VideoWriterFFmpeg.h"
#include "VideoWriterAVFoundation.h"
#include "VideoWriterTGA.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoWriter::VideoWriter(std::shared_ptr<MessageList> message_list,
                         std::string file_name,
                         size_t format_index)
    : m_format_index(format_index)
    , m_file_name(std::move(file_name))
    , m_msg(message_list) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VideoWriter::~VideoWriter() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &VideoWriter::GetFileName() const {
    return m_file_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<MessageList> VideoWriter::GetMessageList() const {
    return m_msg.GetMessageList();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<VideoWriter> CreateVideoWriter(std::shared_ptr<MessageList> message_list,
                                               std::string file_name,
                                               size_t format_index) {

    size_t num_tga = GetNumVideoWriterTGAFormats();
    if (format_index < num_tga) {
        return CreateVideoWriterTGA(std::move(message_list),
                                    std::move(file_name),
                                    format_index);
    }

    format_index -= num_tga;

#if SYSTEM_WINDOWS

    return CreateVideoWriterMF(std::move(message_list),
                               std::move(file_name),
                               format_index);

//#elif SYSTEM_OSX
//
//    return CreateVideoWriterAVFoundation(std::move(message_list),
//                                         std::move(file_name),
//                                         format_index);
//
#elif HAVE_FFMPEG

    return CreateVideoWriterFFmpeg(std::move(message_list),
                                   std::move(file_name),
                                   format_index);

#else

    (void)message_list, (void)file_name, (void)format_index;

    return nullptr;

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumVideoWriterFormats() {
    size_t n = GetNumVideoWriterTGAFormats();

#if SYSTEM_WINDOWS

    n += GetNumVideoWriterMFFormats();

//#elif SYSTEM_OSX
//
//    n+=GetNumVideoWriterAVFoundationFormats();
//
#elif HAVE_FFMPEG

    n += GetNumVideoWriterFFmpegFormats();

#endif

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const VideoWriterFormat *GetVideoWriterFormatByIndex(size_t index) {
    ASSERT(index < GetNumVideoWriterFormats());

    size_t num_tga = GetNumVideoWriterTGAFormats();
    if (index < num_tga) {
        return GetVideoWriterTGAFormatByIndex(index);
    }

    index -= num_tga;

#if SYSTEM_WINDOWS

    return GetVideoWriterMFFormatByIndex(index);

//#elif SYSTEM_OSX
//
//    return GetVideoWriterAVFoundationFormatByIndex(index);
//
#elif HAVE_FFMPEG

    return GetVideoWriterFFmpegFormatByIndex(index);

#else

    (void)index;

    return nullptr;

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
