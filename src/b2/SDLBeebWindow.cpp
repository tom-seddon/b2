#include <shared/system.h>
#include "SDLBeebWindow.h"
#include "BeebWindow.h"
#include <SDL.h>
#include "VBlankMonitor.h"
#include "load_save.h"
#include <SDL_syswm.h>

//SDLBeebWindow::SDLBeebWindow(const BeebWindowInitArguments &init_arguments,
//                             const BeebWindowSettings &settings)
//{
//    m_init_arguments=init_arguments;
//
//    m_message_list=std::make_shared<MessageList>();
//    m_msg=Messages(m_message_list);
//
//    m_beeb_window=new BeebWindow(init_arguments,
//                                 settings,
//                                 m_message_list);
//}

SDLBeebWindow::~SDLBeebWindow() {
//    delete m_beeb_window;
//    m_beeb_window=nullptr;
    
    delete m_imgui_stuff;
    m_imgui_stuff=nullptr;

    for(size_t i=0;i<sizeof m_sdl_cursors/sizeof m_sdl_cursors[0];++i) {
        SDL_FreeCursor(m_sdl_cursors[i]);
        m_sdl_cursors[i]=nullptr;
    }

    for(SDL_Texture *&texture:m_imgui_textures) {
        SDL_DestroyTexture(texture);
        texture=nullptr;
    }

    if(m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer=nullptr;
    }

    if(m_window) {
        SDL_DestroyWindow(m_window);
        m_window=nullptr;
    }

    if(m_pixel_format) {
        SDL_FreeFormat(m_pixel_format);
        m_pixel_format=nullptr;
    }
}

bool SDLBeebWindow::Init(const BeebWindowInitArguments &init_arguments,
                         const BeebWindowSettings &settings,
                         std::shared_ptr<MessageList> message_list,
                         std::vector<uint8_t> window_placement_data,
                         uint32_t *sdl_window_id)
{
    m_init_arguments=init_arguments;

    m_message_list=message_list;
    m_msg=Messages(m_message_list);

    //m_beeb_window=new BeebWindow();

    if(!this->InitInternal(std::move(window_placement_data))) {
        return false;
    }

    m_imgui_stuff=new ImGuiStuff();
    if(!m_imgui_stuff->Init(m_renderer,&m_imgui_textures[ImGuiTexture_Font])) {
        m_msg.e.f("failed to initialize ImGui\n");
        return false;
    }

//    if(!m_beeb_window->Init(m_init_arguments,
//                            settings,
//                            m_message_list,
//                            std::move(imgui_stuff),
//                            m_pixel_format))
//    {
//        return false;
//    }

    *sdl_window_id=SDL_GetWindowID(m_window);

    return true;
}

//BeebWindow *SDLBeebWindow::GetBeebWindow() const {
//    return m_beeb_window;
//}

//void SDLBeebWindow::SaveSettings() {
//    m_beeb_window->SaveSettings();
//    this->SavePosition();
//}

void SDLBeebWindow::HandleSDLKeyEvent(const SDL_KeyboardEvent &event) {
    m_sdl_keyboard_events.push_back(event);
}

void SDLBeebWindow::SetSDLMouseWheelState(int x,int y) {
    m_mouse_wheel_delta_x=x;
    m_mouse_wheel_delta_y=y;
}

void SDLBeebWindow::HandleSDLTextInput(const char *text) {
    m_sdl_text_input.append(text);
}

void SDLBeebWindow::HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event) {
    //m_beeb_window->HandleSDLMouseMotionEvent(event);

    // is this actually necessary??
}

void SDLBeebWindow::HandleVBlank(VBlankMonitor *vblank_monitor,void *display_data,uint64_t ticks) {
    // There's an API for exactly this on Windows. But it's probably
    // better to have the same code on every platform. 99% of the time
    // (and possibly even more often than that...) this will get the
    // right display.
    int wx,wy;
    SDL_GetWindowPosition(m_window,&wx,&wy);

    int ww,wh;
    SDL_GetWindowSize(m_window,&ww,&wh);

    void *dd=vblank_monitor->GetDisplayDataForPoint(wx+ww/2,wy+wh/2);
    if(dd!=display_data) {
        return;
    }
    
    // Quick as possible, draw the last frame's stuff, so it'll be ready before the
    // vertical blank is done.
    //
    // The UI is one frame behind, which is annoying, but the Beeb TV texture will
    // be up to date...
    //
    // Revisit this. Perhaps dear imgui will draw everything quickly enough? Update
    // order could also be a toggle.
    this->RenderLastImGuiFrame();
    this->RunNextImGuiFrame(ticks);
}
 
