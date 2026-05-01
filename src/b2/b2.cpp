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
#include <http/HTTPServer.h>
#include <http/http.h>
#include "HTTPMethodsHandler.h"
#include <beeb/DirectDiscImage.h>
#include "discs.h"
#if SYSTEM_OSX
#include <IOKit/hid/IOHIDLib.h>
#elif SYSTEM_LINUX
#include <glib-2.0/glib.h>
#include <gtk/gtk.h>
#endif
#include "BeebLinkHTTPHandler.h"
#include "joysticks.h"
#include <shared/strings.h>
#include <http/HTTPClient.h>
#include "dear_imgui.h"
#include <http/http.h>
#include <shared/metrics.h>
#include <shared/file_io.h>

#include <shared/enum_decl.h>
#include "b2_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "b2_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_def.h>
#define ENAME int
NBEGIN(SDL_WindowEventID)
NN(SDL_WINDOWEVENT_NONE);
NN(SDL_WINDOWEVENT_SHOWN);
NN(SDL_WINDOWEVENT_HIDDEN);
NN(SDL_WINDOWEVENT_EXPOSED);
NN(SDL_WINDOWEVENT_MOVED);
NN(SDL_WINDOWEVENT_RESIZED);
NN(SDL_WINDOWEVENT_SIZE_CHANGED);
NN(SDL_WINDOWEVENT_MINIMIZED);
NN(SDL_WINDOWEVENT_MAXIMIZED);
NN(SDL_WINDOWEVENT_RESTORED);
NN(SDL_WINDOWEVENT_ENTER);
NN(SDL_WINDOWEVENT_LEAVE);
NN(SDL_WINDOWEVENT_FOCUS_GAINED);
NN(SDL_WINDOWEVENT_FOCUS_LOST);
NN(SDL_WINDOWEVENT_CLOSE);
NN(SDL_WINDOWEVENT_TAKE_FOCUS);
NN(SDL_WINDOWEVENT_HIT_TEST);
#if SDL_VERSION_ATLEAST(22, 0, 18)
NN(SDL_WINDOWEVENT_ICCPROF_CHANGED);
NN(SDL_WINDOWEVENT_DISPLAY_CHANGED);
#endif
NEND()
#undef ENAME
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char DEFAULT_PRODUCT_NAME[] = "b2 - BBC Micro B/B+/Master emulator - " STRINGIZE(RELEASE_NAME);

const char GAMECONTROLLER_DB_FILE_NAME[] = "gamecontrollerdb.txt";

static SDL_threadID g_main_thread_id = 0;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BUILD_TYPE_Debug
#define ENABLE_FAIL_STARTUP 1
#else
#define ENABLE_FAIL_STARTUP 0
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const int DEFAULT_AUDIO_HZ = 48000;

static const int DEFAULT_AUDIO_BUFFER_SIZE = 1024;

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

GlobalSettings g_global_settings;

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

