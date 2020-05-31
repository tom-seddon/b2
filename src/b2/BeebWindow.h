#ifndef HEADER_33477416E2224FD8A7F0B58AC38F831D// -*- mode:c++ -*-
#define HEADER_33477416E2224FD8A7F0B58AC38F831D

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

struct BeebWindowInitArguments;
class VBlankMonitor;
class BeebThread;
class TimelineUI;
struct SDL_Texture;
struct SDL_Renderer;
struct Keycap;
class ConfigsUI;
class KeymapsUI;
#if BBCMICRO_TRACE
class TraceUI;
#endif
class NVRAMUI;
class DataRateUI;
class BeebState;
class CommandKeymapsUI;
class SettingsUI;
class DiscImage;

#include "keys.h"
#include "dear_imgui.h"
#include <string>
#include <SDL.h>
#include <beeb/TVOutput.h>
#include "native_ui.h"
#include <beeb/conf.h>
#include <shared/log.h>
#include <functional>
#include "Messages.h"
#include <map>
#include <set>
#include <limits.h>
#include "BeebConfig.h"
#include "BeebKeymap.h"
#include "commands.h"
#include "SDL_video.h"
#include <beeb/video.h>

#include <shared/enum_decl.h>
#include "BeebWindow.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebWindowTextureDataVersion {
    uint64_t version=0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The config name isn't part of this, because then there'd be two copies
// of the initial config name in BeebWindowInitArguments. Something needs
// fixing...
struct BeebWindowSettings {
    uint64_t popups=0;

    std::string dock_config;

    float bbc_volume=0.f;
    float disc_volume=0.f;
    bool power_on_tone=true;

    bool display_auto_scale=true;
    bool correct_aspect_ratio=true;
    float display_manual_scale=1.f;
    bool display_filter=true;
    bool display_interlace=false;

    const BeebKeymap *keymap=nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO - there are items that only apply to the first window, and they should
// probably go somewhere else...
struct BeebWindowInitArguments {
public:
    // Index of SDL render driver, as supplied to SDL_CreateRenderer.
    int render_driver_index=-1;

    // SDL pixel format to use.
    uint32_t pixel_format=0;

    // SDL_AudioDeviceID of device to play to. The window doesn't
    // create the audio device, but it needs to know which ID to use
    // so it can respond to the appropriate callback.
    uint32_t sound_device=0;

    // Sound playback details.
    SDL_AudioSpec sound_spec={};

    // Name for the window.
    std::string name;

    // If set, reset windows. Only ever set for the first window created.
    bool reset_windows=false;

#if SYSTEM_OSX

    // Frame name to load window frame from, or empty if the default
    // is OK.
    std::string frame_name;

#else

    // Placement data to use for this window. Empty if the default
    // position is OK.
    std::vector<uint8_t> placement_data;

#endif

//    // If set, the emulator is initially paused.
//    //
//    // (This isn't actually terribly useful, because there's no way to
//    // then unpause it from the public API - but the flag has to go
//    // somewhere, and it has to funnel through CreateBeebWindow, so...
//    // here it is.)
//    bool initially_paused=false;

    // When INITIAL_STATE is non-null, it is used as the initial state
    // for the window; otherwise, DEFAULT_CONFIG will be used as the
    // config to start with.
    //
    // DEFAULT_CONFIG must always be valid, as it is the config used
    // for new windows created by Window|New.
    std::shared_ptr<BeebState> initial_state;
    BeebLoadedConfig default_config;

    // Message list to be used to populate the window's message list.
    // This will almost always be null; it's used to collect messages
    // from the initial startup that are printed before there's any
    // windows to display them on.
    //
    // If the window init fails, and preinit_messages!=nullptr, any
    // additional init messages will be added here too.
    std::shared_ptr<MessageList> preinit_message_list;

    // If preinit_messages is null, this should be non-zero. It's the
    // ID of the window to send init messages to if the init fails.
    //
    // (If the initiating window has gone away, messages will be
    // printed to Messages::stdio.)
    uint32_t initiating_window_id=0;

    // If non-empty, name of the initial keymap to select.
    //
    // (Keymap pointer is no good, as this struct gets sent to the
    // BeebThread; the keymap could be deleted by the time it's used.)
    std::string keymap_name;

    // When USE_SETTINGS is true, copy new window's settings from
    // SETTINGS. Otherwise, use BeebWindow::defaults.
    bool use_settings=false;
    BeebWindowSettings settings;

    // Initial disc images, if any, for the drives. Only set for the first
    // window created - the values come from the -0/-1 command line options.
    std::shared_ptr<DiscImage> init_disc_images[NUM_DRIVES];

