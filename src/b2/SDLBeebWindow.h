#ifndef HEADER_939433291D2A4C609E54F31A2B9EC042// -*- mode:c++ -*-
#define HEADER_939433291D2A4C609E54F31A2B9EC042

// Horrible name - will change.
//
// Looks after the SDL thread side window state.

#include "BeebWindow.h"
#include "Messages.h"
//#include <SDL.h>

#include <shared/enum_decl.h>
#include "SDLBeebWindow.inl"
#include <shared/enum_end.h>

struct BeebWindowInitArguments;
struct SDL_KeyboardEvent;
struct SDL_MouseMotionEvent;
class VBlankMonitor;
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

// TODO: remove
struct SDLThreadConstantOutput {
    ImTextureID font_texture_id={};
    ImTextureID tv_texture_id={};
};

// TODO: remove
struct SDLThreadVaryingOutput {
    int renderer_output_width=0;
    int renderer_output_height=0;

    // Buffer for SDL keyboard events, so they can be handled as part of
    // the usual update.
    std::vector<SDL_KeyboardEvent> sdl_keyboard_events;
    std::string sdl_text_input;

    // Mouse position.
    bool got_mouse_focus=false;
    SDL_Point mouse_pos={-1,-1};
    uint32_t mouse_buttons=0;
    SDL_Point mouse_wheel_delta={0,0};

    SDL_Keymod keymod=KMOD_NONE;
};

// TODO: remove
struct BeebThreadVaryingOutput {
    ImGuiMouseCursor imgui_mouse_cursor=ImGuiMouseCursor_None;
    bool filter_tv_texture=true;
    std::vector<ImDrawListUniquePtr> draw_lists;

    // this just points straight into the buffer. There's no locking; the SDL
    // thread just gets whatever it happens to be able to see at the time,
    void *tv_texture_data=nullptr;
};

// The placement data isn't used on OS X, but it's included anyway just to save
// on #if hell.
class SDLBeebWindow {
public:
    SDLBeebWindow()=default;
    ~SDLBeebWindow();

    bool Init(const BeebWindowInitArguments &init_arguments,
              const BeebWindowSettings &settings,
              std::shared_ptr<MessageList> message_list,
              std::vector<uint8_t> window_placement_data,
              uint32_t *sdl_window_id);

    //BeebWindow *GetBeebWindow() const;

    void HandleSDLKeyEvent(const SDL_KeyboardEvent &event);
    void SetSDLMouseWheelState(int x,int y);
    void HandleSDLTextInput(const char *text);
    void HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event);
    void HandleVBlank(VBlankMonitor *vblank_monitor,void *display_data,uint64_t ticks);

    void ThreadFillAudioBuffer(uint32_t audio_device_id,float *mix_buffer,size_t num_samples);

    void UpdateWindowPlacement();
    const std::vector<uint8_t> &GetWindowPlacementData() const;

    std::shared_ptr<MessageList> GetMessageList() const;
    
    UpdateResult UpdateBeeb();
    
    const BeebKeymap *GetKeymap() const;
    void SetKeymap(const BeebKeymap *keymap);
protected:
private:
    enum ImGuiTextures {
        ImGuiTexture_Font,
        ImGuiTexture_TV,
        ImGuiTexture_MaxValue,
    };

    // Message list and associated UI.
    std::shared_ptr<MessageList> m_message_list;
    uint64_t m_msg_last_num_messages_printed=0;
    uint64_t m_msg_last_num_errors_and_warnings_printed=0;
    bool m_messages_popup_ui_active=false;
    uint64_t m_messages_popup_ticks=0;

    BeebWindowInitArguments m_init_arguments;
    BeebWindowSettings m_settings;
    
    // Window title.
    uint64_t m_last_title_update_ticks=0;
    uint64_t m_last_title_update_2MHz_cycles=0;
    
    // SDL bits.
    SDL_Cursor *m_sdl_cursors[ImGuiMouseCursor_COUNT]={};
    SDL_Window *m_window=nullptr;
    SDL_Renderer *m_renderer=nullptr;
    SDL_PixelFormat *m_pixel_format=nullptr;
    //std::vector<SDL_KeyboardEvent> m_sdl_keyboard_events;
    std::string m_sdl_text_input;
    SDL_Point m_mouse_wheel_delta={};
    SDL_Texture *m_imgui_textures[ImGuiTexture_MaxValue]={};
    
    // Dear imgui.
    std::vector<ImDrawListUniquePtr> m_last_imgui_draw_lists;
    ImGuiStuff *m_imgui_stuff=nullptr;
    
    // Window placement data.
