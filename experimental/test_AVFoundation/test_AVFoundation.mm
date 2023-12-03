#include <shared/system.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <shared/CommandLineParser.h>
#include <shared/path.h>
#import <AVFoundation/AVFoundation.h>
#define STB_IMAGE_IMPLEMENTATION
#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <stb_image.h>
#ifdef __clang__
#pragma GCC diagnostic pop
#endif
#include <inttypes.h>
#include <algorithm>

#include <shared/enum_decl.h>
#include "test_AVFoundation.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "test_AVFoundation.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(VOUT, "", &log_printer_stdout_and_debugger, false);
LOG_DEFINE(OUT, "", &log_printer_stdout_and_debugger);
LOG_DEFINE(ERR, "", &log_printer_stderr_and_debugger);

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
    if (!PathLoadBinaryFile(&data, file_name)) {
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

struct Options {
    bool verbose = false;
    std::string input_folder_path;
    std::string output_path;
    int max_num_frames=0;
    int hz=60;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool DoOptions(Options *options, int argc, char *argv[]) {
    CommandLineParser p;

    p.AddOption('v', "verbose").Help("be more verbose").SetIfPresent(&options->verbose);
    p.AddOption('o',"output").Meta("FILE").Help("write output to FILE").Arg(&options->output_path);
    p.AddOption("max-num-frames").Meta("N").Help("limit video to max N frames").Arg(&options->max_num_frames);
    p.AddOption("frame-rate").Meta("HZ").Help("set frame rate to HZ frames per second").Arg(&options->hz).ShowDefault();

    std::vector<std::string> other_args;
    if (!p.Parse(argc, argv, &other_args)) {
        return false;
    }
    
    if(other_args.size()!=1){
        LOGF(ERR,"Must specify exactly one folder\n");
        return false;
    }
    
    options->input_folder_path=other_args[0];

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static bool StringPathLessThanStringPath(const std::string &a, const std::string &b) {
//    return PathCompare(a.c_str(), b.c_str()) < 0;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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

static void PixelBufferFreeSTBImageCallback(void*context,const void*data){
    (void)context;
    free((void*)data);
}

int main(int argc, char *argv[]) {
    Options options;
    if (!DoOptions(&options, argc, argv)) {
        return EXIT_FAILURE;
    }

    if (options.verbose) {
        LOG(VOUT).Enable();
    }
    
    std::vector<std::string> jpg_paths,wav_paths;
    {
        std::vector<std::string> file_names;
        PathGlob(options.input_folder_path,
                 [&file_names](const std::string &path,bool is_folder){
            if(!is_folder){
                file_names.push_back(path);
            }
        });
        
        jpg_paths=FindFileNamesWithExtension(file_names,".jpg");
        wav_paths=FindFileNamesWithExtension(file_names,".wav");
        
        if(wav_paths.size()>1){
            LOGF(ERR,"path contains %zu .wav files: %s\n",wav_paths.size(),options.input_folder_path.c_str());
            return 1;
        }
        
        if(jpg_paths.empty()){
            LOGF(ERR,"path contains no .jpg files: %s\n",options.input_folder_path.c_str());
            return 1;
        }
        
        std::sort(jpg_paths.begin(),jpg_paths.end(),
                  [](const std::string&a,const std::string&b){
            return PathCompare(a.c_str(),b.c_str())<0;
        });
        
        LOGF(VOUT,"%zu wav file(s)\n",wav_paths.size());
        LOGF(VOUT,"%zu jpg file(s)\n",jpg_paths.size());
    }
    
    //STBIDEF stbi_uc *stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp)
    int image_width,image_height,image_comp;
    stbi_uc *image_data=stbi_load(jpg_paths[0].c_str(),&image_width,&image_height,&image_comp,3);
    LOGF(VOUT,"Image: %dx%d, %d bytes/px\n",image_width,image_height,image_comp);
    free(image_data),image_data=nullptr;
    
    @autoreleasepool {
        NSArray<AVOutputSettingsPreset>*presets=[AVOutputSettingsAssistant availableOutputSettingsPresets];
        for(NSUInteger i=0;i<[presets count];++i){
            LOGF(VOUT,"%lu. %s\n",(unsigned long)i,[[presets objectAtIndex:i] UTF8String]);
        }
        
        AVFileType output_file_type=AVFileTypeMPEG4;
        
        // AVVideoExpectedSourceFrameRateKey
        // AVVideoWidthKey
        // AVVideoHeightKey
        // AVVideoPixelAspectRatioKey
        auto*assistant=[AVOutputSettingsAssistant outputSettingsAssistantWithPreset:AVOutputSettingsPreset1920x1080];
        {
            NSDictionary<NSString*,id>*vs=[assistant videoSettings];
            //assistant.outputFileType=output_file_type;
            [vs setValue:[NSNumber numberWithInt:image_width] forKey:AVVideoWidthKey];
            [vs setValue:[NSNumber numberWithInt:image_height] forKey:AVVideoHeightKey];
            [vs setValue:[NSNumber numberWithInt:options.hz] forKey:AVVideoExpectedSourceFrameRateKey];
        }
        
        auto*input=[AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                                      outputSettings:[assistant videoSettings]];
        
        auto*adaptor=[AVAssetWriterInputPixelBufferAdaptor 
                      assetWriterInputPixelBufferAdaptorWithAssetWriterInput:input
                      sourcePixelBufferAttributes:nil];
        
        AVAssetWriter*writer=nil;
        if(!options.output_path.empty()){
            NSError*error;
            auto*output_file_url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:options.output_path.c_str()]];
            writer=[AVAssetWriter assetWriterWithURL:output_file_url
                                            fileType:output_file_type
                                               error:&error];
            if(error){
                LOGF(ERR,"FATAL: Asset writer creation error: %s\n",[[error description] UTF8String]);
                return 1;
            }
        }
        
        if(writer){
            [writer addInput:input];
            [writer startWriting];
            [writer startSessionAtSourceTime:kCMTimeZero];
        }
        
        size_t num_frames=jpg_paths.size();
        if(options.max_num_frames>0){
            num_frames=std::min(num_frames,(size_t)options.max_num_frames);
        }
        for(size_t frame_index=0;frame_index<num_frames;++frame_index){
            const std::string&jpg_path=jpg_paths[frame_index];
            LOGF(VOUT,"Frame %zu: %s\n",frame_index,jpg_path.c_str());
            int frame_width,frame_height,frame_comp;
            stbi_uc*frame_data=stbi_load(jpg_path.c_str(),
                                         &frame_width,
                                         &frame_height,
                                         &frame_comp,
                                         3);
            
            if(frame_width!=image_width||frame_height!=image_height){
                LOGF(ERR,"FATAL: expected %dx%d image, got %dx%d: %s\n",
                     frame_width,frame_height,
                     image_width,image_height,
                     jpg_path.c_str());
                return 1;
            }
            
            CVPixelBufferRef pixel_buffer;
            CVReturn create_result=CVPixelBufferCreateWithBytes(kCFAllocatorDefault,
                                                                (size_t)frame_width,
                                                                (size_t)frame_height,
                                                                kCVPixelFormatType_24RGB,
                                                                frame_data,
                                                                (size_t)frame_width*3,
                                                                &PixelBufferFreeSTBImageCallback,
                                                                nullptr,
                                                                nil,
                                                                &pixel_buffer);
            if(create_result!=kCVReturnSuccess){
                LOGF(ERR,"FATAL: CVPixelBufferCreateWithBytes failed: %" PRId32 " (0x%" PRIx32 ") (%s)\n",
                     create_result,
                     create_result,
                     GetCVReturnEnumName(create_result));
            }
            
            if(writer){
                [adaptor appendPixelBuffer:pixel_buffer
                      withPresentationTime:CMTimeMake((int64_t)frame_index,
                                                      options.hz)];
            }
            
            CFRelease(pixel_buffer),pixel_buffer=nullptr;
        }
        
        [input markAsFinished];
        __block BOOL finished=false;
        [writer finishWritingWithCompletionHandler:^void(void){
            AVAssetWriterStatus status=[writer status];
            LOGF(VOUT,"Status: %d (%s)\n",(int)status,GetAVAssetWriterStatusEnumName(status));
            finished=true;
        }];
        uint64_t num_spins=0;
        while(!finished){
            ++num_spins;
            SleepMS(1);
        }
        LOGF(VOUT,"Spin count: %" PRIu64 "\n",num_spins);
    }
}
