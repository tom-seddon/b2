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
#include "LoadMemoryDiscImage.h"
#include <beeb/MemoryDiscImage.h>
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
#include <beeb/DirectDiscImage.h>
#include "discs.h"
#if SYSTEM_OSX
#include <IOKit/hid/IOHIDLib.h>
#elif SYSTEM_LINUX
#include <glib-2.0/glib.h>
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#include <gtk/gtk.h>
G_GNUC_END_IGNORE_DEPRECATIONS
#endif
#include "BeebLinkHTTPHandler.h"
#include "joysticks.h"

#include <shared/enum_decl.h>
#include "b2.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "b2.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char PRODUCT_NAME[] = "b2 - BBC Micro B/B+/Master 128 emulator - " STRINGIZE(RELEASE_NAME);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
#if BUILD_TYPE_Final || BUILD_TYPE_RelWithDebInfo

// Seems like there's no good way to do this from CMake:
// http://stackoverflow.com/questions/8054734/
//
// But this here is simple enough.
//
// (There's no need to do anything about the entry point; the SDL2main
// stuff already ensures that the program can work either way.)
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

#endif
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const int DEFAULT_AUDIO_HZ = 48000;

static const int DEFAULT_AUDIO_BUFFER_SIZE = 1024;

static const int HTTP_SERVER_PORT = 0xbbcb;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Enabled by the -v/--verbose command line option, or if this is a
// debug build.
LOG_DEFINE(OUTPUT, "", &log_printer_stdout_and_debugger, false);
LOG_DEFINE(OUTPUTND, "", &log_printer_stdout, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static Uint32 g_first_event_type;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool g_option_vsync = true;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
// Assume not detchable, if the process has a console on startup. Either it was
// built that way, or launched that way.
static bool g_can_detach_from_windows_console;
static bool g_got_console = false;
#endif

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

#if SYSTEM_WINDOWS
static void ReopenOutputHandles(const char *dest) {
    fclose(stdout);
    fclose(stderr);

    freopen(dest, "w", stdout);
    freopen(dest, "w", stderr);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
static void InitWindowsConsoleStuff() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE) {
        g_can_detach_from_windows_console = false;
        g_got_console = true;
    } else {
        g_can_detach_from_windows_console = true;
        g_got_console = false;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
bool CanDetachFromWindowsConsole() {
    return g_can_detach_from_windows_console;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
bool HasWindowsConsole() {
    return g_got_console;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
static BOOL WINAPI ConsoleControlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        {
            SDL_Event event = {};
            event.type = SDL_QUIT;

            SDL_PushEvent(&event);

            // Once the ctrl handler is running, the process will inevitably
            // finish, one way or the other. Give it 5 seconds to hopefully shut
            // down from the main thread before then.
            Sleep(5000);
        }
        return TRUE;
    }
    return FALSE;
}
#endif

#if SYSTEM_WINDOWS
void AllocWindowsConsole() {
    if (!g_got_console) {
        if (AllocConsole()) {
            g_got_console = true;

            ReopenOutputHandles("CON");

            // The control handler needs adding for each AllocConsole call.
            SetConsoleCtrlHandler(&ConsoleControlHandler, TRUE);
        }
    }

    printf("stdout redirected to console window\n");
    fflush(stdout);

    fprintf(stderr, "stderr redirected to console window\n");
    fflush(stderr);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
void FreeWindowsConsole() {
    ReopenOutputHandles("NUL");

    FreeConsole();
    g_got_console = false;

    //LOGF(OUTPUT, "%s: HasWindowsConsole=%s\n", __FUNCTION__, BOOL_STR(HasWindowsConsole()));
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class b2VBlankHandler : public VBlankMonitor::Handler {
  public:
    struct VBlank {
        uint64_t ticks;
        int event;
    };

    struct Display {
        Mutex mutex;
        VBlank vblanks[NUM_VBLANK_RECORDS] = {};
        size_t vblank_index = 0;
        bool message_pending = false;
        uint64_t num_thread_vblanks = 0;
        uint64_t num_messages_sent = 0;
    };

    b2VBlankHandler();
    void *AllocateDisplayData(uint32_t display_id) override;
    void FreeDisplayData(uint32_t display_id, void *data) override;
    void ThreadVBlank(uint32_t display_id, void *data) override;

  protected:
  private:
    Mutex m_mutex;
    std::map<uint32_t, std::shared_ptr<Display>> m_data_by_display_id;

    friend std::vector<DisplayData> GetDisplaysData();
};

static std::unique_ptr<b2VBlankHandler> g_vblank_handler;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<DisplayData> GetDisplaysData() {
    std::vector<DisplayData> datas;
    if (!!g_vblank_handler) {
        std::map<uint32_t, std::shared_ptr<b2VBlankHandler::Display>> data_by_display_id;
        {
            LockGuard<Mutex> lock(g_vblank_handler->m_mutex);
            data_by_display_id = g_vblank_handler->m_data_by_display_id;
        }

        datas.reserve(data_by_display_id.size());
        for (const auto &display_id_and_data : data_by_display_id) {
            DisplayData dd;

            dd.display_id = display_id_and_data.first;

            LockGuard<Mutex> lock(display_id_and_data.second->mutex);
            dd.num_messages_sent = display_id_and_data.second->num_messages_sent;
            dd.num_thread_vblanks = display_id_and_data.second->num_thread_vblanks;

            datas.push_back(std::move(dd));
        }
    }

    return datas;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

b2VBlankHandler::b2VBlankHandler() {
    MUTEX_SET_NAME(m_mutex, "b2VBlankHandler");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *b2VBlankHandler::AllocateDisplayData(uint32_t display_id) {
    auto &&display = std::make_shared<Display>();

    MUTEX_SET_NAME(display->mutex, strprintf("DisplayData for display %" PRIu32, display_id));

    LockGuard<Mutex> lock(m_mutex);

    m_data_by_display_id[display_id] = display;

    return display.get();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void b2VBlankHandler::FreeDisplayData(uint32_t display_id, void *data) {
    (void)data;
    (void)display_id;

    LockGuard<Mutex> lock(m_mutex);

    ASSERT(m_data_by_display_id.count(display_id) > 0);
    ASSERT(m_data_by_display_id[display_id].get() == data);
    m_data_by_display_id.erase(display_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void b2VBlankHandler::ThreadVBlank(uint32_t display_id, void *data) {
    auto display = (Display *)data;

    ASSERT(display->vblank_index < NUM_VBLANK_RECORDS);
    VBlank *vblank = &display->vblanks[display->vblank_index];
    ++display->vblank_index;
    display->vblank_index %= NUM_VBLANK_RECORDS;

    ++display->num_thread_vblanks;

    vblank->ticks = GetCurrentTickCount();

    bool message_pending;
    {
        LockGuard<Mutex> lock(display->mutex);

        message_pending = display->message_pending;
        vblank->event = !message_pending;

        if (vblank->event) {
            SDL_Event event = {};
            event.user.type = g_first_event_type + SDLEventType_VBlank;
            event.user.code = (Sint32)display_id;

            SDL_PushEvent(&event);

            display->message_pending = true;
            ++display->num_messages_sent;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct FillAudioBufferData {
    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec spec{};
    std::vector<float> mix_buffer;

    uint64_t first_call_ticks = 0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ThreadFillAudioBuffer(void *userdata, uint8_t *stream, int len) {
    rmt_BeginCPUSample(ThreadFillAudioBuffer, 0);

    auto data = (FillAudioBufferData *)userdata;

    uint64_t now_ticks = GetCurrentTickCount();
    if (data->first_call_ticks == 0) {
        data->first_call_ticks = now_ticks;
    }

    ASSERT(len >= 0);
    ASSERT((size_t)len % 4 == 0);
    size_t num_samples = (size_t)len / 4;
    ASSERT(num_samples <= data->spec.samples);

    //float us_per_sample=1e6f/data->spec.freq;

    if (num_samples > data->mix_buffer.size()) {
        data->mix_buffer.resize(num_samples);
    }

    memset(data->mix_buffer.data(), 0, num_samples * sizeof(float));

    //_mm_lfence();

    BeebWindows::ThreadFillAudioBuffer(data->device, data->mix_buffer.data(), num_samples);

    memcpy(stream, data->mix_buffer.data(), (size_t)len);

    rmt_EndCPUSample();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void PushUpdateWindowTitleEvent(void) {
    SDL_Event event = {};
    event.user.type = g_first_event_type + SDLEventType_UpdateWindowTitle;

    SDL_PushEvent(&event);
}

static Uint32 UpdateWindowTitle(Uint32 interval, void *param) {
    (void)param;

    PushUpdateWindowTitleEvent();

    return interval;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PushNewWindowMessage(BeebWindowInitArguments init_arguments_) {
    SDL_Event event = {};

    event.user.type = g_first_event_type + SDLEventType_NewWindow;

    // This relies on the message loop receiving it, so it can delete
    // it. It's probably possible for an SDL_QUIT to end up ahead of
    // it in the queue, meaning that the object could leak.
    event.user.data1 = new BeebWindowInitArguments(std::move(init_arguments_));

    SDL_PushEvent(&event);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PushFunctionMessage(std::function<void()> fun) {
    SDL_Event event = {};

    event.user.type = g_first_event_type + SDLEventType_Function;

    event.user.data1 = new std::function<void()>(std::move(fun));

    SDL_PushEvent(&event);
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
static IOHIDManagerRef g_hid_manager = nullptr;

extern "C" int SDL_SendKeyboardKey(Uint8 state, SDL_Scancode scancode); //sorry

#endif

#if SYSTEM_OSX
static void HIDCallback(void *context, IOReturn result, void *sender, IOHIDValueRef value) {
    (void)result, (void)sender;

    if (context != g_hid_manager) {
        // An old callback, ignore it (related to bug 2157 below).
        return;
    }

    IOHIDElementRef elem = IOHIDValueGetElement(value);
    if (IOHIDElementGetUsagePage(elem) != kHIDPage_KeyboardOrKeypad) {
        return;
    }

    if (IOHIDElementGetUsage(elem) != kHIDUsage_KeyboardCapsLock) {
        return;
    }

    // This seems to interact correctly with the default SDL_SCANCODE_CAPSLOCK
    // procossing, in that the true caps lock state is reported.
    CFIndex pressed = IOHIDValueGetIntegerValue(value);
    SDL_SendKeyboardKey(pressed ? SDL_PRESSED : SDL_RELEASED, SDL_SCANCODE_CAPSLOCK);
}
#endif

#if SYSTEM_OSX
static CFDictionaryRef CreateHIDDeviceMatchingDictionary(UInt32 usagePage, UInt32 usage) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (dict) {
        CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usagePage);
        if (number) {
            CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsagePageKey), number);
            CFRelease(number);
            number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
            if (number) {
                CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsageKey), number);
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
    if (!g_hid_manager) {
        return;
    }

    // Releasing here causes a crash on Mac OS X 10.10 and earlier, so just leak it for now. See bug 2157 for details.
    IOHIDManagerUnscheduleFromRunLoop(g_hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerRegisterInputValueCallback(g_hid_manager, nullptr, nullptr);
    IOHIDManagerClose(g_hid_manager, 0);

    CFRelease(g_hid_manager);

    g_hid_manager = nullptr;
}
#endif

#if SYSTEM_OSX
static void InitHIDCallback(Messages *msg) {
    IOReturn ior;
    bool good = false;
    CFDictionaryRef keyboard = nullptr;
    CFDictionaryRef keypad = nullptr;
    CFArrayRef matches = nullptr;

    g_hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!g_hid_manager) {
        msg->e.f("InitHIDCallback: IOHIDManagerCreate failed\n");
        goto cleanup;
    }

    keyboard = CreateHIDDeviceMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard);
    if (!keyboard) {
        msg->e.f("InitHIDCallback: CreateHIDDeviceMatchingDictionary failed (kHIDUsage_GD_Keyboard)\n");
        goto cleanup;
    }

    keypad = CreateHIDDeviceMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Keypad);
    if (!keypad) {
        msg->e.f("InitHIDCallback: CreateHIDDeviceMatchingDictionary failed (kHIDUsage_GD_Keypad)\n");
        goto cleanup;
    }

    {
        CFDictionaryRef matches_list[] = {keyboard, keypad};
        matches = CFArrayCreate(kCFAllocatorDefault, (const void **)matches_list, sizeof matches_list / sizeof matches_list[0], nullptr);
        if (!matches) {
            msg->i.f("InitHIDCallback: CFArrayCreate failed\n");
            goto cleanup;
        }
    }

    IOHIDManagerSetDeviceMatchingMultiple(g_hid_manager, matches);
    IOHIDManagerRegisterInputValueCallback(g_hid_manager, &HIDCallback, g_hid_manager);
    IOHIDManagerScheduleWithRunLoop(g_hid_manager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    ior = IOHIDManagerOpen(g_hid_manager, kIOHIDOptionsTypeNone);
    if (ior != kIOReturnSuccess) {
        if (ior == kIOReturnNotPermitted) {
            msg->i.f("b2 has not been granted permission to monitor keyboard input.\n");
            msg->i.f("The emulated BBC will not respond properly to the Caps Lock key!\n");
            msg->i.f("For more info, please see https://github.com/tom-seddon/b2/blob/master/doc/Installing-on-OSX.md\n");
        } else {
            msg->i.f("InitHIDCallback: IOHIDManagerOpen returned: %" PRIu32 " (0x%" PRIx32 ")\n", (uint32_t)ior, (uint32_t)ior);
        }
        goto cleanup;
    }

    good = true;
    msg->i.f("InitHIDCallback: installed\n");

cleanup:
    if (!good) {
        QuitHIDCallback();
    }

    if (matches) {
        CFRelease(matches);
    }
    if (keypad) {
        CFRelease(keypad);
    }
    if (keyboard) {
        CFRelease(keyboard);
    }
    return;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    bool verbose = false;
    std::string discs[NUM_DRIVES];
    bool direct_disc[NUM_DRIVES] = {};
    int audio_hz = DEFAULT_AUDIO_HZ;
    int audio_buffer_size = DEFAULT_AUDIO_BUFFER_SIZE;
    bool boot = false;
    bool help = false;
    std::vector<std::string> enable_logs, disable_logs;
    bool reset_windows = false;
    bool vsync = false;
    bool timer = false;
    std::string config_name;
    bool limit_speed = true;
#if RMT_ENABLED
    bool remotery = false;
    bool remotery_thread_sampler = false;
#endif
    std::string override_config_folder = GetConfigPath("");
    bool override_config_folder_specified = false;

    // File association mode is what you get when b2 has been set up as the
    // program to use when double clicking on disk images.
    //
    // Just one argument is supplied: the disk image.
    bool file_association_mode = false;
    std::string file_association_path;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_OSX
static bool IsPSNArgument(const char *arg) {
    if (arg[0] != '-' || arg[1] != 'p' || arg[2] != 's' || arg[3] != 'n' || arg[4] != '_' || !isdigit(arg[5]) || arg[6] != '_') {
        return false;
    }

    for (int i = 7; arg[i] != 0; ++i) {
        if (!isdigit(arg[i])) {
            return false;
        }
    }

    return true;
}
#endif

#if SYSTEM_OSX
// http://stackoverflow.com/questions/10242115/
static void RemovePSNArguments(std::vector<const char *> *argv) {
    auto &&it = argv->begin();
    while (it != argv->end()) {
        if (IsPSNArgument(*it)) {
            it = argv->erase(it);
        } else {
            ++it;
        }
    }
}
#endif

static bool ParseCommandLineOptions(
    Options *options,
    int argc, char *argv[],
    Messages *init_messages) {

    // Detect the File Explorer double-click case on Windows (also acts as a
    // convenient command-line shortcut).
    if (argc == 2 && PathIsFileOnDisk(argv[1], nullptr, nullptr)) {
        options->file_association_mode = true;
        options->file_association_path = argv[1];

        return true;
    }

    CommandLineParser p(PRODUCT_NAME);

    p.SetLogs(&init_messages->i, &init_messages->e);

    for (int drive = 0; drive < NUM_DRIVES; ++drive) {
        p.AddOption((char)('0' + drive)).Arg(&options->discs[drive]).Meta("FILE").Help("load in-memory disc image from FILE into drive " + std::to_string(drive));

        p.AddOption(0, strprintf("%d-direct", drive)).SetIfPresent(&options->direct_disc[drive]).Help(strprintf("if -%d specified as well, use disc image rather than in-memory disc image", drive));
    }

    p.AddOption('b', "boot").SetIfPresent(&options->boot).Help("attempt to auto-boot disc");
    p.AddOption('c', "config").Arg(&options->config_name).Meta("CONFIG").Help("start emulator with configuration CONFIG");
    p.AddOption("no-limit-speed").ResetIfPresent(&options->limit_speed).Help("start emulator with speed limiting disabled");

    p.AddOption("hz").Arg(&options->audio_hz).Meta("HZ").Help("set sound output frequency to HZ").ShowDefault();
    p.AddOption("buffer").Arg(&options->audio_buffer_size).Meta("SAMPLES").Help("set audio buffer size, in samples (must be a power of two <32768: 512, 1024, 2048, etc.)").ShowDefault();

    std::set<std::string> tags;
    for (const LogWithTag *tagged_log = LogWithTag::GetFirst(); tagged_log; tagged_log = tagged_log->GetNext()) {
        tags.insert(tagged_log->tag);
    }

    std::string list;
    for (const std::string &tag : tags) {
        if (!list.empty()) {
            list += " ";
        }

        list += tag;
    }

    if (!list.empty()) {
        p.AddOption('e', "enable-log").AddArgToList(&options->enable_logs).Meta("LOG").Help("enable additional log LOG. One of: " + list);
        p.AddOption('d', "disable-log").AddArgToList(&options->disable_logs).Meta("LOG").Help("disable additional log LOG. One of: " + list);
    }

    p.AddOption('v', "verbose").SetIfPresent(&options->verbose).Help("be extra verbose");

    p.AddOption(0, "reset-windows").SetIfPresent(&options->reset_windows).Help("reset window position and dock data");

    p.AddOption("vsync").SetIfPresent(&options->vsync).Help("use vsync for timing");
    p.AddOption("timer").SetIfPresent(&options->timer).Help("use timer for timing");

#if RMT_ENABLED
    p.AddOption("remotery").SetIfPresent(&options->remotery).Help("activate Remotery. See: https://github.com/Celtoys/Remotery");
    p.AddOption("remotery-thread-sampler").SetIfPresent(&options->remotery_thread_sampler).Help("activate Remotery thread sampler");
#endif

    p.AddOption("config-folder").Arg(&options->override_config_folder).SetIfPresent(&options->override_config_folder_specified).Help("specify folder for config files (will be created if non-existent)").ShowDefault();

    p.AddHelpOption(&options->help);

    std::vector<const char *> args(argv, argv + argc);

#if SYSTEM_OSX
    RemovePSNArguments(&args);
#endif

    if (!p.Parse((int)args.size(), args.data())) {
        return false;
    }

    if (options->audio_buffer_size <= 0 ||
        options->audio_buffer_size >= 65535 ||
        (options->audio_buffer_size & (options->audio_buffer_size - 1)) != 0) {
        init_messages->e.f("invalid audio buffer size: %d\n", options->audio_buffer_size);
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
    Messages *init_messages) {
    (void)options;

    SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG_FILE, GetAssetPath("gamecontrollerdb.txt").c_str());

    // Initialise SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        init_messages->e.f("FATAL: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_EnableScreenSaver();

    // Allocate user events
    g_first_event_type = SDL_RegisterEvents(SDLEventType_Count);

    //
    SDL_AddTimer(1000, &UpdateWindowTitle, NULL);
    PushUpdateWindowTitleEvent();

    SDL_StartTextInput();

#if SYSTEM_OSX
    InitHIDCallback(init_messages);
#endif

    // Start audio
    {
        SDL_AudioSpec spec = {};

        spec.freq = options.audio_hz;
        spec.format = AUDIO_FORMAT;
        spec.channels = AUDIO_NUM_CHANNELS;
        spec.callback = &ThreadFillAudioBuffer;
        spec.userdata = &g_fill_audio_buffer_data;
        spec.samples = (Uint16)options.audio_buffer_size;

        *device_id = SDL_OpenAudioDevice(
            nullptr,
            0, // playback, not capture
            &spec,
            got_spec,
            SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
        if (*device_id == 0) {
            init_messages->i.f("failed to initialize audio: %s\n", SDL_GetError());
            return false;
        }

        g_fill_audio_buffer_data.spec = *got_spec;
        g_fill_audio_buffer_data.device = *device_id;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void CheckAssetPath(const std::string &path_) {
    (void)path_;

#if ASSERT_ENABLED

#if SYSTEM_OSX

    std::string path = path_;

    for (;;) {
        std::string dot = "/./";
        std::string::size_type pos = path.find(dot);
        if (pos == std::string::npos) {
            break;
        }

        path = path.substr(0, pos) + "/" + path.substr(pos + dot.size());
    }

    for (;;) {
        std::string dotdot = "/../";
        std::string::size_type pos = path.find(dotdot);
        if (pos == std::string::npos) {
            break;
        }

        ASSERT(pos > 0);
        std::string::size_type prev_pos = path.find_last_of('/', pos - 1);
        path = path.substr(0, prev_pos) + "/" + path.substr(pos + dotdot.size());
    }

    char real_path[PATH_MAX];
    ASSERT(realpath(path.c_str(), real_path));
    ASSERT(path == real_path);

#elif SYSTEM_WINDOWS

    // TODO...

#endif

#endif
}

static void CheckAssetPaths() {
    for (size_t i = 0; BEEB_ROMS[i]; ++i) {
        CheckAssetPath(BEEB_ROMS[i]->GetAssetPath());
    }

    for (size_t i = 0; i < NUM_BLANK_DFS_DISCS; ++i) {
        CheckAssetPath(BLANK_DFS_DISCS[i].GetAssetPath());
    }

    for (size_t i = 0; i < NUM_BLANK_ADFS_DISCS; ++i) {
        CheckAssetPath(BLANK_ADFS_DISCS[i].GetAssetPath());
    }

    CheckAssetPath(BEEB_ROM_MASTER_TURBO_PARASITE.GetAssetPath());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void LoadDiscDriveSampleFailed(bool *good, Messages *init_messages, const std::string &path, const char *what) {
    init_messages->e.f("failed to load disc sound sample: %s\n", path.c_str());
    init_messages->i.f("(%s failed: %s)\n", what, SDL_GetError());
    *good = false;
}

static void LoadDiscDriveSound(bool *good, DiscDriveType type, DiscDriveSound sound, const char *fname, Messages *init_messages) {
    if (!*good) {
        return;
    }

    ASSERT(sound >= 0 && sound < DiscDriveSound_EndValue);

    std::string path = GetAssetPath("samples", fname);
    CheckAssetPath(path);

    SDL_AudioSpec spec = {};
    spec.freq = SOUND_CLOCK_HZ;
    spec.format = AUDIO_F32;
    spec.channels = 1;

    Uint8 *buf;
    Uint32 len;
    SDL_AudioSpec *loaded_spec = SDL_LoadWAV(path.c_str(), &spec, &buf, &len);
    if (!loaded_spec) {
        LoadDiscDriveSampleFailed(good, init_messages, path, "SDL_LoadWAV");
        return;
    }

    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt, loaded_spec->format, loaded_spec->channels, loaded_spec->freq, AUDIO_F32, 1, SOUND_CLOCK_HZ) < 0) {
        LoadDiscDriveSampleFailed(good, init_messages, path, "SDL_BuildAudioCVT");
        return;
    }

    std::vector<float> f32_buf;

    if (cvt.needed) {
        ASSERT(cvt.len_mult >= 0);
        ASSERT(len * (unsigned)cvt.len_mult % sizeof(float) == 0);
        f32_buf.resize(len * (unsigned)cvt.len_mult / sizeof(float));
        memcpy(f32_buf.data(), buf, len);

        cvt.len = (int)len;
        cvt.buf = (Uint8 *)f32_buf.data();
        if (SDL_ConvertAudio(&cvt) < 0) {
            LoadDiscDriveSampleFailed(good, init_messages, path, "SDL_ConvertAudio");
            return;
        }

        ASSERT(cvt.len_cvt >= 0);
        ASSERT((size_t)cvt.len_cvt % sizeof(float) == 0);
        f32_buf.resize((size_t)cvt.len_cvt / sizeof(float));
    } else {
        ASSERT(len % sizeof(float) == 0);
        f32_buf.resize(len / sizeof(float));
        memcpy(f32_buf.data(), buf, len);
    }

    BBCMicro::SetDiscDriveSound(type, sound, std::move(f32_buf));

    //init_messages->i.f("%s: %zu bytes\n",fname,sample_data[sound]->size()*sizeof *sample_data[sound]->data());

    SDL_FreeWAV(buf);
    buf = nullptr;
    len = 0;
}

static bool LoadDiscDriveSamples(Messages *init_messages) {
    bool good = true;

    LoadDiscDriveSound(&good, DiscDriveType_90mm, DiscDriveSound_Seek2ms, "35_seek_2ms.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_90mm, DiscDriveSound_Seek6ms, "35_seek_6ms.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_90mm, DiscDriveSound_Seek12ms, "35_seek_12ms.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_90mm, DiscDriveSound_Seek20ms, "35_seek_20ms.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_90mm, DiscDriveSound_SpinEmpty, "35_spin_empty.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_90mm, DiscDriveSound_SpinEnd, "35_spin_end.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_90mm, DiscDriveSound_SpinLoaded, "35_spin_loaded.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_90mm, DiscDriveSound_SpinStartEmpty, "35_spin_start_empty.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_90mm, DiscDriveSound_SpinStartLoaded, "35_spin_start_loaded.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_90mm, DiscDriveSound_Step, "35_step_1_1.wav", init_messages);

    LoadDiscDriveSound(&good, DiscDriveType_133mm, DiscDriveSound_Seek2ms, "525_seek_2ms.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_133mm, DiscDriveSound_Seek6ms, "525_seek_6ms.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_133mm, DiscDriveSound_Seek12ms, "525_seek_12ms.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_133mm, DiscDriveSound_Seek20ms, "525_seek_20ms.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_133mm, DiscDriveSound_SpinEmpty, "525_spin_empty.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_133mm, DiscDriveSound_SpinEnd, "525_spin_end.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_133mm, DiscDriveSound_SpinLoaded, "525_spin_loaded.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_133mm, DiscDriveSound_SpinStartEmpty, "525_spin_start_empty.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_133mm, DiscDriveSound_SpinStartLoaded, "525_spin_start_loaded.wav", init_messages);
    LoadDiscDriveSound(&good, DiscDriveType_133mm, DiscDriveSound_Step, "525_step_1_1.wav", init_messages);

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool InitLogs(const std::vector<std::string> &names_list,
                     void (Log::*mfn)(),
                     Messages *init_messages) {
    for (const std::string &tag : names_list) {
        std::vector<Log *> logs;
        for (const LogWithTag *tagged_log = LogWithTag::GetFirst(); tagged_log; tagged_log = tagged_log->GetNext()) {
            if (tagged_log->tag == tag) {
                logs.push_back(tagged_log->log);
            }
        }

        if (logs.empty()) {
            init_messages->e.f("Unknown log: %s\n", tag.c_str());
            return false;
        }

        for (Log *log : logs) {
            (log->*mfn)();
        }
    }

    return true;
}

static bool InitLogs(const Options &options, Messages *init_messages) {
#if !BUILD_TYPE_Debug    //<---note
    if (options.verbose) //<---note
#endif                   //<---note
    {
        LOG(OUTPUT).Enable();
        LOG(OUTPUTND).Enable();
    }

    // the Log API really isn't very good for this :(

    if (!InitLogs(options.enable_logs, &Log::Enable, init_messages)) {
        return false;
    }

    if (!InitLogs(options.disable_logs, &Log::Disable, init_messages)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_OSX
static void SaveKeyWindowSettings() {
    if (void *key_nswindow = GetKeyWindow()) {
        for (size_t i = 0; i < BeebWindows::GetNumWindows(); ++i) {
            BeebWindow *window = BeebWindows::GetWindowByIndex(i);

            void *window_nswindow = window->GetNSWindow();
            if (window_nswindow == key_nswindow) {
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
static void SetRmtThreadName(const char *name, void *context) {
    (void)context;

    rmt_SetCurrentThreadName(name);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void PushLaunchEvent(uint32_t sdl_window_id, std::unique_ptr<BeebWindowLaunchArguments> arguments) {
    SDL_Event event{};

    event.type = g_first_event_type + SDLEventType_Launch;
    event.user.windowID = sdl_window_id;
    event.user.data1 = arguments.release();

    SDL_PushEvent(&event);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool BootDiskInExistingProcess(const std::string &path, Messages *messages) {
    //auto client_message_list = std::make_shared<MessageList>();
    //Messages client_messages(client_message_list);

    std::unique_ptr<HTTPClient> client = CreateHTTPClient();
    client->SetMessages(messages);

    HTTPRequest request;
    request.url = strprintf("http://127.0.0.1:%d/launch", HTTP_SERVER_PORT);
    request.method = "POST";
    request.AddQueryParameter("path", path);

    HTTPResponse response;
    int status = client->SendRequest(request, &response);
    if (status == 200) {
        // handled.
        return true;
    } else {
        // not handled.
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::unique_ptr<HTTPServer> g_http_server;
static std::shared_ptr<HTTPHandler> g_http_handler;

void StartHTTPServer(Messages *messages) {
    if (GetHTTPServerListenPort() != 0) {
        return;
    }

    g_http_server = CreateHTTPServer();
    if (!g_http_server->Start(HTTP_SERVER_PORT, messages)) {
        g_http_server.reset();
        messages->e.f("Failed to start HTTP server.\n");
        return;
    }

    g_http_handler = CreateHTTPMethodsHandler();
    g_http_server->SetHandler(g_http_handler);
}

void StopHTTPServer() {
    g_http_handler.reset();
    g_http_server.reset();
}

int GetHTTPServerListenPort() {
    if (!g_http_server) {
        return 0;
    } else {
        return HTTP_SERVER_PORT;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BeebWindow *FindBeebKeyboardFocusWindow() {
    SDL_Window *keyboard_focus_window = SDL_GetKeyboardFocus();
    if (!keyboard_focus_window) {
        return nullptr;
    }

    BeebWindow *beeb_window = BeebWindows::FindBeebWindowForSDLWindow(keyboard_focus_window);
    if (!beeb_window) {
        return nullptr;
    }

    return beeb_window;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::shared_ptr<MessageList> GetMRUMessageList() {
    if (BeebWindow *beeb_window = BeebWindows::FindMRUBeebWindow()) {
        return beeb_window->GetMessageList();
    } else {
        return nullptr;
    }
}

static bool main2(int argc, char *argv[], const std::shared_ptr<MessageList> &init_message_list) {
    Messages init_messages(init_message_list);

    init_messages.i.f("%s\n", PRODUCT_NAME);

    CheckAssetPaths();

    // https://curl.haxx.se/libcurl/c/curl_global_init.html
    {
        CURLcode r = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (r != 0) {
            init_messages.e.f("Failed to initialise libcurl: %s\n", curl_easy_strerror(r));
            return false;
        }
    }

    Options options;

    if (!ParseCommandLineOptions(&options, argc, argv, &init_messages)) {
        if (options.help) {
#if SYSTEM_WINDOWS
            if (!GetConsoleWindow()) {
                // Probably a GUI app build, so pop up the message box.
                FailureMessageBox("Command line help", init_message_list, SIZE_MAX);
                return true;
            }
#endif

            return true;
        }

        return false;
    }

    if (options.override_config_folder_specified) {
        SetConfigFolder(options.override_config_folder);
    }

    StartHTTPServer(&init_messages);

    if (options.file_association_mode) {
        if (GetHTTPServerListenPort() != 0) {
            // The HTTP server started, so there's definitely no other instance
            // running that could handle the request.
        } else {
            if (BootDiskInExistingProcess(options.file_association_path, &init_messages)) {
                return true;
            }
        }

        // Fake a -0 PATH -boot.
        options.boot = true;
        options.discs[0] = options.file_association_path;
    }

    if (!InitLogs(options, &init_messages)) {
        return false;
    }

    BBCMicro::PrintInfo(&LOG(OUTPUT));

#if RMT_ENABLED
    if (options.remotery) {
        rmtSettings *settings = rmt_Settings();

        settings->enableThreadSampler = options.remotery_thread_sampler;

        rmtError x = rmt_CreateGlobalInstance(&g_remotery);
        if (x == RMT_ERROR_NONE) {
            SetSetCurrentThreadNameCallback(&SetRmtThreadName, nullptr);
        } else {
            init_messages.w.f("Failed to initialise Remotery\n");
        }
    }
#endif

    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    if (!InitSystem(&audio_device, &audio_spec, options, &init_messages)) {
        return false;
    }

#if SYSTEM_LINUX
    // Need to do this after SDL_Init. See, e.g.,
    // https://discourse.libsdl.org/t/gtk2-sdl2-partial-fail/19274
    gtk_init(&argc, &argv);
#endif

#if SYSTEM_WINDOWS
    if (!InitMF(&init_messages)) {
        return false;
    }
#endif

#if HAVE_FFMPEG
    if (!InitFFmpeg(&init_messages)) {
        return false;
    }
#endif

    if (!LoadDiscDriveSamples(&init_messages)) {
        init_messages.e.f("Failed to initialise disc drive samples.\n");
        return false;
    }

    InitDefaultBeebConfigs();

    {
        //        if(!Timeline::Init()) {
        //            init_messages.e.f(
        //                "FATAL: failed to initialize timeline.\n");
        //            return false;
        //        }

        if (!BeebWindows::Init()) {
            init_messages.e.f(
                "FATAL: failed to initialize window manager.\n");
            return false;
        }

        SDL_PauseAudioDevice(audio_device, 0);

        if (!LoadGlobalConfig(&init_messages)) {
            return false;
        }

        // Ugh. This thing here is really a bit of a bodge...
        if (options.vsync) {
            g_option_vsync = true;
        } else if (options.timer) {
            g_option_vsync = false;
        }

        g_vblank_handler = std::make_unique<b2VBlankHandler>();
        init_messages.i.f("Timing method: %s\n", g_option_vsync ? "vsync" : "timer");
        std::unique_ptr<VBlankMonitor> vblank_monitor = CreateVBlankMonitor(g_vblank_handler.get(),
                                                                            !g_option_vsync,
                                                                            &init_messages);
        if (!vblank_monitor) {
            init_messages.e.f("Failed to initialise vblank monitor.\n");
            return false;
        }

        BeebLoadedConfig initial_loaded_config;
        {
            bool got_initial_loaded_config = false;

            if (!options.config_name.empty()) {
                if (BeebWindows::LoadConfigByName(&initial_loaded_config, options.config_name, &init_messages)) {
                    got_initial_loaded_config = true;
                }
            }

            if (!got_initial_loaded_config) {
                if (!BeebWindows::default_config_name.empty()) {
                    if (BeebWindows::LoadConfigByName(&initial_loaded_config, BeebWindows::default_config_name, &init_messages)) {
                        got_initial_loaded_config = true;
                    }
                }
            }

            if (!got_initial_loaded_config) {
                if (BeebLoadedConfig::Load(&initial_loaded_config, *GetDefaultBeebConfigByIndex(0), &init_messages)) {
                    got_initial_loaded_config = true;
                }
            }

            if (!got_initial_loaded_config) {
                // Ugh, ok.
                return false;
            }
        }

        BeebWindowInitArguments ia;
        {
            //            ia.render_driver_index=options.render_driver_index;
            //            ia.pixel_format=SDL_PIXELFORMAT_ARGB8888;
            //            ASSERT(ia.pixel_format!=SDL_PIXELFORMAT_UNKNOWN);
            ia.sound_device = audio_device;
            ia.sound_spec = audio_spec;
            ia.default_config = initial_loaded_config;
            ia.name = "b2";
            ia.preinit_message_list = init_message_list;

#if SYSTEM_OSX
            ia.frame_name = "b2Frame";
#else
            ia.placement_data = BeebWindows::GetLastWindowPlacementData();
#endif

            ia.reset_windows = options.reset_windows;

            for (int i = 0; i < NUM_DRIVES; ++i) {
                if (options.discs[i].empty()) {
                    continue;
                }

                if (options.direct_disc[i]) {
                    ia.init_disc_images[i] = DirectDiscImage::CreateForFile(options.discs[i], init_messages);
                } else {
                    ia.init_disc_images[i] = LoadMemoryDiscImage(options.discs[i], init_messages);
                }

                if (!ia.init_disc_images[i]) {
                    return false;
                }
            }

            ia.boot = options.boot;
            ia.limit_speed = options.limit_speed;
        }

        if (!BeebWindows::CreateBeebWindow(ia)) {
            init_messages.e.f("FATAL: failed to open initial window.\n");
            return false;
        }

        // not needed any more.
        init_message_list->ClearMessages();

        while (BeebWindows::GetNumWindows() > 0) {
            SDL_Event event;

            {
                rmt_ScopedCPUSample(SDL_WaitEvent, 0);
                if (!SDL_WaitEvent(&event)) {
                    goto done;
                }
            }

            if (event.type == SDL_QUIT) {
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

            switch (event.type) {
            case SDL_WINDOWEVENT:
                {
                    rmt_ScopedCPUSample(SDL_WINDOWEVENT, 0);
                    BeebWindows::HandleSDLWindowEvent(event.window);
                }
                break;

            case SDL_MOUSEMOTION:
                {
                    rmt_ScopedCPUSample(SDL_MOUSEMOTION, 0);
                    if (BeebWindow *window = BeebWindows::FindBeebWindowBySDLWindowID(event.motion.windowID)) {
                        window->HandleSDLMouseMotionEvent(event.motion);
                    }
                }
                break;

            case SDL_TEXTINPUT:
                {
                    rmt_ScopedCPUSample(SDL_TEXTINPUT, 0);
                    if (BeebWindow *window = BeebWindows::FindBeebWindowBySDLWindowID(event.text.windowID)) {
                        window->HandleSDLTextInput(event.text.text);
                    }
                }
                break;

            case SDL_KEYUP:
            case SDL_KEYDOWN:
                {
                    rmt_ScopedCPUSample(SDL_KEYxx, 0);

                    if (BeebWindow *window = BeebWindows::FindBeebWindowBySDLWindowID(event.key.windowID)) {
                        window->HandleSDLKeyEvent(event.key);
                    }
                }
                break;

            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEBUTTONDOWN:
                {
                    if (BeebWindow *window = BeebWindows::FindBeebWindowBySDLWindowID(event.button.windowID)) {
                        window->HandleSDLMouseButtonEvent(event.button);
                    }
                }
                break;

            case SDL_MOUSEWHEEL:
                {
                    rmt_ScopedCPUSample(SDL_MOUSEWHEEL, 0);
                    if (BeebWindow *window = BeebWindows::FindBeebWindowBySDLWindowID(event.wheel.windowID)) {
                        window->HandleSDLMouseWheelEvent(event.wheel);
                    }
                }
                break;

            case SDL_DROPBEGIN:
                LOGF(OUTPUT, "SDL_DROPBEGIN\n");
                break;

            case SDL_DROPFILE:
                {
                    LOGF(OUTPUT, "SDL_DROPFILE: %s\n", event.drop.file);

                    auto arguments = std::make_unique<BeebWindowLaunchArguments>();

                    arguments->type = BeebWindowLaunchType_DragAndDrop;
                    arguments->file_path = event.drop.file;

                    PushLaunchEvent(event.drop.windowID, std::move(arguments));

                    SDL_free(event.drop.file);
                    event.drop.file = nullptr;
                }
                break;

            case SDL_DROPCOMPLETE:
                LOGF(OUTPUT, "SDL_DROPFILE\n");
                break;

            case SDL_JOYDEVICEADDED:
                {
                    // constructing a Messages is a bit expensive, but this
                    // doesn't happen all that often...
                    Messages msg(GetMRUMessageList());
                    JoystickDeviceAdded(event.jdevice.which, &msg);
                }
                break;

            case SDL_JOYDEVICEREMOVED:
                {
                    // constructing a Messages is a bit expensive, but this
                    // doesn't happen all that often...
                    Messages msg(GetMRUMessageList());
                    JoystickDeviceRemoved(event.jdevice.which, &msg);
                }
                break;

            case SDL_CONTROLLERAXISMOTION:
                {
                    if (BeebWindow *window = FindBeebKeyboardFocusWindow()) {
                        window->HandleSDLControllerAxisMotionEvent(event.caxis);
                    }
                }
                break;

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
                {
                    if (BeebWindow *window = FindBeebKeyboardFocusWindow()) {
                        window->HandleSDLControllerButtonEvent(event.cbutton);
                    }
                }
                break;

            default:
                {
                    if (event.type >= g_first_event_type && event.type < g_first_event_type + SDLEventType_Count) {
                        switch ((SDLEventType)(event.type - g_first_event_type)) {
                        case SDLEventType_VBlank:
                            {
                                rmt_ScopedCPUSample(SDLEventType_VBlank, 0);
                                if (auto dd = (b2VBlankHandler::Display *)vblank_monitor->GetDisplayDataForDisplayID((uint32_t)event.user.code)) {
                                    uint64_t ticks = GetCurrentTickCount();

                                    {
                                        BeebWindows::HandleVBlank(vblank_monitor.get(), dd, ticks);
                                    }

                                    {
                                        LockGuard<Mutex> lock(dd->mutex);

                                        dd->message_pending = false;
                                    }
                                }
                            }
                            break;

                        case SDLEventType_UpdateWindowTitle:
                            {
                                rmt_ScopedCPUSample(SDLEventType_UpdateWindowTitle, 0);
                                BeebWindows::UpdateWindowTitles();
                            }
                            break;

                        case SDLEventType_NewWindow:
                            {
                                rmt_ScopedCPUSample(SDLEventType_NewWindow, 0);
                                auto init_arguments = (BeebWindowInitArguments *)event.user.data1;

                                BeebWindows::CreateBeebWindow(*init_arguments);

                                delete init_arguments;
                                init_arguments = nullptr;
                            }
                            break;

                        case SDLEventType_Function:
                            {
                                rmt_ScopedCPUSample(SDLEventType_Function, 0);
                                auto fun = (std::function<void()> *)event.user.data1;

                                (*fun)();

                                delete fun;
                                fun = nullptr;
                            }
                            break;

                        case SDLEventType_Launch:
                            {
                                auto arguments = (BeebWindowLaunchArguments *)event.user.data1;

                                BeebWindow *beeb_window = nullptr;
                                if (event.user.windowID != 0) {
                                    beeb_window = BeebWindows::FindBeebWindowBySDLWindowID(event.user.windowID);
                                }

                                if (!beeb_window) {
                                    beeb_window = BeebWindows::FindMRUBeebWindow();
                                }

                                beeb_window->Launch(*arguments);

                                delete arguments;
                                arguments = nullptr;
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
        ; //<-- fix Visual Studio autoformat bug

        SaveGlobalConfig(&init_messages);

        SDL_PauseAudioDevice(audio_device, 1);

        BeebWindows::Shutdown();
        //Timeline::Shutdown();

        StopHTTPServer();

        vblank_monitor = nullptr;
        g_vblank_handler = nullptr;

        CloseJoysticks();
    }

#if SYSTEM_OSX
    QuitHIDCallback();
#endif

    SDL_Quit();

#if RMT_ENABLED
    if (g_remotery) {
        rmt_DestroyGlobalInstance(g_remotery);
        g_remotery = NULL;
    }
#endif

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
#ifdef _MSC_VER
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_CHECK_ALWAYS_DF);
    //_crtBreakAlloc=12520;
#endif

#if SYSTEM_WINDOWS
    InitWindowsConsoleStuff();
#endif

    LinkCommands();

    SetCurrentThreadName("Main Thread");

    auto &&messages = std::make_shared<MessageList>("b2");

    bool good = main2(argc, argv, messages);

    if (!good) {
        FailureMessageBox("Initialisation failed", messages);
    }

    // If there are any messages, get them printed now.
    messages->SetPrintToStdio(true);

    if (!good) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
