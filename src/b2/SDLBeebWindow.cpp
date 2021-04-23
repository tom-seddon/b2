#include <shared/system.h>
#include "SDLBeebWindow.h"
#include "BeebWindow.h"
#include <SDL.h>
#include "VBlankMonitor.h"
#include "load_save.h"
#include <SDL_syswm.h>
#include <beeb/BBCMicro.h>
#include <beeb/sound.h>
#include "misc.h"
#include "dear_imgui.h"
#include "MessagesUI.h"
#include "BeebWindows.h"

#include <shared/enum_def.h>
#include "SDLBeebWindow.inl"
#include <shared/enum_end.h>

static const double MESSAGES_POPUP_TIME_SECONDS=2.5;
static const double LEDS_POPUP_TIME_SECONDS=1.;
static const double TITLE_UPDATE_TIME_SECONDS=.5;

//struct BeebStuff {
//    BBCMicro *beeb=nullptr;
//    BeebLoadedConfig current_config;
//};

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
    delete m_beeb;
    m_beeb=nullptr;
    
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
    m_settings=settings;

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
    m_mouse_wheel_delta.x=x;
    m_mouse_wheel_delta.y=y;
}

void SDLBeebWindow::HandleSDLTextInput(const char *text) {
    m_sdl_text_input.append(text);
}

void SDLBeebWindow::HandleSDLMouseMotionEvent(const SDL_MouseMotionEvent &event) {
    //m_beeb_window->HandleSDLMouseMotionEvent(event);

    // is this actually necessary??
}