    // If set, try to autoboot. Only set for the first window created - the
    // value comes from the --boot command line option.
    bool boot=false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebWindow {
public:
    struct VBlankRecord {
        uint64_t num_ticks=0;
        size_t num_video_units=0;
    };

    class OptionsUI;

    static const char SDL_WINDOW_DATA_NAME[];

    BeebWindow(BeebWindowInitArguments init_arguments);
    ~BeebWindow();

    bool Init();

    // Saves all settings, including whatever SavePosition does.
    void SaveSettings();

    // Saves position only.
    void SavePosition();

    const std::string &GetName() const;
    void SetName(std::string name);

    bool GetBeebKeyState(BeebKey key) const;

    uint32_t GetSDLWindowID() const;

    void HandleSDLKeyEvent(const SDL_KeyboardEvent &event);

    void SetSDLMouseWheelState(int x,int y);
    void HandleSDLTextInput(const char *text);
    void HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event);

    void ThreadFillAudioBuffer(SDL_AudioDeviceID audio_device_id,float *mix_buffer,size_t num_samples);

    bool HandleVBlank(VBlankMonitor *vblank_monitor,void *display_data,uint64_t ticks);

    bool HandleVBlank(uint64_t ticks);

    void UpdateTitle();

    void BeebKeymapWillBeDeleted(BeebKeymap *keymap);

    // Get pointer to this window's BeebThread. (The lifespan of the
    // BeebThread is the same as that of the window.)
    std::shared_ptr<BeebThread> GetBeebThread() const;

    //
    std::shared_ptr<MessageList> GetMessageList() const;

#if SYSTEM_WINDOWS
    void *GetHWND() const;
#endif
#if SYSTEM_OSX
    void *GetNSWindow() const;
#endif

    std::vector<VBlankRecord> GetVBlankRecords() const;

    const BeebKeymap *GetCurrentKeymap() const;
    void SetCurrentKeymap(const BeebKeymap *keymap);

#if VIDEO_TRACK_METADATA
    const VideoDataUnit *GetVideoDataUnitForMousePixel() const;
#endif

    SettingsUI *GetPopupByType(BeebWindowPopupType type) const;

    bool HardReset(const BeebConfig &config);

    const std::string &GetConfigName() const;
protected:
private:
    struct SettingsUIMetadata;

    struct DriveState {
        OpenFileDialog open_disc_image_file_dialog;
        OpenFileDialog open_direct_disc_image_file_dialog;

        DriveState();
    };

    BeebWindowInitArguments m_init_arguments;
    std::string m_name;

    // SDLstuff.
    SDL_Window *m_window=nullptr;
    SDL_Renderer *m_renderer=nullptr;
    SDL_PixelFormat *m_pixel_format=nullptr;

#if SYSTEM_WINDOWS
    void *m_hwnd=nullptr;
#elif SYSTEM_OSX
    void *m_nswindow=nullptr;
#elif SYSTEM_LINUX
#include <shared/pshpack1.h>
    struct WindowPlacementData {
        uint8_t maximized=0;
        int x=INT_MIN,y=INT_MIN;
        int width=0,height=0;
    };
#include <shared/poppack.h>
#endif
    std::vector<VBlankRecord> m_vblank_records;
    size_t m_vblank_index=0;
    uint64_t m_last_vblank_ticks=0;

    // TV output.
    TVOutput m_tv;
    SDL_Texture *m_tv_texture=nullptr;

    float m_blend_amt=0.f;

    // Audio output
    SDL_AudioDeviceID m_sound_device=0;

    bool m_prefer_shortcuts=false;

    // number of emulated us that had passed at the time of the last
    // title update.
    uint64_t m_last_title_update_2MHz_cycles=0;
    uint64_t m_last_title_update_ticks=0;

    // copy of BeebThread state
    DriveState m_drives[NUM_DRIVES];

    // This is a shared_ptr since it can get passed to another thread
    // during the cloning process. It's set in the constructor and
    // then never modified.
    std::shared_ptr<BeebThread> m_beeb_thread;

    // Track Beeb key syms that were made pressed by a given
    // (unmodified) SDL keycode. SDL modifier key releases affect
    // future keycodes, so you can get (say) Shift+6 down, then Shift
    // up, then 6 up - which is no good if the states are only being
    // tracked by BBC key.
    //
    // This is a pretty crazy data type, but there's only so many keys
    // you can press per unit time.
    std::map<uint32_t,std::set<BeebKeySym>> m_beeb_keysyms_by_keycode;

    // Buffer for SDL keyboard events, so they can be handled as part of
    // the usual update.
    std::vector<SDL_KeyboardEvent> m_sdl_keyboard_events;

    BeebWindowSettings m_settings;

