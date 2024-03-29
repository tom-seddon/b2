#ifndef HEADER_55E0610CD9B641D882861B52B6E9FA67 // -*- mode:c++ -*-
#define HEADER_55E0610CD9B641D882861B52B6E9FA67
#if HAVE_FFMPEG

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VideoWriter;
class MessageList;
class Messages;
struct VideoWriterFormat;

#include <memory>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CanCreateVideoWriterFFmpeg();

std::unique_ptr<VideoWriter> CreateVideoWriterFFmpeg(std::shared_ptr<MessageList> message_list,
                                                     std::string file_name,
                                                     size_t format_index);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool InitFFmpeg(Messages *messages);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumVideoWriterFFmpegFormats();
const VideoWriterFormat *GetVideoWriterFFmpegFormatByIndex(size_t index);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
#endif
