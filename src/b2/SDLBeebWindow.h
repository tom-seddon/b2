#ifndef HEADER_939433291D2A4C609E54F31A2B9EC042// -*- mode:c++ -*-
#define HEADER_939433291D2A4C609E54F31A2B9EC042

// Horrible name - will change.
//
// Looks after the SDL thread side window state.

#include "BeebWindow.h"
#include "Messages.h"
//#include <SDL.h>

struct BeebWindowInitArguments;
struct SDL_KeyboardEvent;
struct SDL_MouseMotionEvent;
class VBlankMonitor;
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

struct SDLThreadConstantOutput {
    ImTextureID font_texture_id={};
    ImTextureID tv_texture_id={};
};

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

struct BeebThreadVaryingOutput {
    ImGuiMouseCursor imgui_mouse_cursor=ImGuiMouseCursor_None;
    bool filter_tv_texture=true;
    std::shared_ptr<std::vector<ImDrawListUniquePtr>> draw_lists;

    // this just points straight into the buffer. There's no locking; the SDL
    // thread just gets whatever it happens to be able to see at the time,
    uint8_t *tv_texture_data=nullptr;
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
    void UpdateTitle();

    void ThreadFillAudioBuffer(uint32_t audio_device_id,float *mix_buffer,size_t num_samples);

    void UpdateWindowPlacement();
    const std::vector<uint8_t> &GetWindowPlacementData() const;

    std::shared_ptr<MessageList> GetMessageList() const;
protected:
private:
    enum ImGuiTextures {
        ImGuiTexture_Font,
        ImGuiTexture_TV,
        ImGuiTexture_MaxValue,
    };

    std::shared_ptr<MessageList> m_message_list;
    BeebWindowInitArguments m_init_arguments;
    BeebWindow *m_beeb_window=nullptr;
    SDL_Cursor *m_sdl_cursors[ImGuiMouseCursor_COUNT]={};
    SDL_Window *m_window=nullptr;
    SDL_Renderer *m_renderer=nullptr;
    SDL_PixelFormat *m_pixel_format=nullptr;
    std::vector<SDL_KeyboardEvent> m_sdl_keyboard_events;
    std::string m_sdl_text_input;
    int m_mouse_wheel_delta_x=0;
    int m_mouse_wheel_delta_y=0;
//    int m_mouse_pos_x=0;
//    int m_mouse_pos_y=0;
    SDL_Texture *m_imgui_textures[ImGuiTexture_MaxValue]={};
    std::vector<uint8_t> m_window_placement_data;

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

    bool InitInternal(std::vector<uint8_t> window_placement_data);
    bool RecreateTexture();

    // Keep this at the end. It's massive.
    Messages m_msg;
};

#endif
