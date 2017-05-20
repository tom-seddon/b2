#include <shared/system.h>
#include "dear_imgui.h"
#include <shared/debug.h>
#include <SDL.h>
#include <vector>
#include <shared/system_specific.h>
#include <limits.h>
#include <IconsFontAwesome.h>
#include "load_save.h"
#include "misc.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if !SYSTEM_WINDOWS
#define USE_SDL_CLIPBOARD 1
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const ImVec4 &DISABLED_BUTTON_COLOUR=ImVec4(.4f,.4f,.4f,1.f);
const ImVec4 &DISABLED_BUTTON_HOVERED_COLOUR=DISABLED_BUTTON_COLOUR;
const ImVec4 &DISABLED_BUTTON_ACTIVE_COLOUR=DISABLED_BUTTON_COLOUR;
const ImVec4 &DISABLED_BUTTON_TEXT_COLOUR=ImVec4(.6f,.6f,.6f,1.f);

const ImGuiStyle IMGUI_DEFAULT_STYLE;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string FA_FILE_NAME="fonts/fontawesome-webfont.ttf";
static const ImWchar FA_ICONS_RANGES[]={ICON_MIN_FA,ICON_MAX_FA,0};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ImGuiShutdownObject {
    ~ImGuiShutdownObject() {
        ImGui::Shutdown();
    }
};

static ImGuiShutdownObject g_imgui_shutdown_object;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiContextSetter::ImGuiContextSetter(ImGuiStuff *stuff):
    m_old_imgui_context(ImGui::GetCurrentContext())
{
    if(stuff->m_context)
        ImGui::SetCurrentContext(stuff->m_context);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ImGuiContextSetter::~ImGuiContextSetter()
{
    ImGui::SetCurrentContext(m_old_imgui_context);
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

        ImGuiIO &io=ImGui::GetIO();

        ImGui::Shutdown();

        delete io.Fonts;
        io.Fonts=m_original_font_atlas;

        ImGui::DestroyContext(m_context);
        m_context=nullptr;
    }

    if(m_font_texture) {
        SDL_DestroyTexture(m_font_texture);
        m_font_texture=nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if USE_SDL_CLIPBOARD
static const char *GetClipboardText(void *user_data) {
    (void)user_data;

    return SDL_GetClipboardText();
}
#endif

#if USE_SDL_CLIPBOARD
static void SetClipboardText(void *user_data,const char *text) {
    (void)user_data;

    SDL_SetClipboardText(text);
}
#endif

bool ImGuiStuff::Init() {
    int rc;

    m_context=ImGui::CreateContext();

    ImGuiContextSetter setter(this);

    ImGuiIO &io=ImGui::GetIO();

#if SYSTEM_WINDOWS

    {
        float x=(float)GetSystemMetrics(SM_CXDOUBLECLK);
        float y=(float)GetSystemMetrics(SM_CYDOUBLECLK);

        io.MouseDoubleClickMaxDist=sqrtf(x*x+y*y);
    }

    io.MouseDoubleClickTime=GetDoubleClickTime()/1000.f;

#else

    // Answers on a postcard.

#endif

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
    io.Fonts->AddFontFromFileTTF(GetAssetPath(FA_FILE_NAME).c_str(),
        16.f,
        &fa_config,
        FA_ICONS_RANGES);

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
    } else {
        io.MousePos.x=-1.f;
        io.MousePos.y=-1.f;

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

    ImGui::NewFrame();

    m_want_capture_keyboard=io.WantCaptureKeyboard;
    m_want_capture_mouse=io.WantCaptureMouse;
    m_want_text_input=io.WantTextInput;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStuff::Render() {
    int rc;

    ImGuiContextSetter setter(this);

    ImGui::Render();

    ImDrawData *draw_data=ImGui::GetDrawData();
    if(!draw_data) {
        return;
    }

    if(!draw_data->Valid) {
        return;
    }
    for(int i=0;i<draw_data->CmdListsCount;++i) {
        ImDrawList *draw_list=draw_data->CmdLists[i];

        int idx_buffer_pos=0;

        SDL_Vertex *vertices=(SDL_Vertex *)&draw_list->VtxBuffer[0];
        int num_vertices=(int)(draw_list->VtxBuffer.size());

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
                (*cmd.UserCallback)(draw_list,&cmd);
            } else {
                SDL_Texture *texture=(SDL_Texture *)cmd.TextureId;

                int *indices=(int *)&draw_list->IdxBuffer[idx_buffer_pos];
                ASSERT(idx_buffer_pos+(int)cmd.ElemCount<=draw_list->IdxBuffer.size());

                rc=SDL_RenderGeometry(m_renderer,texture,vertices,num_vertices,indices,(int)cmd.ElemCount,nullptr);
                ASSERT(rc==0);

                ASSERT(cmd.ElemCount<=INT_MAX);
                idx_buffer_pos+=(int)cmd.ElemCount;
            }
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

bool ImGuiStuff::WantCaptureMouse() const {
    return m_want_capture_mouse;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiStuff::WantCaptureKeyboard() const {
    return m_want_capture_keyboard;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiStuff::WantTextInput() const {
    return m_want_text_input;
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

void ImGuiStyleColourPusher::PushDefault(ImGuiCol idx0,ImGuiCol idx1,ImGuiCol idx2,ImGuiCol idx3) {
    this->PushDefaultInternal(idx0);
    this->PushDefaultInternal(idx1);
    this->PushDefaultInternal(idx2);
    this->PushDefaultInternal(idx3);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiStyleColourPusher::PushDisabledButtonColours() {
    this->Push(ImGuiCol_Text,DISABLED_BUTTON_TEXT_COLOUR);
    this->Push(ImGuiCol_Button,DISABLED_BUTTON_COLOUR);
    this->Push(ImGuiCol_ButtonHovered,DISABLED_BUTTON_HOVERED_COLOUR);
    this->Push(ImGuiCol_ButtonActive,DISABLED_BUTTON_ACTIVE_COLOUR);
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

    if(!ImGui::ItemAdd(total_bb,&id))
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

bool ImGuiBeginFlag(const char *label,uint32_t *open,uint32_t open_mask,ImGuiWindowFlags flags) {
    bool tmp=!!(*open&open_mask);

    bool result=ImGui::Begin(label,&tmp,flags);

    if(tmp) {
        *open|=open_mask;
    } else {
        *open&=~open_mask;
    }

    return result;
}

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
