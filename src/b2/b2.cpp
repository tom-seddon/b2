#define _CRTDBG_MAP_ALLOC
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include <shared/system.h>
#include <shared/debug.h>
#include <shared/path.h>
#include <shared/log.h>
#include <shared/CommandLineParser.h>
#include <SDL.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "MemoryDiscImage.h"
#include <inttypes.h>
#include "VBlankMonitor.h"
#include <Remotery.h>
#include <beeb/sound.h>
#include <beeb/teletext.h>
#include "BeebThread.h"
#include "BeebWindows.h"
#include "keymap.h"
#include "conf.h"
#include "BeebState.h"
#include "load_save.h"
#include "Messages.h"
#include "BeebWindow.h"
#include <beeb/Trace.h>
#include "b2.h"
#include "VideoWriterMF.h"
#include "VideoWriterFFmpeg.h"
#include <beeb/BBCMicro.h>
#include <atomic>
#include <shared/system_specific.h>
#include "HTTPServer.h"
#include "HTTPMethodsHandler.h"
#include <curl/curl.h>
#include "DirectDiscImage.h"
#include "discs.h"
#include "SDLBeebWindow.h"
#if SYSTEM_OSX
#include <IOKit/hid/IOHIDLib.h>
#endif

#include <shared/enum_def.h>
#include "b2.inl"
#include <shared/enum_end.h>

#include <shared/enum_decl.h>
#include "b2_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "b2_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char PRODUCT_NAME[]="b2 - BBC Micro B/B+/Master 128 emulator - " STRINGIZE(RELEASE_NAME);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
#if BUILD_TYPE_Final

// Seems like there's no good way to do this from CMake:
// http://stackoverflow.com/questions/8054734/
//
// But this here is simple enough.
//
// (There's no need to do anything about the entry point; the SDL2main
// stuff already ensures that the program can work either way.)
#pragma comment(linker,"/SUBSYSTEM:WINDOWS")

#endif
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const int DEFAULT_AUDIO_HZ=48000;

static const int DEFAULT_AUDIO_BUFFER_SIZE=1024;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Enabled by the -v/--verbose command line option, or if this is a
// debug build.
LOG_DEFINE(OUTPUT,"",&log_printer_stdout_and_debugger,false)
LOG_DEFINE(OUTPUTND,"",&log_printer_stdout,false)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// SDL_WaitEvent has a Sleep(1 ms) in it: https://github.com/tom-seddon/SDL-1/blob/2fdbae22cb2f75643447c34d2dab7f15305e3567/src/events/SDL_events.c#L790

class GlobalMessage {
public:
    const GlobalMessageType type;
    
