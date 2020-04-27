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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#if !SYSTEM_WINDOWS
#define USE_SDL_CLIPBOARD 1
//#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const ImVec4 &DISABLED_BUTTON_COLOUR=ImVec4(.4f,.4f,.4f,1.f);
const ImVec4 &DISABLED_BUTTON_HOVERED_COLOUR=DISABLED_BUTTON_COLOUR;
const ImVec4 &DISABLED_BUTTON_ACTIVE_COLOUR=DISABLED_BUTTON_COLOUR;
const ImVec4 &DISABLED_BUTTON_TEXT_COLOUR=ImVec4(.6f,.6f,.6f,1.f);

const ImGuiStyle IMGUI_DEFAULT_STYLE;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string FAS_FILE_NAME="fonts/" FONT_ICON_FILE_NAME_FAS;
static const ImWchar FA_ICONS_RANGES[]={ICON_MIN_FA,ICON_MAX_FA,0};

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

ImGuiContextSetter::ImGuiContextSetter(const ImGuiStuff *stuff):
    m_old_imgui_context(ImGui::GetCurrentContext()),
    m_old_dock_context(ImGui::GetCurrentDockContext())
{
    if(stuff->m_context) {
        ImGui::SetCurrentContext(stuff->m_context);
    }

    if(stuff->m_dock_context) {
        ImGui::SetCurrentDockContext(stuff->m_dock_context);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiContextSetter::~ImGuiContextSetter() {
    ImGui::SetCurrentContext(m_old_imgui_context);
    ImGui::SetCurrentDockContext(m_old_dock_context);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStuff::ImGuiStuff(SDL_Renderer *renderer):
    m_renderer(renderer)
{
    m_last_new_frame_ticks=GetCurrentTickCount();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStuff::~ImGuiStuff() {
    if(m_context) {
        ImGuiContextSetter setter(this);

        ImGui::DestroyDockContext(m_dock_context);
        m_dock_context=nullptr;

        if(m_in_frame) {
            ImGui::EndFrame();
            m_in_frame=false;
        }

        ImGuiIO &io=ImGui::GetIO();

        delete io.Fonts;
        io.Fonts=m_original_font_atlas;

        ImGui::DestroyContext(m_context);
        m_context=nullptr;
    }

    if(m_font_texture) {
        SDL_DestroyTexture(m_font_texture);
        m_font_texture=nullptr;
    }

    for(size_t i=0;i<sizeof m_cursors/sizeof m_cursors[0];++i) {
        SDL_FreeCursor(m_cursors[i]);
        m_cursors[i]=nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if USE_SDL_CLIPBOARD

static char *g_last_clipboard_text;
static bool g_FreeLastClipboardText_registered;

static void FreeLastClipboardText() {
    SDL_free(g_last_clipboard_text);
    g_last_clipboard_text=nullptr;
}

static const char *GetClipboardText(void *user_data) {
    (void)user_data;

    // This is quite safe, at least for now, due to the way the
    // callback is called...

    if(!g_FreeLastClipboardText_registered) {
        atexit(&FreeLastClipboardText);
    }

    FreeLastClipboardText();
    g_last_clipboard_text=SDL_GetClipboardText();

    return g_last_clipboard_text;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if USE_SDL_CLIPBOARD
static void SetClipboardText(void *user_data,const char *text) {
    (void)user_data;

    SDL_SetClipboardText(text);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiStuff::Init() {
    int rc;
    (void)rc;

    m_context=ImGui::CreateContext();
    m_dock_context=ImGui::CreateDockContext();

    ImGuiContextSetter setter(this);

    ImGuiIO &io=ImGui::GetIO();

#if SYSTEM_WINDOWS

    {
        float x=(float)GetSystemMetrics(SM_CXDOUBLECLK);
        float y=(float)GetSystemMetrics(SM_CYDOUBLECLK);

        io.MouseDoubleClickMaxDist=sqrtf(x*x+y*y);
    }

    io.MouseDoubleClickTime=GetDoubleClickTime()/1000.f;

#elif SYSTEM_OSX

    io.MouseDoubleClickTime=(float)GetDoubleClickIntervalSeconds();

#endif

    m_cursors[ImGuiMouseCursor_Arrow]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    m_cursors[ImGuiMouseCursor_ResizeAll]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    m_cursors[ImGuiMouseCursor_ResizeEW]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    m_cursors[ImGuiMouseCursor_ResizeNS]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    m_cursors[ImGuiMouseCursor_ResizeNESW]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
    m_cursors[ImGuiMouseCursor_ResizeNWSE]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
    m_cursors[ImGuiMouseCursor_TextInput]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);

    m_imgui_ini_path=GetConfigPath("imgui.ini");
    io.IniFilename=m_imgui_ini_path.c_str();

    // On OS X, could this usefully go in ~/Library/Logs?
    m_imgui_log_txt_path=GetCachePath("imgui_log.txt");
    io.LogFilename=m_imgui_log_txt_path.c_str();

#if USE_SDL_CLIPBOARD

    io.GetClipboardTextFn=&GetClipboardText;
    io.SetClipboardTextFn=&SetClipboardText;

#endif

    io.KeyMap[ImGuiKey_Tab]=SDL_SCANCODE_TAB;// for tabbing through fields
    io.KeyMap[ImGuiKey_LeftArrow]=SDL_SCANCODE_LEFT;// for text edit
    io.KeyMap[ImGuiKey_RightArrow]=SDL_SCANCODE_RIGHT;// for text edit
    io.KeyMap[ImGuiKey_UpArrow]=SDL_SCANCODE_UP;// for text edit
    io.KeyMap[ImGuiKey_DownArrow]=SDL_SCANCODE_DOWN;// for text edit
    io.KeyMap[ImGuiKey_PageUp]=SDL_SCANCODE_PAGEUP;
    io.KeyMap[ImGuiKey_PageDown]=SDL_SCANCODE_PAGEDOWN;
    io.KeyMap[ImGuiKey_Home]=SDL_SCANCODE_HOME;// for text edit
    io.KeyMap[ImGuiKey_End]=SDL_SCANCODE_END;// for text edit
    io.KeyMap[ImGuiKey_Delete]=SDL_SCANCODE_DELETE;// for text edit
    io.KeyMap[ImGuiKey_Backspace]=SDL_SCANCODE_BACKSPACE;// for text edit
    io.KeyMap[ImGuiKey_Enter]=SDL_SCANCODE_RETURN;// for text edit
    io.KeyMap[ImGuiKey_Escape]=SDL_SCANCODE_ESCAPE;// for text edit
    io.KeyMap[ImGuiKey_A]=SDL_GetScancodeFromKey(SDLK_a);// for text edit CTRL+A: select all
    io.KeyMap[ImGuiKey_C]=SDL_GetScancodeFromKey(SDLK_c);// for text edit CTRL+C: copy
    io.KeyMap[ImGuiKey_V]=SDL_GetScancodeFromKey(SDLK_v);// for text edit CTRL+V: paste
    io.KeyMap[ImGuiKey_X]=SDL_GetScancodeFromKey(SDLK_x);// for text edit CTRL+X: cut
    io.KeyMap[ImGuiKey_Y]=SDL_GetScancodeFromKey(SDLK_y);// for text edit CTRL+Y: redo
    io.KeyMap[ImGuiKey_Z]=SDL_GetScancodeFromKey(SDLK_z);// for text edit CTRL+Z: undo

    // https://github.com/ocornut/imgui/commit/aa11934efafe4db75993e23aacacf9ed8b1dd40c#diff-bbaa16f299ca6d388a3a779b16572882L446

    m_original_font_atlas=io.Fonts;
    io.Fonts=new ImFontAtlas;
    io.Fonts->AddFontDefault();

    ImFontConfig fa_config;
    fa_config.MergeMode=true;
    fa_config.PixelSnapH=true;
    io.Fonts->AddFontFromFileTTF(GetAssetPath(FAS_FILE_NAME).c_str(),12.f,&fa_config,FA_ICONS_RANGES);

    unsigned char *pixels;
    int width,height;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);

    SetRenderScaleQualityHint(false);
    m_font_texture=SDL_CreateTexture(m_renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,width,height);
    if(!m_font_texture) {
        return false;
    }

    SDL_SetTextureBlendMode(m_font_texture,SDL_BLENDMODE_BLEND);

    Uint32 font_texture_format;
    rc=SDL_QueryTexture(m_font_texture,&font_texture_format,NULL,NULL,NULL);
    ASSERT(rc==0);

    std::vector<uint8_t> tmp((size_t)(width*height*4));

    rc=SDL_ConvertPixels(width,height,SDL_PIXELFORMAT_ARGB8888,pixels,width*4,font_texture_format,tmp.data(),width*4);
    ASSERT(rc==0);

    rc=SDL_UpdateTexture(m_font_texture,NULL,tmp.data(),width*4);
    ASSERT(rc==0);

    io.Fonts->TexID=m_font_texture;

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
extern "C" HCURSOR SDL_cursor;
#endif

void ImGuiStuff::NewFrame(bool got_mouse_focus) {
    ImGuiContextSetter setter(this);
    ImGuiIO &io=ImGui::GetIO();

    uint64_t now_ticks=GetCurrentTickCount();
    io.DeltaTime=(float)GetSecondsFromTicks(now_ticks-m_last_new_frame_ticks);
    m_last_new_frame_ticks=now_ticks;

    if(got_mouse_focus) {
        int x,y;
        Uint32 b=SDL_GetMouseState(&x,&y);

        io.MousePos.x=(float)x;
        io.MousePos.y=(float)y;

        io.MouseDown[0]=!!(b&SDL_BUTTON_LMASK);
        io.MouseDown[1]=!!(b&SDL_BUTTON_RMASK);
        io.MouseDown[2]=!!(b&SDL_BUTTON_MMASK);

        SDL_Keymod m=SDL_GetModState();

        io.KeyCtrl=!!(m&KMOD_CTRL);
        io.KeyAlt=!!(m&KMOD_ALT);
        io.KeyShift=!!(m&KMOD_SHIFT);
        io.KeySuper=!!(m&KMOD_GUI);

        {
            ImGuiMouseCursor cursor=ImGui::GetMouseCursor();

            if(cursor>=0&&cursor<ImGuiMouseCursor_COUNT&&m_cursors[cursor]) {
                SDL_SetCursor(m_cursors[cursor]);
            } else {
                SDL_SetCursor(nullptr);
            }
        }

    } else {
        io.MousePos.x=-FLT_MAX;
        io.MousePos.y=-FLT_MAX;

        io.MouseDown[0]=false;
        io.MouseDown[1]=false;
        io.MouseDown[2]=false;

        io.KeyCtrl=false;
        io.KeyAlt=false;
        io.KeyShift=false;
    }

    {
        int output_width,output_height;
        SDL_GetRendererOutputSize(m_renderer,&output_width,&output_height);

        io.DisplaySize.x=(float)output_width;
        io.DisplaySize.y=(float)output_height;
    }

    io.MouseWheel=(float)m_next_wheel;
    m_next_wheel=0;

    if(m_reset_dock_context) {
        bool set=false;
        if(ImGui::GetCurrentDockContext()==m_dock_context) {
            set=true;
        }

        ImGui::DestroyDockContext(m_dock_context);
        m_dock_context=ImGui::CreateDockContext();

        if(set) {
            ImGui::SetCurrentDockContext(m_dock_context);
        }

        m_reset_dock_context=false;
    }

    ImGui::NewFrame();
    m_in_frame=true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::Render() {
    int rc;
    (void)rc;

    ImGuiContextSetter setter(this);

    ImGui::Render();
    m_in_frame=false;

#if STORE_DRAWLISTS
    m_draw_lists.clear();
#endif

    ImDrawData *draw_data=ImGui::GetDrawData();
    if(!draw_data) {
        return;
    }

    if(!draw_data->Valid) {
        return;
    }

#if STORE_DRAWLISTS
    ASSERT(draw_data->CmdListsCount>=0);
    m_draw_lists.resize((size_t)draw_data->CmdListsCount);
#endif

    for(int i=0;i<draw_data->CmdListsCount;++i) {
        ImDrawList *draw_list=draw_data->CmdLists[i];

        int idx_buffer_pos=0;

        SDL_Vertex *vertices=(SDL_Vertex *)&draw_list->VtxBuffer[0];
        Uint16 num_vertices=(Uint16)(draw_list->VtxBuffer.size());
        ASSERT(draw_list->VtxBuffer.size()<=(std::numeric_limits<decltype(num_vertices)>::max)());

#if STORE_DRAWLISTS
        StoredDrawList *stored_list=&m_draw_lists[(size_t)i];
        ASSERT(draw_list->CmdBuffer.size()>=0);
        stored_list->cmds.resize((size_t)draw_list->CmdBuffer.size());

        if(draw_list->_OwnerName) {
            stored_list->name=draw_list->_OwnerName;
        } else {
            stored_list->name.clear();
        }
#endif

#if STORE_DRAWLISTS
        StoredDrawCmd *stored_cmd=stored_list->cmds.data();
#endif

        for(const ImDrawCmd &cmd:draw_list->CmdBuffer) {
            SDL_Rect clip_rect={
                (int)cmd.ClipRect.x,
                (int)cmd.ClipRect.y,
                (int)(cmd.ClipRect.z-cmd.ClipRect.x),
                (int)(cmd.ClipRect.w-cmd.ClipRect.y),
            };

            rc=SDL_RenderSetClipRect(m_renderer,&clip_rect);
            ASSERT(rc==0);

            if(cmd.UserCallback) {
#if STORE_DRAWLISTS
                stored_cmd->callback=true;
#endif

                (*cmd.UserCallback)(draw_list,&cmd);
            } else {
                SDL_Texture *texture=(SDL_Texture *)cmd.TextureId;

#if STORE_DRAWLISTS
                stored_cmd->callback=false;

                if(texture) {
                    SDL_QueryTexture(texture,nullptr,nullptr,&stored_cmd->texture_width,&stored_cmd->texture_height);
                }

                stored_cmd->num_indices=cmd.ElemCount;
#endif

                const uint16_t *indices=&draw_list->IdxBuffer[idx_buffer_pos];
                ASSERT(idx_buffer_pos+(int)cmd.ElemCount<=draw_list->IdxBuffer.size());

                rc=SDL_RenderGeometry(m_renderer,texture,vertices,num_vertices,indices,(int)cmd.ElemCount,nullptr);
                ASSERT(rc==0);

                ASSERT(cmd.ElemCount<=INT_MAX);
                idx_buffer_pos+=(int)cmd.ElemCount;

            }

#if STORE_DRAWLISTS
            ++stored_cmd;
#endif
        }
    }
    rc=SDL_RenderSetClipRect(m_renderer,NULL);
    ASSERT(rc==0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::SetMouseWheel(int delta) {
    m_next_wheel=delta;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::SetKeyDown(uint32_t scancode,bool state) {
    ImGuiContextSetter setter(this);
    ImGuiIO &io=ImGui::GetIO();

    if(scancode<sizeof io.KeysDown/sizeof io.KeysDown[0]) {
        io.KeysDown[scancode]=state;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::AddInputCharactersUTF8(const char *text) {
    ImGuiContextSetter setter(this);
    ImGuiIO &io=ImGui::GetIO();

    io.AddInputCharactersUTF8(text);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiStuff::LoadDockContext(const std::string &config) {
    ImGuiContextSetter setter(this);

    ImGuiHelper::Deserializer deserializer(config.data(),config.size());
    return ImGui::LoadDock(deserializer);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string ImGuiStuff::SaveDockContext() const {
    ImGuiContextSetter setter(this);

    ImGuiHelper::Serializer serializer;
    ImGui::SaveDock(serializer);

    ASSERT(serializer.getBufferSize()>=0);
    std::string result(serializer.getBuffer());
    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::ResetDockContext() {
    m_reset_dock_context=true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if STORE_DRAWLISTS
void ImGuiStuff::DoStoredDrawListWindow() {
    if(ImGui::Begin("Drawlists")) {
        ImGui::Text("%zu draw lists",m_draw_lists.size());

        for(size_t i=0;i<m_draw_lists.size();++i) {
            const StoredDrawList *list=&m_draw_lists[i];

            if(ImGui::TreeNode((const void *)(uintptr_t)i,"\"%s\"; %zu commands",list->name.c_str(),list->cmds.size())) {
                for(size_t j=0;j<list->cmds.size();++j) {
                    const StoredDrawCmd *cmd=&list->cmds[j];

                    if(cmd->callback) {
                        ImGui::Text("%zu. (callback)",j);
                    } else if(cmd->texture_width>0&&cmd->texture_height>0) {
                        ImGui::Text("%zu. %u indices, %dx%d texture",j,cmd->num_indices,cmd->texture_width,cmd->texture_height);
                    } else {
                        ImGui::Text("%zu. %u indices",j,cmd->num_indices);
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

static void DoImGuiWindowText(const char *name,const ImGuiWindow *window) {
    if(window) {
        ImGui::Text("%s: %s (0x%x)",name,window->Name,window->ID);
    } else {
        ImGui::Text("%s: *none*",name);
    }
}

void ImGuiStuff::DoDebugWindow() {
    if(ImGui::Begin("Debug")) {
        DoImGuiWindowText("NavWindow",GImGui->NavWindow);
        ImGui::Text("NavID: 0x%x (alive=%s)",GImGui->NavId,BOOL_STR(GImGui->NavIdIsAlive));

        ImGui::Separator();

        DoImGuiWindowText("HoveredWindow",GImGui->HoveredWindow);
        ImGui::Text("HoveredID: 0x%x",GImGui->HoveredId);

        ImGui::Separator();

        DoImGuiWindowText("ActiveIdWindow",GImGui->ActiveIdWindow);
        ImGui::Text("ActiveID: 0x%x (alive=%s)",GImGui->ActiveId,BOOL_STR(GImGui->ActiveIdIsAlive));

//        if (window != ignore_window && window->WasActive && !(window->Flags & ImGuiWindowFlags_ChildWindow))
//            if ((window->Flags & (ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoNavInputs)) != (ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoNavInputs))
//            {
//                ImGuiWindow* focus_window = NavRestoreLastChildNavWindow(window);
//                FocusWindow(focus_window);
//                return;
//            }

        ImGui::Text("IsAnyItemFocused: %s",BOOL_STR(ImGui::IsAnyItemFocused()));

        ImGui::Separator();

        if(ImGui::CollapsingHeader("WindowsFocusOrder")) {
            for(int i=0;i<GImGui->WindowsFocusOrder.size();++i) {
                //ImGuiIDPusher id_pusher(i);

                ImGuiWindow *w=GImGui->WindowsFocusOrder[i];
                ImGui::Text("%-2d. %s",i,w->Name);
                if(ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Name: %s",w->Name);
                    ImGui::Text("ID: 0x%x",w->ID);
                    ImGui::Text("WasActive: %s",BOOL_STR(w->WasActive));
                    ImGui::Text("Flags: Child: %s",BOOL_STR(w->Flags&ImGuiWindowFlags_ChildWindow));
                    ImGui::Text("       NoMouseInputs: %s",BOOL_STR(w->Flags&ImGuiWindowFlags_NoMouseInputs));
                    ImGui::Text("       NoNavInputs: %s",BOOL_STR(w->Flags&ImGuiWindowFlags_NoNavInputs));
                    ImGui::Text("NavLastIDs: Main: 0x%x",w->NavLastIds[0]);
                    ImGui::Text("            Menu: 0x%x",w->NavLastIds[1]);
                    ImGui::EndTooltip();
                }
            }
        }
    }
    ImGui::End();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiIDPusher::ImGuiIDPusher(const char* str_id) {
    ImGui::PushID(str_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiIDPusher::ImGuiIDPusher(const char* str_id_begin,const char* str_id_end) {
    ImGui::PushID(str_id_begin,str_id_end);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiIDPusher::ImGuiIDPusher(const void* ptr_id) {
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

//bool ImGuiGetListBoxItemFromStringVector(void *data_,int idx,const char **out_text) {
//    auto data=(std::vector<std::string> *)data_;
//
//    if(idx>=0&&(size_t)idx<data->size()) {
//        *out_text=(*data)[idx].c_str();
//        return true;
//    } else {
//        return false;
//    }
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleColourPusher::ImGuiStyleColourPusher() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleColourPusher::ImGuiStyleColourPusher(ImGuiCol idx0,const ImVec4& col0) {
    this->Push(idx0,col0);
}

ImGuiStyleColourPusher::ImGuiStyleColourPusher(ImGuiCol idx0,const ImVec4& col0,ImGuiCol idx1,const ImVec4& col1) {
    this->Push(idx0,col0);
    this->Push(idx1,col1);
}

ImGuiStyleColourPusher::ImGuiStyleColourPusher(ImGuiCol idx0,const ImVec4& col0,ImGuiCol idx1,const ImVec4& col1,ImGuiCol idx2,const ImVec4& col2) {
    this->Push(idx0,col0);
    this->Push(idx1,col1);
    this->Push(idx2,col2);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleColourPusher::~ImGuiStyleColourPusher() {
    ImGui::PopStyleColor(m_num_pushes);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleColourPusher::ImGuiStyleColourPusher(ImGuiStyleColourPusher &&oth) {
    m_num_pushes=oth.m_num_pushes;
    oth.m_num_pushes=0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// returns *this.
ImGuiStyleColourPusher &ImGuiStyleColourPusher::Push(ImGuiCol idx,const ImVec4 &col) {
    ImGui::PushStyleColor(idx,col);
    ++m_num_pushes;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStyleColourPusher::Pop(int count) {
    ASSERT(m_num_pushes>=count);
    ImGui::PopStyleColor(count);
    m_num_pushes-=count;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStyleColourPusher::PushDefault(ImGuiCol idx0,ImGuiCol idx1,ImGuiCol idx2,ImGuiCol idx3) {
    this->PushDefaultInternal(idx0);
    this->PushDefaultInternal(idx1);
    this->PushDefaultInternal(idx2);
    this->PushDefaultInternal(idx3);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStyleColourPusher::PushDisabledButtonColours(bool disabled) {
    if(disabled) {
        this->Push(ImGuiCol_Text,DISABLED_BUTTON_TEXT_COLOUR);
        this->Push(ImGuiCol_Button,DISABLED_BUTTON_COLOUR);
        this->Push(ImGuiCol_ButtonHovered,DISABLED_BUTTON_HOVERED_COLOUR);
        this->Push(ImGuiCol_ButtonActive,DISABLED_BUTTON_ACTIVE_COLOUR);
    } else {
        this->PushDefaultButtonColours();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStyleColourPusher::PushDefaultButtonColours() {
    this->PushDefault(ImGuiCol_Text,ImGuiCol_Button,ImGuiCol_ButtonHovered,ImGuiCol_ButtonActive);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStyleColourPusher::PushDefaultInternal(ImGuiCol idx) {
    if(idx!=ImGuiCol_COUNT) {
        ASSERT(idx>=0&&idx<ImGuiCol_COUNT);
        this->Push(idx,IMGUI_DEFAULT_STYLE.Colors[idx]);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher::ImGuiStyleVarPusher() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher::ImGuiStyleVarPusher(ImGuiStyleVar idx0,float val0) {
    this->Push(idx0,val0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher::ImGuiStyleVarPusher(ImGuiStyleVar idx0,const ImVec2 &val0) {
    this->Push(idx0,val0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher::~ImGuiStyleVarPusher() {
    ImGui::PopStyleVar(m_num_pushes);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher &ImGuiStyleVarPusher::Push(ImGuiStyleVar idx,float val) {
    ImGui::PushStyleVar(idx,val);
    ++m_num_pushes;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher &ImGuiStyleVarPusher::Push(ImGuiStyleVar idx,const ImVec2 &val) {
    ImGui::PushStyleVar(idx,val);
    ++m_num_pushes;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiStyleVarPusher::ImGuiStyleVarPusher(ImGuiStyleVarPusher &&oth):
    m_num_pushes(oth.m_num_pushes)
{
    oth.m_num_pushes=0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImRect MakeImRectFromPosAndSize(const ImVec2 &pos,const ImVec2 &size) {
    return ImRect(pos.x,pos.y,pos.x+size.x,pos.y+size.y);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetImRectPosX(ImRect *rect,float x) {
    rect->Max.x=x+rect->GetWidth();
    rect->Min.x=x;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetImRectPosY(ImRect *rect,float y) {
    rect->Max.y=y+rect->GetHeight();
    rect->Min.y=y;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetImRectPos(ImRect *rect,const ImVec2 &pos) {
    SetImRectPosX(rect,pos.x);
    SetImRectPosY(rect,pos.y);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TranslateImRect(ImRect *rect,const ImVec2 &delta) {
    rect->Min+=delta;
    rect->Max+=delta;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiInputText(std::string *new_str,
                    const char *name,
                    const std::string &old_str)
{
    // This is a bit lame - but ImGui insists on editing a char
    // buffer.
    //
    // 5,000 is supposed to be lots.
    char buf[5000];

    strlcpy(buf,old_str.c_str(),sizeof buf);

    ImGuiInputTextFlags flags=ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll;
    if(!new_str) {
        flags|=ImGuiInputTextFlags_ReadOnly;
    }

    if(!ImGui::InputText(name,buf,sizeof buf,flags)) {
        return false;
    }

    if(new_str) {
        new_str->assign(buf);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiHeader(const char *str) {
    ImGui::CollapsingHeader(str,ImGuiTreeNodeFlags_NoTreePushOnOpen|ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_Bullet);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ImGuiLED2(bool on,const char *begin,const char *end) {
    ImGuiWindow *window=ImGui::GetCurrentWindow();
    if(window->SkipItems) {
        return;
    }

    const ImVec2 label_size=ImGui::CalcTextSize(begin,end);
    const ImGuiID id=window->GetID(begin,end);

    const float h=label_size.y+GImGui->Style.FramePadding.y*2-1;
    const ImRect check_bb(window->DC.CursorPos,window->DC.CursorPos+ImVec2(h,h));
    ImGui::ItemSize(check_bb,GImGui->Style.FramePadding.y);

    const ImRect total_bb=check_bb;
    if(label_size.x>0.f) {
        ImGui::SameLine(0.f,GImGui->Style.ItemInnerSpacing.x);
    }

    ImRect text_bb(window->DC.CursorPos,window->DC.CursorPos);
    text_bb.Min.y+=GImGui->Style.FramePadding.y;
    text_bb.Max.y+=GImGui->Style.FramePadding.y;
    text_bb.Max+=label_size;

    if(label_size.x>0.f) {
        ImGui::ItemSize(ImVec2(text_bb.GetWidth(),check_bb.GetHeight()),GImGui->Style.FramePadding.y);
    }

    if(!ImGui::ItemAdd(total_bb,id))
        return;

    ImVec2 centre=check_bb.GetCenter();
    centre.x=(float)(int)centre.x+.5f;
    centre.y=(float)(int)centre.y+.5f;

    const float radius=check_bb.GetHeight()*.5f;

    ImGuiCol colour;
    if(on) {
        colour=ImGuiCol_CheckMark;
    } else {
        colour=ImGuiCol_FrameBg;
    }

    //const float check_sz=ImMin(check_bb.GetWidth(),check_bb.GetHeight());
    //const float pad=ImMax(1.f,(float)(int)(check_sz/6.f));
    window->DrawList->AddCircleFilled(centre,radius,ImGui::GetColorU32(colour));

    if(label_size.x>0.f) {
        ImGui::RenderText(text_bb.GetTL(),begin,end);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiLED(bool on,const char *str) {
    ImGuiWindow *window=ImGui::GetCurrentWindow();
    if(window->SkipItems) {
        return;
    }

    ImGuiLED2(on,str,str+strlen(str));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiLEDv(bool on,const char *fmt,va_list v) {
    ImGuiWindow *window=ImGui::GetCurrentWindow();
    if(window->SkipItems) {
        return;
    }

    const char *text_end=GImGui->TempBuffer+ImFormatStringV(GImGui->TempBuffer,IM_ARRAYSIZE(GImGui->TempBuffer),fmt,v);
    ImGuiLED2(on,GImGui->TempBuffer,text_end);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiLEDf(bool on,const char *fmt,...) {
    va_list v;

    va_start(v,fmt);
    ImGuiLEDv(on,fmt,v);
    va_end(v);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//bool ImGuiBeginFlag(const char *label,uint32_t *open,uint32_t open_mask,ImGuiWindowFlags flags) {
//    bool tmp=!!(*open&open_mask);
//
//    bool result=ImGui::Begin(label,&tmp,flags);
//
//    if(tmp) {
//        *open|=open_mask;
//    } else {
//        *open&=~open_mask;
//    }
//
//    return result;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiMenuItemFlag(const char* label,const char* shortcut,uint32_t *selected,uint32_t selected_mask,bool enabled) {
    bool tmp=!!(*selected&selected_mask);

    bool result=ImGui::MenuItem(label,shortcut,&tmp,enabled);

    if(tmp) {
        *selected|=selected_mask;
    } else {
        *selected&=~selected_mask;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiButton(const char *label,bool enabled) {
    ImGuiStyleColourPusher pusher;

    if(!enabled) {
        pusher.PushDisabledButtonColours();
    }

    if(ImGui::Button(label)) {
        if(enabled) {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char CONFIRM_BUTTON_POPUP[]="confirm_popup";

bool ImGuiConfirmButton(const char *label,bool needs_confirm) {
    if(needs_confirm) {
        bool click=false;

        ImGuiIDPusher pusher(label);

        if(ImGui::Button(label)) {
            ImGui::OpenPopup(CONFIRM_BUTTON_POPUP);
        }

        if(ImGui::BeginPopup(CONFIRM_BUTTON_POPUP)) {
            if(ImGui::Button("Confirm")) {
                click=true;
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

// Lame mostly copy-paste.

static void PlotEx2(
    ImGuiPlotType plot_type,
    const char* label,
    float (*values_getter)(void* data,int idx),void* data,
    int values_count,int values_offset,
    const char* overlay_text,
    float scale_min,float scale_max,
    ImVec2 graph_size,
    ImVec2 markers)
{
    using namespace ImGui;//lame... oh well.

    ImGuiWindow* window=GetCurrentWindow();
    if(window->SkipItems)
        return;

    ImGuiContext& g=*GImGui;
    const ImGuiStyle& style=g.Style;

    const ImVec2 label_size=CalcTextSize(label,NULL,true);
    if(graph_size.x==0.0f)
        graph_size.x=CalcItemWidth();
    if(graph_size.y==0.0f)
        graph_size.y=label_size.y+(style.FramePadding.y*2);

    const ImRect frame_bb(window->DC.CursorPos,window->DC.CursorPos+ImVec2(graph_size.x,graph_size.y));
    const ImRect inner_bb(frame_bb.Min+style.FramePadding,frame_bb.Max-style.FramePadding);
    const ImRect total_bb(frame_bb.Min,frame_bb.Max+ImVec2(label_size.x>0.0f?style.ItemInnerSpacing.x+label_size.x:0.0f,0));
    ItemSize(total_bb,style.FramePadding.y);
    if(!ItemAdd(total_bb,0))
        return;

    // Determine scale from values if not specified
    if(scale_min==FLT_MAX||scale_max==FLT_MAX)
    {
        float v_min=FLT_MAX;
        float v_max=-FLT_MAX;
        for(int i=0; i<values_count; i++)
        {
            const float v=values_getter(data,i);
            v_min=ImMin(v_min,v);
            v_max=ImMax(v_max,v);
        }
        if(scale_min==FLT_MAX)
            scale_min=v_min;
        if(scale_max==FLT_MAX)
            scale_max=v_max;
    }

    RenderFrame(frame_bb.Min,frame_bb.Max,GetColorU32(ImGuiCol_FrameBg),true,style.FrameRounding);

    // TODO: X markers.

    if(markers.y>0.f) {
        // use colour for other graph type, for contrast.
        const ImU32 col=GetColorU32((plot_type==ImGuiPlotType_Lines)?ImGuiCol_PlotHistogram:ImGuiCol_PlotLines);
        float dy=markers.y/(scale_max-scale_min)*inner_bb.GetHeight();

        for(float y=inner_bb.Max.y;y>=inner_bb.Min.y;y-=dy) {
            window->DrawList->AddLine(ImVec2(inner_bb.Min.x,y),ImVec2(inner_bb.Max.x,y),col);
        }
    }

    if(values_count>0)
    {
        int res_w=ImMin((int)graph_size.x,values_count)+((plot_type==ImGuiPlotType_Lines)?-1:0);
        int item_count=values_count+((plot_type==ImGuiPlotType_Lines)?-1:0);

        // Tooltip on hover
        int v_hovered=-1;
        if(ItemHoverable(inner_bb,0))
        {
            const float t=ImClamp((g.IO.MousePos.x-inner_bb.Min.x)/(inner_bb.Max.x-inner_bb.Min.x),0.0f,0.9999f);
            const int v_idx=(int)(t * item_count);
            IM_ASSERT(v_idx>=0&&v_idx<values_count);

            const float v0=values_getter(data,(v_idx+values_offset)%values_count);
            const float v1=values_getter(data,(v_idx+1+values_offset)%values_count);
            if(plot_type==ImGuiPlotType_Lines)
                SetTooltip("%d: %8.4g\n%d: %8.4g",v_idx,v0,v_idx+1,v1);
            else if(plot_type==ImGuiPlotType_Histogram)
                SetTooltip("%d: %8.4g",v_idx,v0);
            v_hovered=v_idx;
        }

        const float t_step=1.0f/(float)res_w;

        float v0=values_getter(data,(0+values_offset)%values_count);
        float t0=0.0f;
        ImVec2 tp0=ImVec2(t0,1.0f-ImSaturate((v0-scale_min)/(scale_max-scale_min)));    // Point in the normalized space of our target rectangle

        const ImU32 col_base=GetColorU32((plot_type==ImGuiPlotType_Lines)?ImGuiCol_PlotLines:ImGuiCol_PlotHistogram);
        const ImU32 col_hovered=GetColorU32((plot_type==ImGuiPlotType_Lines)?ImGuiCol_PlotLinesHovered:ImGuiCol_PlotHistogramHovered);

        for(int n=0; n<res_w; n++)
        {
            const float t1=t0+t_step;
            const int v1_idx=(int)(t0 * item_count+0.5f);
            IM_ASSERT(v1_idx>=0&&v1_idx<values_count);
            const float v1=values_getter(data,(v1_idx+values_offset+1)%values_count);
            const ImVec2 tp1=ImVec2(t1,1.0f-ImSaturate((v1-scale_min)/(scale_max-scale_min)));

            // NB: Draw calls are merged together by the DrawList system. Still, we should render our batch are lower level to save a bit of CPU.
            ImVec2 pos0=ImLerp(inner_bb.Min,inner_bb.Max,tp0);
            ImVec2 pos1=ImLerp(inner_bb.Min,inner_bb.Max,(plot_type==ImGuiPlotType_Lines)?tp1:ImVec2(tp1.x,1.0f));
            if(plot_type==ImGuiPlotType_Lines)
            {
                window->DrawList->AddLine(pos0,pos1,v_hovered==v1_idx?col_hovered:col_base);
            } else if(plot_type==ImGuiPlotType_Histogram)
            {
                if(pos1.x>=pos0.x+2.0f)
                    pos1.x-=1.0f;
                window->DrawList->AddRectFilled(pos0,pos1,v_hovered==v1_idx?col_hovered:col_base);
            }

            t0=t1;
            tp0=tp1;
        }
    }

    // Text overlay
    if(overlay_text)
        RenderTextClipped(ImVec2(frame_bb.Min.x,frame_bb.Min.y+style.FramePadding.y),frame_bb.Max,overlay_text,NULL,NULL,ImVec2(0.5f,0.0f));

    if(label_size.x>0.0f)
        RenderText(ImVec2(frame_bb.Max.x+style.ItemInnerSpacing.x,inner_bb.Min.y),label);
}

struct ImGuiPlotArrayGetterData2 {
    const float* Values;
    int Stride;

    ImGuiPlotArrayGetterData2(const float* values,int stride) { Values=values; Stride=stride; }
};

static float Plot2_ArrayGetter(void* data,int idx) {
    ImGuiPlotArrayGetterData2* plot_data=(ImGuiPlotArrayGetterData2*)data;
    ASSERT(idx>=0);
    ASSERT(plot_data->Stride>=0);
    const float v=*(float*)(void*)((unsigned char*)plot_data->Values+(size_t)idx * (size_t)plot_data->Stride);
    return v;
}

void ImGuiPlotLines(const char* label,
                    const float* values,
                    int values_count,
                    int values_offset,
                    const char* overlay_text,
                    float scale_min,
                    float scale_max,
                    ImVec2 graph_size,
                    ImVec2 markers,
                    int stride)
{
    ImGuiPlotArrayGetterData2 data(values,stride);
    PlotEx2(ImGuiPlotType_Lines,label,&Plot2_ArrayGetter,(void*)&data,values_count,values_offset,overlay_text,scale_min,scale_max,graph_size,markers);
}

void ImGuiPlotLines(const char* label,
                    float (*values_getter)(void* data,int idx),
                    void* data,
                    int values_count,
                    int values_offset,
                    const char* overlay_text,
                    float scale_min,
                    float scale_max,
                    ImVec2 graph_size,
                    ImVec2 markers)
{
    PlotEx2(ImGuiPlotType_Lines,label,values_getter,data,values_count,values_offset,overlay_text,scale_min,scale_max,graph_size,markers);
}

void ImGuiPlotHistogram(const char* label,
                        const float* values,
                        int values_count,
                        int values_offset,
                        const char* overlay_text,
                        float scale_min,
                        float scale_max,
                        ImVec2 graph_size,
                        ImVec2 markers,
                        int stride)
{
    ImGuiPlotArrayGetterData2 data(values,stride);
    PlotEx2(ImGuiPlotType_Histogram,label,&Plot2_ArrayGetter,(void*)&data,values_count,values_offset,overlay_text,scale_min,scale_max,graph_size,markers);
}

void ImGuiPlotHistogram(const char* label,
                        float (*values_getter)(void* data,int idx),
                        void* data,
                        int values_count,
                        int values_offset,
                        const char* overlay_text,
                        float scale_min,
                        float scale_max,
                        ImVec2 graph_size,
                        ImVec2 markers)
{
    PlotEx2(ImGuiPlotType_Histogram,label,values_getter,data,values_count,values_offset,overlay_text,scale_min,scale_max,graph_size,markers);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool IsModifierKey(SDL_Scancode k) {
    switch(k) {
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

uint32_t ImGuiConsumePressedKeycode() {
    ImGuiIO *io=&ImGui::GetIO();

    // Slightly ugly mishmash of SDL and dear imgui here.
    uint32_t modifiers=GetPCKeyModifiersFromSDLKeymod((uint16_t)SDL_GetModState());

    for(int scancode_=0;scancode_<SDL_NUM_SCANCODES;++scancode_) {
        auto scancode=(SDL_Scancode)scancode_;

        if(io->KeysDown[scancode]) {
            if(IsModifierKey(scancode)) {
                // Ignore...
            } else {
                SDL_Keycode keycode=SDL_GetKeyFromScancode(scancode);
                if(keycode!=0) {
                    io->KeysDown[scancode]=false;
                    
                    return (uint32_t)keycode|modifiers;
                }
            }
        }
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiRecentMenu(std::string *selected_path,
                     const char *title,
                     const SelectorDialog &selector)
{
    RecentPaths *rp=selector.GetRecentPaths();

    size_t num_rp=rp->GetNumPaths();
    bool selected=false;

    if(ImGui::BeginMenu(title,num_rp>0)) {
        for(size_t i=0;i<num_rp;++i) {
            const std::string &path=rp->GetPathByIndex(i);
            if(ImGui::MenuItem(path.c_str())) {
                *selected_path=path;
                selected=true;
            }
        }

        ImGui::Separator();

        if(ImGui::BeginMenu("Remove item")) {
            size_t i=0;

            while(i<rp->GetNumPaths()) {
                if(ImGui::MenuItem(rp->GetPathByIndex(i).c_str())) {
                    rp->RemovePathByIndex(i);
                } else {
                    ++i;
                }
            }
            
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    return selected;
}