AppHandler::~AppHandler() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void AppHandler::SetActualHttpServerListenPort(int port) {
    (void)port;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void AppHandler::DearImGuiTestEngineDidBecomeReady(BeebWindow *beeb_window, ImGuiStuff *imgui_stuff) {
    (void)beeb_window, (void)imgui_stuff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void AppHandler::MessageLoopWillStart() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void AppHandler::HandleBeebWindowPostInit(BeebWindow *beeb_window) {
    (void)beeb_window;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void AppHandler::SetSelectorDialogResult(const Guid &guid, const std::string &result) {
    (void)guid, (void)result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

OrdinaryAppHandler::OrdinaryAppHandler(int argc, char *argv[])
    : m_argv(argv + 0, argv + argc) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string OrdinaryAppHandler::GetProductName() const {
    return DEFAULT_PRODUCT_NAME;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool OrdinaryAppHandler::IsHeadless() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool OrdinaryAppHandler::IsHighDPIEnabled() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool OrdinaryAppHandler::IsSoundEnabled() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::string> OrdinaryAppHandler::GetCommandLineArgs() const {
    return m_argv;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool OrdinaryAppHandler::GetConfigAndCacheOverrideFolder(std::string *folder) const {
    (void)folder;

    // the default logic is sensible.
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int OrdinaryAppHandler::GetRequestedHttpServerListenPort() const {
    return 0xbbcb;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int OrdinaryAppHandler::GetLaunchRequestHttpServerPort() const {
    return this->GetRequestedHttpServerListenPort();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool OrdinaryAppHandler::IsDearImGuiTestEngineEnabled() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool OrdinaryAppHandler::HandleSelectorDialogOpen(std::string *result, const Guid &guid) {
    (void)result, (void)guid;
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool OrdinaryAppHandler::ShouldQuitWhenTestQueueEmpty() const {
    return false;
}

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
            PushQuitMessage(3);

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

    if (g_can_detach_from_windows_console) {
        printf("stdout redirected to console window\n");
        fflush(stdout);

        fprintf(stderr, "stderr redirected to console window\n");
        fflush(stderr);
    } else {
        // If the console window isn't detachable, this is a console program,
        // and stdout/stderr were already redirected to the console. Don't print
        // the message.
    }
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
        std::shared_ptr<MetricSet> metric_set;
        Counter *thread_vblanks_counter = nullptr;
        Counter *messages_sent_counter = nullptr;
    };

    b2VBlankHandler();
    void *AllocateDisplayData(uint32_t display_id) override;
    void FreeDisplayData(uint32_t display_id, void *data) override;
    void ThreadVBlank(uint32_t display_id, void *data) override;

  protected:
  private:
    Mutex m_mutex;
    std::map<uint32_t, std::shared_ptr<Display>> m_data_by_display_id;
};

static std::unique_ptr<b2VBlankHandler> g_vblank_handler;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

b2VBlankHandler::b2VBlankHandler() {
    MUTEX_SET_NAME(m_mutex, "b2VBlankHandler");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *b2VBlankHandler::AllocateDisplayData(uint32_t display_id) {
    auto &&display = std::make_shared<Display>();

    display->metric_set = MetricSet::Create(strprintf("Display %" PRIu32, display_id));
    display->thread_vblanks_counter = MetricSet::CreateCounter(display->metric_set, "Thread vblanks");
    display->messages_sent_counter = MetricSet::CreateCounter(display->metric_set, "Messages sent");
    MetricSet::CreateDerivedValue(display->metric_set,
                                  "Vblanks skipped",
                                  [thread_vblanks_counter = display->thread_vblanks_counter, messages_sent_counter = display->messages_sent_counter]() {
                                      return thread_vblanks_counter->GetValue() - messages_sent_counter->GetValue();
                                  });

    MUTEX_SET_NAME(display->mutex, strprintf("DisplayData for display %" PRIu32, display_id));

    LockGuard<Mutex> lock(m_mutex);

    m_data_by_display_id[display_id] = display;

    LOGF(OUTPUT, "b2VBlankHandler::AllocateDisplayData: display_id=%" PRIu32 ": data=%p\n", display_id, (void *)display.get());

    return display.get();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void b2VBlankHandler::FreeDisplayData(uint32_t display_id, void *data) {
    (void)data;
    (void)display_id;

    LockGuard<Mutex> lock(m_mutex);

    LOGF(OUTPUT, "b2VBlankHandler::FreeDisplayData: display_id=%" PRIu32 "; data=%p\n", display_id, data);

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

    display->thread_vblanks_counter->Increment();

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
            display->messages_sent_counter->Increment();
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

static void Push1HzMainThreadTimer(void) {
    SDL_Event event = {};
    event.user.type = g_first_event_type + SDLEventType_1HzMainThreadTimer;

    SDL_PushEvent(&event);
}

static Uint32 Handle1HzMainThreadTimer(Uint32 interval, void *param) {
    (void)param;

    Push1HzMainThreadTimer();

    return interval;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PushMainThreadMessage(std::unique_ptr<MainThreadMessage> message) {
    SDL_Event event = {};

    event.user.type = g_first_event_type + SDLEventType_Message;

    // This relies on the message loop receiving it, so it can delete
    // it. It's probably possible for an SDL_QUIT to end up ahead of
    // it in the queue, meaning that the object could leak.
    event.user.data1 = message.release();

    SDL_PushEvent(&event);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class NewWindowMessage : public MainThreadMessage {
  public:
    explicit NewWindowMessage(BeebWindowInitArguments init_arguments)
        : m_init_arguments(std::move(init_arguments)) {
    }

    void HandleMessage() override {
        rmt_ScopedCPUSample(SDLEventType_NewWindow, 0);

        BeebWindows::CreateBeebWindow(std::move(m_init_arguments));
    }

  protected:
  private:
    BeebWindowInitArguments m_init_arguments;
};

void PushNewWindowMessage(BeebWindowInitArguments init_arguments) {
    PushMainThreadMessage(std::make_unique<NewWindowMessage>(std::move(init_arguments)));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FunctionMessage::FunctionMessage(std::function<void()> fun)
    : m_fun(std::move(fun)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void FunctionMessage::HandleMessage() {
    rmt_ScopedCPUSample(SDLEventType_Function, 0);

    m_fun();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PushQuitMessage(int exit_code) {
    SDL_Event event = {};

    event.user.type = g_first_event_type + SDLEventType_Quit;
    event.user.code = exit_code;

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
    bool version = false;
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

    // File association mode is what you get when b2 has been set up as the
    // program to use when double clicking on disk images.
    //
    // Just one argument is supplied: the disk image.
    bool file_association_mode = false;
    std::string file_association_path;

    bool enable_high_dpi = true;

#if SYSTEM_LINUX
    float gui_scale = 0.f;
#endif

#if ENABLE_FAIL_STARTUP
    bool fail_startup_late = false;
    bool fail_startup_early = false;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_OSX
static bool IsPSNArgument(std::string arg) {
    if (arg.size() < 8) {
        return false;
    }

    if (arg[0] != '-' || arg[1] != 'p' || arg[2] != 's' || arg[3] != 'n' || arg[4] != '_' || !isdigit(arg[5]) || arg[6] != '_') {
        return false;
    }

    for (size_t i = 7; i < arg.size(); ++i) {
        if (!isdigit(arg[i])) {
            return false;
        }
    }

    return true;
}
#endif

#if SYSTEM_OSX
// http://stackoverflow.com/questions/10242115/
static void RemovePSNArguments(std::vector<std::string> *argv) {
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
    const AppHandler *app_handler,
    Messages *init_messages) {

    std::vector<std::string> argv = app_handler->GetCommandLineArgs();

    // Detect the File Explorer double-click case on Windows (also acts as a
    // convenient command-line shortcut).
    if (argv.size() == 2 && PathIsFileOnDisk(argv[1], nullptr, nullptr)) {
        options->file_association_mode = true;
        options->file_association_path = argv[1];

        return true;
    }

    CommandLineParser p(app_handler->GetProductName());

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

    p.AddOption(0, "version").SetIfPresent(&options->version).Help("display some version info and exit");

    p.AddOption(0, "reset-windows").SetIfPresent(&options->reset_windows).Help("reset window position and dock data");

    p.AddOption("vsync").SetIfPresent(&options->vsync).Help("use vsync for timing");
    p.AddOption("timer").SetIfPresent(&options->timer).Help("use timer for timing");

#if RMT_ENABLED
    p.AddOption("remotery").SetIfPresent(&options->remotery).Help("activate Remotery. See: https://github.com/Celtoys/Remotery");
    p.AddOption("remotery-thread-sampler").SetIfPresent(&options->remotery_thread_sampler).Help("activate Remotery thread sampler");
#endif

    if (app_handler->IsHighDPIEnabled()) {
        p.AddOption("disable-high-dpi").ResetIfPresent(&options->enable_high_dpi).Help("disable handling of high-DPI displays");
    }

#if SYSTEM_LINUX
    p.AddOption("gui-scale").Arg(&options->gui_scale).Meta("SCALE").Help("set GUI scale to SCALE, overriding any value previously set via the UI");
#endif

    p.AddHelpOption(&options->help);

#if SYSTEM_OSX
    RemovePSNArguments(&argv);
#endif

#if ENABLE_FAIL_STARTUP
    p.AddOption("fail-startup-early").SetIfPresent(&options->fail_startup_early).Help("fail the startup process at an early stage, even if it actually succeeded. Use this to test the failure UI");
    p.AddOption("fail-startup-late").SetIfPresent(&options->fail_startup_late).Help("fail the startup process at a late stage, even if it actually succeeded. Use this to test the failure UI");
#endif

    if (!p.Parse(argv)) {
        return false;
    }

    if (options->audio_buffer_size <= 0 ||
        options->audio_buffer_size >= 65535 ||
        (options->audio_buffer_size & (options->audio_buffer_size - 1)) != 0) {
        init_messages->e.f("Invalid audio buffer size: %d\n", options->audio_buffer_size);
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
static void SDLCALL HandleWindowsMessage(void *userdata, void *hWnd, unsigned int message, Uint64 wParam, Sint64 lParam) {
    (void)userdata, (void)hWnd, (void)message, (void)wParam, (void)lParam;

    // Experimental detection of display sleep, so that the vblank monitor can
    // potentially be kept informed. But it doesn't seem to work usefully:
    // SC_SCREENSAVE doesn't get invoked for display sleep, and in the
    // SC_MONITORPOWER case the lParam is always 2 (display shut off).
    //
    //
    // See https://learn.microsoft.com/en-us/windows/win32/menurc/wm-syscommand

    //if (message == WM_SYSCOMMAND) {
    //    if (wParam == SC_MONITORPOWER) {
    //        LOGF(OUTPUT, "WM_SYSCOMMAND: got SC_MONITORPOWER: lParam=%" PRId64 "\n", lParam);
    //    } else if (wParam == SC_SCREENSAVE) {
    //        LOGF(OUTPUT, "WM_SYSCOMMAND: got SC_SCREENSAVE\n");
    //    }
    //}
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// I'm not careful enough about tidying things up before returning
// from `main2' with an error to trust this as a local...
static FillAudioBufferData g_fill_audio_buffer_data;

//
static std::unique_ptr<std::thread> g_headless_audio_thread;

static bool IsHighDPIEnabled(const Options &options, const AppHandler *app_handler) {
    if (!options.enable_high_dpi) {
        return false;
    }

    if (!app_handler->IsHighDPIEnabled()) {
        return false;
    }

    return true;
}

static bool InitSystem(
    SDL_AudioDeviceID *device_id,
    SDL_AudioSpec *got_spec,
    const Options &options,
    const AppHandler *app_handler,
    Messages *init_messages) {
    (void)options;

    SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG_FILE, GetAssetPath(GAMECONTROLLER_DB_FILE_NAME).c_str());

    if (IsHighDPIEnabled(options, app_handler)) {
#if SYSTEM_WINDOWS
#ifdef SDL_HINT_WINDOWS_DPI_AWARENESS
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
#endif
    }

    // Click through when the window was unfocused. Might need to be system-dependent?
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

#if SYSTEM_WINDOWS
    SDL_SetWindowsMessageHook(&HandleWindowsMessage, nullptr);
#endif

    // Initialise SDL
    Uint32 sdl_init_flags = SDL_INIT_TIMER | SDL_INIT_EVENTS;
    if (!app_handler->IsHeadless()) {
        sdl_init_flags |= SDL_INIT_VIDEO;
        sdl_init_flags |= SDL_INIT_AUDIO;
        sdl_init_flags |= SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;
    }
    if (!app_handler->IsSoundEnabled()) {
        sdl_init_flags &= ~SDL_INIT_AUDIO;
    }
    if (SDL_Init(sdl_init_flags) != 0) {
        init_messages->e.f("FATAL: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    g_main_thread_id = SDL_GetThreadID(nullptr);

    SDL_EnableScreenSaver();

    // Allocate user events
    g_first_event_type = SDL_RegisterEvents(SDLEventType_Count);

    // Also send an inaugural event to kick things off.
    SDL_AddTimer(1000, &Handle1HzMainThreadTimer, NULL);
    Push1HzMainThreadTimer();

    SDL_StartTextInput();

#if SYSTEM_OSX
    if (!app_handler->IsHeadless()) {
        InitHIDCallback(init_messages);
    }
#endif

    // Start audio
    if (sdl_init_flags & SDL_INIT_AUDIO) {
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
    } else {
        *device_id = 0;
        *got_spec = {};
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

    char real_path_buffer[PATH_MAX];
    ASSERT(realpath(path.c_str(), real_path_buffer));
    std::string real_path = real_path_buffer;

    // Don't compare the entire real path, because that'll be
    // different from the apparent path when run from a symlinked
    // location. Strip out everything before the assets folder, easy
    // enough to do as a string processing operation.
    std::string bundle_assets_folder = "/assets/";

    std::string::size_type real_relative_path_start = real_path.rfind(bundle_assets_folder);
    ASSERT(real_relative_path_start != std::string::npos);
    std::string real_relative_path = real_path.substr(real_relative_path_start + bundle_assets_folder.size());

    std::string::size_type relative_path_start = path.rfind(bundle_assets_folder);
    ASSERT(relative_path_start != std::string::npos);
    std::string relative_path = path.substr(relative_path_start + bundle_assets_folder.size());

    ASSERT(relative_path == real_relative_path);

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

    for (size_t i = 0; i < NUM_BLANK_HARD_DISKS; ++i) {
        CheckAssetPath(BLANK_HARD_DISKS[i].GetDATAssetPath());
        CheckAssetPath(BLANK_HARD_DISKS[i].GetDSCAssetPath());
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
#if SYSTEM_WINDOWS
        AllocWindowsConsole();
#endif

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

class LaunchMessage : public MainThreadMessage {
  public:
    LaunchMessage(uint32_t sdl_window_id, BeebWindowLaunchArguments arguments)
        : m_sdl_window_id(sdl_window_id)
        , m_arguments(std::move(arguments)) {
    }

    void HandleMessage() override {
        BeebWindow *beeb_window = nullptr;
        if (m_sdl_window_id != 0) {
            beeb_window = BeebWindows::FindBeebWindowBySDLWindowID(m_sdl_window_id);
        }

        if (!beeb_window) {
            beeb_window = BeebWindows::FindMRUBeebWindow();
        }

        if (beeb_window) {
            beeb_window->Launch(m_arguments);
        }
    }

  protected:
  private:
    // there's a window ID in the event, but simplest to have everything part
    // of the message payload.
    uint32_t m_sdl_window_id = 0;
    BeebWindowLaunchArguments m_arguments;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool BootDiskInExistingProcess(const std::string &path, const AppHandler *app_handler, Messages *messages) {
    //auto client_message_list = std::make_shared<MessageList>();
    //Messages client_messages(client_message_list);

    std::unique_ptr<HTTPClient> client = CreateHTTPClient();
    client->SetLogs(messages);

    HTTPRequest request;
    // If there's a copy of b2 listening on some other port, it won't be found -
    // which is deliberate.
    request.url = strprintf("http://127.0.0.1:%d/launch", app_handler->GetLaunchRequestHttpServerPort());
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
static int g_requested_http_server_port = -1;

void StartHTTPServer(Messages *messages) {
    if (g_requested_http_server_port < 0) {
        return;
    }

    if (GetHTTPServerListenPort() != 0) {
        return;
    }

    g_http_server = CreateHTTPServer();
    if (!g_http_server->Start(g_requested_http_server_port, messages)) {
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
        return g_http_server->GetListenPort();
    }
}

bool CanStartHTTPServer() {
    if (g_requested_http_server_port >= 0) {
        return true;
    } else {
        return false;
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ShowOutputMessagesDialog(const char *description, const Messages &messages) {
    (void)description, (void)messages;

#if SYSTEM_WINDOWS
    if (!GetConsoleWindow()) {
        // Probably a GUI app build, so pop up the message box.
        FailureMessageBox(description, messages.GetMessageList(), SIZE_MAX);
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void FreeEventData(SDL_Event *event) {
    switch (event->type) {
    case SDL_DROPFILE:
        SDL_free(event->drop.file), event->drop.file = nullptr;
        break;

    default:
        if (event->type >= g_first_event_type && event->type < g_first_event_type + SDLEventType_Count) {
            switch ((SDLEventType)(event->type - g_first_event_type)) {
            default:
                break;

            case SDLEventType_Message:
                delete (MainThreadMessage *)event->user.data1;
                break;
            }

            event->user.data1 = nullptr;
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TickNoopMessageLoop(int *exit_code) {
    bool keep_running = true;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // The exit_code parameter exists so that the
        // SDLEventType_Quit exit code can be reported for forwarding
        // on.
        //
        // But it's a bit pointless, because the main reason a
        // quit-type event would show up here is that SDL2's SIGTERM
        // handler pushed one. And it always pushes a SDL_QUIT, which
        // has an implicit exit code of 0.
        if (event.type == SDL_QUIT) {
            keep_running = false;
            *exit_code = 0;
        } else if (event.type == g_first_event_type + SDLEventType_Quit) {
            keep_running = false;
            *exit_code = event.user.code;
        }

        // Whatever it was, in the bin it goes.
        FreeEventData(&event);
    }

    return keep_running;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool IsMainThread() {
    SDL_threadID thread_id = SDL_GetThreadID(nullptr);

    if (thread_id == g_main_thread_id) {
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int main2(AppHandler *app_handler, const std::shared_ptr<MessageList> &init_message_list) {
    int exit_code = EXIT_SUCCESS;
    Messages init_messages(init_message_list);

    CheckAssetPaths();

    Options options;
    if (!ParseCommandLineOptions(&options, app_handler, &init_messages)) {
        if (options.help) {
            ShowOutputMessagesDialog("Command line help", init_messages);
            return EXIT_SUCCESS;
        }

        return EXIT_FAILURE;
    }

    //#ifdef IMGUI_ENABLE_TEST_ENGINE
    //    if (options.imgui_list_tests) {
    //        std::vector<std::string> test_names = BeebWindow::GetAllDearImGuiTestNames();
    //        for (const std::string &test_name : test_names) {
    //            printf("2fcf9707-9498-4a03-9b27-ef501fa2fbb6:%s\n", test_name.c_str());
    //        }
    //        return EXIT_SUCCESS
    //    }
    //#endif

    if (options.version) {
        init_messages.i.f("b2 version: %s\n", STRINGIZE(RELEASE_NAME));
        init_messages.i.f("Dear ImGui version: %s\n", IMGUI_VERSION);
        init_messages.i.f("SDL headers version: %d.%d.%d\n", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
        {
            SDL_version version;
            SDL_GetVersion(&version);
            init_messages.i.f("SDL runtime version: %d.%d.%d (revision: %s)\n", version.major, version.minor, version.patch, SDL_GetRevision());
        }

        HTTPDependencyVersions versions = GetHTTPDependencyVersions();
        init_messages.i.f("libuv version: %s\n", versions.libuv_version.c_str());
        init_messages.i.f("libcurl version: %s\n", versions.libcurl_version.c_str());

        ShowOutputMessagesDialog("Version information", init_messages);
        return EXIT_SUCCESS;
    }

    {
        std::string folder;
        if (app_handler->GetConfigAndCacheOverrideFolder(&folder)) {
            SetConfigFolder(folder);

            // TODO: cache stuff should go in its own folder, if only to avoid
            // name conflicts. But DearImGuiTest::Run2 has some code in it that
            // assumes the config folder doesn't have any subfolders.
            SetCacheFolder(folder);
        }
    }

#if ENABLE_FAIL_STARTUP
    if (options.fail_startup_early) {
        init_messages.i.f("Failing startup early: info message.\n");
        init_messages.w.f("Failing startup early: warning message.\n");
        init_messages.e.f("Failing startup early: error message.\n");
        return EXIT_FAILURE;
    }
#endif

    // https://curl.haxx.se/libcurl/c/curl_global_init.html
    if (!InitHTTPDependencies(&init_messages)) {
        ShowOutputMessagesDialog("Initialisation failure", init_messages);
        return EXIT_FAILURE;
    }

    //if (g_app_handler->IsHeadless()) {
    //    // Listen on any old port.
    //    g_http_server_requested_listen_port = 0;
    //}

    // TODO: not very nice, but StartHTTPServer gets called via the UI as well,
    // and the app handler isn't accessible there.
    g_requested_http_server_port = app_handler->GetRequestedHttpServerListenPort();
    StartHTTPServer(&init_messages);

    app_handler->SetActualHttpServerListenPort(GetHTTPServerListenPort());

    //if (g_app_handler->IsHeadless()) {
    //    // This isn't really the right use for the config path, but in headless
    //    // mode it can be assumed to point somewhere transient.
    //    SaveTextFile(std::to_string(GetHTTPServerListenPort()), GetConfigPath("b2_http_listen_port.txt"), nullptr, 0);
    //}

    //#ifdef IMGUI_ENABLE_TEST_ENGINE
    //    // If the test engine isn't enabled, the b2_tests list will always be empty.
    //    std::vector<std::shared_ptr<b2Test>> b2_tests;
    //    if (options.imgui_enable_test_engine) {
    //        b2_tests = BeebWindow::Getb2Tests(options.imgui_tests);
    //    }
    //#endif

    if (options.file_association_mode) {
        if (GetHTTPServerListenPort() != 0) {
            // The HTTP server started, so there's definitely no other instance
            // running that could handle the request (even if possibly because
            // it's headless mode and a random port was picked - but that's ok).
        } else {
            if (BootDiskInExistingProcess(options.file_association_path, app_handler, &init_messages)) {
                return EXIT_SUCCESS;
            }
        }

        // Fake a -0 PATH -boot.
        options.boot = true;
        options.discs[0] = options.file_association_path;
    }

    if (!InitLogs(options, &init_messages)) {
        return EXIT_FAILURE;
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

    if (!BeebWindows::Init()) {
        init_messages.e.f(
            "FATAL: failed to initialize window manager.\n");
        return EXIT_FAILURE;
    }

    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    if (!InitSystem(&audio_device, &audio_spec, options, app_handler, &init_messages)) {
        return EXIT_FAILURE;
    }

#if SYSTEM_LINUX
    if (!app_handler->IsHeadless()) {
        // Need to do this after SDL_Init. See, e.g.,
        // https://discourse.libsdl.org/t/gtk2-sdl2-partial-fail/19274
        gtk_init();
    }
#endif

#if SYSTEM_WINDOWS
    if (!InitMF(&init_messages)) {
        return EXIT_FAILURE;
    }
#endif

#if HAVE_FFMPEG
    if (!InitFFmpeg(&init_messages)) {
        return EXIT_FAILURE;
    }
#endif

    if (!LoadDiscDriveSamples(&init_messages)) {
        init_messages.e.f("Failed to initialise disc drive samples.\n");
        return EXIT_FAILURE;
    }

    InitDefaultBeebConfigs();

    {
        //        if(!Timeline::Init()) {
        //            init_messages.e.f(
        //                "FATAL: failed to initialize timeline.\n");
        //            return EXIT_FAILURE;
        //        }

        SDL_PauseAudioDevice(audio_device, 0);

        if (!LoadGlobalConfig(&init_messages)) {
            return EXIT_FAILURE;
        }

        // Ugh. This thing here is really a bit of a bodge...
        if (options.vsync) {
            g_global_settings.vsync = true;
        } else if (options.timer) {
            g_global_settings.vsync = false;
        }

        g_vblank_handler = std::make_unique<b2VBlankHandler>();
        std::unique_ptr<VBlankMonitor> vblank_monitor;
        {
            bool use_vsync = g_global_settings.vsync && !app_handler->IsHeadless();
            init_messages.i.f("Timing method: %s\n", use_vsync ? "vsync" : "timer");
            vblank_monitor = CreateVBlankMonitor(g_vblank_handler.get(),
                                                 !use_vsync,
                                                 &init_messages);
        }
        if (!vblank_monitor) {
            init_messages.e.f("Failed to initialise vblank monitor.\n");
            return EXIT_FAILURE;
        }

        BeebLoadedConfig initial_loaded_config;
        {
            bool got_initial_loaded_config = false;

            if (!options.config_name.empty()) {
                if (BeebWindows::LoadConfigByName(&initial_loaded_config, options.config_name, {}, &init_messages)) {
                    got_initial_loaded_config = true;
                }
            }

            if (!got_initial_loaded_config) {
                if (!BeebWindows::defaults.config.empty()) {
                    if (BeebWindows::LoadConfigByName(&initial_loaded_config,
                                                      BeebWindows::defaults.config,
                                                      {},
                                                      &init_messages)) {
                        got_initial_loaded_config = true;
                    }
                }
            }

            if (!got_initial_loaded_config) {
                if (BeebLoadedConfig::Load(&initial_loaded_config, *GetDefaultBeebConfigByIndex(0), {}, &init_messages)) {
                    got_initial_loaded_config = true;
                }
            }

            if (!got_initial_loaded_config) {
                // Ugh, ok.
                return EXIT_FAILURE;
            }
        }

#if ENABLE_FAIL_STARTUP
        if (options.fail_startup_late) {
            init_messages.i.f("Failing startup late: info message.\n");
            init_messages.w.f("Failing startup late: warning message.\n");
            init_messages.e.f("Failing startup late: error message.\n");
            return EXIT_FAILURE;
        }
#endif

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
            ia.verbose = options.verbose;
            ia.enable_high_dpi = IsHighDPIEnabled(options, app_handler);
#if SYSTEM_LINUX
            ia.gui_scale = options.gui_scale;
#endif

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
                    return EXIT_FAILURE;
                }
            }

            ia.boot = options.boot;

            ia.limit_speed = options.limit_speed;
            ia.app_handler = app_handler;

            //#ifdef IMGUI_ENABLE_TEST_ENGINE
            //            ia.imgui_enable_test_engine = options.imgui_enable_test_engine;
            //            ia.imgui_tests = options.imgui_tests;
            //#endif
        }

        //#ifdef IMGUI_ENABLE_TEST_ENGINE
        //        for (const std::shared_ptr<b2Test> &b2_test : b2_tests) {
        //            printf("ea73a8dc-2d1a-43bc-ae41-078e441e53c5:%s\n", b2_test->name.c_str());
        //            if (!!b2_test->will_create_BeebWindow_fn) {
        //                b2_test->will_create_BeebWindow_fn(&ia);
        //            }
        //        }
        //#endif

        if (!BeebWindows::CreateBeebWindow(ia)) {
            init_messages.e.f("FATAL: failed to open initial window.\n");
            return EXIT_FAILURE;
        }

        // not needed any more.
        init_message_list->ClearMessages();

        app_handler->MessageLoopWillStart();

        while (BeebWindows::GetNumWindows() > 0) {
            SDL_Event event;

            {
                rmt_ScopedCPUSample(SDL_WaitEvent, 0);
                if (!SDL_WaitEvent(&event)) {
                    const char *error = SDL_GetError();
                    (void)error;
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

                    //LOGF(OUTPUT,"SDL_WINDOWEVENT: windowID=%d, event=%d (%s)\n", event.window.windowID, event.window.event, GetSDL_WindowEventIDEnumName(event.window.event));

                    // Works differently because it may poke about with the
                    // overall BeebWindows list to remove the closed window.
                    BeebWindows::HandleSDLWindowEvent(event.window);
                }
                break;

            case SDL_MOUSEMOTION:
                {
                    rmt_ScopedCPUSample(SDL_MOUSEMOTION, 0);

                    //LOGF(OUTPUT,"SDL_MOUSEMOTION: windowID=%d, x=%d, y=%d\n", event.motion.windowID, event.motion.x, event.motion.y);

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
                    //LOGF(OUTPUT,"SDL_MOUSEBUTTON%s: windowID=%d, button=%d\n", event.type == SDL_MOUSEBUTTONUP ? "UP" : "DOWN", event.button.windowID, event.button.button);

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

                    BeebWindowLaunchArguments arguments;

                    arguments.type = BeebWindowLaunchType_DragAndDrop;
                    arguments.file_path = event.drop.file;

                    PushMainThreadMessage(std::make_unique<LaunchMessage>(event.drop.windowID, std::move(arguments)));
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

                        case SDLEventType_1HzMainThreadTimer:
                            {
                                rmt_ScopedCPUSample(SDLEventType_1HzMainThreadTimer, 0);
                                BeebWindows::Handle1HzTimer();

                                if (vblank_monitor->NeedsRefreshDisplayList()) {
                                    auto &&message_list = std::make_shared<MessageList>("RefreshDisplayList");

                                    {
                                        Messages msg(message_list);

                                        vblank_monitor->RefreshDisplayList(&msg);
                                    }

                                    // There's nowhere particularly good for the
                                    // messages to go here. The OUTPUT log is as
                                    // good as any.
                                    message_list->ForEachMessage([](MessageList::Message *m) {
                                        LOGF(OUTPUT, "%s\n", m->text.c_str());
                                    });
                                }
                            }
                            break;

                        case SDLEventType_Message:
                            {
                                if (auto message = (MainThreadMessage *)event.user.data1) {
                                    message->HandleMessage();
                                }
                            }
                            break;

                        case SDLEventType_Quit:
                            {
                                exit_code = event.user.code;

                                // and post another quit event, to be retrieved
                                // straight away next time round the loop.
                                SDL_Event event2 = {};
                                event2.type = SDL_QUIT;
                                SDL_PushEvent(&event2);
                            }
                            break;

                        case SDLEventType_Count:
                            // only here to avoid incomplete switch warning.
                            ASSERT(false);
                            break;
                        }
                    }
                }
                break;
            }

            FreeEventData(&event);
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
    if (!app_handler->IsHeadless()) {
        QuitHIDCallback();
    }
#endif

    SDL_Quit();

#if RMT_ENABLED
    if (g_remotery) {
        rmt_DestroyGlobalInstance(g_remotery);
        g_remotery = NULL;
    }
#endif

    return exit_code;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static AppHandler *g_app_handler_mutable = nullptr;
//AppHandler *const &g_app_handler = g_app_handler_mutable;

int b2_main(AppHandler *app_handler) {
    ASSERT(app_handler);
    //g_app_handler_mutable = app_handler;

#ifdef _MSC_VER
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_CHECK_ALWAYS_DF);
    //_crtBreakAlloc=93384;
#endif

#if SYSTEM_WINDOWS
    InitWindowsConsoleStuff();
#endif

    LinkCommands();

    SetCurrentThreadName("Main Thread");

    auto &&messages = std::make_shared<MessageList>("b2");

    int exit_code = main2(app_handler, messages);

    //g_app_handler_mutable = nullptr;

    if (exit_code != EXIT_SUCCESS) {
        if (!app_handler->IsHeadless()) {
#if SYSTEM_LINUX
            // Do this here, just in case main2 didn't get to its own
            // post-SDL2_Init gtk_init call.
            gtk_init();
#endif
            FailureMessageBox("Initialisation failed", messages);
        }
    }

    // If there are any messages, get them printed now.
    messages->SetFlags(messages->GetFlags() | MessageListFlags_Stdio);

    return exit_code;
}