#if SYSTEM_WINDOWS
    void *m_hwnd=nullptr;
#elif SYSTEM_OSX
    void *m_nswindow=nullptr;
#elif SYSTEM_LINUX
#include <shared/pshpack1.h>
    struct WindowPlacementData {
        uint8_t maximized;
        int x,y;
        int width,height;
    };
#include <shared/poppack.h>
#endif
    std::vector<uint8_t> m_window_placement_data;
    
    // Keyboard input.
    const BeebKeymap *m_keymap=nullptr;
    bool m_prefer_shortcuts=false;
    BeebShiftState m_fake_shift_state=BeebShiftState_Any;
    bool m_auto_boot=false;
    KeyStates m_real_key_states;
    
    // Keycodes that are to be used to execute commands.
    //
    // The 
    std::vector<uint32_t> m_command_keycodes;
    
    // Set if the BBC display panel has focus. This isn't entirely regular,
    // because the BBC display panel is handled by separate code - this will
    // probably get fixed eventually.
    //
    // The BBC display panel never has any dear imgui text widgets in it.
    bool m_beeb_focus=false;
    
    // Track Beeb key syms that were made pressed by a given
    // (unmodified) SDL keycode. SDL modifier key releases affect
    // future keycodes, so you can get (say) Shift+6 down, then Shift
    // up, then 6 up - which is no good if the states are only being
    // tracked by BBC key.
    //
    // This is a pretty crazy data type, but there's only so many keys
    // you can press per unit time.
    std::map<uint32_t,std::set<BeebKeySym>> m_beeb_keysyms_by_keycode;
    
    // Emulator stuff.
    BeebLoadedConfig m_current_config;
    BBCMicro *m_beeb=nullptr;
    TVOutput m_tv;
    std::vector<uint32_t> m_tv_texture_pixels;

    uint32_t m_leds=0;
    bool m_leds_popup_ui_active=false;
    uint64_t m_leds_popup_ticks=0;

#if ENABLE_IMGUI_DEMO
    bool m_imgui_demo=false;
#endif
    bool m_imgui_dock_debug=false;
    bool m_imgui_debug=false;
    bool m_imgui_metrics=false;
#if STORE_DRAWLISTS
    bool m_imgui_drawlists=false;
    std::vector<ImGuiStuff::StoredDrawList> m_stored_draw_lists;
#endif
    
    bool InitInternal(std::vector<uint8_t> window_placement_data);
    bool RecreateTexture();
    void RenderLastImGuiFrame();
    void RunNextImGuiFrame(uint64_t ticks);
    void HardReset(uint32_t flags,
                   const BeebLoadedConfig &loaded_config,
                   const std::vector<uint8_t> &nvram_contents);
    bool DoBeebDisplayUI();
    void DoMenuUI();
    void DoFileMenu();
    void DoEditMenu();
    void DoHardwareMenu();
    void DoKeyboardMenu();
    void DoToolsMenu();
    void DoDebugMenu();
    void DoPopupUI(uint64_t now,int output_width,int output_height);
    void UpdateTitle(uint64_t ticks);
    void ShowPrioritizeCommandShortcutsStatus();
    bool HandleBeebKey(const SDL_Keysym &keysym,bool state);
    void RecordKeySym(BeebKeySym beeb_key_sym,bool down);
    void RecordKey(BeebKey beeb_key,bool down);
    void SetKeyState(BeebKey beeb_key,bool down);
    void SetFakeShiftState(BeebShiftState shift_state);
    void SetAutoBootState(bool auto_boot);
    void UpdateShiftKeyState();
    
    // Keep this at the end. It's massive.
    Messages m_msg;
};

#endif
