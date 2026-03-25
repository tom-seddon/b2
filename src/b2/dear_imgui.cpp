#include <shared/system.h>
#include "dear_imgui.h"
#include <shared/debug.h>
#include <SDL.h>
#include <vector>
#include <shared/system_specific.h>
#include <limits.h>
#include <IconsFontAwesome5.h>
#include "load_save.h"
#include "misc.h"
#include "native_ui.h"
#include <SDL_opengl.h>
#include "b2.h"
#include <SDL_syswm.h>

#ifdef IMGUI_ENABLE_TEST_ENGINE
#include <imgui_test_engine/imgui_te_engine.h>
#include <imgui_test_engine/imgui_te_ui.h>
#endif

#include <shared/enum_def.h>
#include "dear_imgui.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// X11... sigh...
#ifdef Status
#undef Status
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#if !SYSTEM_WINDOWS
#define USE_SDL_CLIPBOARD 1
//#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const ImVec4 &DISABLED_BUTTON_COLOUR = ImVec4(.4f, .4f, .4f, 1.f);
const ImVec4 &DISABLED_BUTTON_HOVERED_COLOUR = DISABLED_BUTTON_COLOUR;
const ImVec4 &DISABLED_BUTTON_ACTIVE_COLOUR = DISABLED_BUTTON_COLOUR;
const ImVec4 &DISABLED_BUTTON_TEXT_COLOUR = ImVec4(.6f, .6f, .6f, 1.f);

const ImGuiStyle IMGUI_DEFAULT_STYLE;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string FAS_FILE_NAME = "fonts/" FONT_ICON_FILE_NAME_FAS;
static const std::string DEFAULT_OUTLINE_FONT_FILE_NAME = "fonts/LiberationMono-Regular.ttf";
static const ImWchar FA_ICONS_RANGES[] = {ICON_MIN_FA, ICON_MAX_FA, 0};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//struct ImGuiShutdownObject {
//    ~ImGuiShutdownObject() {
//        ImGui::Shutdown();
//    }
//};

