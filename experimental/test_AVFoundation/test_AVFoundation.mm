#include <shared/system.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <shared/CommandLineParser.h>
#include <shared/path.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <AppKit/AppKit.h>
#import <AppKit/NSGraphicsContext.h>
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
#include <atomic>
#include <shared/file_io.h>

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
    //uint16_t cbSize;
};
#include <shared/poppack.h>
CHECK_SIZEOF(WAVEFORMATEX, 16);

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

template <class T>
static const T *GetBuf(const std::vector<char> &buf) {
    if (buf.size() >= sizeof(T)) {
        return (T *)buf.data();
    } else {
        return nullptr;
    }
}

static const WAVEFORMATEX *GetWAVEFORMATEX(const WAVFile &wav_file) {
    return GetBuf<WAVEFORMATEX>(wav_file.fmt_buf);
}

//static const WAVEFORMAT*GetWAVEFORMAT(const WAVFile&wav_file){
//    return GetBuf<WAVEFORMAT>(wav_file.fmt_buf);
//}

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

struct Options {
    bool verbose = false;
    std::string input_folder_path;
    std::string output_path;
    int max_num_frames = 0;
    int hz = 0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool DoOptions(Options *options, int argc, char *argv[]) {
    CommandLineParser p;

    p.AddOption('v', "verbose").Help("be more verbose").SetIfPresent(&options->verbose);
    p.AddOption('o', "output").Meta("FILE").Help("write output to FILE").Arg(&options->output_path);
    p.AddOption("max-num-frames").Meta("N").Help("limit video to max N frames").Arg(&options->max_num_frames);
    p.AddOption("frame-rate").Meta("HZ").Help("set frame rate to HZ frames per second").Arg(&options->hz);

    std::vector<std::string> other_args;
    if (!p.Parse(argc, argv, &other_args)) {
        return false;
    }

    if (other_args.size() != 1) {
        LOGF(ERR, "Must specify exactly one folder\n");
        return false;
    }

    options->input_folder_path = other_args[0];

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

static void PixelBufferFreeSTBImageCallback(void *context, const void *data) {
    (void)context;
    free((void *)data);
}

static void DumpSettings(const char *name, NSDictionary<NSString *, id> *dict) {
    LOGF(VOUT, "%s settings: ", name);
    LOGI(VOUT);
    @autoreleasepool {
        NSEnumerator *enumerator = [dict keyEnumerator];
        NSString *key;
        while ((key = [enumerator nextObject])) {
            LOGF(VOUT, "%s: ", [key UTF8String]);
            id value = [dict objectForKey:key];
            if ([value isKindOfClass:[NSNumber class]]) {
                auto number = (NSNumber *)value;
                LOGF(VOUT, "%s", [[number stringValue] UTF8String]);
            } else {
                LOGF(VOUT, "<<type: %s>>", [NSStringFromClass([value class]) UTF8String]);
            }
            LOGF(VOUT, "\n");
        }
    }
}

int main(int argc, char *argv[]) {
    Options options;
    if (!DoOptions(&options, argc, argv)) {
        return EXIT_FAILURE;
    }

    if (options.verbose) {
        LOG(VOUT).Enable();
    }

    std::vector<std::string> jpg_paths, wav_paths;
    {
        std::vector<std::string> file_names;
        PathGlob(options.input_folder_path,
                 [&file_names](const std::string &path, bool is_folder) {
                     if (!is_folder) {
                         file_names.push_back(path);
                     }
                 });

        jpg_paths = FindFileNamesWithExtension(file_names, ".jpg");
        wav_paths = FindFileNamesWithExtension(file_names, ".wav");

        if (wav_paths.size() > 1) {
            LOGF(ERR, "path contains %zu .wav files: %s\n", wav_paths.size(), options.input_folder_path.c_str());
            return 1;
        } else if (wav_paths.empty()) {
            if (options.hz <= 0) {
                LOGF(ERR, "FATAL: must specify frames per second if no .wav file\n");
                return 1;
            }
        }

        if (jpg_paths.empty()) {
            LOGF(ERR, "path contains no .jpg files: %s\n", options.input_folder_path.c_str());
            return 1;
        }

        std::sort(jpg_paths.begin(), jpg_paths.end(),
                  [](const std::string &a, const std::string &b) {
                      return PathCompare(a.c_str(), b.c_str()) < 0;
                  });

        LOGF(VOUT, "%zu wav file(s)\n", wav_paths.size());
        LOGF(VOUT, "%zu jpg file(s)\n", jpg_paths.size());
    }

    bool got_wav_file = false;
    WAVFile wav_file;
    if (!wav_paths.empty()) {
        if (!LoadWAVFile(&wav_file, wav_paths[0])) {
            LOGF(ERR, "FATAL: failed to load .wav file: %s\n", wav_paths[0].c_str());
            return 1;
        }

        if (const WAVEFORMATEX *fmt = GetWAVEFORMATEX(wav_file)) {
            if (fmt->wFormatTag != WAVE_FORMAT_PCM) {
                LOGF(ERR, "FATAL: .wav format is 0x%04X, not 0x%04X (WAVE_FORMAT_PCM): %s\n",
                     fmt->wFormatTag,
                     WAVE_FORMAT_PCM,
                     wav_paths[0].c_str());
                return 1;
            }
        } else {
            LOGF(ERR, "FATAL: .wav file has no good fmt chunk: %s\n", wav_paths[0].c_str());
            return 1;
        }

        got_wav_file = true;
    }

    double frames_per_second = options.hz;
    if (got_wav_file) {
        const WAVEFORMATEX *fmt = GetWAVEFORMATEX(wav_file);
        double length_seconds = wav_file.data.size() / (double)fmt->nAvgBytesPerSec;
        frames_per_second = jpg_paths.size() / length_seconds;
        LOGF(VOUT, "Frames per second: %.3f\n", frames_per_second);
    }

    //STBIDEF stbi_uc *stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp)
    int image_width, image_height, image_comp;
    stbi_uc *image_data = stbi_load(jpg_paths[0].c_str(), &image_width, &image_height, &image_comp, 3);
    LOGF(VOUT, "Image: %dx%d, %d bytes/px\n", image_width, image_height, image_comp);
    free(image_data), image_data = nullptr;

    @autoreleasepool {
        NSArray<AVOutputSettingsPreset> *presets = [AVOutputSettingsAssistant availableOutputSettingsPresets];
        for (NSUInteger i = 0; i < [presets count]; ++i) {
            LOGF(VOUT, "%lu. %s\n", (unsigned long)i, [[presets objectAtIndex:i] UTF8String]);
        }

        AVFileType output_file_type = AVFileTypeMPEG4;

        // AVVideoExpectedSourceFrameRateKey
        // AVVideoWidthKey
        // AVVideoHeightKey
        // AVVideoPixelAspectRatioKey
        NSDictionary<NSString *, id> *video_settings = nil;
        {
            auto *assistant = [AVOutputSettingsAssistant outputSettingsAssistantWithPreset:AVOutputSettingsPreset1920x1080];

            video_settings = [assistant videoSettings];

            //            // wtf. this doesn't actually seem to affect the video settings...?
            //
            //            CMVideoFormatDescriptionRef vfd_ref;
            //            OSStatus result=CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
            //                                                           kCMVideoCodecType_H264,
            //                                                           image_width,
            //                                                           image_height,
            //                                                           nil,
            //                                                           &vfd_ref);
            //            if(result!=noErr){
            //                LOGF(ERR,"FATAL: CMVideoFormatDescriptionCreate failed: %" PRId32 " (0x%" PRIx32 ")\n",result,result);
            //                return 1;
            //            }
            //
            //            assistant.sourceVideoFormat=vfd_ref;
            //
            //            CFRelease(vfd_ref),vfd_ref=nullptr;
            //
            //            assistant.sourceVideoMinFrameDuration=CMTimeMake(1000,(int32_t)(frames_per_second*1000.));
            //            assistant.sourceVideoAverageFrameDuration=assistant.sourceVideoMinFrameDuration;

            //            NSDictionary<NSString*,id>*vs=[assistant videoSettings];
            //            NSDictionary<NSString*,id>*vs2=[assistant videoSettings];
            [video_settings setValue:[NSNumber numberWithInt:image_width] forKey:AVVideoWidthKey];
            [video_settings setValue:[NSNumber numberWithInt:image_height] forKey:AVVideoHeightKey];
            //[video_settings setValue:[NSNumber numberWithDouble:frames_per_second] forKey:AVVideoExpectedSourceFrameRateKey];
        }

        //        NSDictionary<NSString*,id>*audio_settings=nil;
        //        if(got_wav_file)        {
        //            auto *assistant=[AVOutputSettingsAssistant outputSettingsAssistantWithPreset:AVOutputSettingsPreset1920x1080];
        //            const WAVEFORMATEX*fmt=GetWAVEFORMATEX(wav_file);
        //
        ////            AVAudioChannelLayout*layout;
        ////            if(fmt->nChannels==1){
        ////                layout=[AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];
        ////            }else if(fmt->nChannels==2){
        ////                layout=[AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];
        ////            }else{
        ////                LOGF(ERR,"FATAL: unexpected number of .wav channels: %" PRIu16 "\n",fmt->nChannels);
        ////                return 1;
        ////            }
        //
        //            audio_settings=[assistant audioSettings];
        //            //[audio_settings setValue:layout forKey:AVChannelLayoutKey];
        //            [audio_settings setValue:[NSNumber numberWithInt:fmt->wBitsPerSample] forKey:AVLinearPCMBitDepthKey];
        //            [audio_settings setValue:[NSNumber numberWithBool:NO] forKey:AVLinearPCMIsBigEndianKey];
        //            [audio_settings setValue:[NSNumber numberWithBool:NO] forKey:AVLinearPCMIsFloatKey];
        //            [audio_settings setValue:[NSNumber numberWithBool:NO] forKey:AVLinearPCMIsNonInterleavedKey];
        //            [audio_settings setValue:[NSNumber numberWithUnsignedInt:fmt->nSamplesPerSec] forKey:AVSampleRateKey];
        //        }

        DumpSettings("video", video_settings);
        //DumpSettings("audio",audio_settings);

        auto *video_input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                                               outputSettings:video_settings];

        auto *video_adaptor = [AVAssetWriterInputPixelBufferAdaptor
            assetWriterInputPixelBufferAdaptorWithAssetWriterInput:video_input
                                       sourcePixelBufferAttributes:nil];

        AVAssetWriterInput *audio_input = nil;
        if (got_wav_file) {
            const WAVEFORMATEX *fmt = GetWAVEFORMATEX(wav_file);

            AudioStreamBasicDescription asbd = {};
            asbd.mSampleRate = fmt->nSamplesPerSec;
            asbd.mFormatID = kAudioFormatLinearPCM;
            asbd.mFramesPerPacket = 1;
            asbd.mChannelsPerFrame = fmt->nChannels;
            asbd.mBitsPerChannel = fmt->wBitsPerSample;
            asbd.mBytesPerFrame = asbd.mChannelsPerFrame * asbd.mBitsPerChannel / 8;
            asbd.mBytesPerPacket = asbd.mFramesPerPacket * asbd.mBytesPerFrame;
            asbd.mFormatFlags = (kAudioFormatFlagIsSignedInteger |
                                 kAudioFormatFlagsNativeEndian |
                                 kAudioFormatFlagIsPacked);

            CMFormatDescriptionRef afd_ref;
            OSStatus result = CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &asbd, 0, nullptr, 0, nullptr, nullptr, &afd_ref);
            if (result != noErr) {
                LOGF(ERR, "FATAL: CMAudioFormatDescriptionCreate failed: %" PRId32 " (0x%" PRIx32 ")\n", result, result);
                return 1;
            }

            audio_input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeAudio
                                                             outputSettings:nil
                                                           sourceFormatHint:afd_ref];

            CFRelease(afd_ref), afd_ref = nullptr;

            audio_input = nil;
        }