    //
    //bool m_ui=false;
    bool m_pushed_window_padding=false;
    ImGuiStuff *m_imgui_stuff=nullptr;

#if ENABLE_IMGUI_DEMO
    bool m_imgui_demo=false;
#endif
    bool m_imgui_dock_debug=false;
#if STORE_DRAWLISTS
    bool m_imgui_drawlists=false;
#endif
    bool m_imgui_debug=false;
    bool m_imgui_metrics=false;

    std::vector<std::string> m_display_size_options;

    std::unique_ptr<SettingsUI> m_popups[BeebWindowPopupType_MaxValue];

    bool m_messages_popup_ui_active=false;
    uint64_t m_messages_popup_ticks=0;
    uint32_t m_leds=0;

    bool m_leds_popup_ui_active=false;
    uint64_t m_leds_popup_ticks=0;

#if BBCMICRO_DEBUGGER
    bool m_test_pattern=false;
    mutable bool m_debug_halted=false;
    mutable bool m_got_debug_halted=false;
#endif

    //
    std::shared_ptr<MessageList> m_message_list;
    uint64_t m_msg_last_num_messages_printed=0;
    uint64_t m_msg_last_num_errors_and_warnings_printed=0;
#if VIDEO_TRACK_METADATA
    bool m_got_mouse_pixel_unit=false;
    VideoDataUnit m_mouse_pixel_unit={};
#endif

    SDL_Point m_mouse_pos={-1,-1};

#if HTTP_SERVER
    struct HTTPPoke {
        uint32_t addr=0;
        std::vector<uint8_t> data;
    };

    std::vector<HTTPPoke> m_http_pokes;
#endif

    const CommandContext m_cc{this,&ms_command_table};

    bool InitInternal();
    bool DoImGui(uint64_t ticks);
    bool HandleCommandKey(uint32_t keycode,const CommandContext *ccs,size_t num_ccs);
    bool DoMenuUI();
    CommandContext DoSettingsUI();
    void DoPopupUI(uint64_t now,int output_width,int output_height);
    void DoFileMenu();
    void DoEditMenu();
    void DoHardwareMenu();
    void DoKeyboardMenu();
    void DoToolsMenu();
    void DoDebugMenu();
    bool DoWindowMenu();
    BeebWindowInitArguments GetNewWindowInitArguments() const;
    void MaybeSaveConfig(bool save_config);
    void HardReset();
    void SaveState();
    bool SaveStateIsEnabled() const;
    bool HandleBeebKey(const SDL_Keysym &keysym,bool state);
    bool RecreateTexture();
    void Exit();
    void CleanUpRecentFilesLists();
    void ResetDockWindows();
    void ClearConsole();
    void PrintSeparator();
    void UpdateTVTexture(VBlankRecord *vblank_record);
    VBlankRecord *NewVBlankRecord(uint64_t ticks);
    bool DoBeebDisplayUI();

    template<BeebWindowPopupType>
    void TogglePopupCommand();

    template<BeebWindowPopupType>
    bool IsPopupCommandTicked() const;

    template<BeebWindowPopupType>
    static ObjectCommandTable<BeebWindow>::Initializer GetTogglePopupCommand();

    void Paste();
    void PasteThenReturn();
    void DoPaste(bool add_return);
    bool IsPasteTicked() const;

    template<bool IS_TEXT>
    void CopyOSWRCH();
    void SetClipboardData(std::vector<uint8_t> data,bool is_text);
    bool IsCopyOSWRCHTicked() const;

    void CopyBASIC();
    bool IsCopyBASICEnabled() const;

#if BBCMICRO_DEBUGGER
    void DebugStop();
    void DebugRun();
    void DebugStepOver();
    void DebugStepIn();
    void DebugStepInLocked(BBCMicro *m);
    bool DebugIsStopEnabled() const;
    bool DebugIsRunEnabled() const;
    bool DebugIsHalted() const;
#endif

    void ResetDefaultNVRAM();
    void SaveDefaultNVRAM();
    bool SaveDefaultNVRAMIsEnabled() const;

    void SaveConfig();

    void TogglePrioritizeCommandShortcuts();
    bool IsPrioritizeCommandShortcutsTicked() const;

    void ShowPrioritizeCommandShortcutsStatus();

    static std::unique_ptr<SettingsUI> CreateOptionsUI(BeebWindow *beeb_window);
    static std::unique_ptr<SettingsUI> CreateTimelineUI(BeebWindow *beeb_window);
    static std::unique_ptr<SettingsUI> CreateSavedStatesUI(BeebWindow *beeb_window);

    static ObjectCommandTable<BeebWindow> ms_command_table;
    static const SettingsUIMetadata ms_settings_uis[];

    // Keep this at the end. It's massive.
    Messages m_msg;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