//static ImGuiShutdownObject g_imgui_shutdown_object;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// true if any ImGuiStuff is in a frame. Only one can be in a frame at a given
// point.
static bool g_in_frame = false;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
// TODO
static UINT (*g_GetDpiForWindow)(HWND);
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiContextSetter::ImGuiContextSetter(const ImGuiStuff *stuff)
    : m_old_imgui_context(ImGui::GetCurrentContext()) {
    if (stuff->m_context) {
        ImGui::SetCurrentContext(stuff->m_context);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiContextSetter::~ImGuiContextSetter() {
    ImGui::SetCurrentContext(m_old_imgui_context);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStuff::ImGuiStuff(SDL_Window *window, SDL_Renderer *renderer, bool enable_test_engine, const ImVec2 &default_display_size)
    : m_window(window)
    , m_renderer(renderer)
    , m_default_display_size(default_display_size) {
    m_last_new_frame_ticks = GetCurrentTickCount();

    static_assert(sizeof m_imgui_key_from_sdl_scancode / sizeof m_imgui_key_from_sdl_scancode[0] == SDL_NUM_SCANCODES);

#ifdef IMGUI_ENABLE_TEST_ENGINE
    m_enable_test_engine = enable_test_engine;
#else
    (void)enable_test_engine;
#endif

#if SYSTEM_WINDOWS
    FindProcAddress("user32.dll", "GetDpiForWindow", &g_GetDpiForWindow);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStuff::~ImGuiStuff() {
    if (m_context) {
        ImGuiContextSetter setter(this);

        if (g_in_frame) {
            ImGui::EndFrame();
            g_in_frame = false;
        }

        ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();

        for (ImTextureData *im_texture : platform_io.Textures) {
            if (im_texture->RefCount == 1) {
                im_texture->SetStatus(ImTextureStatus_WantDestroy);
                this->UpdateImTextureData(im_texture);
            }
        }

#ifdef IMGUI_ENABLE_TEST_ENGINE
        if (m_test_engine) {
            ImGuiTestEngine_Stop(m_test_engine);
        }
#endif

        ImGui::DestroyPlatformWindows();
        ImGui::DestroyContext(m_context);
        m_context = nullptr;

#ifdef IMGUI_ENABLE_TEST_ENGINE
        if (m_test_engine) {
            ImGuiTestEngine_DestroyContext(m_test_engine);
            m_test_engine = nullptr;
        }
#endif
    }

    for (size_t i = 0; i < sizeof m_cursors / sizeof m_cursors[0]; ++i) {
        SDL_FreeCursor(m_cursors[i]);
        m_cursors[i] = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if USE_SDL_CLIPBOARD

static char *g_last_clipboard_text;
static bool g_FreeLastClipboardText_registered;

static void FreeLastClipboardText() {
    SDL_free(g_last_clipboard_text);
    g_last_clipboard_text = nullptr;
}

static const char *GetClipboardText(ImGuiContext *context) {
    (void)context;

    // This is quite safe, at least for now, due to the way the
    // callback is called...

    if (!g_FreeLastClipboardText_registered) {
        atexit(&FreeLastClipboardText);
        g_FreeLastClipboardText_registered = true;
    }

    FreeLastClipboardText();
    g_last_clipboard_text = SDL_GetClipboardText();

    return g_last_clipboard_text;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if USE_SDL_CLIPBOARD
static void SetClipboardText(ImGuiContext *context, const char *text) {
    (void)context;

    SDL_SetClipboardText(text);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
static float GetDpiScale(HWND hwnd) {
    if (!g_GetDpiForWindow) {
        return 1.f;
    }

    UINT dpi = (*g_GetDpiForWindow)(hwnd);
    float dpi_scale = (float)dpi / USER_DEFAULT_SCREEN_DPI;
    return dpi_scale;
}

static float HandleGetWindowDpiScaleWindows(ImGuiViewport *vp) {
    if (auto hwnd = (HWND)vp->PlatformHandleRaw) {
        return GetDpiScale(hwnd);
    } else {
        return 1.f;
    }
}
#endif

#if SYSTEM_OSX
static float GetDpiScale(SDL_Window *window) {
    SDL_Renderer *renderer = SDL_GetRenderer(window);
    if (!renderer) {
        return 1.f;
    }

    int ww, wh;
    SDL_GetWindowSize(window, &ww, &wh);

    int ow, oh;
    SDL_GetRendererOutputSize(renderer, &ow, &oh);

    // There's only one DPI scale.
    return (float)ow / ww;
}

static float HandleGetWindowDpiScaleOSX(ImGuiViewport *vp) {
    if (auto window = (SDL_Window *)vp->PlatformHandle) {
        return GetDpiScale(window);
    } else {
        return 1.f;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiStuff::Init(ImGuiConfigFlags extra_config_flags) {
    int rc;
    (void)rc;

    m_context = ImGui::CreateContext();

    ImGuiContextSetter setter(this);

    ImGuiIO &io = ImGui::GetIO();
    ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();

    if (m_renderer) {
        io.BackendRendererName = "b2 SDL/OpenGL";
    } else {
        io.BackendRendererName = "b2 Headless";
    }
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

#if SYSTEM_WINDOWS

    // TODO: I think this will need to be made DPI-aware too?
    {
        float x = (float)GetSystemMetrics(SM_CXDOUBLECLK);
        float y = (float)GetSystemMetrics(SM_CYDOUBLECLK);

        io.MouseDoubleClickMaxDist = sqrtf(x * x + y * y);
    }

    io.MouseDoubleClickTime = GetDoubleClickTime() / 1000.f;

#elif SYSTEM_OSX

    io.MouseDoubleClickTime = (float)GetDoubleClickIntervalSeconds();

#endif

    // See ImGui_ImplSDL2_Init.

    uint32_t window_flags;
    if (m_window) {
        window_flags = SDL_GetWindowFlags(m_window);
    } else {
        window_flags = 0;
    }

    if (window_flags & SDL_WINDOW_ALLOW_HIGHDPI) {
#if SYSTEM_WINDOWS
        SDL_SysWMinfo wm_info;
        SDL_VERSION(&wm_info.version);
        SDL_GetWindowWMInfo(m_window, &wm_info);

        ImGuiViewport *main_vp = ImGui::GetMainViewport();
        main_vp->PlatformHandleRaw = wm_info.info.win.window;

        platform_io.Platform_GetWindowDpiScale = &HandleGetWindowDpiScaleWindows;

        // Update scales if DPI changes.
        io.ConfigDpiScaleFonts = true;
        io.ConfigDpiScaleViewports = true;

        // The correct initial scale factor does still need to be set.
        ImGuiStyle &style = ImGui::GetStyle();
        style.FontScaleDpi = GetDpiScale(wm_info.info.win.window);

#elif SYSTEM_OSX

        ImGuiViewport *main_vp = ImGui::GetMainViewport();
        main_vp->PlatformHandle = m_window;

        platform_io.Platform_GetWindowDpiScale = &HandleGetWindowDpiScaleOSX;

        // Update scales if DPI changes.
        io.ConfigDpiScaleFonts = true;
        io.ConfigDpiScaleViewports = true;

        // The correct initial scale factor does still need to be set.
        ImGuiStyle &style = ImGui::GetStyle();
        style.FontScaleDpi = GetDpiScale(m_window);

#elif SYSTEM_LINUX

#endif
    }

    m_cursors[ImGuiMouseCursor_Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    m_cursors[ImGuiMouseCursor_ResizeAll] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    m_cursors[ImGuiMouseCursor_ResizeEW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    m_cursors[ImGuiMouseCursor_ResizeNS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    m_cursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
    m_cursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
    m_cursors[ImGuiMouseCursor_TextInput] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);

    // 1.90.7 or 1.91.6 (not sure which specifically - my notes aren't clear)
    // changed some stuff around, and old imgui.ini and newer library (or vice
    // versa) are no longer quite compatible. Symptoms with b2's UI are a mite
    // confusing.
    //
    // Don't bother trying to be clever about this: just have a completely
    // separate config file for the later versions. This is tedious but a
    // one-off fix.
    m_imgui_ini_path = GetConfigPath("imgui.1.92+.ini");
    io.IniFilename = m_imgui_ini_path.c_str();

    // On OS X, could this usefully go in ~/Library/Logs?
    m_imgui_log_txt_path = GetCachePath("imgui_log.txt");
    io.LogFilename = m_imgui_log_txt_path.c_str();

    io.ConfigFlags |= extra_config_flags;

#if USE_SDL_CLIPBOARD

    platform_io.Platform_GetClipboardTextFn = &GetClipboardText;
    platform_io.Platform_SetClipboardTextFn = &SetClipboardText;

#endif

    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_TAB] = ImGuiKey_Tab;          // for tabbing through fields
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_LEFT] = ImGuiKey_LeftArrow;   // for text edit
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_RIGHT] = ImGuiKey_RightArrow; // for text edit
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_UP] = ImGuiKey_UpArrow;       // for text edit
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_DOWN] = ImGuiKey_DownArrow;   // for text edit
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_PAGEUP] = ImGuiKey_PageUp;
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_PAGEDOWN] = ImGuiKey_PageDown;
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_HOME] = ImGuiKey_Home;           // for text edit
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_END] = ImGuiKey_End;             // for text edit
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_DELETE] = ImGuiKey_Delete;       // for text edit
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_BACKSPACE] = ImGuiKey_Backspace; // for text edit
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_RETURN] = ImGuiKey_Enter;        // for text edit
    m_imgui_key_from_sdl_scancode[SDL_SCANCODE_ESCAPE] = ImGuiKey_Escape;       // for text edit
    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_a)] = ImGuiKey_A; // for text edit CTRL+A: select all
    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_c)] = ImGuiKey_C; // for text edit CTRL+C: copy
    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_v)] = ImGuiKey_V; // for text edit CTRL+V: paste
    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_x)] = ImGuiKey_X; // for text edit CTRL+X: cut
    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_y)] = ImGuiKey_Y; // for text edit CTRL+Y: redo
    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_z)] = ImGuiKey_Z; // for text edit CTRL+Z: undo

    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_LCTRL)] = ImGuiKey_LeftCtrl;
    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_RCTRL)] = ImGuiKey_RightCtrl;

    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_LSHIFT)] = ImGuiKey_LeftShift;
    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_RSHIFT)] = ImGuiKey_RightShift;

    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_LALT)] = ImGuiKey_LeftAlt;
    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_RALT)] = ImGuiKey_RightAlt;

    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_LGUI)] = ImGuiKey_LeftSuper;
    m_imgui_key_from_sdl_scancode[SDL_GetScancodeFromKey(SDLK_RGUI)] = ImGuiKey_RightSuper;

    m_default_style = ImGui::GetStyle();

#ifdef IMGUI_ENABLE_TEST_ENGINE
    m_test_engine = ImGuiTestEngine_CreateContext();
    ImGuiTestEngine_Start(m_test_engine, ImGui::GetCurrentContext());
#endif

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

