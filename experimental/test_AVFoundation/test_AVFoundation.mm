#include <shared/system.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <shared/CommandLineParser.h>
#import <AVFoundation/AVFoundation.h>

#if 0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const int FPS=50;
static const int SAMPLE_RATE=48000;

struct VideoWriterAVFoundationFormat {
    VideoWriterFormat vwf;
    int vwidth;
    int vheight;
    int vbitrate;
    int abitrate;
};

static std::vector<VideoWriterAVFoundationFormat> g_formats;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VideoWriterAVFoundation:
public VideoWriter
{
public:
    VideoWriterAVFoundation(std::shared_ptr<MessageList> message_list,
                            std::string file_name,
                            size_t format_index):
    VideoWriter(std::move(message_list),std::move(file_name),format_index)
    {
    }

    ~VideoWriterAVFoundation() {
        [m_asset_writer release];
        m_asset_writer=nil;

        [m_vinput release];
        m_vinput=nullptr;

        [m_ainput release];
        m_ainput=nullptr;
    }

    bool BeginWrite() override {
        auto pool=[[NSAutoreleasePool alloc] init];

        bool good=this->DoBeginWrite();

        [pool release];
        return good;
    }

    bool EndWrite() override {
        auto pool=[[NSAutoreleasePool alloc] init];

        bool good=this->DoEndWrite();

        [pool release];
        return good;
    }

    bool GetAudioFormat(SDL_AudioSpec *spec) const override {
        memset(spec,0,sizeof *spec);

        spec->freq=SAMPLE_RATE;
        spec->format=

        return true;
    }

    bool GetVideoFormat(uint32_t *format_ptr,int *width_ptr,int *height_ptr) const override {
        *format_ptr=SDL_PIXELFORMAT_ARGB8888;
        *width_ptr=TV_TEXTURE_WIDTH;
        *height_ptr=TV_TEXTURE_HEIGHT;

        return true;
    }


protected:
private:
    AVAssetWriter *m_asset_writer=nullptr;
    AVAssetWriterInput *m_vinput=nullptr;
    AVAssetWriterInput *m_ainput=nullptr;

    bool DoBeginWrite() {
        const VideoWriterAVFoundationFormat *format=&g_formats[m_format_index];

        NSError *error;

        ASSERT(!m_asset_writer);
        m_asset_writer=[[AVAssetWriter alloc] initWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:m_file_name.c_str()]]
                                                 fileType:AVFileTypeQuickTimeMovie
                                                    error:&error];
        if(!m_asset_writer) {
            return this->Error("asset writer creation",error);
        }

        auto vsettings=@{AVVideoCodecKey:AVVideoCodecH264,
                         AVVideoCompressionPropertiesKey:@{AVVideoAverageBitRateKey:@(format->vbitrate)},
                         AVVideoWidthKey:@(format->vwidth),
                         AVVideoHeightKey:@(format->vheight),
                         AVVideoExpectedSourceFrameRateKey:@(FPS),
                         };

        m_vinput=[[AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                                     outputSettings:vsettings] retain];

        if(![m_asset_writer canAddInput:m_vinput]) {
            return this->Error(nullptr,"can't add video to asset writer");
        }

        // https://stackoverflow.com/questions/7649427/how-do-i-use-avassetwriter-to-write-aac-audio-out-in-ios

        auto asettings=@{AVFormatIDKey:@(kAudioFormatMPEG4AAC),
                         AVNumberOfChannelsKey:@(1),
                         AVSampleRateKey:@(SAMPLE_RATE),
                         AVEncoderBitRateKey:@(format->abitrate),
                         };

        m_ainput=[[AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeAudio
                                                     outputSettings:asettings] retain];

        if(![m_asset_writer canAddInput:m_ainput]) {
            return this->Error(nullptr,"can't add audio to asset writer");
        }

        [m_asset_writer addInput:m_vinput];
        [m_asset_writer addInput:m_ainput];

        m_sdl_audio_spec.freq=SAMPLE_RATE;
        m_sdl_audio_spec.format=

        return true;
    }

    bool DoEndWrite() {
    }

    bool PRINTF_LIKE(3,4) Error(const char *what,const char *fmt,...) {
        m_msg.e.f("failed to save video to: %s\n",m_file_name.c_str());

        if(fmt) {
            if(what) {
                m_msg.i.f("(%s failed: ",what);
            } else {
                m_msg.i.f("(");
            }

            va_list v;
            va_start(v,fmt);
            m_msg.i.v(fmt,v);
            va_end(v);

            m_msg.i.f(")\n");
        }

        return false;
    }

    bool Error(const char *what,NSError *error) {
        return this->Error(what,"%s",[[error localizedDescription] UTF8String]);
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<VideoWriter> CreateVideoWriterAVFoundation(std::shared_ptr<MessageList> message_list,
                                                           std::string file_name,
                                                           size_t format_index)
{
    return std::make_unique<VideoWriterAVFoundation>(std::move(message_list),
                                                     std::move(file_name),
                                                     format_index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumVideoWriterAVFoundationFormats() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const VideoWriterFormat *GetVideoWriterAVFoundationFormatByIndex(size_t index) {
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(VOUT,"",&log_printer_stdout_and_debugger,false)
LOG_DEFINE(OUT,"",&log_printer_stdout_and_debugger)
LOG_DEFINE(ERR,"",&log_printer_stderr_and_debugger)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    bool verbose=false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool DoOptions(Options *options,int argc,char *argv[]) {
    CommandLineParser p;

    p.AddOption('v',"verbose").Help("be more verbose").SetIfPresent(&options->verbose);

    std::vector<std::string> other_args;
    if(!p.Parse(argc,argv,&other_args)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc,char *argv[]) {
    Options options;
    if(!DoOptions(&options,argc,argv)) {
        return EXIT_FAILURE;
    }

    if(options.verbose) {
        LOG(VOUT).Enable();
    }
}