void SDLBeebWindow::RenderLastImGuiFrame() {
    m_tv.CopyTexturePixels(&m_tv_texture_pixels);
    
    SDL_UpdateTexture(m_imgui_textures[ImGuiTexture_TV],nullptr,m_tv_texture_pixels.data(),TV_TEXTURE_WIDTH*4);
    
    SDL_RenderClear(m_renderer);

    if(!m_last_imgui_draw_lists.empty()) {
        ImGuiStuff::RenderSDL(m_renderer,
                              m_last_imgui_draw_lists,
                              nullptr,
                              m_imgui_textures,
                              sizeof m_imgui_textures/sizeof m_imgui_textures[0]);
    }
    
    SDL_RenderPresent(m_renderer);
}

void SDLBeebWindow::RunNextImGuiFrame(uint64_t ticks) {
    ImGuiContextSetter setter(m_imgui_stuff);
    
    ImGuiStyleVarPusher pusher(ImGuiStyleVar_WindowPadding,ImVec2(0.f,0.f));
    
    bool got_mouse_focus;
    SDL_Window *mouse_window=SDL_GetMouseFocus();
    if(mouse_window==m_window) {
        got_mouse_focus=true;
    } else {
        got_mouse_focus=false;
    }
    
    SDL_Point mouse_pos;
    uint32_t mouse_buttons=SDL_GetMouseState(&mouse_pos.x,&mouse_pos.y);
    
    SDL_Point mouse_wheel_delta={m_mouse_wheel_delta_x,m_mouse_wheel_delta_y};
    m_mouse_wheel_delta_x=0;
    m_mouse_wheel_delta_y=0;
    
    SDL_Keymod keymod=SDL_GetModState();
    
    int renderer_output_width;
    int renderer_output_height;
    SDL_GetRendererOutputSize(m_renderer,
                              &renderer_output_width,
                              &renderer_output_height);
    
    for(const SDL_KeyboardEvent &event:m_sdl_keyboard_events) {
        switch(event.type) {
            case SDL_KEYDOWN:
                if(event.repeat) {
                    // Don't set again if it's just key repeat. If the flag is
                    // still set from last time, that's fine; if it's been reset,
                    // there'll be a reason, so don't set it again.
                } else {
                    m_imgui_stuff->SetKeyDown(event.keysym.scancode,true);
                }
                break;
                
            case SDL_KEYUP:
                m_imgui_stuff->SetKeyDown(event.keysym.scancode,false);
                break;
        }
    }
    m_sdl_keyboard_events.clear();
    
    m_imgui_stuff->AddInputCharactersUTF8(m_sdl_text_input.c_str());
    m_sdl_text_input.clear();
    
    std::string text_input=std::move(m_sdl_text_input);
    
    m_imgui_stuff->NewFrame(got_mouse_focus,
                            mouse_pos,
                            mouse_buttons,
                            mouse_wheel_delta,
                            keymod,
                            renderer_output_width,
                            renderer_output_height,
                            (ImTextureID)ImGuiTexture_Font);
    
    ImGui::ShowDemoWindow();
    
    m_imgui_stuff->RenderImGui();
    
    m_last_imgui_draw_lists=m_imgui_stuff->CloneDrawLists();
}

//
//    //
//    SDLThreadConstantOutput sdl_koutput;
//    {
//        sdl_koutput.font_texture_id=(ImTextureID)ImGuiTexture_Font;
//        sdl_koutput.tv_texture_id=(ImTextureID)ImGuiTexture_TV;
//    }
//
//    //
//    SDLThreadVaryingOutput sdl_voutput;
//    {
//        SDL_Window *mouse_window=SDL_GetMouseFocus();
//        if(mouse_window==m_window) {
//            sdl_voutput.got_mouse_focus=true;
//        } else {
//            sdl_voutput.got_mouse_focus=false;
//        }
//
//        sdl_voutput.mouse_buttons=SDL_GetMouseState(&sdl_voutput.mouse_pos.x,
//                                                    &sdl_voutput.mouse_pos.y);
//
//        sdl_voutput.mouse_wheel_delta.x=m_mouse_wheel_delta_x;
//        sdl_voutput.mouse_wheel_delta.y=m_mouse_wheel_delta_y;
//        m_mouse_wheel_delta_x=0;
//        m_mouse_wheel_delta_y=0;
//
//        sdl_voutput.keymod=SDL_GetModState();
//
//        sdl_voutput.sdl_keyboard_events.swap(m_sdl_keyboard_events);
//
//        sdl_voutput.sdl_text_input.swap(m_sdl_text_input);
//
//        SDL_GetRendererOutputSize(m_renderer,
//                                  &sdl_voutput.renderer_output_width,
//                                  &sdl_voutput.renderer_output_height);
//    }
//
//    BeebThreadVaryingOutput beeb_voutput;
//
//    m_beeb_window->HandleVBlank(ticks,sdl_koutput,sdl_voutput,&beeb_voutput);
//
//    if(beeb_voutput.imgui_mouse_cursor>=0&&
//       beeb_voutput.imgui_mouse_cursor<ImGuiMouseCursor_COUNT&&
//       m_sdl_cursors[beeb_voutput.imgui_mouse_cursor])
//    {
//        SDL_SetCursor(m_sdl_cursors[beeb_voutput.imgui_mouse_cursor]);
//    } else {
//        SDL_SetCursor(nullptr);
//    }
//
//    SDL_UpdateTexture(m_imgui_textures[ImGuiTexture_TV],nullptr,beeb_voutput.tv_texture_data,TV_TEXTURE_WIDTH*4);
//
//    SDL_RenderClear(m_renderer);
//
//    ImGuiStuff::RenderSDL(m_renderer,beeb_voutput.draw_lists,nullptr,m_imgui_textures,sizeof m_imgui_textures/sizeof m_imgui_textures[0]);
//
//    SDL_RenderPresent(m_renderer);