float ImGuiStuff::GetScale() const {
    return m_scale;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::SetScale(float scale) {
    // Not sure there's much point allowing scale<1?
    m_scale = (std::max)(scale, 1.f);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t ImGuiStuff::ConsumePressedKeycode() {
    switch (m_consume_pressed_keycode_state) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case ConsumePressedKeycodeState_Off:
        m_consume_pressed_keycode_state = ConsumePressedKeycodeState_Waiting;
        return 0;

    case ConsumePressedKeycodeState_Waiting:
        return 0;

    case ConsumePressedKeycodeState_Consumed:
        m_consume_pressed_keycode_state = ConsumePressedKeycodeState_Off;
        return m_consumed_keycode;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiStuff::GetPixelFont() const {
    return m_pixel_font;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::SetPixelFont(bool pixel_font) {
    if (pixel_font != m_pixel_font) {
        m_pixel_font = pixel_font;
        m_fonts_dirty = true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImVec2 ImGuiStuff::GetDisplaySize() const {
    if (m_renderer) {
        int output_width, output_height;
        SDL_GetRendererOutputSize(m_renderer, &output_width, &output_height);

        return {(float)output_width, (float)output_height};
    } else {
        return m_default_display_size;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::NewFrame() {
    ASSERT(!g_in_frame);

    ImGuiContextSetter setter(this);
    ImGuiIO &io = ImGui::GetIO();

    uint64_t now_ticks = GetCurrentTickCount();
    io.DeltaTime = (float)GetSecondsFromTicks(now_ticks - m_last_new_frame_ticks);
    m_last_new_frame_ticks = now_ticks;

    {
        ImGuiMouseCursor cursor = ImGui::GetMouseCursor();

        if (cursor >= 0 && cursor < ImGuiMouseCursor_COUNT && m_cursors[cursor]) {
            SDL_SetCursor(m_cursors[cursor]);
        } else {
            SDL_SetCursor(nullptr);
        }
    }

    io.DisplaySize = this->GetDisplaySize();

    if (m_renderer) {
        int window_width, window_height;
        SDL_GetWindowSize(m_window, &window_width, &window_height);

        // Always ends up as 1.0 on Windows. May end up as 2.0 on macOS with display scaling.
        //
        // TODO: there's probably somewhere better to get this value from... right?!
        m_mouse_scale = (float)io.DisplaySize.x / window_width;
    } else {
        m_mouse_scale = 1.f;
    }

    this->EnsureFontsReady();

    ImGui::NewFrame();

    ImGuiStyle *style = &ImGui::GetStyle();
    *style = m_default_style;

    // Scale the default sizes up by the additional scale.
    style->FontScaleDpi *= m_scale;
    style->ScaleAllSizes(m_mouse_scale * m_scale);

    g_in_frame = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::RenderImGui() {
    ASSERT(g_in_frame);

    ImGuiContextSetter setter(this);

    ImGui::Render();
    g_in_frame = false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::RenderSDL() {
    ImGuiContextSetter setter(this);

    int rc;
    (void)rc;

#if STORE_DRAWLISTS
    m_draw_lists.clear();
#endif

    ImDrawData *draw_data = ImGui::GetDrawData();
    if (!draw_data) {
        return;
    }

    if (!draw_data->Valid) {
        return;
    }

    if (!m_renderer) {
        return;
    }

#if STORE_DRAWLISTS
    ASSERT(draw_data->CmdListsCount >= 0);
    m_draw_lists.resize((size_t)draw_data->CmdListsCount);
#endif

    SDL_RenderFlush(m_renderer);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

    if (draw_data->Textures) {
        for (ImTextureData *texture : *draw_data->Textures) {
            if (texture->Status != ImTextureStatus_OK) {
                this->UpdateImTextureData(texture);
            }
        }
    }

    int output_width, output_height;
    SDL_GetRendererOutputSize(m_renderer, &output_width, &output_height);

    glViewport(0, 0, output_width, output_height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, output_width, output_height, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_SCISSOR_TEST);

    glEnable(GL_TEXTURE_2D);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    for (int i = 0; i < draw_data->CmdListsCount; ++i) {
        ImDrawList *draw_list = draw_data->CmdLists[i];

        int idx_buffer_pos = 0;

        ImDrawVert *vertex0 = &draw_list->VtxBuffer[0];

        glVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), &vertex0->pos);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), &vertex0->col);
        glTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), &vertex0->uv);

#if STORE_DRAWLISTS
        StoredDrawList *stored_list = &m_draw_lists[(size_t)i];
        ASSERT(draw_list->CmdBuffer.size() >= 0);
        stored_list->cmds.resize((size_t)draw_list->CmdBuffer.size());

        if (draw_list->_OwnerName) {
            stored_list->name = draw_list->_OwnerName;
        } else {
            stored_list->name.clear();
        }
#endif

#if STORE_DRAWLISTS
        StoredDrawCmd *stored_cmd = stored_list->cmds.data();
#endif

        for (const ImDrawCmd &cmd : draw_list->CmdBuffer) {
            auto clip_h = (GLsizei)(cmd.ClipRect.w - cmd.ClipRect.y);
            glScissor((GLsizei)cmd.ClipRect.x,
                      (GLsizei)(output_height - clip_h - cmd.ClipRect.y),
                      (GLsizei)(cmd.ClipRect.z - cmd.ClipRect.x),
                      clip_h);

            if (cmd.UserCallback) {
#if STORE_DRAWLISTS
                stored_cmd->callback = true;
#endif

                (*cmd.UserCallback)(draw_list, &cmd);
            } else {
                SDL_Texture *texture = (SDL_Texture *)cmd.TexRef.GetTexID();
                SDL_GL_BindTexture(texture, nullptr, nullptr);

                GLint gl_scale_mode;
                SDL_ScaleMode scale_mode;
                SDL_GetTextureScaleMode(texture, &scale_mode);
                switch (scale_mode) {
                default:
                case SDL_ScaleModeNearest:
                    gl_scale_mode = GL_NEAREST;
                    break;

                case SDL_ScaleModeLinear:
                    gl_scale_mode = GL_LINEAR;
                    break;
                }

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_scale_mode);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_scale_mode);

                SDL_BlendMode blend_mode;
                SDL_GetTextureBlendMode(texture, &blend_mode);
                switch (blend_mode) {
                default:
                case SDL_BLENDMODE_NONE:
                    glDisable(GL_BLEND);
                    break;

                case SDL_BLENDMODE_BLEND:
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    break;
                }

#if STORE_DRAWLISTS
                stored_cmd->callback = false;

                if (texture) {
                    SDL_QueryTexture(texture, nullptr, nullptr, &stored_cmd->texture_width, &stored_cmd->texture_height);
                }

                stored_cmd->num_indices = cmd.ElemCount;
#endif

                const uint16_t *indices = &draw_list->IdxBuffer[idx_buffer_pos];
                ASSERT(idx_buffer_pos + (int)cmd.ElemCount <= draw_list->IdxBuffer.size());

                glDrawElements(GL_TRIANGLES, (GLsizei)cmd.ElemCount, GL_UNSIGNED_SHORT, indices);

                ASSERT(cmd.ElemCount <= INT_MAX);
                idx_buffer_pos += (int)cmd.ElemCount;
            }

#if STORE_DRAWLISTS
            ++stored_cmd;
#endif
        }
    }

    glPopClientAttrib();
    glPopAttrib();

    //    rc=SDL_RenderSetClipRect(m_renderer,NULL);
    //    ASSERT(rc==0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::PostSwap() {
#ifdef IMGUI_ENABLE_TEST_ENGINE
    if (m_test_engine) {
        ImGuiTestEngine_PostSwap(m_test_engine);
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::AddFocusEvent(bool got_focus) {
    ImGuiContextSetter setter(this);
    ImGuiIO &io = ImGui::GetIO();

    io.AddFocusEvent(got_focus);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void AddMouseSourceEvent(ImGuiIO *io, uint32_t mouse_id) {
    io->AddMouseSourceEvent(mouse_id == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::AddMouseWheelEvent(uint32_t mouse_id, float x, float y) {
    ImGuiContextSetter setter(this);
    ImGuiIO &io = ImGui::GetIO();

    //printf("%s: x=%f y=%f\n",__func__,x,y);
    AddMouseSourceEvent(&io, mouse_id);
    io.AddMouseWheelEvent(x, y);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::AddMouseButtonEvent(uint32_t mouse_id, uint8_t button, bool state) {
    ImGuiContextSetter setter(this);
    ImGuiIO &io = ImGui::GetIO();

    int imgui_button;
    switch (button) {
    default:
        imgui_button = -1;
        break;

    case SDL_BUTTON_LEFT:
        imgui_button = ImGuiMouseButton_Left;
        break;

    case SDL_BUTTON_MIDDLE:
        imgui_button = ImGuiMouseButton_Middle;
        break;

    case SDL_BUTTON_RIGHT:
        imgui_button = ImGuiMouseButton_Right;
        break;
    }

    if (imgui_button >= 0) {
        //printf("%s: button=%d state=%s\n", __func__, imgui_button, BOOL_STR(state));
        AddMouseSourceEvent(&io, mouse_id);
        io.AddMouseButtonEvent(imgui_button, state);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::AddMouseMotionEvent(uint32_t mouse_id, int x, int y) {
    ImGuiContextSetter setter(this);
    ImGuiIO &io = ImGui::GetIO();

    //printf("%s: x=%d y=%d\n",__func__,x,y);
    AddMouseSourceEvent(&io, mouse_id);
    io.AddMousePosEvent(x * m_mouse_scale, y * m_mouse_scale);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Synthesize modifier press/release at the appropriate moment.
static void AddModEvent(ImGuiIO *io, ImGuiKey key, bool state, ImGuiKey lkey, ImGuiKey rkey, ImGuiKey mkey) {
    if (key == lkey || key == rkey) {
        if (state) {
            // The physical key is down, therefore the modifier is down. The
            // other physical key's state doesn't matter.
            io->AddKeyEvent(mkey, true);
        } else {
            // The physical key is up. If the other physical key is up too, the
            // modifier is up.
            if (!ImGui::IsKeyDown(key == lkey ? rkey : lkey)) {
                io->AddKeyEvent(mkey, false);
            }
        }
    }
}

static bool IsModifierKey(SDL_Scancode k) {
    switch (k) {
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
    case SDL_SCANCODE_LGUI:
    case SDL_SCANCODE_RGUI:
    case SDL_SCANCODE_MODE:
        return true;

    default:
        return false;
    }
}

bool ImGuiStuff::AddKeyEvent(uint32_t scancode, bool state) {
    ImGuiContextSetter setter(this);
    ImGuiIO &io = ImGui::GetIO();

    if (m_consume_pressed_keycode_state == ConsumePressedKeycodeState_Waiting) {
        if (state) {
            if (!IsModifierKey((SDL_Scancode)scancode)) {
                SDL_Keycode keycode = SDL_GetKeyFromScancode((SDL_Scancode)scancode);
                if (keycode != 0) {
                    uint32_t modifiers = GetPCKeyModifiersFromSDLKeymod((uint16_t)SDL_GetModState());
                    m_consumed_keycode = (uint32_t)keycode | modifiers;
                    m_consume_pressed_keycode_state = ConsumePressedKeycodeState_Consumed;
                    return false;
                }
            }
        }

    } else {
        if (scancode < SDL_NUM_SCANCODES) {
            ImGuiKey imgui_key = m_imgui_key_from_sdl_scancode[scancode];
            io.AddKeyEvent(imgui_key, state);

            AddModEvent(&io, imgui_key, state, ImGuiKey_LeftShift, ImGuiKey_RightShift, ImGuiMod_Shift);
            AddModEvent(&io, imgui_key, state, ImGuiKey_LeftAlt, ImGuiKey_RightAlt, ImGuiMod_Alt);
            AddModEvent(&io, imgui_key, state, ImGuiKey_LeftCtrl, ImGuiKey_RightCtrl, ImGuiMod_Ctrl);
            AddModEvent(&io, imgui_key, state, ImGuiKey_LeftSuper, ImGuiKey_RightSuper, ImGuiMod_Super);
        }
    }

    return true;
    //ImGuiKey key = ImGuiKey_None;

    //if (scancode < sizeof io.KeysDown / sizeof io.KeysDown[0]) {
    //    io.KeysDown[scancode] = state;
    //}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::AddInputCharactersUTF8(const char *text) {
    ImGuiContextSetter setter(this);
    ImGuiIO &io = ImGui::GetIO();

    io.AddInputCharactersUTF8(text);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if STORE_DRAWLISTS
void ImGuiStuff::DoStoredDrawListWindow() {
    if (ImGui::Begin("Drawlists")) {
        ImGui::Text("%zu draw lists", m_draw_lists.size());

        for (size_t i = 0; i < m_draw_lists.size(); ++i) {
            const StoredDrawList *list = &m_draw_lists[i];

            if (ImGui::TreeNode((const void *)(uintptr_t)i, "\"%s\"; %zu commands", list->name.c_str(), list->cmds.size())) {
                for (size_t j = 0; j < list->cmds.size(); ++j) {
                    const StoredDrawCmd *cmd = &list->cmds[j];

                    if (cmd->callback) {
                        ImGui::Text("%zu. (callback)", j);
                    } else if (cmd->texture_width > 0 && cmd->texture_height > 0) {
                        ImGui::Text("%zu. %u indices, %dx%d texture", j, cmd->num_indices, cmd->texture_width, cmd->texture_height);
                    } else {
                        ImGui::Text("%zu. %u indices", j, cmd->num_indices);
                    }
                }
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef IMGUI_ENABLE_TEST_ENGINE
bool ImGuiStuff::IsTestEngineEnabled() const {
    return m_enable_test_engine;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef IMGUI_ENABLE_TEST_ENGINE
void ImGuiStuff::DoTestEngineWindow(bool *p_open) {
    if (m_test_engine) {
        ImGuiTestEngine_ShowTestEngineWindows(m_test_engine, p_open);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef IMGUI_ENABLE_TEST_ENGINE
ImGuiTestEngine *ImGuiStuff::GetTestEngine() const {
    return m_test_engine;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DoImGuiWindowText(const char *name, const ImGuiWindow *window) {
    if (window) {
        ImGui::Text("%s: %s (0x%x)", name, window->Name, window->ID);
    } else {
        ImGui::Text("%s: *none*", name);
    }
}

void ImGuiStuff::DoDebugGui() {
    ImGuiHeader("Windows");

    DoImGuiWindowText("NavWindow", GImGui->NavWindow);
    ImGui::Text("NavID: 0x%x (alive=%s)", GImGui->NavId, BOOL_STR(GImGui->NavIdIsAlive));

    ImGui::Separator();

    DoImGuiWindowText("HoveredWindow", GImGui->HoveredWindow);
    ImGui::Text("HoveredID: 0x%x", GImGui->HoveredId);

    ImGui::Separator();

    DoImGuiWindowText("ActiveIdWindow", GImGui->ActiveIdWindow);
    ImGui::Text("ActiveID: 0x%x (alive=%s)", GImGui->ActiveId, BOOL_STR(GImGui->ActiveIdIsAlive));

    //        if (window != ignore_window && window->WasActive && !(window->Flags & ImGuiWindowFlags_ChildWindow))
    //            if ((window->Flags & (ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoNavInputs)) != (ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoNavInputs))
    //            {
    //                ImGuiWindow* focus_window = NavRestoreLastChildNavWindow(window);
    //                FocusWindow(focus_window);
    //                return;
    //            }

    ImGui::Text("IsAnyItemFocused: %s", BOOL_STR(ImGui::IsAnyItemFocused()));

    ImGui::Separator();

    ImGuiHeader("IO");

    {
        ImGuiIO &io = ImGui::GetIO();

        ImGui::Text("WantCaptureKeyboard: %d", io.WantCaptureKeyboard);
        ImGui::Text("WantCaptureMouse: %d", io.WantCaptureMouse);
        ImGui::Text("WantCaptureMouseUnlessPopupClose: %d", io.WantCaptureMouseUnlessPopupClose);
        ImGui::Text("WantTextInput: %d", io.WantTextInput);
    }

    if (ImGui::CollapsingHeader("WindowsFocusOrder")) {
        for (int i = 0; i < GImGui->WindowsFocusOrder.size(); ++i) {
            //ImGuiIDPusher id_pusher(i);

            ImGuiWindow *w = GImGui->WindowsFocusOrder[i];
            ImGui::Text("%-2d. %s", i, w->Name);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Name: %s", w->Name);
                ImGui::Text("ID: 0x%x", w->ID);
                ImGui::Text("WasActive: %s", BOOL_STR(w->WasActive));
                ImGui::Text("Flags: Child: %s", BOOL_STR(w->Flags & ImGuiWindowFlags_ChildWindow));
                ImGui::Text("       NoMouseInputs: %s", BOOL_STR(w->Flags & ImGuiWindowFlags_NoMouseInputs));
                ImGui::Text("       NoNavInputs: %s", BOOL_STR(w->Flags & ImGuiWindowFlags_NoNavInputs));
                ImGui::Text("NavLastIDs: Main: 0x%x", w->NavLastIds[0]);
                ImGui::Text("            Menu: 0x%x", w->NavLastIds[1]);
                ImGui::EndTooltip();
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::UpdateImTextureData(ImTextureData *im_texture) {
    switch (im_texture->Status) {
    default:
        ASSERT(false);
        break;

    case ImTextureStatus_WantCreate:
        if (m_renderer) {
            ASSERT(im_texture->TexID == 0);
            ASSERT(!im_texture->BackendUserData);
            ASSERT(im_texture->Format == ImTextureFormat_RGBA32);

            SetRenderScaleQualityHint(false);
            SDL_Texture *sdl_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, im_texture->Width, im_texture->Height);
            if (!sdl_texture) {
                // Welp
                im_texture->SetTexID(0);
            } else {
                SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND);

                const void *pixels = im_texture->GetPixels();
                int pitch = im_texture->GetPitch();
                SDL_UpdateTexture(sdl_texture, nullptr, pixels, pitch);

                im_texture->SetTexID((ImTextureID)(intptr_t)sdl_texture);
                im_texture->SetStatus(ImTextureStatus_OK);
            }
        }
        break;

    case ImTextureStatus_WantUpdates:
        if (m_renderer) {
            if (auto sdl_texture = (SDL_Texture *)im_texture->TexID) {
                for (const ImTextureRect &im_rect : im_texture->Updates) {
                    SDL_Rect sdl_rect;
                    sdl_rect.x = im_rect.x;
                    sdl_rect.y = im_rect.y;
                    sdl_rect.w = im_rect.w;
                    sdl_rect.h = im_rect.h;

                    const void *pixels = im_texture->GetPixelsAt(im_rect.x, im_rect.y);
                    int pitch = im_texture->GetPitch();
                    SDL_UpdateTexture(sdl_texture, &sdl_rect, pixels, pitch);
                }
            }
        }
        break;

    case ImTextureStatus_WantDestroy:
        {
            if (m_renderer) {
                if (auto sdl_texture = (SDL_Texture *)im_texture->TexID) {
                    SDL_DestroyTexture(sdl_texture);
                    im_texture->SetTexID(0);
                    im_texture->SetStatus(ImTextureStatus_Destroyed);
                }
            }
        }
        break;
    }
}

void ImGuiStuff::EnsureFontsReady() {
    if (!m_fonts_dirty) {
        return;
    }

    ImGuiIO &io = ImGui::GetIO();

    io.Fonts->Clear();

    ImFontConfig font_config;
    font_config.SizePixels = 13.f;
    if (m_pixel_font) {
        io.Fonts->AddFontDefault(&font_config);
    } else {
        io.Fonts->AddFontFromFileTTF(GetAssetPath(DEFAULT_OUTLINE_FONT_FILE_NAME).c_str(), font_config.SizePixels, &font_config);
    }

    ImFontConfig fa_config;
    fa_config.MergeMode = true;
    fa_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF(GetAssetPath(FAS_FILE_NAME).c_str(), font_config.SizePixels, &fa_config, FA_ICONS_RANGES);

    m_fonts_dirty = false;
    return;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiIDPusher::ImGuiIDPusher(const char *str_id) {
    ImGui::PushID(str_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiIDPusher::ImGuiIDPusher(const char *str_id_begin, const char *str_id_end) {
    ImGui::PushID(str_id_begin, str_id_end);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiIDPusher::ImGuiIDPusher(const void *ptr_id) {
    ImGui::PushID(ptr_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiIDPusher::ImGuiIDPusher(int int_id) {
    ImGui::PushID(int_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiIDPusher::ImGuiIDPusher(uint32_t uint_id) {
    ImGui::PushID((int)uint_id);
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiIDPusher::~ImGuiIDPusher() {
    ImGui::PopID();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiItemWidthPusher::ImGuiItemWidthPusher(float width) {
    ImGui::PushItemWidth(width);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiItemWidthPusher::~ImGuiItemWidthPusher() {
    ImGui::PopItemWidth();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleColourPusher::ImGuiStyleColourPusher() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleColourPusher::ImGuiStyleColourPusher(ImGuiCol idx0, const ImVec4 &col0) {
    this->Push(idx0, col0);
}

ImGuiStyleColourPusher::ImGuiStyleColourPusher(ImGuiCol idx0, const ImVec4 &col0, ImGuiCol idx1, const ImVec4 &col1) {
    this->Push(idx0, col0);
    this->Push(idx1, col1);
}

ImGuiStyleColourPusher::ImGuiStyleColourPusher(ImGuiCol idx0, const ImVec4 &col0, ImGuiCol idx1, const ImVec4 &col1, ImGuiCol idx2, const ImVec4 &col2) {
    this->Push(idx0, col0);
    this->Push(idx1, col1);
    this->Push(idx2, col2);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleColourPusher::~ImGuiStyleColourPusher() {
    ImGui::PopStyleColor(m_num_pushes);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleColourPusher::ImGuiStyleColourPusher(ImGuiStyleColourPusher &&oth) {
    m_num_pushes = oth.m_num_pushes;
    oth.m_num_pushes = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// returns *this.
ImGuiStyleColourPusher &ImGuiStyleColourPusher::Push(ImGuiCol idx, const ImVec4 &col) {
    ImGui::PushStyleColor(idx, col);
    ++m_num_pushes;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStyleColourPusher::Pop(int count) {
    ASSERT(m_num_pushes >= count);
    ImGui::PopStyleColor(count);
    m_num_pushes -= count;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStyleColourPusher::PushDefault(ImGuiCol idx0, ImGuiCol idx1, ImGuiCol idx2, ImGuiCol idx3) {
    this->PushDefaultInternal(idx0);
    this->PushDefaultInternal(idx1);
    this->PushDefaultInternal(idx2);
    this->PushDefaultInternal(idx3);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStyleColourPusher::PushDisabledButtonColours(bool disabled) {
    if (disabled) {
        this->Push(ImGuiCol_Text, DISABLED_BUTTON_TEXT_COLOUR);
        this->Push(ImGuiCol_Button, DISABLED_BUTTON_COLOUR);
        this->Push(ImGuiCol_ButtonHovered, DISABLED_BUTTON_HOVERED_COLOUR);
        this->Push(ImGuiCol_ButtonActive, DISABLED_BUTTON_ACTIVE_COLOUR);
    } else {
        this->PushDefaultButtonColours();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStyleColourPusher::PushDefaultButtonColours() {
    this->PushDefault(ImGuiCol_Text, ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStyleColourPusher::PushDefaultInternal(ImGuiCol idx) {
    if (idx != ImGuiCol_COUNT) {
        ASSERT(idx >= 0 && idx < ImGuiCol_COUNT);
        this->Push(idx, IMGUI_DEFAULT_STYLE.Colors[idx]);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher::ImGuiStyleVarPusher() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher::ImGuiStyleVarPusher(ImGuiStyleVar idx0, float val0) {
    this->Push(idx0, val0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher::ImGuiStyleVarPusher(ImGuiStyleVar idx0, const ImVec2 &val0) {
    this->Push(idx0, val0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher::~ImGuiStyleVarPusher() {
    ImGui::PopStyleVar(m_num_pushes);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher &ImGuiStyleVarPusher::Push(ImGuiStyleVar idx, float val) {
    ImGui::PushStyleVar(idx, val);
    ++m_num_pushes;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher &ImGuiStyleVarPusher::Push(ImGuiStyleVar idx, const ImVec2 &val) {
    ImGui::PushStyleVar(idx, val);
    ++m_num_pushes;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher::ImGuiStyleVarPusher(ImGuiStyleVarPusher &&oth)
    : m_num_pushes(oth.m_num_pushes) {
    oth.m_num_pushes = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiItemFlagPusher::ImGuiItemFlagPusher(ImGuiItemFlags flag, bool enabled) {
    this->Push(flag, enabled);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiItemFlagPusher::~ImGuiItemFlagPusher() {
    for (int i = 0; i < m_num_pushes; ++i) {
        ImGui::PopItemFlag();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiItemFlagPusher &ImGuiItemFlagPusher::Push(ImGuiItemFlags flag, bool enabled) {
    ImGui::PushItemFlag(flag, enabled);
    ++m_num_pushes;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetImRectPosX(ImRect *rect, float x) {
    rect->Max.x = x + rect->GetWidth();
    rect->Min.x = x;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetImRectPosY(ImRect *rect, float y) {
    rect->Max.y = y + rect->GetHeight();
    rect->Min.y = y;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetImRectPos(ImRect *rect, const ImVec2 &pos) {
    SetImRectPosX(rect, pos.x);
    SetImRectPosY(rect, pos.y);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TranslateImRect(ImRect *rect, const ImVec2 &delta) {
    rect->Min += delta;
    rect->Max += delta;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// copied from imgui_stdlib.h, with a bit of a C++17 tweak.
struct ImGuiStdStringInputTextCallbackUserData {
    std::string *str = nullptr;
    ImGuiInputTextCallback next_callback = nullptr;
    void *next_callback_user_data = nullptr;
};

static int ImGuiStdStringInputTextCallback(ImGuiInputTextCallbackData *data) {
    auto user_data = (ImGuiStdStringInputTextCallbackUserData *)data->UserData;

    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        ASSERT(data->Buf == user_data->str->data());
        ASSERT(data->BufTextLen >= 0);
        user_data->str->resize((size_t)data->BufTextLen);
        data->Buf = user_data->str->data();
        return 0;
    } else if (user_data->next_callback) {
        data->UserData = user_data->next_callback_user_data;
        return (*user_data->next_callback)(data);
    } else {
        return 0;
    }
}

bool ImGuiInputText(const char *label, std::string *str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data) {
    ASSERT(!(flags & ImGuiInputTextFlags_CallbackResize));
    flags |= ImGuiInputTextFlags_CallbackResize;

    ImGuiStdStringInputTextCallbackUserData u;
    u.str = str;
    u.next_callback = callback;
    u.next_callback_user_data = user_data;

    bool result = ImGui::InputText(label, str->data(), str->size() + 1, flags, &ImGuiStdStringInputTextCallback, &u);
    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiInputText(std::string *new_str,
                    const char *name,
                    std::string old_str) {
    std::string tmp_str = std::move(old_str);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
    if (!new_str) {
        flags |= ImGuiInputTextFlags_ReadOnly;
    }

    if (!ImGuiInputText(name, &tmp_str, flags)) {
        return false;
    }

    if (new_str) {
        new_str->assign(std::move(tmp_str));
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiHeader(const char *str) {
    ImGui::CollapsingHeader(str, ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Leaf);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ImGuiLED2(ImGuiLEDStyle style, bool on, const char *begin, const char *end) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        return;
    }

    const ImVec2 label_size = ImGui::CalcTextSize(begin, end);
    const ImGuiID id = window->GetID(begin, end);

    const float h = label_size.y + GImGui->Style.FramePadding.y * 2 - 1;
    const ImRect check_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(h, h));
    ImGui::ItemSize(check_bb, GImGui->Style.FramePadding.y);

    const ImRect total_bb = check_bb;
    if (label_size.x > 0.f) {
        ImGui::SameLine(0.f, GImGui->Style.ItemInnerSpacing.x);
    }

    ImRect text_bb(window->DC.CursorPos, window->DC.CursorPos);
    text_bb.Min.y += GImGui->Style.FramePadding.y;
    text_bb.Max.y += GImGui->Style.FramePadding.y;
    text_bb.Max += label_size;

    if (label_size.x > 0.f) {
        ImGui::ItemSize(ImVec2(text_bb.GetWidth(), check_bb.GetHeight()), GImGui->Style.FramePadding.y);
    }

    if (!ImGui::ItemAdd(total_bb, id))
        return;

    ImGuiCol colour;
    if (on) {
        colour = ImGuiCol_CheckMark;
    } else {
        colour = ImGuiCol_FrameBg;
    }

    //const float check_sz=ImMin(check_bb.GetWidth(),check_bb.GetHeight());
    //const float pad=ImMax(1.f,(float)(int)(check_sz/6.f));
    switch (style) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case ImGuiLEDStyle_Circle:
        {
            ImVec2 centre = check_bb.GetCenter();
            centre.x = (float)(int)centre.x + .5f;
            centre.y = (float)(int)centre.y + .5f;

            const float radius = check_bb.GetHeight() * .5f;

            window->DrawList->AddCircleFilled(centre, radius, ImGui::GetColorU32(colour));
        }
        break;

    case ImGuiLEDStyle_Rectangle:
        {
            ImVec2 rect_min(check_bb.Min.x, check_bb.Min.y + check_bb.GetHeight() * (1 / 3.f));
            ImVec2 rect_max(check_bb.Max.x, check_bb.Min.y + check_bb.GetHeight() * (2 / 3.f));
            window->DrawList->AddRectFilled(rect_min, rect_max, ImGui::GetColorU32(colour));
        }
        break;
    };

    if (label_size.x > 0.f) {
        ImGui::RenderText(text_bb.GetTL(), begin, end);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiLED(ImGuiLEDStyle style, bool on, const char *str) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        return;
    }

    ImGuiLED2(style, on, str, str + strlen(str));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiLEDv(ImGuiLEDStyle style, bool on, const char *fmt, va_list v) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        return;
    }

    char buf[1000];
    int n = ImFormatStringV(buf, IM_ARRAYSIZE(buf), fmt, v);
    ImGuiLED2(style, on, buf, buf + n);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiLEDf(ImGuiLEDStyle style, bool on, const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    ImGuiLEDv(style, on, fmt, v);
    va_end(v);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiMenuItemFlag(const char *label, const char *shortcut, uint32_t *selected, uint32_t selected_mask, bool enabled) {
    bool tmp = !!(*selected & selected_mask);

    bool result = ImGui::MenuItem(label, shortcut, &tmp, enabled);

    if (tmp) {
        *selected |= selected_mask;
    } else {
        *selected &= ~selected_mask;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiButton(const char *label, bool enabled) {
    ImGuiStyleColourPusher pusher;

    if (!enabled) {
        pusher.PushDisabledButtonColours();
    }

    if (ImGui::Button(label)) {
        if (enabled) {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char CONFIRM_BUTTON_POPUP[] = "confirm_popup";

bool ImGuiConfirmButton(const char *label, bool needs_confirm) {
    if (needs_confirm) {
        bool click = false;

        ImGuiIDPusher pusher(label);

        if (ImGui::Button(label)) {
            ImGui::OpenPopup(CONFIRM_BUTTON_POPUP);
        }

        if (ImGui::BeginPopup(CONFIRM_BUTTON_POPUP)) {
            if (ImGui::Button("Confirm")) {
                click = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        return click;
    } else {
        return ImGui::Button(label);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Lame mostly copy-paste. As PlotEx, but with Y axis markers.
//
// How to update:
//
// 1. copy body of ImGui::PlotEx into PlotEx2
//
// 2. stick 'using namespace ImGui' somewhere, so it builds
//
// 3. add call to PlotEx2Markers as required, so that the markers appear.

static void PlotEx2Markers(ImGuiPlotType plot_type,
                           float scale_min,
                           float scale_max,
                           const ImVec2 &markers,
                           ImGuiWindow *window,
                           const ImRect &inner_bb) {
    if (markers.y > 0.f) {
        // use colour for other graph type, for contrast.
        const ImU32 col = ImGui::GetColorU32((plot_type == ImGuiPlotType_Lines) ? ImGuiCol_PlotHistogram : ImGuiCol_PlotLines);
        float dy = markers.y / (scale_max - scale_min) * inner_bb.GetHeight();

        for (float y = inner_bb.Max.y; y >= inner_bb.Min.y; y -= dy) {
            window->DrawList->AddLine(ImVec2(inner_bb.Min.x, y), ImVec2(inner_bb.Max.x, y), col);
        }
    }
}

static int PlotEx2(
    ImGuiPlotType plot_type,
    const char *label,
    float (*values_getter)(void *data, int idx), void *data,
    int values_count, int values_offset,
    const char *overlay_text,
    float scale_min, float scale_max,
    ImVec2 frame_size,
    ImVec2 markers) {
    using namespace ImGui;

    ImGuiContext &g = *GImGui;
    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems)
        return -1;

    const ImGuiStyle &style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    if (frame_size.x == 0.0f)
        frame_size.x = CalcItemWidth();
    if (frame_size.y == 0.0f)
        frame_size.y = label_size.y + (style.FramePadding.y * 2);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, 0, &frame_bb))
        return -1;
    const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);

    // Determine scale from values if not specified
    if (scale_min == FLT_MAX || scale_max == FLT_MAX) {
        float v_min = FLT_MAX;
        float v_max = -FLT_MAX;
        for (int i = 0; i < values_count; i++) {
            const float v = values_getter(data, i);
            if (v != v) // Ignore NaN values
                continue;
            v_min = ImMin(v_min, v);
            v_max = ImMax(v_max, v);
        }
        if (scale_min == FLT_MAX)
            scale_min = v_min;
        if (scale_max == FLT_MAX)
            scale_max = v_max;
    }

    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    PlotEx2Markers(plot_type, scale_min, scale_max, markers, window, inner_bb);

    const int values_count_min = (plot_type == ImGuiPlotType_Lines) ? 2 : 1;
    int idx_hovered = -1;
    if (values_count >= values_count_min) {
        int res_w = ImMin((int)frame_size.x, values_count) + ((plot_type == ImGuiPlotType_Lines) ? -1 : 0);
        int item_count = values_count + ((plot_type == ImGuiPlotType_Lines) ? -1 : 0);

        // Tooltip on hover
        if (hovered && inner_bb.Contains(g.IO.MousePos)) {
            const float t = ImClamp((g.IO.MousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
            const int v_idx = (int)(t * item_count);
            IM_ASSERT(v_idx >= 0 && v_idx < values_count);

            const float v0 = values_getter(data, (v_idx + values_offset) % values_count);
            const float v1 = values_getter(data, (v_idx + 1 + values_offset) % values_count);
            if (plot_type == ImGuiPlotType_Lines)
                SetTooltip("%d: %8.4g\n%d: %8.4g", v_idx, v0, v_idx + 1, v1);
            else if (plot_type == ImGuiPlotType_Histogram)
                SetTooltip("%d: %8.4g", v_idx, v0);
            idx_hovered = v_idx;
        }

        const float t_step = 1.0f / (float)res_w;
        const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));

        float v0 = values_getter(data, (0 + values_offset) % values_count);
        float t0 = 0.0f;
        ImVec2 tp0 = ImVec2(t0, 1.0f - ImSaturate((v0 - scale_min) * inv_scale));                                                   // Point in the normalized space of our target rectangle
        float histogram_zero_line_t = (scale_min * scale_max < 0.0f) ? (-scale_min * inv_scale) : (scale_min < 0.0f ? 0.0f : 1.0f); // Where does the zero line stands

        const ImU32 col_base = GetColorU32((plot_type == ImGuiPlotType_Lines) ? ImGuiCol_PlotLines : ImGuiCol_PlotHistogram);
        const ImU32 col_hovered = GetColorU32((plot_type == ImGuiPlotType_Lines) ? ImGuiCol_PlotLinesHovered : ImGuiCol_PlotHistogramHovered);

        for (int n = 0; n < res_w; n++) {
            const float t1 = t0 + t_step;
            const int v1_idx = (int)(t0 * item_count + 0.5f);
            IM_ASSERT(v1_idx >= 0 && v1_idx < values_count);
            const float v1 = values_getter(data, (v1_idx + values_offset + 1) % values_count);
            const ImVec2 tp1 = ImVec2(t1, 1.0f - ImSaturate((v1 - scale_min) * inv_scale));

            // NB: Draw calls are merged together by the DrawList system. Still, we should render our batch are lower level to save a bit of CPU.
            ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
            ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, (plot_type == ImGuiPlotType_Lines) ? tp1 : ImVec2(tp1.x, histogram_zero_line_t));
            if (plot_type == ImGuiPlotType_Lines) {
                window->DrawList->AddLine(pos0, pos1, idx_hovered == v1_idx ? col_hovered : col_base);
            } else if (plot_type == ImGuiPlotType_Histogram) {
                if (pos1.x >= pos0.x + 2.0f)
                    pos1.x -= 1.0f;
                window->DrawList->AddRectFilled(pos0, pos1, idx_hovered == v1_idx ? col_hovered : col_base);
            }

            t0 = t1;
            tp0 = tp1;
        }
    }

    // Text overlay
    if (overlay_text)
        RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, overlay_text, NULL, NULL, ImVec2(0.5f, 0.0f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);

    // Return hovered index or -1 if none are hovered.
    // This is currently not exposed in the public API because we need a larger redesign of the whole thing, but in the short-term we are making it available in PlotEx().
    return idx_hovered;
}

struct ImGuiPlotArrayGetterData2 {
    const float *Values;
    int Stride;

    ImGuiPlotArrayGetterData2(const float *values, int stride) {
        Values = values;
        Stride = stride;
    }
};

static float Plot2_ArrayGetter(void *data, int idx) {
    ImGuiPlotArrayGetterData2 *plot_data = (ImGuiPlotArrayGetterData2 *)data;
    ASSERT(idx >= 0);
    ASSERT(plot_data->Stride >= 0);
    const float v = *(float *)(void *)((unsigned char *)plot_data->Values + (size_t)idx * (size_t)plot_data->Stride);
    return v;
}

void ImGuiPlotLines(const char *label,
                    const float *values,
                    int values_count,
                    int values_offset,
                    const char *overlay_text,
                    float scale_min,
                    float scale_max,
                    ImVec2 graph_size,
                    ImVec2 markers,
                    int stride) {
    ImGuiPlotArrayGetterData2 data(values, stride);
    PlotEx2(ImGuiPlotType_Lines,
            label,
            &Plot2_ArrayGetter,
            (void *)&data,
            values_count,
            values_offset,
            overlay_text,
            scale_min,
            scale_max,
            graph_size,
            markers);
}

void ImGuiPlotLines(const char *label,
                    float (*values_getter)(void *data, int idx),
                    void *data,
                    int values_count,
                    int values_offset,
                    const char *overlay_text,
                    float scale_min,
                    float scale_max,
                    ImVec2 graph_size,
                    ImVec2 markers) {
    PlotEx2(ImGuiPlotType_Lines,
            label,
            values_getter,
            data,
            values_count,
            values_offset,
            overlay_text,
            scale_min,
            scale_max,
            graph_size,
            markers);
}

void ImGuiPlotHistogram(const char *label,
                        const float *values,
                        int values_count,
                        int values_offset,
                        const char *overlay_text,
                        float scale_min,
                        float scale_max,
                        ImVec2 graph_size,
                        ImVec2 markers,
                        int stride) {
    ImGuiPlotArrayGetterData2 data(values, stride);
    PlotEx2(ImGuiPlotType_Histogram,
            label,
            &Plot2_ArrayGetter,
            (void *)&data,
            values_count,
            values_offset,
            overlay_text,
            scale_min,
            scale_max,
            graph_size,
            markers);
}

void ImGuiPlotHistogram(const char *label,
                        float (*values_getter)(void *data, int idx),
                        void *data,
                        int values_count,
                        int values_offset,
                        const char *overlay_text,
                        float scale_min,
                        float scale_max,
                        ImVec2 graph_size,
                        ImVec2 markers) {
    PlotEx2(ImGuiPlotType_Histogram,
            label,
            values_getter,
            data,
            values_count,
            values_offset,
            overlay_text,
            scale_min,
            scale_max,
            graph_size,
            markers);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiRecentMenu(std::string *selected_path,
                     const char *title,
                     RecentPaths *paths) {
    size_t num_paths = paths->GetNumPaths();
    bool selected = false;

    if (ImGui::BeginMenu(title, num_paths > 0)) {
        for (size_t path_index = 0; path_index < num_paths; ++path_index) {
            const std::string &path = paths->GetPathByIndex(path_index);
            if (ImGui::MenuItem(path.c_str())) {
                *selected_path = path;
                selected = true;
            }
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Remove item")) {
            size_t path_index = 0;

            while (path_index < paths->GetNumPaths()) {
                if (ImGui::MenuItem(paths->GetPathByIndex(path_index).c_str())) {
                    paths->RemovePathByIndex(path_index);
                } else {
                    ++path_index;
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    return selected;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