void SDLBeebWindow::HandleVBlank(VBlankMonitor *vblank_monitor,void *display_data,uint64_t ticks) {
    this->UpdateTitle(ticks);
    
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
 
UpdateResult SDLBeebWindow::UpdateBeeb() {
    if(m_beeb) {
        static const size_t MAX_NUM_CYCLES=10000;
        size_t i;
        VideoDataUnit vu;
        SoundDataUnit su;
        
        for(i=0;i<MAX_NUM_CYCLES;++i) {
            if(m_beeb->DebugIsHalted()) {
                break;
            }
            
            if(m_beeb->Update(&vu,&su)) {
                // TODO: process sound unit
            }
            
            m_tv.Update(&vu);
        }
        
        return UpdateResult_FlatOut;
    } else {
        return UpdateResult_SpeedLimited;
    }
}

void SDLBeebWindow::RenderLastImGuiFrame() {
    m_tv.CopyTexturePixels(&m_tv_texture_pixels);
    
    SDL_UpdateTexture(m_imgui_textures[ImGuiTexture_TV],nullptr,m_tv_texture_pixels.data(),TV_TEXTURE_WIDTH*4);
    
    SDL_RenderClear(m_renderer);

    if(!m_last_imgui_draw_lists.empty()) {
        std::vector<ImGuiStuff::StoredDrawList> *stored_draw_lists=nullptr;
#if STORE_DRAWLISTS
        stored_draw_lists=&m_stored_draw_lists;
#endif
        
        ImGuiStuff::RenderSDL(m_renderer,
                              m_last_imgui_draw_lists,
                              stored_draw_lists,
                              m_imgui_textures,
                              sizeof m_imgui_textures/sizeof m_imgui_textures[0]);
    }
    
    SDL_RenderPresent(m_renderer);
}

void SDLBeebWindow::RunNextImGuiFrame(uint64_t ticks) {
    ImGuiContextSetter setter(m_imgui_stuff);
    
    bool got_mouse_focus;
    SDL_Window *mouse_window=SDL_GetMouseFocus();
    if(mouse_window==m_window) {
        got_mouse_focus=true;
    } else {
        got_mouse_focus=false;
    }
    
    SDL_Point mouse_pos;
    uint32_t mouse_buttons=SDL_GetMouseState(&mouse_pos.x,&mouse_pos.y);
    
    SDL_Point mouse_wheel_delta=m_mouse_wheel_delta;
    m_mouse_wheel_delta={};

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
    
    m_imgui_stuff->NewFrame(got_mouse_focus,
                            mouse_pos,
                            mouse_buttons,
                            mouse_wheel_delta,
                            keymod,
                            renderer_output_width,
                            renderer_output_height,
                            (ImTextureID)ImGuiTexture_Font);
    
    // Set if the BBC display panel has focus. This isn't entirely regular,
    // because the BBC display panel is handled by separate code - this will
    // probably get fixed eventually.
    //
    // The BBC display panel never has any dear imgui text widgets in it.
    bool beeb_focus=false;

    {
        // Set the window padding to 0x0, so that the docking stuff, which
        // makes its own child windows, ends up tightly aligned to the
        // window border. (Well, tightly enough, anyway... in fact there's
        // still a 1 pixel border that I haven't figured out yet.)
        //
        // 0x0 padding makes everything else look a bit of a mess, so this
        // does mean that the default window padding has to be pushed in
        // various places. Need to fix this...
        ImGuiStyleVarPusher vpusher1(ImGuiStyleVar_WindowPadding,ImVec2(0.f,0.f));
        
        {
            ImGuiStyleVarPusher vpusher2(ImGuiStyleVar_WindowPadding,IMGUI_DEFAULT_STYLE.WindowPadding);
            
            this->DoMenuUI();
        }
        
        {
            ImVec2 pos=ImGui::GetCursorPos();
            pos.x=0.f;
            ImVec2 size={(float)renderer_output_width-pos.x,(float)renderer_output_height-pos.y};
            
            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize(size);
        }
        
        ImGuiCol style_colour=ImGuiCol_WindowBg;
        ImGuiStyleColourPusher cpusher1(style_colour,ImVec4(0.f,0.f,0.f,0.f));
        
        if(ImGui::Begin("DockHolder",
                        nullptr,
                        (ImGuiWindowFlags_NoScrollWithMouse|
                         ImGuiWindowFlags_NoTitleBar|
                         ImGuiWindowFlags_NoResize|
                         ImGuiWindowFlags_NoMove|
                         ImGuiWindowFlags_NoScrollbar|
                         ImGuiWindowFlags_NoSavedSettings|
                         ImGuiWindowFlags_NoBringToFrontOnFocus)))
        {
            {
                ImGuiStyleColourPusher cpusher2;
                cpusher2.PushDefault(style_colour);
                
                ImGui::BeginDockspace();
                {
                    ImGuiStyleVarPusher vpusher2(ImGuiStyleVar_WindowPadding,IMGUI_DEFAULT_STYLE.WindowPadding);
                    
                    //ccs[0]=this->DoSettingsUI();
                    
                    beeb_focus=this->DoBeebDisplayUI();
                }
                ImGui::EndDockspace();
                
                {
                    ImGuiStyleVarPusher vpusher2(ImGuiStyleVar_WindowPadding,IMGUI_DEFAULT_STYLE.WindowPadding);
                    
#if ENABLE_IMGUI_DEMO
                    if(m_imgui_demo) {
                        ImGui::ShowDemoWindow();
                    }
#endif
                    
                    if(m_imgui_dock_debug) {
                        ImGui::DockDebugWindow();
                    }
                    
#if STORE_DRAWLISTS
                    if(m_imgui_drawlists) {
                        m_imgui_stuff->DoStoredDrawListWindow(m_stored_draw_lists);
                    }
#endif
                    
                    if(m_imgui_debug) {
                        m_imgui_stuff->DoDebugWindow();
                    }
                    
                    if(m_imgui_metrics) {
                        ImGui::ShowMetricsWindow();
                    }
                    
                    this->DoPopupUI(ticks,renderer_output_width,renderer_output_height);
                }
            }
            ImGui::End();
        }
    }
    
    m_imgui_stuff->RenderImGui();
    
    m_last_imgui_draw_lists=m_imgui_stuff->CloneDrawLists();
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
    
    m_tv.Init(m_pixel_format->Rshift,
              m_pixel_format->Gshift,
              m_pixel_format->Bshift);

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
    
    if(!LoadBeebConfigByName(&m_current_config,m_settings.config_name,&m_msg)) {
        if(!BeebLoadedConfig::Load(&m_current_config,*GetDefaultBeebConfigByIndex(0),&m_msg)) {
            m_msg.e.f("Failed to create Beeb config.\n");
            return false;
        }
    }
    
    std::vector<uint8_t> nvram=GetDefaultNVRAMContents(m_current_config.config.type);
    this->HardReset(0,
                    m_current_config,
                    std::move(nvram));

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

void SDLBeebWindow::HardReset(uint32_t reset_flags,
                              const BeebLoadedConfig &loaded_config,
                              const std::vector<uint8_t> &nvram_contents)
{
    uint32_t replace_flags=ReplaceFlag_KeepCurrentDiscs;

    if(reset_flags&ResetFlag_Boot) {
        replace_flags|=ReplaceFlag_Autoboot;
    }

//    if(ts->timeline_mode!=BeebThreadTimelineMode_Replay) {
//        replace_flags|=BeebThreadReplaceFlag_ResetKeyState;
//    }

    m_current_config=loaded_config;

    tm now=GetLocalTimeNow();

    uint64_t num_2MHz_cycles=0;
    if(m_beeb) {
        num_2MHz_cycles=*m_beeb->GetNum2MHzCycles();
    }
    
//    if(ts->current_config.config.beeblink) {
//        if(!ts->beeblink_handler) {
//            std::string sender_id=strprintf("%" PRIu64,beeb_thread->m_uid);
//            ts->beeblink_handler=std::make_unique<BeebLinkHTTPHandler>(beeb_thread,
//                                                                       std::move(sender_id),
//                                                                       beeb_thread->m_message_list);
//
//            if(!ts->beeblink_handler->Init(&ts->msgs)) {
//                // Ugh. Just give up.
//                ts->beeblink_handler.reset();
//            }
//        }
//    } else {
//        ts->beeblink_handler.reset();
//    }

//    BBCMicro(const BBCMicroType *type,
//             const DiscInterfaceDef *def,
//             const std::vector<uint8_t> &nvram_contents,
//             const tm *rtc_time,
//             bool video_nula,
//             bool ext_mem,
//             bool power_on_tone,
//             BeebLinkHandler *beeblink_handler,
//             uint64_t initial_num_2MHz_cycles);

    delete m_beeb;
    m_beeb=nullptr;
    
    m_beeb=new BBCMicro(m_current_config.config.type,
                        m_current_config.config.disc_interface,
                        nvram_contents,
                        &now,
                        m_current_config.config.video_nula,
                        m_current_config.config.ext_mem,
                        true,
                        nullptr,
                        num_2MHz_cycles);
    
    m_beeb->SetOSROM(m_current_config.os);

    for(uint8_t i=0;i<16;++i) {
        if(m_current_config.config.roms[i].writeable) {
            if(!!m_current_config.roms[i]) {
                m_beeb->SetSidewaysRAM(i,m_current_config.roms[i]);
            } else {
                m_beeb->SetSidewaysRAM(i,nullptr);
            }
        } else {
            if(!!m_current_config.roms[i]) {
                m_beeb->SetSidewaysROM(i,m_current_config.roms[i]);
            } else {
                m_beeb->SetSidewaysROM(i,nullptr);
            }
        }
    }
    
//#if BBCMICRO_DEBUGGER
//    if(m_flags&BeebThreadHardResetFlag_Run) {
//        ts->beeb->DebugRun();
//    }
//#endif
}

bool SDLBeebWindow::DoBeebDisplayUI() {
    //bool opened=m_imgui_stuff->AreAnyDocksDocked();
    bool focus=false;
    
    double scale_x;
    if(m_settings.correct_aspect_ratio) {
        scale_x=1.2/1.25;
    } else {
        scale_x=1;
    }
    
    ImGuiWindowFlags flags=0;//ImGuiWindowFlags_NoTitleBar;
    ImVec2 pos;
    ImVec2 size;
    if(!m_settings.display_auto_scale) {
        pos.x=0.f;
        pos.y=0.f;
        
        size.x=(float)(m_settings.display_manual_scale*TV_TEXTURE_WIDTH*scale_x);
        size.y=(float)(m_settings.display_manual_scale*TV_TEXTURE_HEIGHT);
        
        flags|=ImGuiWindowFlags_HorizontalScrollbar;
        
        ImGui::SetNextWindowContentSize(size);
    }
    
    if(ImGui::BeginDock("Display",nullptr,flags)) {
        
        if(ImGui::IsWindowAppearing()) {
            ImGui::FocusWindow(GImGui->CurrentWindow);
        }
        
        ImVec2 padding=GImGui->Style.WindowPadding;
        
        focus=ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        
        ImGuiStyleVarPusher vpusher(ImGuiStyleVar_WindowPadding,ImVec2(0.f,0.f));
#if BEEB_DISPLAY_FILL
        
        const ImVec2 &size=ImGui::GetWindowSize();
        ImVec2 pos(0.f,0.f);
        
#else
        
        ImVec2 window_size=ImGui::GetWindowSize()-padding*2.f;
        
        if(m_settings.display_auto_scale) {
            double tv_aspect=(TV_TEXTURE_WIDTH*scale_x)/TV_TEXTURE_HEIGHT;
            
            double width=window_size.x;
            double height=width/tv_aspect;
            
            if(height>window_size.y) {
                height=window_size.y;
                width=height*tv_aspect;
            }
            
            size.x=(float)width;
            size.y=(float)height;
            
            pos=(window_size-size)*.5f;
            
            // Don't fight any half pixel offset.
            pos.x=(float)(int)pos.x;
            pos.y=(float)(int)pos.y;
        }
        
#endif
        
        ImGui::SetCursorPos(pos);
        ImVec2 screen_pos=ImGui::GetCursorScreenPos();
        ImGui::Image((ImTextureID)ImGuiTexture_TV,size);
        
#if VIDEO_TRACK_METADATA
        
        //m_got_mouse_pixel_unit=false;
        
        //            if(ImGui::IsItemHovered()) {
        //                ImVec2 mouse_pos=ImGui::GetMousePos();
        //                mouse_pos-=screen_pos;
        //
        //                double tx=mouse_pos.x/size.x;
        //                double ty=mouse_pos.y/size.y;
        //
        //                if(tx>=0.&&tx<1.&&ty>=0.&&ty<1.) {
        //                    int x=(int)(tx*TV_TEXTURE_WIDTH);
        //                    int y=(int)(ty*TV_TEXTURE_HEIGHT);
        //
        //                    ASSERT(x>=0&&x<TV_TEXTURE_WIDTH);
        //                    ASSERT(y>=0&&y<TV_TEXTURE_HEIGHT);
        //
        //                    const VideoDataUnit *units=m_tv.GetTextureUnits();
        //                    m_mouse_pixel_unit=units[y*TV_TEXTURE_WIDTH+x];
        //                    m_got_mouse_pixel_unit=true;
        //                }
        //            }
#else
        (void)screen_pos;
#endif
    }
    ImGui::EndDock();
    
    return focus;
}

void SDLBeebWindow::DoMenuUI() {
    if(ImGui::BeginMainMenuBar()) {
        this->DoFileMenu();
        this->DoEditMenu();
        this->DoHardwareMenu();
        this->DoKeyboardMenu();
        this->DoToolsMenu();
        this->DoDebugMenu();
        ImGui::EndMainMenuBar();
    }
}

void SDLBeebWindow::DoFileMenu() {
}

void SDLBeebWindow::DoEditMenu() {
}

void SDLBeebWindow::DoHardwareMenu() {
}

void SDLBeebWindow::DoKeyboardMenu() {
}

void SDLBeebWindow::DoToolsMenu() {
}

void SDLBeebWindow::DoDebugMenu() {
#if ENABLE_DEBUG_MENU
    if(ImGui::BeginMenu("Debug")) {
//#if ENABLE_IMGUI_TEST
//        m_cc.DoMenuItemUI("toggle_dear_imgui_test");
//#endif
//        m_cc.DoMenuItemUI("toggle_event_trace");
//        m_cc.DoMenuItemUI("toggle_date_rate");

//#if SYSTEM_WINDOWS
//        if(GetConsoleWindow()) {
//            m_cc.DoMenuItemUI("clear_console");
//            m_cc.DoMenuItemUI("print_separator");
//        }
//#endif

//#if VIDEO_TRACK_METADATA
//        m_cc.DoMenuItemUI("toggle_pixel_metadata");
//#endif

//#if BBCMICRO_DEBUGGER
//        ImGui::Separator();
//
//        m_cc.DoMenuItemUI("toggle_6502_debugger");
//        if(ImGui::BeginMenu("Memory Debug")) {
//            m_cc.DoMenuItemUI("toggle_memory_debugger1");
//            m_cc.DoMenuItemUI("toggle_memory_debugger2");
//            m_cc.DoMenuItemUI("toggle_memory_debugger3");
//            m_cc.DoMenuItemUI("toggle_memory_debugger4");
//            ImGui::EndMenu();
//        }
//        if(ImGui::BeginMenu("External Memory Debug")) {
//            m_cc.DoMenuItemUI("toggle_ext_memory_debugger1");
//            m_cc.DoMenuItemUI("toggle_ext_memory_debugger2");
//            m_cc.DoMenuItemUI("toggle_ext_memory_debugger3");
//            m_cc.DoMenuItemUI("toggle_ext_memory_debugger4");
//            ImGui::EndMenu();
//        }
//        if(ImGui::BeginMenu("Disassembly Debug")) {
//            m_cc.DoMenuItemUI("toggle_disassembly_debugger1");
//            m_cc.DoMenuItemUI("toggle_disassembly_debugger2");
//            m_cc.DoMenuItemUI("toggle_disassembly_debugger3");
//            m_cc.DoMenuItemUI("toggle_disassembly_debugger4");
//            ImGui::EndMenu();
//        }
//        m_cc.DoMenuItemUI("toggle_crtc_debugger");
//        m_cc.DoMenuItemUI("toggle_video_ula_debugger");
//        m_cc.DoMenuItemUI("toggle_system_via_debugger");
//        m_cc.DoMenuItemUI("toggle_user_via_debugger");
//        m_cc.DoMenuItemUI("toggle_nvram_debugger");
//        m_cc.DoMenuItemUI("toggle_sn76489_debugger");
//        m_cc.DoMenuItemUI("toggle_paging_debugger");
//        m_cc.DoMenuItemUI("toggle_breakpoints_debugger");
//        m_cc.DoMenuItemUI("toggle_stack_debugger");
//
//        ImGui::Separator();
//
//        m_cc.DoMenuItemUI("debug_stop");
//        m_cc.DoMenuItemUI("debug_run");
//        m_cc.DoMenuItemUI("debug_step_over");
//        m_cc.DoMenuItemUI("debug_step_in");
//
//#endif

        ImGui::Separator();

#if ENABLE_IMGUI_DEMO
        ImGui::MenuItem("ImGui demo...",NULL,&m_imgui_demo);
#endif
        ImGui::MenuItem("ImGui dock debug...",nullptr,&m_imgui_dock_debug);
#if STORE_DRAWLISTS
        ImGui::MenuItem("ImGui drawlists...",nullptr,&m_imgui_drawlists);
#endif
        ImGui::MenuItem("ImGui debug...",nullptr,&m_imgui_debug);
        ImGui::MenuItem("ImGui metrics...",nullptr,&m_imgui_metrics);

        ImGui::EndMenu();
    }
#endif
}

void SDLBeebWindow::DoPopupUI(uint64_t now,int output_width,int output_height) {
    (void)output_width;

    if(ValueChanged(&m_msg_last_num_messages_printed,m_message_list->GetNumMessagesPrinted())) {
        m_messages_popup_ui_active=true;
        m_messages_popup_ticks=now;
    }

    if(m_messages_popup_ui_active) {
        ImGuiWindowFlags flags=(ImGuiWindowFlags_NoTitleBar|
                                //ImGuiWindowFlags_ShowBorders|
                                ImGuiWindowFlags_AlwaysAutoResize|
                                ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f,ImGuiCond_Always,ImVec2(0.5f,0.5f));

        // What's supposed to happen here: the window is 90% of the
        // screen width and as tall as it needs to be ("set axis to
        // 0.0f to force an auto-fit on this axis").
        //
        // What actually happens: it's the right width, but 0 pixels
        // high, and gets saved to imgui.ini that way. Then on the
        // next run, it's 32 pixels high (?) - and that seems to
        // persist for future runs with the same ini file. But 32
        // pixels high is still too short.
        //
        // Fortunately, with AlwaysAutoResize and
        // SetNextWindowPosCenter, the default size is sensible.

        //ImGui::SetNextWindowSize(ImVec2(output_width*0.9f,0));

        if(ImGui::Begin("Recent Messages",&m_messages_popup_ui_active,flags)) {
            m_message_list->ForEachMessage(5,[](MessageList::Message *m) {
                if(!m->seen) {
                    ImGuiMessageListMessage(m);
                }
            });
        }
        ImGui::End();

        if(GetSecondsFromTicks(now-m_messages_popup_ticks)>MESSAGES_POPUP_TIME_SECONDS) {
            m_message_list->ForEachMessage([](MessageList::Message *m) {
                m->seen=true;
            });

            m_messages_popup_ui_active=false;
        }
    }

//    BeebThreadTimelineState timeline_state;
//    m_beeb_thread->GetTimelineState(&timeline_state);

    bool show_leds_popup=false;

//    bool pasting=m_beeb_thread->IsPasting();
//    bool copying=m_beeb_thread->IsCopying();
    uint32_t leds=0;
    if(m_beeb) {
        leds=m_beeb->GetLEDs();
    }
    
    if(ValueChanged(&m_leds,leds)||(m_leds&BBCMicroLEDFlags_AllDrives)) {
        if(m_settings.leds_popup_mode!=BeebWindowLEDsPopupMode_Off) {
            show_leds_popup=true;
        }
//    } else if(timeline_state.mode!=BeebThreadTimelineMode_None||copying||pasting) {
//        // The LEDs window always appears in this situation.
//        show_leds_popup=true;
    } else if(m_settings.leds_popup_mode==BeebWindowLEDsPopupMode_On) {
        show_leds_popup=true;
    }

    if(show_leds_popup) {
        if(!m_leds_popup_ui_active) {
            m_leds_popup_ticks=now;
        }

        m_leds_popup_ui_active=true;
    }

    if(m_leds_popup_ui_active) {
        ImGuiWindowFlags flags=(ImGuiWindowFlags_NoTitleBar|
                                //ImGuiWindowFlags_ShowBorders|
                                ImGuiWindowFlags_AlwaysAutoResize|
                                ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::SetNextWindowPos(ImVec2(10.f,output_height-50.f));

        if(ImGui::Begin("LEDs",&m_leds_popup_ui_active,flags)) {
            ImGuiStyleColourPusher colour_pusher;
            colour_pusher.Push(ImGuiCol_CheckMark,ImVec4(1.f,0.f,0.f,1.f));

            ImGuiLED(!!(m_leds&BBCMicroLEDFlag_CapsLock),"Caps Lock");

            ImGui::SameLine();
            ImGuiLED(!!(m_leds&BBCMicroLEDFlag_ShiftLock),"Shift Lock");

            for(int i=0;i<NUM_DRIVES;++i) {
                ImGui::SameLine();
                ImGuiLEDf(!!(m_leds&(uint32_t)BBCMicroLEDFlag_Drive0<<i),"Drive %d",i);
            }

            colour_pusher.Push(ImGuiCol_CheckMark,ImVec4(0.f,1.f,0.f,1.f));

//            switch(timeline_state.mode) {
//            case BeebThreadTimelineMode_None:
//                ImGuiLED(false,"Replay");
//                break;
//
//            case BeebThreadTimelineMode_Replay:
//                ImGuiLED(true,"Replay");
//                ImGui::SameLine();
//                if(ImGui::Button("Stop")) {
//                    m_beeb_thread->Send(std::make_shared<BeebThread::StopReplayMessage>());
//                }
//                break;
//
//            case BeebThreadTimelineMode_Record:
//                colour_pusher.Push(ImGuiCol_CheckMark,ImVec4(1.f,0.f,0.f,1.f));
//                ImGuiLED(true,"Record");
//                ImGui::SameLine();
//                if(ImGuiConfirmButton("Stop")) {
//                    m_beeb_thread->Send(std::make_shared<BeebThread::StopRecordingMessage>());
//                }
//                colour_pusher.Pop();
//                break;
//            }

//            ImGui::SameLine();
//            ImGuiLED(copying,"Copy");
//            if(copying) {
//                ImGui::SameLine();
//                if(ImGui::Button("Cancel")) {
//                    m_beeb_thread->Send(std::make_shared<BeebThread::StopCopyMessage>());
//                }
//            }
//
//            ImGui::SameLine();
//            ImGuiLED(pasting,"Paste");
//            if(pasting) {
//                ImGui::SameLine();
//                if(ImGui::Button("Cancel")) {
//                    m_beeb_thread->Send(std::make_shared<BeebThread::StopPasteMessage>());
//                }
//            }
        }
        ImGui::End();

        if(GetSecondsFromTicks(now-m_leds_popup_ticks)>LEDS_POPUP_TIME_SECONDS) {
            if(m_settings.leds_popup_mode!=BeebWindowLEDsPopupMode_On) {
                m_leds_popup_ui_active=false;
            }
        }
    }

    std::vector<std::shared_ptr<JobQueue::Job>> jobs=BeebWindows::GetJobs();
    if(!jobs.empty()) {
        bool open=false;

        for(const std::shared_ptr<JobQueue::Job> &job:jobs) {
            if(!job->HasImGui()) {
                continue;
            }

            if(open) {
                ImGui::Separator();
            } else {
                ImGuiWindowFlags flags=(ImGuiWindowFlags_NoTitleBar|
                                        //ImGuiWindowFlags_ShowBorders|
                                        ImGuiWindowFlags_AlwaysAutoResize|
                                        ImGuiWindowFlags_NoFocusOnAppearing);

                ImGui::SetNextWindowPos(ImVec2(10.f,30.f));

                open=true;

                if(!ImGui::Begin("Jobs",nullptr,flags)) {
                    goto jobs_imgui_done;
                }
            }

            job->DoImGui();

            if(ImGui::Button("Cancel")) {
                job->Cancel();
            }
        }

    jobs_imgui_done:
        if(open) {
            ImGui::End();
        }
    }
}

void SDLBeebWindow::UpdateTitle(uint64_t ticks) {
    double secs_elapsed=GetSecondsFromTicks(ticks-m_last_title_update_ticks);
    if(secs_elapsed<TITLE_UPDATE_TIME_SECONDS) {
        return;
    }
    
    char title[1000];
    if(!m_beeb) {
        snprintf(title,sizeof title,"b2 [-]");
        m_last_title_update_2MHz_cycles=0;
    } else {
        double speed=0.0;
        {
            uint64_t num_2MHz_cycles=*m_beeb->GetNum2MHzCycles();
            uint64_t num_2MHz_cycles_elapsed=num_2MHz_cycles-m_last_title_update_2MHz_cycles;
            
            if(m_last_title_update_ticks!=0) {
                double hz=num_2MHz_cycles_elapsed/secs_elapsed;
                speed=hz/2.e6;
            }
            
            m_last_title_update_2MHz_cycles=num_2MHz_cycles;
            m_last_title_update_ticks=ticks;
        }
        
        snprintf(title,sizeof title,"b2 [%.2fx]",speed);
    }
    
    SDL_SetWindowTitle(m_window,title);
    m_last_title_update_ticks=ticks;
}

