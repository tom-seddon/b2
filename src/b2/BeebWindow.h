#ifdef HEADER_33477416E2224FD8A7F0B58AC38F831D// -*- mode:c++ -*-
// sorry... this header is too much of a monster.
#error
#else
#define HEADER_33477416E2224FD8A7F0B58AC38F831D

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

struct BeebWindowInitArguments;
class VBlankMonitor;
class Keymap;
class BeebThread;
class TimelineUI;
struct SDL_Texture;
struct SDL_Renderer;
struct Keycap;
class ConfigsUI;
class BeebConfig;
class BeebLoadedConfig;
class KeymapsUI;
class MessagesUI;
#if BBCMICRO_TRACE
class TraceUI;
#endif
class NVRAMUI;
class AudioCallbackUI;

#include "keys.h"
#include "BeebWindowInitArguments.h"
#include "dear_imgui.h"
#include <string>
#include <SDL.h>
#include "TVOutput.h"
#include "native_ui.h"
#include <beeb/conf.h>
#include <shared/log.h>
#include <functional>
#include "Messages.h"
#include <map>
#include <set>
#include <limits.h>

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

struct Defaults {
    uint32_t ui_flags=0;

    float bbc_volume=0.f;
    float disc_volume=0.f;

    bool display_auto_scale=true;
    float display_overall_scale=1.f;
    float display_scale_x=1.2f/1.25f;
    float display_scale_y=1.f;
    BeebWindowDisplayAlignment display_alignment_x=BeebWindowDisplayAlignment_Centre;
    BeebWindowDisplayAlignment display_alignment_y=BeebWindowDisplayAlignment_Centre;
    bool display_filter=false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebWindow {
public:
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

    bool GetBeebKeyState(uint8_t key) const;

    uint32_t GetSDLWindowID() const;

    void HandleSDLKeyEvent(const SDL_KeyboardEvent &event);

    void SetSDLMouseWheelState(int x,int y);
    void HandleSDLTextInput(const char *text);

    void ThreadFillAudioBuffer(SDL_AudioDeviceID audio_device_id,float *mix_buffer,size_t num_samples);

    bool HandleVBlank(VBlankMonitor *vblank_monitor,void *display_data,uint64_t ticks);

    bool HandleVBlank(uint64_t ticks);

    void UpdateTitle();

    void KeymapWillBeDeleted(Keymap *keymap);

    // If this BeebWindow has a SDL_Texture holding the current
    // display contents that's suitable for rendering with the given
    // SDL_Renderer, return a pointer to it. Otherwise, nullptr.
    SDL_Texture *GetTextureForRenderer(SDL_Renderer *renderer) const;

    // If this BeebWindow's texture data has changed since the last
    // time this function was called with the given version object, or
    // if it's the first time of calling with the given version
    // object, fill the out parameters appropriately and return true.
    // Otherwise, out parameters are filled with unspecified values,
    // and returns false.
    bool GetTextureData(BeebWindowTextureDataVersion *version,
        const SDL_PixelFormat **format_ptr,const void **pixels_ptr) const;

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
protected:
private:
    struct DriveState {
        FolderDialog open_65link_folder_dialog;
        OpenFileDialog open_disc_image_file_dialog;

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
    uint64_t m_vblank_ticks[NUM_VBLANK_RECORDS]={};
    //int m_us_per_vblank[NUM_VBLANK_RECORDS]={};
    size_t m_vblank_index=0;

    // TV output.
    TVOutput m_tv;
    SDL_Texture *m_tv_texture=nullptr;

    BeebWindowDisplayAlignment m_display_alignment_x;
    BeebWindowDisplayAlignment m_display_alignment_y;
    bool m_auto_scale;
    float m_overall_scale;
    
    // The default is 1x2, since the texture is 640x272 or so.
    float m_tv_scale_x;
    float m_tv_scale_y;
    float m_blend_amt=0.f;

    bool m_display_filter=false;

    // Audio output
    SDL_AudioDeviceID m_sound_device=0;

    const Keymap *m_keymap;

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

    uint32_t m_ui_flags=0;

    //
    //bool m_ui=false;
    ImGuiStuff *m_imgui_stuff=nullptr;
    bool m_imgui_has_kb_focus=false;

#if ENABLE_IMGUI_DEMO
    bool m_imgui_demo=false;
#endif

    std::vector<std::string> m_display_size_options;

    std::unique_ptr<KeymapsUI> m_keymaps_ui;
    std::unique_ptr<MessagesUI> m_messages_ui;
#if TIMELINE_UI_ENABLED
    std::unique_ptr<TimelineUI> m_timeline_ui;
#endif
    std::unique_ptr<ConfigsUI> m_configs_ui;
#if BBCMICRO_TRACE
    std::unique_ptr<TraceUI> m_trace_ui;
#endif
    std::unique_ptr<NVRAMUI> m_nvram_ui;
    std::unique_ptr<AudioCallbackUI> m_audio_callback_ui;

    bool m_messages_popup_ui_active=false;
    uint64_t m_messages_popup_ticks=0;
    uint32_t m_leds=0;

    bool m_leds_popup_ui_active=false;
    uint64_t m_leds_popup_ticks=0;

    //
    std::shared_ptr<MessageList> m_message_list;
    uint64_t m_msg_last_num_messages_printed=0;
    uint64_t m_msg_last_num_errors_and_warnings_printed=0;
    // Keep this towards the end. It's quite a large object.
    Messages m_msg;

    bool InitInternal();
    bool LoadDiscImageFile(int drive,const std::string &path);
    bool Load65LinkFolder(int drive,const std::string &path);
    void DoOptionsGui();
    bool DoImGui(int output_width,int output_height);
    BeebWindowInitArguments GetNewWindowInitArguments() const;
    void MaybeSaveConfig(bool save_config);
    void DoOptionsCheckbox(const char *label,bool (BeebThread::*get_mfn)() const,void (BeebThread::*send_mfn)(bool));
    void HandleBeebSpecialKey(uint8_t beeb_key,bool state);
    void LoadLastState();
    void SaveState();
    bool HandleBeebKey(const SDL_Keysym &keysym,bool state);
    bool RecreateTexture();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
