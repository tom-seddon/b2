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
#include "65link.h"
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
#include "Timeline.h"
#include <beeb/BBCMicro.h>
#include <atomic>
#include <shared/system_specific.h>
#include "HTTPServer.h"
#include "HTTPMethodsHandler.h"

#include <shared/enum_decl.h>
#include "b2.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "b2.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char PRODUCT_NAME[]=
"b2 - BBC Micro B/B+/Master 128 emulator"
#ifdef RELEASE_NAME
" - " STRINGIZE(RELEASE_NAME)
#endif
;

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

static Uint32 g_first_event_type;

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
    struct VBlank {
        uint64_t ticks;
        int event;
    };

    struct Display {
        VBlank vblanks[NUM_VBLANK_RECORDS]={};
        size_t vblank_index=0;
        std::atomic<bool> message_pending{false};
    };

    void *AllocateDisplayData(uint32_t display_id) override;
    void FreeDisplayData(uint32_t display_id,void *data) override;
    void ThreadVBlank(uint32_t display_id,void *data) override;
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *b2VBlankHandler::AllocateDisplayData(uint32_t display_id) {
    (void)display_id;

    auto display=new Display;

    return display;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void b2VBlankHandler::FreeDisplayData(uint32_t display_id,void *data) {
    (void)display_id;

    auto display=(Display *)data;

    delete display;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void b2VBlankHandler::ThreadVBlank(uint32_t display_id,void *data) {
    auto display=(Display *)data;

    ASSERT(display->vblank_index<NUM_VBLANK_RECORDS);
    VBlank *vblank=&display->vblanks[display->vblank_index];
    ++display->vblank_index;
    display->vblank_index%=NUM_VBLANK_RECORDS;

    vblank->ticks=GetCurrentTickCount();

    bool expected=false;
    vblank->event=display->message_pending.compare_exchange_strong(expected,true);

    if(vblank->event) {
        SDL_Event event={};
        event.user.type=g_first_event_type+SDLEventType_VBlank;
        event.user.code=(Sint32)display_id;

        SDL_PushEvent(&event);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct FillAudioBufferData {
    SDL_AudioDeviceID device=0;
    SDL_AudioSpec spec{};
    std::vector<float> mix_buffer;

    uint64_t first_call_ticks=0;
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

    BeebWindows::ThreadFillAudioBuffer(data->device,data->mix_buffer.data(),num_samples);

    memcpy(stream,data->mix_buffer.data(),(size_t)len);

    rmt_EndCPUSample();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void PushUpdateWindowTitleEvent(void) {
    SDL_Event event={};
    event.user.type=g_first_event_type+SDLEventType_UpdateWindowTitle;

    SDL_PushEvent(&event);
}

static Uint32 UpdateWindowTitle(Uint32 interval,void *param) {
    (void)param;

    PushUpdateWindowTitleEvent();

    return interval;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PushNewWindowMessage(BeebWindowInitArguments init_arguments_) {
    SDL_Event event={};

    event.user.type=g_first_event_type+SDLEventType_NewWindow;

    // This relies on the message loop receiving it, so it can delete
    // it. It's probably possible for an SDL_QUIT to end up ahead of
    // it in the queue, meaning that the object could leak.
    event.user.data1=new BeebWindowInitArguments(std::move(init_arguments_));

    SDL_PushEvent(&event);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PushFunctionMessage(std::function<void()> fun) {
    SDL_Event event={};

    event.user.type=g_first_event_type+SDLEventType_Function;

    event.user.data1=new std::function<void()>(std::move(fun));

    SDL_PushEvent(&event);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if RMT_ENABLED
static Remotery *g_remotery;
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint32_t GetPixelFormatForRendererInfo(const SDL_RendererInfo &info) {
    uint32_t want_flags=SDL_RENDERER_ACCELERATED|SDL_RENDERER_RENDERGEOMETRY;

    if((info.flags&want_flags)==want_flags) {
        for(size_t i=0;i<info.num_texture_formats;++i) {
            uint32_t f=info.texture_formats[i];

            if(SDL_PIXELTYPE(f)==SDL_PIXELTYPE_PACKED32) {
                if(SDL_BYTESPERPIXEL(f)==4) {
                    return f;
                }
            }
        }
    }

    return SDL_PIXELFORMAT_UNKNOWN;
}

static uint32_t GetPixelFormatForRenderer(int index) {
    SDL_RendererInfo info;
    if(SDL_GetRenderDriverInfo(index,&info)!=0) {
        return SDL_PIXELFORMAT_UNKNOWN;
    }

    return GetPixelFormatForRendererInfo(info);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    bool verbose=false;
    std::string discs[NUM_DRIVES];
    int render_driver_index=-1;
    int audio_hz=DEFAULT_AUDIO_HZ;
    int audio_buffer_size=DEFAULT_AUDIO_BUFFER_SIZE;
    bool boot=0;
    bool help=false;
    std::vector<std::string> enable_logs,disable_logs;
#if HTTP_SERVER
    bool http_listen_on_all_interfaces=false;
#endif
    bool ignore_vblank=false;
    bool reset_windows=false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool InitializeOptions(
    Options *options,
    Messages *init_messages)
{
    *options=Options();

    // Check there's at least one valid renderer.
    for(int i=0;i<SDL_GetNumRenderDrivers();++i) {
        if(GetPixelFormatForRenderer(i)!=SDL_PIXELFORMAT_UNKNOWN) {
            options->render_driver_index=i;
            break;
        }
    }

    if(options->render_driver_index<0) {
        init_messages->e.f("No suitable SDL renderers found.\n");
        return false;
    }

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

static bool HandleDriverNameOption(
    int *index,
    const char *type,
    const std::map<std::string,int> &driver_index_by_name,
    const std::string &name,
    Messages *init_messages)
{
    if(!name.empty()) {
        auto &&it=driver_index_by_name.find(name);
        if(it==driver_index_by_name.end()) {
            init_messages->e.f("Unknown SDL %s driver: %s\n",
                               type,name.c_str());
            return false;
        }

        *index=it->second;
    }

    return true;
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
        p.AddOption((char)('0'+drive)).Arg(&options->discs[drive]).Meta("PATH").Help("load drive "+std::to_string(drive)+" from PATH - an SSD/DSD file, or 65Link volume/drive folder");
    }

    p.AddOption('b',"boot").SetIfPresent(&options->boot).Help("attempt to auto-boot disc");

    std::map<std::string,int> render_driver_index_by_name;
    std::string render_driver_name;
    for(int i=0;i<SDL_GetNumRenderDrivers();++i) {
        SDL_RendererInfo info;
        if(SDL_GetRenderDriverInfo(i,&info)==0) {
            if(GetPixelFormatForRendererInfo(info)!=SDL_PIXELFORMAT_UNKNOWN) {
                render_driver_index_by_name[info.name]=i;

                // if(!render_driver_name) {
                //     render_driver_name=info.name;
                // }
            }
        }
    }

    ASSERT(!render_driver_index_by_name.empty());
    if(render_driver_index_by_name.size()>1) {
        p.AddOption('r',"render-driver").Arg(&render_driver_name).Meta("DRIVER").Help(GetDriverHelp("render",render_driver_index_by_name));
    }

    p.AddOption("hz").Arg(&options->audio_hz).Meta("HZ").Help("set sound output frequency to HZ").ShowDefault();
    p.AddOption("buffer").Arg(&options->audio_buffer_size).Meta("SAMPLES").Help("set audio buffer size, in samples (must be a power of two <32768: 512, 1024, 2048, etc.)").ShowDefault();

    if(!GetLogListsByTag().empty()) {
        std::string list=GetLogList();

        p.AddOption('e',"enable-log").AddArgToList(&options->enable_logs).Meta("LOG").Help("enable additional log LOG (one of: "+list+")");
        p.AddOption('d',"disable-log").AddArgToList(&options->disable_logs).Meta("LOG").Help("disable additional log LOG (one of: "+list+")");
    }

    p.AddOption(0,"ignore-vblank").SetIfPresent(&options->ignore_vblank).Help("ignore vblank; present at 50Hz. YMMV");

    p.AddOption('v',"verbose").SetIfPresent(&options->verbose).Help("be extra verbose");

    p.AddOption(0,"reset-windows").SetIfPresent(&options->reset_windows).Help("reset window position and dock data");

#if HTTP_SERVER
    p.AddOption("http-listen-on-all-interfaces").SetIfPresent(&options->http_listen_on_all_interfaces).Help("at own risk, listen for HTTP connections on all network interfaces, not just localhost");
#endif

    p.AddHelpOption(&options->help);

    std::vector<const char *> args(argv,argv+argc);

#if SYSTEM_OSX
    RemovePSNArguments(&args);
#endif

    if(!p.Parse((int)args.size(),args.data())) {
        return false;
    }

    if(!HandleDriverNameOption(
        &options->render_driver_index,
        "render",
        render_driver_index_by_name,
        render_driver_name,
        init_messages))
    {
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

    // Allocate user events
    g_first_event_type=SDL_RegisterEvents(SDLEventType_Count);

    // 
    SDL_AddTimer(1000,&UpdateWindowTitle,NULL);
    PushUpdateWindowTitleEvent();

    SDL_StartTextInput();

    //// Allocate vblank monitor
    //*vblank_monitor_ptr=CreateVBlankMonitor();

    //if(VBlankMonitorInit(*vblank_monitor_ptr)==-1) {
    //    init_messages->e.f("FATAL: failed to initialize vblank monitor: %s\n",VBlankMonitorGetError(*vblank_monitor_ptr));
    //    return false;
    //}

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

static bool LoadInitialDiscImages(
    std::unique_ptr<DiscImage> *init_disc_images,
    const Options &options,
    Messages *init_messages)
{
    for(int i=0;i<NUM_DRIVES;++i) {
        if(options.discs[i].empty()) {
            continue;
        }

        if(PathIsFolderOnDisk(options.discs[i])) {
            init_disc_images[i]=LoadDiscImageFrom65LinkFolder(options.discs[i],init_messages);
        } else {
            init_disc_images[i]=MemoryDiscImage::LoadFromFile(options.discs[i],init_messages);
        }

        if(!init_disc_images[i]) {
            return false;
        }
    }

    return true;
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

#if SYSTEM_OSX
static void SaveKeyWindowSettings() {
    if(void *key_nswindow=GetKeyWindow()) {
        for(size_t i=0;i<BeebWindows::GetNumWindows();++i) {
            BeebWindow *window=BeebWindows::GetWindowByIndex(i);

            void *window_nswindow=window->GetNSWindow();
            if(window_nswindow==key_nswindow) {
                window->SaveSettings();
                break;
            }
        }
    }
}
#endif

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

static bool main2(int argc,char *argv[],const std::shared_ptr<MessageList> &init_message_list) {
    Messages init_messages(init_message_list);

#if RMT_ENABLED
    {
        rmtError x=rmt_CreateGlobalInstance(&g_remotery);
        if(x==RMT_ERROR_NONE) {
            SetSetCurrentThreadNameCallback(&SetRmtThreadName,nullptr);
        } else {
            init_messages.w.f("Failed to initialise Remotery\n");
        }
    }
#endif

    Options options;
    if(!InitializeOptions(&options,&init_messages)) {
        return false;
    }

    if(!DoCommandLineOptions(&options,argc,argv,&init_messages)) {
        if(options.help) {
#if SYSTEM_WINDOWS
            if(!GetConsoleWindow()) {
                // Probably a GUI app build, so pop up the message box.
                FailureMessageBox("Command line help",init_message_list,SIZE_MAX);
                return true;
            }
#endif

            return true;
        }

        return false;
    }

    if(!InitLogs(options,&init_messages)) {
        return false;
    }

    init_messages.i.f("%s\n",PRODUCT_NAME);

    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    if(!InitSystem(&audio_device,&audio_spec,options,&init_messages)) {
        return false;
    }

    auto &&vblank_handler=std::make_unique<b2VBlankHandler>();
    std::unique_ptr<VBlankMonitor> vblank_monitor=CreateVBlankMonitor(vblank_handler.get(),
                                                                      options.ignore_vblank,
                                                                      &init_messages);
    if(!vblank_monitor) {
        init_messages.e.f("Failed to initialise vblank monitor.\n");
        return false;
    }

#if SYSTEM_WINDOWS
    if(!InitMF(&init_messages)) {
        return false;
    }
#endif

#if HTTP_SERVER
    std::unique_ptr<HTTPServer> http_server=CreateHTTPServer();
    if(!http_server->Start(0xbbcb)) {
        init_messages.w.f("Failed to start HTTP server.\n");
        // but carry on... it's not fatal.
    }

    auto http_handler=CreateHTTPMethodsHandler();
    http_server->SetHandler(http_handler.get());
#endif

#if HAVE_FFMPEG
    if(!InitFFmpeg(&init_messages)) {
        return false;
    }
#endif

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
    if(!LoadDiscDriveSamples(&init_messages)) {
        init_messages.e.f("Failed to initialise disc drive samples.\n");
        return false;
    }
#endif

    InitDefaultBeebConfigs();

    std::unique_ptr<DiscImage> init_disc_images[NUM_DRIVES];
    if(!LoadInitialDiscImages(init_disc_images,options,&init_messages)) {
        return false;
    }

    {
        if(!Timeline::Init()) {
            init_messages.e.f(
                "FATAL: failed to initialize timeline.\n");
            return false;
        }

        if(!BeebWindows::Init()) {
            init_messages.e.f(
                "FATAL: failed to initialize window manager.\n");
            return false;
        }

        SDL_PauseAudioDevice(audio_device,0);

        if(!LoadGlobalConfig(&init_messages)) {
            return false;
        }

        BeebLoadedConfig default_loaded_config;
        if(!BeebWindows::LoadConfigByName(&default_loaded_config,BeebWindows::GetDefaultConfigName(),&init_messages)) {
            return false;
        }

        BeebWindowInitArguments ia;
        {
            ia.render_driver_index=options.render_driver_index;
            ia.pixel_format=GetPixelFormatForRenderer(ia.render_driver_index);
            ASSERT(ia.pixel_format!=SDL_PIXELFORMAT_UNKNOWN);
            ia.sound_device=audio_device;
            ia.sound_spec=audio_spec;
            ia.default_config=default_loaded_config;
            ia.name="b2";
            ia.preinit_message_list=init_message_list;

#if SYSTEM_OSX
            ia.frame_name="b2Frame";
#else
            ia.placement_data=BeebWindows::GetLastWindowPlacementData();
#endif

            ia.reset_windows=options.reset_windows;
        }

        {
            BeebWindow *init_beeb_window=BeebWindows::CreateBeebWindow(ia);

            if(!init_beeb_window) {
                init_messages.e.f(
                    "FATAL: failed to open initial window.\n");
                return false;
            }

            // 
            // Or does this want to be part of the window init
            // arguments??? That becomes a bit inconvenient, though,
            // because unique_ptr is moveable and not copyable...
            {
                std::shared_ptr<BeebThread> thread=init_beeb_window->GetBeebThread();

                for(int i=0;i<NUM_DRIVES;++i) {
                    if(!!init_disc_images[i]) {
                        thread->Send(std::make_unique<BeebThread::LoadDiscMessage>(i,std::move(init_disc_images[i]),true));
                    }
                }

                if(options.boot) {
                    auto message=std::make_unique<BeebThread::HardResetMessage>();

                    message->boot=true;

                    thread->Send(std::move(message));
                }
            }
        }

        // not needed any more.
        init_message_list->ClearMessages();

        while(BeebWindows::GetNumWindows()>0) {
            SDL_Event event;

            {
                rmt_ScopedCPUSample(SDL_WaitEvent,0);
                if(!SDL_WaitEvent(&event)) {
                    goto done;
                }
            }

            if(event.type==SDL_QUIT) {
#if SYSTEM_OSX
                // On OS X, quit just does a quit, and there are no
                // window-specific messages sent to indicate that it's
                // happening. So jump through a few hoops in order
                // that the settings from the key window (if there is
                // one) are saved.
                //
                // This is a bit of a hack.
                SaveKeyWindowSettings();
#endif
                goto done;
            }

            switch(event.type) {
            case SDL_WINDOWEVENT:
                {
                    rmt_ScopedCPUSample(SDL_WINDOWEVENT,0);
                    BeebWindows::HandleSDLWindowEvent(event.window);
                }
                break;

            case SDL_MOUSEMOTION:
                {
                    rmt_ScopedCPUSample(SDL_MOUSEMOTION,0);
                    BeebWindows::HandleSDLMouseMotionEvent(event.motion);
                }
                break;

            case SDL_TEXTINPUT:
                {
                    rmt_ScopedCPUSample(SDL_TEXTINPUT,0);
                    BeebWindows::HandleSDLTextInput(event.text.windowID,event.text.text);
                }
                break;

            case SDL_KEYUP:
            case SDL_KEYDOWN:
                {
                    rmt_ScopedCPUSample(SDL_KEYxx,0);
                    BeebWindows::HandleSDLKeyEvent(event.key);
                }
                break;

            case SDL_MOUSEWHEEL:
                {
                    rmt_ScopedCPUSample(SDL_MOUSEWHEEL,0);
                    BeebWindows::SetSDLMouseWheelState(event.wheel.windowID,event.wheel.x,event.wheel.y);
                }
                break;

            default:
                {
                    if(event.type>=g_first_event_type&&event.type<g_first_event_type+SDLEventType_Count) {
                        switch((SDLEventType)(event.type-g_first_event_type)) {
                        case SDLEventType_VBlank:
                            {
                                rmt_ScopedCPUSample(SDLEventType_VBlank,0);
                                if(auto dd=(b2VBlankHandler::Display *)vblank_monitor->GetDisplayDataForDisplayID((uint32_t)event.user.code)) {
                                    uint64_t ticks=GetCurrentTickCount();

                                    {
                                        BeebWindows::HandleVBlank(vblank_monitor.get(),dd,ticks);
                                    }

                                    dd->message_pending=false;
                                }
                            }
                            break;

                        case SDLEventType_UpdateWindowTitle:
                            {
                                rmt_ScopedCPUSample(SDLEventType_UpdateWindowTitle,0);
                                BeebWindows::UpdateWindowTitles();
                            }
                            break;

                        case SDLEventType_NewWindow:
                            {
                                rmt_ScopedCPUSample(SDLEventType_NewWindow,0);
                                auto init_arguments=(BeebWindowInitArguments *)event.user.data1;

                                BeebWindows::CreateBeebWindow(*init_arguments);

                                delete init_arguments;
                                init_arguments=nullptr;
                            }
                            break;

                        case SDLEventType_Function:
                            {
                                rmt_ScopedCPUSample(SDLEventType_Function,0);
                                auto fun=(std::function<void()> *)event.user.data1;

                                (*fun)();

                                delete fun;
                                fun=nullptr;
                            }
                            break;

                        case SDLEventType_Count:
                            // only here avoid incomplete switch warning.
                            ASSERT(false);
                            break;
                        }
                    }
                }
                break;
            }
        }

    done:;
        ;//<-- fix Visual Studio autoformat bug

        SaveGlobalConfig(&init_messages);

        SDL_PauseAudioDevice(audio_device,1);

        BeebWindows::Shutdown();
        Timeline::Shutdown();

#if HTTP_SERVER
        http_server=nullptr;
        http_handler=nullptr;
#endif
    }

    vblank_monitor=nullptr;
    vblank_handler=nullptr;

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
