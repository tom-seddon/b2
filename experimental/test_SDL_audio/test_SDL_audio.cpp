#include <shared/system.h>
#include <SDL.h>
#include <shared/testing.h>
#include <shared/debug.h>
#if SYSTEM_OSX
#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#endif

static UInt32 g_reopen_audio_device_event;

#if SYSTEM_OSX

static SDL_AudioDeviceID g_audio_device_id = 0;

static void SleepCallback(void *refCon, io_service_t service, natural_t message, void *message_arg) {
    (void)refCon, (void)service, (void)message_arg;

    if (message == kIOMessageSystemWillSleep) {
        printf("SleepCallback: kIOMessageSystemWillSleep (now=%.4f s)\n", GetSecondsFromTicks(GetCurrentTickCount()));

        //SDL_PauseAudioDevice(g_audio_device_id, 1);
    } else if (message == kIOMessageSystemHasPoweredOn) {
        printf("SleepCallback: kIOMessageSystemHasPoweredOn (now=%.4f s)\n", GetSecondsFromTicks(GetCurrentTickCount()));

        // leave the thing paused...

        SDL_Event event;
        event.type = g_reopen_audio_device_event;

        SDL_PushEvent(&event);
    }
}

static io_connect_t g_root_port;
static IONotificationPortRef g_notify_port_ref;
static io_object_t g_notifier;

static void AddSleepCallback() {
    g_root_port = IORegisterForSystemPower(nullptr, &g_notify_port_ref, &SleepCallback, &g_notifier);
    TEST_NE_UU(g_root_port, 0);

    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                       IONotificationPortGetRunLoopSource(g_notify_port_ref), kCFRunLoopCommonModes);
}

static void RemoveSleepCallback() {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
                          IONotificationPortGetRunLoopSource(g_notify_port_ref),
                          kCFRunLoopCommonModes);

    IODeregisterForSystemPower(&g_notifier), g_notifier = 0;

    IOServiceClose(g_root_port), g_root_port = 0;

    IONotificationPortDestroy(g_notify_port_ref), g_notify_port_ref = nullptr;
}
#endif

// https://developer.apple.com/library/archive/qa/qa1340/_index.html

struct AudioCallbackState {
    uint64_t last_callback_ticks = 0;
    uint64_t num_calls = 0;
};

static void SDLCALL AudioCallback(void *userdata, Uint8 *stream, int len) {
    ASSERT(len >= 0);
    auto s = (AudioCallbackState *)userdata;

    uint64_t now_ticks = GetCurrentTickCount();

    printf("%" PRIu64 ": now=%.4f s; dt=%.3f ms\n", s->num_calls, GetSecondsFromTicks(now_ticks), GetMillisecondsFromTicks(now_ticks - s->last_callback_ticks));

    s->last_callback_ticks = now_ticks;
    ++s->num_calls;

    memset(stream, 0, (size_t)len);
}

static AudioCallbackState g_audio_callback_state;

static void CloseAudioDevice() {
    if (g_audio_device_id != 0) {
        printf("closing audio device: %" PRIu32 "\n", g_audio_device_id);
        SDL_PauseAudioDevice(g_audio_device_id, 1);
        SDL_CloseAudioDevice(g_audio_device_id);
        g_audio_device_id = 0;
    }
}

static void ReopenAudioDevice() {
    printf("ReopenAudioDevice...\n");

    if (g_audio_device_id != 0) {
        CloseAudioDevice();
    }

    g_audio_callback_state = {};

    SDL_AudioSpec desired_spec = {};
    desired_spec.freq = 48000;
    desired_spec.format = AUDIO_F32SYS;
    desired_spec.samples = 1024;
    desired_spec.callback = &AudioCallback;
    desired_spec.userdata = &g_audio_callback_state;

    SDL_AudioSpec obtained_spec = {};

    g_audio_device_id = SDL_OpenAudioDevice(nullptr,
                                            0, //playback
                                            &desired_spec,
                                            &obtained_spec,
                                            SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

    printf("device_id=%d\n", g_audio_device_id);

    TEST_GT_II(g_audio_device_id, 0);

    printf("obtained spec: freq=%d\n", obtained_spec.freq);
    printf("               format=%d (BITSIZE=%d ISFLOAT=%d BE=%d SIGNED=%d)\n", obtained_spec.format, SDL_AUDIO_BITSIZE(obtained_spec.format), !!SDL_AUDIO_ISFLOAT(obtained_spec.format), !!SDL_AUDIO_ISBIGENDIAN(obtained_spec.format), !!SDL_AUDIO_ISSIGNED(obtained_spec.format));
    printf("               channels=%" PRIu8 "\n", obtained_spec.channels);
    printf("               samples=%" PRIu16 "\n", obtained_spec.samples);

    SDL_PauseAudioDevice(g_audio_device_id, 0);
}

int main(int argc, char *argv[]) {
    (void)argc, (void)argv;

    TEST_EQ_II(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_AUDIO), 0);

    g_reopen_audio_device_event = SDL_RegisterEvents(1);

    SDL_Window *w = SDL_CreateWindow("audio test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 250, 250, 0);
    TEST_NON_NULL(w);

    // 0=query sound playback devices
    for (int i = 0; i < SDL_GetNumAudioDevices(0); ++i) {
        const char *name = SDL_GetAudioDeviceName(i, 0);
        printf("Device %d: %s\n", i, name);
    }

#if SYSTEM_OSX
    AddSleepCallback();
#endif

    ReopenAudioDevice();

    SDL_Event ev;
    while (SDL_WaitEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            goto done;

        default:
            if (ev.type == g_reopen_audio_device_event) {
                ReopenAudioDevice();
            }
            break;
        }
    }

done:

#if SYSTEM_OSX
    RemoveSleepCallback();
#endif

    CloseAudioDevice();

    SDL_Quit();
}