void SDLBeebWindow::UpdateTitle() {
//    char title[100];
//    m_beeb_window->GetTitle(title,sizeof title);
//
//    SDL_SetWindowTitle(m_window,title);
}

void SDLBeebWindow::ThreadFillAudioBuffer(SDL_AudioDeviceID audio_device_id,float *mix_buffer,size_t num_samples) {
//    m_beeb_window->ThreadFillAudioBuffer(audio_device_id,mix_buffer,num_samples);
}

void SDLBeebWindow::UpdateWindowPlacement() {
#if SYSTEM_WINDOWS

    if(m_hwnd) {
        m_window_placement_data.resize(sizeof(WINDOWPLACEMENT));

        auto wp=(WINDOWPLACEMENT *)m_window_placement_data.data();
        memset(wp,0,sizeof *wp);
        wp->length=sizeof *wp;

        if(!GetWindowPlacement((HWND)m_hwnd,wp)) {
            m_window_placement_data.clear();
        }
    }

#elif SYSTEM_OSX

    SaveCocoaFrameUsingName(m_nswindow,COCOA_FRAME_NAME);

#else

    WindowPlacementData *wp;
    if(m_window_placement_data.size()!=sizeof(WindowPlacementData)) {
        m_window_placement_data.clear();
        m_window_placement_data.resize(sizeof(WindowPlacementData));

        wp=(WindowPlacementData *)m_window_placement_data.data();

        wp->maximized=0;
        wp->x=INT_MIN;
        wp->y=INT_MIN;
        wp->width=0;
        wp->height=0;
    } else {
        wp=(WindowPlacementData *)m_window_placement_data.data();
    }

    uint32_t flags=SDL_GetWindowFlags(m_window);

    wp->maximized=!!(flags&SDL_WINDOW_MAXIMIZED);

    if(flags&(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_MINIMIZED|SDL_WINDOW_HIDDEN)) {
        // Don't update the size in this case.
    } else {
        SDL_GetWindowPosition(m_window,&wp->x,&wp->y);
        SDL_GetWindowSize(m_window,&wp->width,&wp->height);
    }

#endif
}

const std::vector<uint8_t> &SDLBeebWindow::GetWindowPlacementData() const {
    return m_window_placement_data;
}

std::shared_ptr<MessageList> SDLBeebWindow::GetMessageList() const {
    return m_message_list;
}