    explicit GlobalMessage(GlobalMessageType type);
    virtual ~GlobalMessage()=0;
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

GlobalMessage::GlobalMessage(GlobalMessageType type_):
type(type_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

GlobalMessage::~GlobalMessage() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class FunctionGlobalMessage:
public GlobalMessage
{
public:
    std::function<void()> fun;
    
    FunctionGlobalMessage(std::function<void()> fun);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FunctionGlobalMessage::FunctionGlobalMessage(std::function<void()> fun_):
GlobalMessage(GlobalMessageType_Function),
fun(std::move(fun_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VBlankGlobalMessage:
public GlobalMessage
{
public:
    const uint32_t display_id;
    const uint64_t creation_ticks;
    
    explicit VBlankGlobalMessage(uint32_t display_id);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

VBlankGlobalMessage::VBlankGlobalMessage(uint32_t display_id_):
GlobalMessage(GlobalMessageType_VBlank),
display_id(display_id_),
creation_ticks(GetCurrentTickCount())
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class RemoveDisplayMessage:
public GlobalMessage
{
public:
    const uint32_t display_id;
    
    explicit RemoveDisplayMessage(uint32_t display_id);
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RemoveDisplayMessage::RemoveDisplayMessage(uint32_t display_id_):
GlobalMessage(GlobalMessageType_RemoveDisplay),
display_id(display_id_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static MessageQueue<std::unique_ptr<GlobalMessage>> g_global_message_queue("Global MQ");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool g_option_vsync=true;

static GlobalStats g_global_stats;

const GlobalStats *GetGlobalStats() {
    return &g_global_stats;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void *operator new(size_t n) {
//    return malloc(n);
//}
//
//void *operator new[](size_t n) {
//    return malloc(n);
//}
//
//void operator delete(void *p) {
//    free(p);
//}
//
//void operator delete[](void *p) {
//    free(p);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class b2VBlankHandler:
    public VBlankMonitor::Handler
{
public:
//    struct VBlank {
//        uint64_t ticks;
//        int event;
//    };
//
//    struct Display {
//        Mutex mutex;
//        VBlank vblanks[NUM_VBLANK_RECORDS]={};
//        size_t vblank_index=0;
//    };

    void *AllocateDisplayData(uint32_t display_id) override;
    void FreeDisplayData(uint32_t display_id,void *data) override;
    void ThreadVBlank(size_t display_index,uint32_t display_id,void *data) override;
protected:
private:
    std::atomic<uint32_t> m_display_id{};
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *b2VBlankHandler::AllocateDisplayData(uint32_t display_id) {
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void b2VBlankHandler::FreeDisplayData(uint32_t display_id,void *data) {
    g_global_message_queue.ProducerPush(std::make_unique<RemoveDisplayMessage>(display_id));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void b2VBlankHandler::ThreadVBlank(size_t display_index,uint32_t display_id,void *data) {
//    if(display_id==m_display_id.load(std::memory_order_acquire)) {
////    auto display=(Display *)data;
////
////    ASSERT(display->vblank_index<NUM_VBLANK_RECORDS);
////    VBlank *vblank=&display->vblanks[display->vblank_index];
////    ++display->vblank_index;
////    display->vblank_index%=NUM_VBLANK_RECORDS;
////
////    vblank->ticks=GetCurrentTickCount();
//
    g_global_message_queue.ProducerPushIndexed(GlobalMessageQueueIndex_VBlankDisplay0+display_index,
                                               std::make_unique<VBlankGlobalMessage>(display_id));
//        //g_global_message_queue.ProducerPush(std::make_unique<VBlankGlobalMessage>(display_id));
//    }
    //g_global_message_queue.ProducerPush(std::make_unique<VBlankGlobalMessage>(display_id));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct FillAudioBufferData {
    SDL_AudioDeviceID device=0;
    SDL_AudioSpec spec{};
    std::vector<float> mix_buffer;

    uint64_t first_call_ticks=0;
    SDLBeebWindow *beeb_window=nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ThreadFillAudioBuffer(void *userdata,uint8_t *stream,int len) {
    rmt_BeginCPUSample(ThreadFillAudioBuffer,0);

    auto data=(FillAudioBufferData *)userdata;

    uint64_t now_ticks=GetCurrentTickCount();
    if(data->first_call_ticks==0) {
        data->first_call_ticks=now_ticks;
    }

    ASSERT(len>=0);
    ASSERT((size_t)len%4==0);
    size_t num_samples=(size_t)len/4;
    ASSERT(num_samples<=data->spec.samples);

    //float us_per_sample=1e6f/data->spec.freq;

    if(num_samples>data->mix_buffer.size()) {
        data->mix_buffer.resize(num_samples);
    }

    memset(data->mix_buffer.data(),0,num_samples*sizeof(float));

    //_mm_lfence();

    if(data->beeb_window) {
        data->beeb_window->ThreadFillAudioBuffer(data->device,data->mix_buffer.data(),num_samples);
    }

    memcpy(stream,data->mix_buffer.data(),(size_t)len);

    rmt_EndCPUSample();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static void PushUpdateWindowTitleEvent(void) {
//    SDL_Event event={};
//    event.user.type=g_first_event_type+SDLEventType_UpdateWindowTitle;
//
//    SDL_PushEvent(&event);
//}

//static Uint32 UpdateWindowTitle(Uint32 interval,void *param) {
//    (void)param;
//
//    PushUpdateWindowTitleEvent();
//
//    return interval;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PushFunctionMessage(std::function<void()> fun) {
    g_global_message_queue.ProducerPush(std::make_unique<FunctionGlobalMessage>(std::move(fun)));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if RMT_ENABLED
static Remotery *g_remotery;
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Handle Caps Lock up/down state on macOS.
//
// Copied from an older version of SDL. See https://github.com/tom-seddon/SDL/blob/cfcedfccf3a079be983ae571fc5160461a70ca95/src/video/cocoa/SDL_cocoakeyboard.m#L190

#if SYSTEM_OSX
// Placeholder value for the real Caps Lock state. On macOS, the event loop
// discards any SDL_SCANCODE_CAPSLOCK events (as they're bogus), and turns
// SDL_SCANCODE_CAPSLOCK_MACOS events into SDL_SCANCODE_CAPSLOCK ones.
//
// This isn't quite right, because SDL_SCANCODE_CAPSLOCK_MACOS won't affect
// the caps lock key modifier state. But hopefully that'll get set correctly
// by the usual SDL processing? (And besides, b2 never uses the caps lock
// modifier state anyway...)
static const SDL_Scancode SDL_SCANCODE_CAPSLOCK_MACOS=(SDL_Scancode)511;

static IOHIDManagerRef g_hid_manager=nullptr;

extern "C" int SDL_SendKeyboardKey(Uint8 state,SDL_Scancode scancode);//sorry

#endif

#if SYSTEM_OSX
static void HIDCallback(void *context,IOReturn result,void *sender,IOHIDValueRef value) {
    if(context!=g_hid_manager) {
        // An old callback, ignore it (related to bug 2157 below).
        return;
    }

    IOHIDElementRef elem=IOHIDValueGetElement(value);
    if (IOHIDElementGetUsagePage(elem)!=kHIDPage_KeyboardOrKeypad) {
        return;
    }
    
    if(IOHIDElementGetUsage(elem)!=kHIDUsage_KeyboardCapsLock) {
        return;
    }
    
    CFIndex pressed=IOHIDValueGetIntegerValue(value);
    SDL_SendKeyboardKey(pressed?SDL_PRESSED:SDL_RELEASED,SDL_SCANCODE_CAPSLOCK_MACOS);
}
#endif

#if SYSTEM_OSX
static CFDictionaryRef CreateHIDDeviceMatchingDictionary(UInt32 usagePage,UInt32 usage) {
    CFMutableDictionaryRef dict=CFDictionaryCreateMutable(kCFAllocatorDefault,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    if(dict) {
        CFNumberRef number=CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&usagePage);
        if(number) {
            CFDictionarySetValue(dict,CFSTR(kIOHIDDeviceUsagePageKey),number);
            CFRelease(number);
            number=CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&usage);
            if(number) {
                CFDictionarySetValue(dict,CFSTR(kIOHIDDeviceUsageKey),number);
                CFRelease(number);
                return dict;
            }
        }
        CFRelease(dict);
    }
    return nullptr;
}
#endif

#if SYSTEM_OSX
static void QuitHIDCallback() {
    if(!g_hid_manager) {
        return;
    }

    // Releasing here causes a crash on Mac OS X 10.10 and earlier, so just leak it for now. See bug 2157 for details.
    IOHIDManagerUnscheduleFromRunLoop(g_hid_manager,CFRunLoopGetCurrent(),kCFRunLoopDefaultMode);
    IOHIDManagerRegisterInputValueCallback(g_hid_manager,nullptr,nullptr);
    IOHIDManagerClose(g_hid_manager,0);

    CFRelease(g_hid_manager);

    g_hid_manager=nullptr;
}
#endif

#if SYSTEM_OSX
static void InitHIDCallback() {
    g_hid_manager=IOHIDManagerCreate(kCFAllocatorDefault,kIOHIDOptionsTypeNone);
    if(!g_hid_manager) {
        return;
    }
    
    CFDictionaryRef keyboard=nullptr;
    CFDictionaryRef keypad=nullptr;
    CFArrayRef matches=nullptr;
    
    keyboard=CreateHIDDeviceMatchingDictionary(kHIDPage_GenericDesktop,kHIDUsage_GD_Keyboard);
    if(!keyboard) {
        goto fail;
    }
    
    keypad=CreateHIDDeviceMatchingDictionary(kHIDPage_GenericDesktop,kHIDUsage_GD_Keypad);
    if(!keypad) {
        goto fail;
    }
    
    {
        CFDictionaryRef matches_list[]={keyboard,keypad};
        matches = CFArrayCreate(kCFAllocatorDefault,(const void **)matches_list,sizeof matches_list/sizeof matches_list[0],nullptr);
        if(!matches) {
            goto fail;
        }
    }
    IOHIDManagerSetDeviceMatchingMultiple(g_hid_manager,matches);
    IOHIDManagerRegisterInputValueCallback(g_hid_manager,&HIDCallback,g_hid_manager);
    IOHIDManagerScheduleWithRunLoop(g_hid_manager,CFRunLoopGetMain(),kCFRunLoopDefaultMode);
    if (IOHIDManagerOpen(g_hid_manager,kIOHIDOptionsTypeNone)==kIOReturnSuccess) {
        goto cleanup;
    }

fail:
    QuitHIDCallback();

cleanup:
    if (matches) {
        CFRelease(matches);
    }
    if (keypad) {
        CFRelease(keypad);
    }
    if (keyboard) {
        CFRelease(keyboard);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    bool verbose=false;
    std::string discs[NUM_DRIVES];
    bool direct_disc[NUM_DRIVES]={};
    int audio_hz=DEFAULT_AUDIO_HZ;
    int audio_buffer_size=DEFAULT_AUDIO_BUFFER_SIZE;
    bool boot=0;
    bool help=false;
    std::vector<std::string> enable_logs,disable_logs;
#if HTTP_SERVER
    bool http_listen_on_all_interfaces=false;
#endif
    bool reset_windows=false;
    bool vsync=false;
    bool timer=false;
    std::string config_name;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool InitializeOptions(
    Options *options,
    Messages *msg)
{
    (void)msg;
    
    *options=Options();

    // ???

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetDriverHelp(
    std::string driver_type,
    const std::map<std::string,int> &driver_index_by_name)
{
    std::string help="use SDL "+driver_type+" driver DRIVER (one of: ";
    const char *sep="";

    for(auto &&it:driver_index_by_name) {
        help+=sep;
        help+=it.first;

        sep=", ";
    }

    help+=")";

    return help;
}

static std::string GetLogList() {
    std::string list;

    for(auto &&it:GetLogListsByTag()) {
        if(!list.empty()) {
            list+=", ";
        }

        list+=it.first;
    }

    return list;
}

#if SYSTEM_OSX
static bool IsPSNArgument(const char *arg) {
    if(arg[0]!='-'||arg[1]!='p'||arg[2]!='s'||arg[3]!='n'||arg[4]!='_'||!isdigit(arg[5])||arg[6]!='_') {
        return false;
    }

    for(int i=7;arg[i]!=0;++i) {
        if(!isdigit(arg[i])) {
            return false;
    }
}

    return true;
}
#endif

#if SYSTEM_OSX
// http://stackoverflow.com/questions/10242115/
static void RemovePSNArguments(std::vector<const char *> *argv) {
    auto &&it=argv->begin();
    while(it!=argv->end()) {
        if(IsPSNArgument(*it)) {
            it=argv->erase(it);
        } else {
            ++it;
        }
    }
}
#endif

static bool DoCommandLineOptions(
    Options *options,
    int argc,char *argv[],
    Messages *init_messages)
{
    CommandLineParser p(PRODUCT_NAME);

    p.SetLogs(&init_messages->i,&init_messages->e);

    for(int drive=0;drive<NUM_DRIVES;++drive) {
        p.AddOption((char)('0'+drive)).Arg(&options->discs[drive]).Meta("FILE").Help("load drive "+std::to_string(drive)+" from disc image FILE");

        p.AddOption(0,strprintf("%d-direct",drive)).SetIfPresent(&options->direct_disc[drive]).Help(strprintf("if -%d specified, load a direct disc image",drive));
    }

    p.AddOption('b',"boot").SetIfPresent(&options->boot).Help("attempt to auto-boot disc");
    p.AddOption('c',"config").Arg(&options->config_name).Meta("CONFIG").Help("start emulator with configuration CONFIG");

    p.AddOption("hz").Arg(&options->audio_hz).Meta("HZ").Help("set sound output frequency to HZ").ShowDefault();
    p.AddOption("buffer").Arg(&options->audio_buffer_size).Meta("SAMPLES").Help("set audio buffer size, in samples (must be a power of two <32768: 512, 1024, 2048, etc.)").ShowDefault();

    if(!GetLogListsByTag().empty()) {
        std::string list=GetLogList();

        p.AddOption('e',"enable-log").AddArgToList(&options->enable_logs).Meta("LOG").Help("enable additional log LOG (one of: "+list+")");
        p.AddOption('d',"disable-log").AddArgToList(&options->disable_logs).Meta("LOG").Help("disable additional log LOG (one of: "+list+")");
    }

    p.AddOption('v',"verbose").SetIfPresent(&options->verbose).Help("be extra verbose");

    p.AddOption(0,"reset-windows").SetIfPresent(&options->reset_windows).Help("reset window position and dock data");

#if HTTP_SERVER
    p.AddOption("http-listen-on-all-interfaces").SetIfPresent(&options->http_listen_on_all_interfaces).Help("at own risk, listen for HTTP connections on all network interfaces, not just localhost");
#endif

    p.AddOption("vsync").SetIfPresent(&options->vsync).Help("use vsync for timing");
    p.AddOption("timer").SetIfPresent(&options->timer).Help("use timer for timing");

    p.AddHelpOption(&options->help);

    std::vector<const char *> args(argv,argv+argc);

#if SYSTEM_OSX
    RemovePSNArguments(&args);
#endif

    if(!p.Parse((int)args.size(),args.data())) {
        return false;
    }

    if(options->audio_buffer_size<=0||
       options->audio_buffer_size>=65535||
       (options->audio_buffer_size&(options->audio_buffer_size-1))!=0)
    {
        init_messages->e.f("invalid audio buffer size: %d\n",options->audio_buffer_size);
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// I'm not careful enough about tidying things up before returning
// from `main2' with an error to trust this as a local...
static FillAudioBufferData g_fill_audio_buffer_data;

static bool InitSystem(
    SDL_AudioDeviceID *device_id,
    SDL_AudioSpec *got_spec,
    const Options &options,
    Messages *init_messages)
{
    (void)options;

    // Initialise SDL
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER)!=0) {
        init_messages->e.f("FATAL: SDL_Init failed: %s\n",SDL_GetError());
        return false;
    }

    SDL_EnableScreenSaver();

    // Allocate user events
    //g_first_event_type=SDL_RegisterEvents(SDLEventType_Count);

    // 
//    SDL_AddTimer(1000,&UpdateWindowTitle,NULL);
//    PushUpdateWindowTitleEvent();

    SDL_StartTextInput();
    
#if SYSTEM_OSX
    InitHIDCallback();
#endif

    // Start audio
    {
        SDL_AudioSpec spec={};

        spec.freq=options.audio_hz;
        spec.format=AUDIO_FORMAT;
        spec.channels=AUDIO_NUM_CHANNELS;
        spec.callback=&ThreadFillAudioBuffer;
        spec.userdata=&g_fill_audio_buffer_data;
        spec.samples=(Uint16)options.audio_buffer_size;

        *device_id=SDL_OpenAudioDevice(
            nullptr,
            0, // playback, not capture
            &spec,
            got_spec,
            SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
        if(*device_id==0) {
            init_messages->i.f("failed to initialize audio: %s\n",SDL_GetError());
            return false;
        }

        g_fill_audio_buffer_data.spec=*got_spec;
        g_fill_audio_buffer_data.device=*device_id;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void CheckAssetPath(const std::string &path_) {
    (void)path_;

#if ASSERT_ENABLED

#if SYSTEM_OSX

    std::string path=path_;

    ASSERT(path.find("/./")==std::string::npos);

    for(;;) {
        std::string dotdot="/../";
        std::string::size_type pos=path.find(dotdot);
        if(pos==std::string::npos) {
            break;
        }

        ASSERT(pos>0);
        std::string::size_type prev_pos=path.find_last_of('/',pos-1);
        path=path.substr(0,prev_pos)+"/"+path.substr(pos+dotdot.size());
    }

    char real_path[PATH_MAX];
    ASSERT(realpath(path.c_str(),real_path));
    ASSERT(path==real_path);

#elif SYSTEM_WINDOWS

    // TODO...

#endif
    
#endif
}

static void CheckAssetPaths() {
    for(size_t i=0;BEEB_ROMS[i];++i) {
        CheckAssetPath(BEEB_ROMS[i]->GetAssetPath());
    }

    for(size_t i=0;i<NUM_BLANK_DFS_DISCS;++i) {
        CheckAssetPath(BLANK_DFS_DISCS[i].GetAssetPath());
    }

    for(size_t i=0;i<NUM_BLANK_ADFS_DISCS;++i) {
        CheckAssetPath(BLANK_ADFS_DISCS[i].GetAssetPath());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
static void LoadDiscDriveSampleFailed(bool *good,Messages *init_messages,const std::string &path,const char *what) {
    init_messages->e.f("failed to load disc sound sample: %s\n",path.c_str());
    init_messages->i.f("(%s failed: %s)\n",what,SDL_GetError());
    *good=false;
}
#endif

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
static void LoadDiscDriveSound(bool *good,DiscDriveType type,DiscDriveSound sound,const char *fname,Messages *init_messages) {
    if(!*good) {
        return;
    }

    ASSERT(sound>=0&&sound<DiscDriveSound_EndValue);

    std::string path=GetAssetPath("samples",fname);
    CheckAssetPath(path);

    SDL_AudioSpec spec={};
    spec.freq=SOUND_CLOCK_HZ;
    spec.format=AUDIO_F32;
    spec.channels=1;

    Uint8 *buf;
    Uint32 len;
    SDL_AudioSpec *loaded_spec=SDL_LoadWAV(path.c_str(),&spec,&buf,&len);
    if(!loaded_spec) {
        LoadDiscDriveSampleFailed(good,init_messages,path,"SDL_LoadWAV");
        return;
    }

    SDL_AudioCVT cvt;
    if(SDL_BuildAudioCVT(&cvt,loaded_spec->format,loaded_spec->channels,loaded_spec->freq,AUDIO_F32,1,SOUND_CLOCK_HZ)<0) {
        LoadDiscDriveSampleFailed(good,init_messages,path,"SDL_BuildAudioCVT");
        return;
    }

    std::vector<float> f32_buf;

    if(cvt.needed) {
        ASSERT(cvt.len_mult>=0);
        ASSERT(len*(unsigned)cvt.len_mult%sizeof(float)==0);
        f32_buf.resize(len*(unsigned)cvt.len_mult/sizeof(float));
        memcpy(f32_buf.data(),buf,len);

        cvt.len=(int)len;
        cvt.buf=(Uint8 *)f32_buf.data();
        if(SDL_ConvertAudio(&cvt)<0) {
            LoadDiscDriveSampleFailed(good,init_messages,path,"SDL_ConvertAudio");
            return;
        }

        ASSERT(cvt.len_cvt>=0);
        ASSERT((size_t)cvt.len_cvt%sizeof(float)==0);
        f32_buf.resize((size_t)cvt.len_cvt/sizeof(float));
    } else {
        ASSERT(len%sizeof(float)==0);
        f32_buf.resize(len/sizeof(float));
        memcpy(f32_buf.data(),buf,len);
    }

    BBCMicro::SetDiscDriveSound(type,sound,std::move(f32_buf));

    //init_messages->i.f("%s: %zu bytes\n",fname,sample_data[sound]->size()*sizeof *sample_data[sound]->data());

    SDL_FreeWAV(buf);
    buf=nullptr;
    len=0;
}
#endif

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND

static bool LoadDiscDriveSamples(Messages *init_messages) {
    bool good=true;

    LoadDiscDriveSound(&good,DiscDriveType_133mm,DiscDriveSound_Seek2ms,"525_seek_2ms.wav",init_messages);
    LoadDiscDriveSound(&good,DiscDriveType_133mm,DiscDriveSound_Seek6ms,"525_seek_6ms.wav",init_messages);
    LoadDiscDriveSound(&good,DiscDriveType_133mm,DiscDriveSound_Seek12ms,"525_seek_12ms.wav",init_messages);
    LoadDiscDriveSound(&good,DiscDriveType_133mm,DiscDriveSound_Seek20ms,"525_seek_20ms.wav",init_messages);
    LoadDiscDriveSound(&good,DiscDriveType_133mm,DiscDriveSound_SpinEmpty,"525_spin_empty.wav",init_messages);
    LoadDiscDriveSound(&good,DiscDriveType_133mm,DiscDriveSound_SpinEnd,"525_spin_end.wav",init_messages);
    LoadDiscDriveSound(&good,DiscDriveType_133mm,DiscDriveSound_SpinLoaded,"525_spin_loaded.wav",init_messages);
    LoadDiscDriveSound(&good,DiscDriveType_133mm,DiscDriveSound_SpinStartEmpty,"525_spin_start_empty.wav",init_messages);
    LoadDiscDriveSound(&good,DiscDriveType_133mm,DiscDriveSound_SpinStartLoaded,"525_spin_start_loaded.wav",init_messages);
    LoadDiscDriveSound(&good,DiscDriveType_133mm,DiscDriveSound_Step,"525_step_1_1.wav",init_messages);

    return good;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool InitLogs(const std::vector<std::string> &names_list,
                     void (Log::*mfn)(),
                     Messages *init_messages)
{
    for(const std::string &tag:names_list) {
        const std::vector<Log *> &logs=GetLogListByTag(tag);

        if(logs.empty()) {
            init_messages->e.f("Unknown log: %s\n",tag.c_str());
            return false;
        }

        for(Log *log:logs) {
            (log->*mfn)();
        }
    }

    return true;
}

static bool InitLogs(const Options &options,Messages *init_messages) {
#if !BUILD_TYPE_Debug////<---note
    if(options.verbose)//<---note
#endif///////////////////<---note
    {
        LOG(OUTPUT).Enable();
        LOG(OUTPUTND).Enable();
    }

    // the Log API really isn't very good for this :(

    if(!InitLogs(options.enable_logs,&Log::Enable,init_messages)) {
        return false;
    }

    if(!InitLogs(options.disable_logs,&Log::Disable,init_messages)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if RMT_ENABLED
static void SetRmtThreadName(const char *name,void *context) {
    (void)context;

    rmt_SetCurrentThreadName(name);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static void SaveConfig(const std::unique_ptr<SDLBeebWindow> &beeb_window,
//                       const BeebWindowSettings &default_window_settings,
//                       std::shared_ptr<MessageList> message_list)
//{
//    Messages msg(message_list);
//
//    const std::vector<uint8_t> &window_placement_data=beeb_window->GetWindowPlacementData();
//
//    SaveGlobalConfig(window_placement_data,
//                     default_window_settings,
//                     &msg);
//
//    msg.i.f("Configuration saved.\n");
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool HandleEvents(SDLBeebWindow *beeb_window,
                         uint32_t beeb_window_id,
                         VBlankMonitor *vblank_monitor,
                         UpdateResult last_update_result)
{
    std::vector<std::unique_ptr<GlobalMessage>> global_messages;
    SDL_Event sdl_event;
    bool got_sdl_event;
    
    for(;;) {
        switch(last_update_result) {
            default:
                ASSERT(false);
            case UpdateResult_Quit:
                return false;
                
            case UpdateResult_SpeedLimited:
                ASSERT(false);
                if(!SDL_WaitEvent(&sdl_event)) {
                    // WaitEvent error = quit.
                    return false;
                }
                got_sdl_event=true;
                break;
                
            case UpdateResult_FlatOut:
                got_sdl_event=SDL_PollEvent(&sdl_event);
                g_global_message_queue.ConsumerPollForMessages(&global_messages);
                break;
        }
        
        if(!got_sdl_event) {
            if(global_messages.empty()) {
                break;
            }
        }
        
        if(!global_messages.empty()) {
            for(const std::unique_ptr<GlobalMessage> &global_message:global_messages) {
                switch(global_message->type) {
                    case GlobalMessageType_RemoveDisplay:
                    {
                        auto message=(RemoveDisplayMessage *)global_message.get();
                        
                        for(auto it=g_global_stats.display_stats.begin();it!=g_global_stats.display_stats.end();++it) {
                            if(it->display_id==message->display_id) {
                                g_global_stats.display_stats.erase(it);
                                break;
                            }
                        }
                        break;
                    }
                        
                    case GlobalMessageType_VBlank:
                    {
                        auto message=(VBlankGlobalMessage *)global_message.get();
                        
                        rmt_ScopedCPUSample(SDLEventType_VBlank,0);
                        
                        GlobalStats::DisplayStats *dstats=nullptr;
                        for(auto &&d:g_global_stats.display_stats) {
                            if(d.display_id==message->display_id) {
                                dstats=&d;
                                break;
                            }
                        }
                        
                        if(!dstats) {
                            g_global_stats.display_stats.emplace_back();
                            dstats=&g_global_stats.display_stats.back();
                            
                            dstats->display_id=message->display_id;
                            
                            SDL_Rect rect;
                            if(vblank_monitor->GetDisplayRectForDisplayID(dstats->display_id,&rect)) {
                                dstats->x=rect.x;
                                dstats->y=rect.y;
                                dstats->w=rect.w;
                                dstats->h=rect.h;
                            }
                        }
                        
                        ++g_global_stats.total.num_vblank_messages;
                        uint64_t ticks=GetCurrentTickCount();
                        
                        GlobalStats::VBlankStats *vstats,*prev_vstats=nullptr;
                        if(dstats->vblank_stats.size()<100) {
                            dstats->vblank_stats.emplace_back();
                            
                            if(dstats->vblank_stats.size()>1) {
                                prev_vstats=&dstats->vblank_stats[dstats->vblank_stats.size()-2];
                            }
                            vstats=&dstats->vblank_stats.back();
                        } else {
                            size_t tail;
                            if(dstats->vblank_stats_head==0) {
                                tail=dstats->vblank_stats.size()-1;
                            } else {
                                tail=dstats->vblank_stats_head-1;
                            }
                            
                            prev_vstats=&dstats->vblank_stats[tail];
                            vstats=&dstats->vblank_stats[dstats->vblank_stats_head];
                            ++dstats->vblank_stats_head;
                            dstats->vblank_stats_head%=dstats->vblank_stats.size();
                        }
                        
                        vstats->production_ticks=message->creation_ticks;
                        vstats->consumption_ticks=ticks;
                        
                        if(prev_vstats) {
                            vstats->production_delta_ticks=vstats->production_ticks-prev_vstats->production_ticks;
                            vstats->consumption_delta_ticks=vstats->consumption_ticks-prev_vstats->consumption_ticks;
                        }
                        
                        if(beeb_window->HandleVBlank(vblank_monitor,message->display_id,ticks)) {
                            ++g_global_stats.window.num_vblank_messages;
                            
                        }
                        break;
                    }
                        
                    case GlobalMessageType_Function:
                    {
                        auto message=(FunctionGlobalMessage *)global_message.get();
                        
                        rmt_ScopedCPUSample(GlobalMessageType_Function,0);
                        
                        message->fun();
                        
                        break;
                    }
                }
            }
            global_messages.clear();
        }
        
        if(got_sdl_event) {
            switch(sdl_event.type) {
                case SDL_QUIT:
                    return false;
                    
                case SDL_WINDOWEVENT:
                    if(sdl_event.window.windowID==beeb_window_id) {
                        switch(sdl_event.window.event) {
                            case SDL_WINDOWEVENT_CLOSE:
                                //beeb_window->SaveSettings();
                                break;
                                
                            case SDL_WINDOWEVENT_SHOWN:
                            case SDL_WINDOWEVENT_HIDDEN:
                            case SDL_WINDOWEVENT_MOVED:
                            case SDL_WINDOWEVENT_RESIZED:
                            case SDL_WINDOWEVENT_SIZE_CHANGED:
                            case SDL_WINDOWEVENT_MAXIMIZED:
                            case SDL_WINDOWEVENT_RESTORED:
                            case SDL_WINDOWEVENT_FOCUS_GAINED:
                                beeb_window->UpdateWindowPlacement();
                                break;
                        }
                    }
                    break;
                    
                case SDL_MOUSEMOTION:
                    ++g_global_stats.total.num_mouse_motion_events;
                    if(sdl_event.motion.windowID==beeb_window_id) {
                        ++g_global_stats.window.num_mouse_motion_events;
                        beeb_window->HandleSDLMouseMotionEvent(sdl_event.motion);
                    }
                    break;
                    
                case SDL_TEXTINPUT:
                    ++g_global_stats.total.num_text_input_events;
                    if(sdl_event.text.windowID==beeb_window_id) {
                        ++g_global_stats.window.num_text_input_events;
                        beeb_window->HandleSDLTextInput(sdl_event.text.text);
                    }
                    break;
                    
                case SDL_KEYUP:
                case SDL_KEYDOWN:
                    ++g_global_stats.total.num_key_events;
                    if(sdl_event.key.windowID==beeb_window_id) {
                        ++g_global_stats.window.num_key_events;
                        
                        bool handle=true;
                        
#if SYSTEM_OSX
                        if(sdl_event.key.keysym.scancode==SDL_SCANCODE_CAPSLOCK_MACOS) {
                            sdl_event.key.keysym.scancode=SDL_SCANCODE_CAPSLOCK;
                            sdl_event.key.keysym.sym=SDL_GetKeyFromScancode(sdl_event.key.keysym.scancode);
                        } else if(sdl_event.key.keysym.scancode==SDL_SCANCODE_CAPSLOCK) {
                            handle=false;
                        }
#endif
                        
                        if(handle) {
                            beeb_window->HandleSDLKeyEvent(sdl_event.key);
                        }
                    }
                    break;
                    
                case SDL_MOUSEWHEEL:
                    ++g_global_stats.total.num_mouse_wheel_events;
                    if(sdl_event.wheel.windowID==beeb_window_id) {
                        ++g_global_stats.window.num_mouse_wheel_events;
                        beeb_window->SetSDLMouseWheelState(sdl_event.wheel.x,sdl_event.wheel.y);
                    }
                    break;
                    
                default:
                    break;
            }
        }
    }
    
    return true;
}

static bool main2(int argc,char *argv[],const std::shared_ptr<MessageList> &message_list) {
    Messages msg(message_list);

    CheckAssetPaths();

#if RMT_ENABLED
    {
        rmtError x=rmt_CreateGlobalInstance(&g_remotery);
        if(x==RMT_ERROR_NONE) {
            SetSetCurrentThreadNameCallback(&SetRmtThreadName,nullptr);
        } else {
            msg.w.f("Failed to initialise Remotery\n");
        }
    }
#endif

    // https://curl.haxx.se/libcurl/c/curl_global_init.html
    {
        CURLcode r=curl_global_init(CURL_GLOBAL_DEFAULT);
        if(r!=0) {
            msg.e.f("Failed to initialise libcurl: %s\n",curl_easy_strerror(r));
            return false;
        }
    }

    Options options;
    if(!InitializeOptions(&options,&msg)) {
        return false;
    }

    if(!DoCommandLineOptions(&options,argc,argv,&msg)) {
        if(options.help) {
#if SYSTEM_WINDOWS
            if(!GetConsoleWindow()) {
                // Probably a GUI app build, so pop up the message box.
                FailureMessageBox("Command line help",message_list,SIZE_MAX);
                return true;
            }
#endif

            return true;
        }

        return false;
    }

    if(!InitLogs(options,&msg)) {
        return false;
    }

    msg.i.f("%s\n",PRODUCT_NAME);

    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    if(!InitSystem(&audio_device,&audio_spec,options,&msg)) {
        return false;
    }

#if SYSTEM_WINDOWS
    if(!InitMF(&msg)) {
        return false;
    }
#endif

#if HTTP_SERVER
    std::unique_ptr<HTTPServer> http_server;
    msg.w.f("TODO: HTTP server temporarily disabled...\n");
//    std::unique_ptr<HTTPServer> http_server=CreateHTTPServer();
//    if(!http_server->Start(0xbbcb)) {
//        msg.w.f("Failed to start HTTP server.\n");
//        // but carry on... it's not fatal.
//        http_server=nullptr;
//    }
#endif

#if HAVE_FFMPEG
    if(!InitFFmpeg(&msg)) {
        return false;
    }
#endif

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
    if(!LoadDiscDriveSamples(&msg)) {
        msg.e.f("Failed to initialise disc drive samples.\n");
        return false;
    }
#endif

    InitDefaultBeebConfigs();

    {
//        if(!Timeline::Init()) {
//            msg.e.f(
//                "FATAL: failed to initialize timeline.\n");
//            return false;
//        }

        if(!BeebWindows::Init()) {
            msg.e.f(
                "FATAL: failed to initialize window manager.\n");
            return false;
        }

        SDL_PauseAudioDevice(audio_device,0);

        std::vector<uint8_t> window_placement_data;
        BeebWindowSettings default_window_settings;
        if(!LoadGlobalConfig(&window_placement_data,
                             &default_window_settings,
                             &msg))
        {
            return false;
        }

        if(!options.config_name.empty()) {
            default_window_settings.config_name=options.config_name;
        }

        // Ugh. This thing here is really a bit of a bodge...
        if(options.vsync) {
            g_option_vsync=true;
        } else if(options.timer) {
            g_option_vsync=false;
        }

        auto &&vblank_handler=std::make_unique<b2VBlankHandler>();
        msg.i.f("Timing method: %s\n",g_option_vsync?"vsync":"timer");
        std::unique_ptr<VBlankMonitor> vblank_monitor=CreateVBlankMonitor(vblank_handler.get(),
                                                                          !g_option_vsync,
                                                                          &msg);
        if(!vblank_monitor) {
            msg.e.f("Failed to initialise vblank monitor.\n");
            return false;
        }

        BeebWindowInitArguments ia;
        {
//            ia.render_driver_index=options.render_driver_index;
//            ia.pixel_format=SDL_PIXELFORMAT_ARGB8888;
//            ASSERT(ia.pixel_format!=SDL_PIXELFORMAT_UNKNOWN);
            ia.sound_device=audio_device;
            ia.sound_spec=audio_spec;
            //ia.default_config=initial_loaded_config;
            ia.reset_windows=options.reset_windows;

            for(int i=0;i<NUM_DRIVES;++i) {
                if(options.discs[i].empty()) {
                    continue;
                }

                if(options.direct_disc[i]) {
                    ia.init_disc_images[i]=DirectDiscImage::CreateForFile(options.discs[i],&msg);
                } else {
                    ia.init_disc_images[i]=MemoryDiscImage::LoadFromFile(options.discs[i],&msg);
                }

                if(!ia.init_disc_images[i]) {
                    return false;
                }
            }

            ia.boot=options.boot;
        }

        auto beeb_window=std::make_unique<SDLBeebWindow>();

        uint32_t beeb_window_id;
        if(!beeb_window->Init(ia,
                              default_window_settings,
                              message_list,
                              window_placement_data,
                              &beeb_window_id))
        {
            beeb_window=nullptr;

            msg.e.f("FATAL: failed to create window.\n");
            return false;
        }

#if HTTP_SERVER
        std::unique_ptr<HTTPHandler> http_handler;
//        if(!!http_server) {
//            http_handler=CreateHTTPMethodsHandler(beeb_window->GetBeebWindow());
//            http_server->SetHandler(http_handler.get());
//        }
#endif

        SDL_LockAudioDevice(audio_device);
        g_fill_audio_buffer_data.beeb_window=beeb_window.get();
        SDL_UnlockAudioDevice(audio_device);
        
        UpdateResult last_update_result=UpdateResult_FlatOut;
        
        while(HandleEvents(beeb_window.get(),
                           beeb_window_id,
                           vblank_monitor.get(),
                           last_update_result))
        {
            last_update_result=beeb_window->UpdateBeeb();
            last_update_result=UpdateResult_FlatOut;
        }
        

        // not needed any more.
        //message_list->ClearMessages();

//        for(;;) {
//            SDL_Event event;
//
//            {
//                rmt_ScopedCPUSample(SDL_WaitEvent,0);
//                if(!SDL_WaitEvent(&event)) {
//                    goto done;
//                }
//            }
//
//            if(event.type==SDL_QUIT) {
////#if SYSTEM_OSX
////                // On OS X, quit just does a quit, and there are no
////                // window-specific messages sent to indicate that it's
////                // happening. So jump through a few hoops in order
////                // that the settings from the key window (if there is
////                // one) are saved.
////                //
////                // This is a bit of a hack.
////                beeb_window->SaveSettings();
////#endif
//                goto done;
//            }
//
//            switch(event.type) {
//            case SDL_WINDOWEVENT:
//                {
//                    rmt_ScopedCPUSample(SDL_WINDOWEVENT,0);
//
//                    if(event.window.windowID==beeb_window_id) {
//                        switch(event.window.event) {
//                        case SDL_WINDOWEVENT_CLOSE:
//                            //beeb_window->SaveSettings();
//                            break;
//
//                        case SDL_WINDOWEVENT_SHOWN:
//                        case SDL_WINDOWEVENT_HIDDEN:
//                        case SDL_WINDOWEVENT_MOVED:
//                        case SDL_WINDOWEVENT_RESIZED:
//                        case SDL_WINDOWEVENT_SIZE_CHANGED:
//                        case SDL_WINDOWEVENT_MAXIMIZED:
//                        case SDL_WINDOWEVENT_RESTORED:
//                        case SDL_WINDOWEVENT_FOCUS_GAINED:
//                            beeb_window->UpdateWindowPlacement();
//                            break;
//                        }
//                    }
//                }
//                break;
//
//            case SDL_MOUSEMOTION:
//                {
//                    rmt_ScopedCPUSample(SDL_MOUSEMOTION,0);
//
//                    if(event.motion.windowID==beeb_window_id) {
//                        beeb_window->HandleSDLMouseMotionEvent(event.motion);
//                    }
//                }
//                break;
//
//            case SDL_TEXTINPUT:
//                {
//                    rmt_ScopedCPUSample(SDL_TEXTINPUT,0);
//
//                    if(event.text.windowID==beeb_window_id) {
//                        beeb_window->HandleSDLTextInput(event.text.text);
//                    }
//                }
//                break;
//
//            case SDL_KEYUP:
//            case SDL_KEYDOWN:
//                {
//                    rmt_ScopedCPUSample(SDL_KEYxx,0);
//
//                    if(event.key.windowID==beeb_window_id) {
//                        beeb_window->HandleSDLKeyEvent(event.key);
//                    }
//                }
//                break;
//
//            case SDL_MOUSEWHEEL:
//                {
//                    rmt_ScopedCPUSample(SDL_MOUSEWHEEL,0);
//
//                    if(event.wheel.windowID==beeb_window_id) {
//                        beeb_window->SetSDLMouseWheelState(event.wheel.x,event.wheel.y);
//                    }
//                }
//                break;
//
//            default:
//                {
//                    if(event.type>=g_first_event_type&&event.type<g_first_event_type+SDLEventType_Count) {
//                        switch((SDLEventType)(event.type-g_first_event_type)) {
//                        case SDLEventType_VBlank:
//                            {
//                                rmt_ScopedCPUSample(SDLEventType_VBlank,0);
//                                if(auto dd=(b2VBlankHandler::Display *)vblank_monitor->GetDisplayDataForDisplayID((uint32_t)event.user.code)) {
//                                    uint64_t ticks=GetCurrentTickCount();
//
//                                    beeb_window->HandleVBlank(vblank_monitor.get(),dd,ticks);
//
//                                    {
//                                        std::lock_guard<Mutex> lock(dd->mutex);
//
//                                        dd->message_pending=false;
//                                    }
//                                }
//                            }
//                            break;
//
//                        case SDLEventType_UpdateWindowTitle:
//                            {
//                                rmt_ScopedCPUSample(SDLEventType_UpdateWindowTitle,0);
//                                beeb_window->UpdateTitle();
//                            }
//                            break;
//
//                        case SDLEventType_Function:
//                            {
//                                rmt_ScopedCPUSample(SDLEventType_Function,0);
//                                auto fun=(std::function<void()> *)event.user.data1;
//
//                                (*fun)();
//
//                                delete fun;
//                                fun=nullptr;
//                            }
//                            break;
//
//                        case SDLEventType_SaveConfig:
//                            {
//                                auto settings=(BeebWindowSettings *)event.user.data1;
//
//                                SaveConfig(beeb_window,
//                                           *settings,
//                                           beeb_window->GetMessageList());
//
//                                delete settings;
//                                settings=nullptr;
//                            }
//                            break;
//
//                        case SDLEventType_Count:
//                            // only here avoid incomplete switch warning.
//                            ASSERT(false);
//                            break;
//                        }
//                    }
//                }
//                break;
//            }
//        }

    done:;
        beeb_window->SaveConfig();
        ;//<-- fix Visual Studio autoformat bug

//        SaveConfig(beeb_window,
//                   message_list);
//        {
//            std::vector<uint8_t> window_placement_data=beeb_window->GetWindowPlacementData();
//            SaveGlobalConfig(window_placement_data,&msg);
//        }

        SDL_LockAudioDevice(audio_device);
        g_fill_audio_buffer_data.beeb_window=nullptr;
        SDL_UnlockAudioDevice(audio_device);

        SDL_PauseAudioDevice(audio_device,1);

#if HTTP_SERVER
        http_server=nullptr;
        http_handler=nullptr;
#endif

        beeb_window=nullptr;

        BeebWindows::Shutdown();
        //Timeline::Shutdown();

        vblank_monitor=nullptr;
        vblank_handler=nullptr;
    }
    
#if SYSTEM_OSX
    QuitHIDCallback();
#endif

    SDL_Quit();

#if RMT_ENABLED
    if(g_remotery) {
        rmt_DestroyGlobalInstance(g_remotery);
        g_remotery=NULL;
    }
#endif


    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc,char *argv[]) {
#ifdef _MSC_VER
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG)|_CRTDBG_LEAK_CHECK_DF);
    //_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG)|_CRTDBG_CHECK_ALWAYS_DF|);
    //_crtBreakAlloc=12520;
#endif

    auto &&messages=std::make_shared<MessageList>();

    bool good=main2(argc,argv,messages);

    if(!good) {
        FailureMessageBox("Initialisation failed",messages);
    }

    // If there are any messages, get them printed now.
    messages->SetPrintToStdio(true);

    if(!good) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