        AVAssetWriter *writer = nil;
        NSURL *output_file_url = nil;
        if (!options.output_path.empty()) {
            NSError *error;

            output_file_url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:options.output_path.c_str()]];
            auto *temp_output_file_url = [[NSFileManager defaultManager] URLForDirectory:NSItemReplacementDirectory
                                                                                inDomain:NSUserDomainMask
                                                                       appropriateForURL:output_file_url
                                                                                  create:YES
                                                                                   error:&error];
            if (!temp_output_file_url) {
                LOGF(ERR, "FATAL: Temp directory creation error: %s\n", [[error description] UTF8String]);
                return 1;
            }

            std::string output_name = PathGetName(options.output_path);
            temp_output_file_url = [temp_output_file_url URLByAppendingPathComponent:[NSString stringWithUTF8String:output_name.c_str()]
                                                                         isDirectory:NO];

            LOGF(VOUT, "Output file URL: %s\n", [[temp_output_file_url absoluteString] UTF8String]);

            writer = [AVAssetWriter assetWriterWithURL:temp_output_file_url
                                              fileType:output_file_type
                                                 error:&error];
            if (error) {
                LOGF(ERR, "FATAL: Asset writer creation error: %s\n", [[error description] UTF8String]);
                return 1;
            }
        }

        if (writer) {
            [writer addInput:video_input];
            if (audio_input) {
                [writer addInput:audio_input];
            }
            [writer startWriting];
            [writer startSessionAtSourceTime:kCMTimeZero];
        }

        size_t num_frames = jpg_paths.size();
        if (options.max_num_frames > 0) {
            num_frames = std::min(num_frames, (size_t)options.max_num_frames);
        }

        double audio_data_index = 0.;
        double audio_data_samples_per_frame = 0.;
        if (got_wav_file) {
            const WAVEFORMATEX *fmt = GetWAVEFORMATEX(wav_file);
            audio_data_samples_per_frame = wav_file.data.size() / (fmt->nSamplesPerSec / frames_per_second);
        }

        LOGF(VOUT, "audio samples per frame: %.3f\n", audio_data_samples_per_frame);

        uint64_t num_append_spins = 0;
        bool use_stb = false;

        // For NSImage conversion.
        auto *pixel_buffer_dictionary = [NSDictionary dictionaryWithObjectsAndKeys:
                                                          [NSNumber numberWithBool:YES], kCVPixelBufferCGImageCompatibilityKey,
                                                          [NSNumber numberWithBool:YES], kCVPixelBufferCGBitmapContextCompatibilityKey,
                                                          nil];

        CGColorSpaceRef colour_space_rgb = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);

        for (size_t frame_index = 0; frame_index < num_frames; ++frame_index) {
            const std::string &jpg_path = jpg_paths[frame_index];

            CVPixelBufferRef pixel_buffer;

            if (use_stb) {
                LOGF(VOUT, "(stb) Frame %zu: %s\n", frame_index, jpg_path.c_str());
                int frame_width, frame_height, frame_comp;
                stbi_uc *frame_data = stbi_load(jpg_path.c_str(),
                                                &frame_width,
                                                &frame_height,
                                                &frame_comp,
                                                3);

                if (frame_width != image_width || frame_height != image_height) {
                    LOGF(ERR, "FATAL: expected %dx%d image, got %dx%d: %s\n",
                         image_width, image_height,
                         frame_width, frame_height,
                         jpg_path.c_str());
                    return 1;
                    CVReturn create_result = CVPixelBufferCreateWithBytes(kCFAllocatorDefault,
                                                                          (size_t)frame_width,
                                                                          (size_t)frame_height,
                                                                          kCVPixelFormatType_24RGB,
                                                                          frame_data,
                                                                          (size_t)frame_width * 3,
                                                                          &PixelBufferFreeSTBImageCallback,
                                                                          nullptr,
                                                                          nil,
                                                                          &pixel_buffer);
                    if (create_result != kCVReturnSuccess) {
                        LOGF(ERR, "FATAL: CVPixelBufferCreateWithBytes failed: %" PRId32 " (0x%" PRIx32 ") (%s)\n",
                             create_result,
                             create_result,
                             GetCVReturnEnumName(create_result));
                    }
                }
            } else {
                CVReturn cvr;

                LOGF(VOUT, "(NS) Frame %zu: %s\n", frame_index, jpg_path.c_str());
                NSImage *image = [[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:jpg_path.c_str()]];
                if (!image) {
                    LOGF(ERR, "FATAL: failed to load NSImage from: %s\n", jpg_path.c_str());
                    return 1;
                }

                NSSize size = [image size];
                if (size.width != image_width || size.height != image_height) {
                    LOGF(ERR, "FATAL: expected %dx%d image, got %fx%f: %s\n",
                         image_width, image_height,
                         size.width, size.height,
                         jpg_path.c_str());
                    return 1;
                }

                cvr = CVPixelBufferCreate(kCFAllocatorDefault,
                                          (size_t)image_width,
                                          (size_t)image_height,
                                          k32ARGBPixelFormat,
                                          (CFDictionaryRef)pixel_buffer_dictionary,
                                          &pixel_buffer);
                if (cvr != kCVReturnSuccess) {
                    LOGF(ERR, "FATAL: CVPixelBufferCreateWithBytes failed: %" PRId32 " (0x%" PRIx32 ") (%s)\n", cvr, cvr, GetCVReturnEnumName(cvr));
                    return 1;
                }

                cvr = CVPixelBufferLockBaseAddress(pixel_buffer, 0);
                if (cvr != kCVReturnSuccess) {
                    LOGF(ERR, "FATAL: CVPixelBufferLockBaseAddress failed: %" PRId32 " (0x%" PRIx32 ") (%s)\n", cvr, cvr, GetCVReturnEnumName(cvr));
                    return 1;
                }

                void *base_address = CVPixelBufferGetBaseAddress(pixel_buffer);
                size_t bytes_per_row = CVPixelBufferGetBytesPerRow(pixel_buffer);

                CGContextRef context_ref = CGBitmapContextCreate(base_address,
                                                                 (size_t)image_width,
                                                                 (size_t)image_height,
                                                                 8,
                                                                 bytes_per_row,
                                                                 colour_space_rgb,
                                                                 kCGImageAlphaNoneSkipFirst);
                if (!context_ref) {
                    LOGF(ERR, "FATAL: CGBitmapContextCreate failed\n");
                    return 1;
                }

                auto *graphics_context = [NSGraphicsContext graphicsContextWithCGContext:context_ref
                                                                                 flipped:NO];

                [NSGraphicsContext saveGraphicsState];
                [NSGraphicsContext setCurrentContext:graphics_context];
                //- (void)drawAtPoint:(NSPoint)point fromRect:(NSRect)fromRect operation:(NSCompositingOperation)op fraction:(CGFloat)delta;
                [image compositeToPoint:NSMakePoint(0, 0)
                              operation:NSCompositingOperationCopy];
                [NSGraphicsContext restoreGraphicsState];

                cvr = CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);
                if (cvr != kCVReturnSuccess) {
                    LOGF(ERR, "FATAL: CVPixelBufferUnlockBaseAddress failed: %" PRId32 " (0x%" PRIx32 ") (%s)\n", cvr, cvr, GetCVReturnEnumName(cvr));
                    return 1;
                }

                CFRelease(context_ref);
            }

            if (writer) {
                while (!video_input.isReadyForMoreMediaData) {
                    SleepMS(1);
                    ++num_append_spins;
                }
                [video_adaptor appendPixelBuffer:pixel_buffer
                            withPresentationTime:CMTimeMake((int64_t)frame_index * 1000,
                                                            (int32_t)(frames_per_second * 1000.))];

                //                if(got_wav_file){
                //                    //OSStatus CMBlockBufferCreateWithMemoryBlock(CFAllocatorRef structureAllocator,
                //                    //                                            void *memoryBlock,
                //                    //                                            size_t blockLength,
                //                    //                                            CFAllocatorRef blockAllocator,
                //                    //                                            const CMBlockBufferCustomBlockSource *customBlockSource,
                //                    //                                            size_t offsetToData,
                //                    //                                            size_t dataLength,
                //                    //                                            CMBlockBufferFlags flags,
                //                    //                                            CMBlockBufferRef  _Nullable *blockBufferOut);
                //
                //                    double begin=audio_data_index;
                //                    double end=audio_data_index+audio_data_samples_per_frame;
                //                    //size_t num_bytes=
                //
                //                    CMBlockBufferRef bb_ref;
                //                    OSStatus status=CMBlockBufferCreateWithMemoryBlock(nullptr,
                //                                                                       wav_file.data.data(),
                //                                                                       wav_file.data.size(),
                //                                                                       kCFAllocatorNull,
                //                                                                       nullptr,
                //                                                                       (size_t)begin,
                //                                                                       (size_t)end-(size_t)begin,
                //                                                                       0,
                //                                                                       &bb_ref);
                //                    if(status!=noErr){
                //                        LOGF(ERR,"FATAL: CMBlockBufferCreateWithMemoryBlock failed: %" PRId32 " (0x%" PRIx32 ")\n",
                //                             status,
                //                             status);
                //                        return 1;
                //                    }
                //
                //                    //OSStatus CMSampleBufferCreate(CFAllocatorRef allocator,
                //                    //                              CMBlockBufferRef dataBuffer,
                //                    //                              Boolean dataReady,
                //                    //                              CMSampleBufferMakeDataReadyCallback makeDataReadyCallback,
                //                    //                              void *makeDataReadyRefcon,
                //                    //                              CMFormatDescriptionRef formatDescription,
                //                    //                              CMItemCount numSamples,
                //                    //                              CMItemCount numSampleTimingEntries,
                //                    //                              const CMSampleTimingInfo *sampleTimingArray,
                //                    //                              CMItemCount numSampleSizeEntries,
                //                    //                              const size_t *sampleSizeArray,
                //                    //                              CMSampleBufferRef  _Nullable *sampleBufferOut);
                //
                //                    CMSampleBufferRef sb_ref;
                //                    status=CMSampleBufferCreate(kCFAllocatorDefault,
                //                                                bb_ref,
                //                                                1,
                //                                                nullptr,
                //                                                nullptr,
                //                                                ??CMFortatDescriptionRef,
                //
                //
                //                    [audio_input appendSampleBuffer:
                //                }
            }

            CFRelease(pixel_buffer), pixel_buffer = nullptr;
        }

        [video_input markAsFinished];
        [audio_input markAsFinished];
        __block auto finished = std::make_shared<std::atomic<BOOL>>(NO);
        [writer finishWritingWithCompletionHandler:^void(void) {
          AVAssetWriterStatus status = [writer status];
          LOGF(VOUT, "Status: %d (%s)\n", (int)status, GetAVAssetWriterStatusEnumName(status));
          finished->store(YES, std::memory_order_release);
        }];
        uint64_t num_finish_spins = 0;
        while (!finished->load(std::memory_order_acquire)) {
            ++num_finish_spins;
            SleepMS(1);
        }
        LOGF(VOUT, "Spin count while writing: %" PRIu64 "\n", num_append_spins);
        LOGF(VOUT, "Spin count while finishing: %" PRIu64 "\n", num_finish_spins);
    }
}