bool SDLBeebWindow::InitInternal(std::vector<uint8_t> window_placement_data) {
    // Add some extra space round the edges so the display doesn't have to
    // be scaled down noticeably.
    //
    // 19 is the height of the dear imgui menu bar with the default font.
    // (Ideally this would be retrieved at runtime, but that can't be done
    // until after the window is created.)
    //
    // Maddeningly, this still isn't quite perfect - at least on OS X. It
    // seems like there's a window border that's drawn on top of everything,
    // inside the window? Bleargh. The dear imgui window position is probably
    // wrong as well. Maybe all the border size saving and restoring is
    // causing problems.
    //
    // Anyway, obvious with the test pattern, but in practice not an issue,
    // as the borders are so large...
    m_window=SDL_CreateWindow("",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              TV_TEXTURE_WIDTH+(int)(IMGUI_DEFAULT_STYLE.WindowPadding.x*2.f),
                              19+TV_TEXTURE_HEIGHT+(int)(IMGUI_DEFAULT_STYLE.WindowPadding.y*2.f),
                              SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL);
    if(!m_window) {
        m_msg.e.f("SDL_CreateWindow failed: %s\n",SDL_GetError());
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengl");
    m_renderer=SDL_CreateRenderer(m_window,-1,0);
    if(!m_renderer) {
        m_msg.e.f("SDL_CreateRenderer failed: %s\n",SDL_GetError());
        return false;
    }

    if(SDL_GL_GetCurrentContext()) {
        if(SDL_GL_SetSwapInterval(0)!=0) {
            m_msg.i.f("failed to set GL swap interval to 0: %s\n",SDL_GetError());
        }
    }

    // The OpenGL driver supports SDL_PIXELFORMAT_ARGB8888.
    m_pixel_format=SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    if(!m_pixel_format) {
        m_msg.e.f("SDL_AllocFormat failed: %s\n",SDL_GetError());
        return false;
    }

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    SDL_GetWindowWMInfo(m_window,&wmi);

#if SYSTEM_WINDOWS

    m_hwnd=wmi.info.win.window;

    if(!m_init_arguments.reset_windows) {
        if(m_hwnd) {
            if(window_placement_data.size()==sizeof(WINDOWPLACEMENT)) {
                m_window_placement_data=std::move(window_placement_data);
                auto wp=(const WINDOWPLACEMENT *)m_window_placement_data.data();

                if(!SetWindowPlacement((HWND)m_hwnd,wp)) {
                    m_window_placement_data.clear();
                }
            }
        }
    }

#elif SYSTEM_OSX

    (void)window_placement_data;

    m_nswindow=wmi.info.cocoa.window;

    if(!m_init_arguments.reset_windows) {
        SetCocoaFrameUsingName(m_nswindow,COCOA_FRAME_NAME);
    }

#else

    if(!m_init_arguments.reset_windows) {
        if(window_placement_data.size()==sizeof(WindowPlacementData)) {
            m_window_placement_data=std::move(window_placement_data);
            auto wp=(const WindowPlacementData *)m_window_placement_data.data();

            SDL_RestoreWindow(m_window);

            if(wp->x!=INT_MIN&&wp->y!=INT_MIN) {
                SDL_SetWindowPosition(m_window,wp->x,wp->y);
            }

            if(wp->width>0&&wp->height>0) {
                SDL_SetWindowSize(m_window,wp->width,wp->height);
            }

            if(wp->maximized) {
                SDL_MaximizeWindow(m_window);
            }
        }
    }

#endif

    SDL_RendererInfo info;
    if(SDL_GetRendererInfo(m_renderer,&info)<0) {
        m_msg.e.f("SDL_GetRendererInfo failed: %s\n",SDL_GetError());
        return false;
    }

    if(!this->RecreateTexture()) {
        return false;
    }

    if(m_pixel_format->BitsPerPixel!=32) {
        m_msg.e.f("Pixel format not 32 bpp\n");
        return false;
    }

    m_sdl_cursors[ImGuiMouseCursor_Arrow]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    m_sdl_cursors[ImGuiMouseCursor_ResizeAll]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    m_sdl_cursors[ImGuiMouseCursor_ResizeEW]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    m_sdl_cursors[ImGuiMouseCursor_ResizeNS]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    m_sdl_cursors[ImGuiMouseCursor_ResizeNESW]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
    m_sdl_cursors[ImGuiMouseCursor_ResizeNWSE]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
    m_sdl_cursors[ImGuiMouseCursor_TextInput]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);

    {
        Uint32 format;
        int width,height;
        SDL_QueryTexture(m_imgui_textures[ImGuiTexture_TV],&format,nullptr,&width,&height);
        m_msg.i.f("Renderer: %s, %dx%d %s\n",
                  info.name,
                  width,
                  height,
                  SDL_GetPixelFormatName(format));
    }
    
    m_tv_texture_pixels.resize(TV_TEXTURE_WIDTH*TV_TEXTURE_HEIGHT);

    return true;
}

bool SDLBeebWindow::RecreateTexture() {
    if(m_imgui_textures[ImGuiTexture_TV]) {
        SDL_DestroyTexture(m_imgui_textures[ImGuiTexture_TV]);
        m_imgui_textures[ImGuiTexture_TV]=nullptr;
    }

    //SetRenderScaleQualityHint(m_settings.display_filter);

    m_imgui_textures[ImGuiTexture_TV]=SDL_CreateTexture(m_renderer,m_pixel_format->format,SDL_TEXTUREACCESS_STREAMING,TV_TEXTURE_WIDTH,TV_TEXTURE_HEIGHT);
    if(!m_imgui_textures[ImGuiTexture_TV]) {
        m_msg.e.f("Failed to create TV texture: %s\n",SDL_GetError());
        return false;
    }

    return true;
}
