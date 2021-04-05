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
class FileMenuItem;

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

    BeebWindowLEDsPopupMode leds_popup_mode=BeebWindowLEDsPopupMode_Auto;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO - there are items that only apply to the first window, and they should
// probably go somewhere else...
struct BeebWindowInitArguments {
public:
    // SDL_AudioDeviceID of device to play to. The window doesn't
    // create the audio device, but it needs to know which ID to use
    // so it can respond to the appropriate callback.
    uint32_t sound_device=0;

    // Sound playback details.
    SDL_AudioSpec sound_spec={};

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

    // If non-empty, name of the initial keymap to select.
    //
    // (Keymap pointer is no good, as this struct gets sent to the
    // BeebThread; the keymap could be deleted by the time it's used.)
    std::string keymap_name;

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

    bool Init(uint32_t *sdl_window_id);

    // Saves all settings, including whatever SavePosition does. Called on
    // shutdown and from the UI code.
    void SaveSettings();

    // Saves position only. Called by SaveSettings and from the message loop.
    void SavePosition();

    // called by message loop
    void HandleSDLKeyEvent(const SDL_KeyboardEvent &event);
    void SetSDLMouseWheelState(int x,int y);
    void HandleSDLTextInput(const char *text);
    void HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event);
    void HandleVBlank(VBlankMonitor *vblank_monitor,void *display_data,uint64_t ticks);
    void UpdateTitle();

    // called from audio thread
    void ThreadFillAudioBuffer(SDL_AudioDeviceID audio_device_id,float *mix_buffer,size_t num_samples);

    // Get pointer to this window's BeebThread. (The lifespan of the
    // BeebThread is the same as that of the window.) (already MT OK)
    std::shared_ptr<BeebThread> GetBeebThread() const;

    // called by various bits and pieces (already MT OK)
    std::shared_ptr<MessageList> GetMessageList() const;

    // used by DataRateUI
    std::vector<VBlankRecord> GetVBlankRecords() const;

    // used by KeymapsUI
    void BeebKeymapWillBeDeleted(BeebKeymap *keymap);
    bool GetBeebKeyState(BeebKey key) const;
    const BeebKeymap *GetCurrentKeymap() const;
    void SetCurrentKeymap(const BeebKeymap *keymap);

#if VIDEO_TRACK_METADATA
    // used by debugger
    const VideoDataUnit *GetVideoDataUnitForMousePixel() const;
#endif

    // used by debugger
    SettingsUI *GetPopupByType(BeebWindowPopupType type) const;

    // used by ConfigsUI
    const std::string &GetConfigName() const;
protected:
private:
    struct SettingsUIMetadata;

    struct BeebThreadInitData {
        SDL_Texture *tv_texture=nullptr;
    };

    // Stuff from the SDL event thread for the Beeb thread.
    struct SDLThreadOutput {
        // Buffer for SDL keyboard events, so they can be handled as part of
        // the usual update.
        std::vector<SDL_KeyboardEvent> sdl_keyboard_events;

        // Mouse position.
        bool got_mouse_focus=false;
        SDL_Point mouse_pos={-1,-1};
        uint32_t mouse_buttons=0;
        SDL_Point mouse_wheel_delta={0,0};

        SDL_Keymod keymod=KMOD_NONE;
    };

    // Stuff from the Beeb thread for the SDL event thread.
    struct SDLThreadInput {
        ImGuiMouseCursor imgui_mouse_cursor=ImGuiMouseCursor_None;
    };

    struct DriveState {
        SaveFileDialog new_disc_image_file_dialog;
        OpenFileDialog open_disc_image_file_dialog;
        SaveFileDialog new_direct_disc_image_file_dialog;
        OpenFileDialog open_direct_disc_image_file_dialog;

        DriveState();
    };

    //
    SDL_Cursor *m_sdl_cursors[ImGuiMouseCursor_COUNT]={};

    // The same mutex serializes access to both input and output.
    Mutex m_sdl_thread_io_mutex;
    SDLThreadInput m_sdl_thread_input;
    SDLThreadOutput m_sdl_thread_output;

    BeebWindowInitArguments m_init_arguments;

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
    std::vector<uint8_t> m_tv_texture_buffer;
    SDL_Texture *m_tv_texture=nullptr;

    float m_blend_amt=0.f;

    // Audio output
    SDL_AudioDeviceID m_sound_device=0;

    bool m_prefer_shortcuts=false;

    // number of emulated us that had passed at the time of the last
    // title update.ƒ
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

#if HTTP_SERVER
    struct HTTPPoke {
        uint32_t addr=0;
        std::vector<uint8_t> data;
    };

    std::vector<HTTPPoke> m_http_pokes;
#endif

    const CommandContext m_cc{this,&ms_command_table};

    void HandleVBlank(uint64_t ticks);
    bool HardReset(const BeebConfig &config);
    bool InitInternal();
    void DoImGui(uint64_t ticks,const std::vector<SDL_KeyboardEvent> &sdl_keyboard_events);
    bool HandleCommandKey(uint32_t keycode,const CommandContext *ccs,size_t num_ccs);
    void DoMenuUI();
    CommandContext DoSettingsUI();
    void DoPopupUI(uint64_t now,int output_width,int output_height);
    void DoFileMenu();
    void DoDiscDriveSubMenu(int drive,const std::shared_ptr<const DiscImage> &disc_image);
    void DoDiscImageSubMenu(int drive,bool boot);
    void DoDiscImageSubMenuItem(int drive,std::shared_ptr<DiscImage> disc_image,FileMenuItem *item,bool boot);
    void DoEditMenu();
    void DoHardwareMenu();
    void DoKeyboardMenu();
    void DoToolsMenu();
    void DoDebugMenu();
    bool DoWindowMenu();
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
    static size_t ConsumeTVTexture(OutputDataBuffer<VideoDataUnit> *video_output,TVOutput *tv,bool inhibit_update);
    bool InhibitUpdateTVTexture() const;
    void BeginUpdateTVTexture(bool threaded,void *dest_pixels,int dest_pitch);
    void EndUpdateTVTexture(bool threaded,VBlankRecord *vblank_record,void *dest_pixels,int dest_pitch);
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
