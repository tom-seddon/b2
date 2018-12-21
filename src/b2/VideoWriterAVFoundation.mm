#include <shared/system.h>
#include "VideoWriterAVFoundation.h"
#include "VideoWriter.h"
#import <AVFoundation/AVFoundation.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//class VideoWriterAVFoundation:
//public VideoWriter
//{
//public:
//    VideoWriterAVFoundation(std::shared_ptr<MessageList> message_list):
//    VideoWriter(std::move(message_list))
//    {
//    }
//
//    void AddFileDialogFilters(FileDialog *fd) const override {
//        NSArray *types=[AVURLAsset audiovisualTypes];
//        for(NSUInteger i=0;i<[types count];++i) {
//
//        }
//    }
//protected:
//private:
//};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<VideoWriter> CreateVideoWriterAVFoundation(std::shared_ptr<MessageList> message_list) {
    return nullptr;//std::make_unique<VideoWriterAVFoundation>(std::move(message_list));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

