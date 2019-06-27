#include <SDL.h>
#include <stdio.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <limits>

#include "pushwarn_imgui_whatever.h"
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include "popwarn.h"

#include "imgui_node_graph_test_github.h"
#include "emoon_nodes.h"
#include "test_imgui_memory_editor.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static_assert(sizeof(ImDrawVert)==sizeof(SDL_Vertex),"");
static_assert(offsetof(ImDrawVert,pos)==offsetof(SDL_Vertex,position),"");
static_assert(offsetof(ImDrawVert,col)==offsetof(SDL_Vertex,color),"");
static_assert(offsetof(ImDrawVert,uv)==offsetof(SDL_Vertex,tex_coord),"");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#define ASSERT(X) ((X)?(void)0:__debugbreak(),(void)0)
#else
#define ASSERT(X) assert(X)
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static Uint32 g_update_window_event_type;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void FatalError(const char *fmt,...) {
    va_list v;
    va_start(v,fmt);
    vfprintf(stderr,fmt,v);
    va_end(v);

    exit(1);
}

static void FatalSDLError(const char *fmt,...) {
    va_list v;
    va_start(v,fmt);
    vfprintf(stderr,fmt,v);
    va_end(v);

    fprintf(stderr," failed: %s\n",SDL_GetError());

    exit(1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SDL_Deleter {
    void operator()(SDL_Window *w) const {
        SDL_DestroyWindow(w);
    }

    void operator()(SDL_Renderer *r) const {
        SDL_DestroyRenderer(r);
    }

    void operator()(SDL_Texture *t) const {
        SDL_DestroyTexture(t);
    }

    void operator()(SDL_Surface *s) const {
        SDL_FreeSurface(s);
    }

    void operator()(SDL_PixelFormat *p) const {
        SDL_FreeFormat(p);
    }
};

template<class SDLType>
using SDLUniquePtr=std::unique_ptr<SDLType,SDL_Deleter>;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static Uint32 HandleUpdateWindowTimer(Uint32 interval,void *param) {
    (void)param;

    SDL_Event event={};
    event.user.type=g_update_window_event_type;

    SDL_PushEvent(&event);

    return interval;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static void InitImGui(void) {
//    ImGuiIO &io=ImGui::GetIO();
//
//    //io.RenderDrawListsFn=&RenderImGuiDrawLists;
//}
//
//static SDLUniquePtr<SDL_Texture> CreateImGuiFontTexture(SDL_Renderer *renderer) {
//    ImGuiIO &io=ImGui::GetIO();
//
//    io.IniFilename="imgui.ini";
//
//    unsigned char *pixels;
//    int width,height;
//    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
//
//    SDLUniquePtr<SDL_Texture> texture(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_STATIC,width,height));
//    if(!texture) {
//        FatalSDLError("SDL_CreateTexture - %dx%d ImGui font texture",width,height);
//    }
//
//    SDL_UpdateTexture(texture.get(),NULL,pixels,width*4);
//
//    io.Fonts->TexID=texture.get();//ugh.
//
//    return texture;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ImGuiContextSetter
{
public:
    explicit ImGuiContextSetter(ImGuiContext *imgui_context);
    ~ImGuiContextSetter();

    ImGuiContextSetter(const ImGuiContextSetter &)=delete;
    ImGuiContextSetter &operator=(const ImGuiContextSetter &)=delete;

    ImGuiContextSetter(ImGuiContextSetter &&)=delete;
    ImGuiContextSetter &operator=(ImGuiContextSetter &&)=delete;
protected:
private:
    ImGuiContext *m_old_imgui_context;
};

ImGuiContextSetter::ImGuiContextSetter(ImGuiContext *imgui_context):
    m_old_imgui_context(ImGui::GetCurrentContext())
{
    ImGui::SetCurrentContext(imgui_context);
}

ImGuiContextSetter::~ImGuiContextSetter()
{
    ImGui::SetCurrentContext(m_old_imgui_context);
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Window {
public:
    static const char SDL_DATA_NAME[];

    Window();
    ~Window();

    Window(const Window &)=delete;
    Window &operator=(const Window &)=delete;

    bool Init();

    void Update();
    void Render();

    void HandleKeyEvent(const SDL_KeyboardEvent &event);
    void HandleWindowEvent(const SDL_WindowEvent &event);
    void HandleTextInputEvent(const SDL_TextInputEvent &event);
    void HandleMouseWheelEvent(const SDL_MouseWheelEvent &event);
protected:
private:
    SDL_Window *m_window=nullptr;
    SDL_Renderer *m_renderer=nullptr;
    ImGuiContext *m_imgui_context=nullptr;
    SDL_Texture *m_font_texture=nullptr;

    void NewImGuiFrame();
    void RenderImGuiDrawData();
};

static std::map<Uint32,Window *> g_all_windows;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char Window::SDL_DATA_NAME[]="W";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void NewWindow(void) {
    (new Window)->Init();//yum.
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Window::Window() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Window::~Window() {
    Uint32 id=SDL_GetWindowID(m_window);
    ASSERT(g_all_windows.count(id)==1);
    g_all_windows.erase(id);

    if(m_imgui_context) {
        ImGuiContextSetter context_setter(m_imgui_context);

        //ImGui::Shutdown();

        ImGui::DestroyContext(m_imgui_context);
        m_imgui_context=nullptr;
    }

    SDL_DestroyRenderer(m_renderer);
    m_renderer=nullptr;

    SDL_DestroyWindow(m_window);
    m_window=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Window::Init() {
    int rc;
    (void)rc;

    if(SDL_CreateWindowAndRenderer(640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE,&m_window,&m_renderer)!=0) {
        return false;
    }

    m_imgui_context=ImGui::CreateContext();

    Uint32 id=SDL_GetWindowID(m_window);
    ASSERT(g_all_windows.count(id)==0);
    g_all_windows[id]=this;

    ImGuiContextSetter context_setter(m_imgui_context);

    {
        ImGuiIO &io=ImGui::GetIO();

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

        unsigned char *pixels;
        int width,height;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);

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
    }

    SDL_RendererInfo info;
    SDL_GetRendererInfo(m_renderer,&info);

    SDL_SetWindowTitle(m_window,info.name);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Window::Update() {
    ImGuiContextSetter context_setter(m_imgui_context);

    this->NewImGuiFrame();

    //ImGui::BulletText("hello 0");
    //ImGui::BulletText("hello 1");
    //ImGui::BulletText("hello 2");
    //ImGui::BulletText("hello 3");
    //ImGui::BulletText("hello 4");
    //ImGui::BulletText("hello 5");
    //ImGui::BulletText("hello 6");
    //ImGui::BulletText("hello 7");
    //ImGui::BulletText("hello 8");
    //ImGui::BulletText("hello 9");

    //for(size_t i=0;i<10;++i) {
    //    ImGui::BulletText("hello %zu",i);
    //}

    ImGui::ShowDemoWindow();
    ImGuiNodeGraphTestGithub();
    EmoonNodes();
    TestImguiMemoryEditor();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Window::Render() {
    ImGuiContextSetter context_setter(m_imgui_context);
    ImGuiIO &io=ImGui::GetIO();

    int output_width,output_height;
    SDL_GetRendererOutputSize(m_renderer,&output_width,&output_height);

    io.DisplaySize.x=(float)output_width;
    io.DisplaySize.y=(float)output_height;

    SDL_SetRenderDrawColor(m_renderer,64,0,0,255);
    SDL_RenderClear(m_renderer);

    ImGui::Render();
    this->RenderImGuiDrawData();

    SDL_RenderPresent(m_renderer);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Window::HandleKeyEvent(const SDL_KeyboardEvent &event) {
    ImGuiContextSetter context_setter(m_imgui_context);
    ImGuiIO &io=ImGui::GetIO();

    int k=event.keysym.scancode;

    //printf("down=%d; scancode=%s (%d; 0x%x); sym=%s (%d; 0x%x)\n",
    //       event.type==SDL_KEYDOWN,
    //       SDL_GetScancodeName(event.keysym.scancode),(int)event.keysym.scancode,(int)event.keysym.scancode,
    //       SDL_GetKeyName(event.keysym.sym),(int)event.keysym.sym,(int)event.keysym.sym);

    if(k>=0&&(size_t)k<sizeof io.KeysDown/sizeof io.KeysDown[0]) {
        io.KeysDown[k]=event.type==SDL_KEYDOWN;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Window::HandleTextInputEvent(const SDL_TextInputEvent &event) {
    ImGuiContextSetter context_setter(m_imgui_context);
    ImGuiIO &io=ImGui::GetIO();

    io.AddInputCharactersUTF8(event.text);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Window::HandleWindowEvent(const SDL_WindowEvent &event) {
    switch(event.event) {
    case SDL_WINDOWEVENT_CLOSE:
        delete this;
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Window::HandleMouseWheelEvent(const SDL_MouseWheelEvent &event) {
    ImGuiContextSetter context_setter(m_imgui_context);
    ImGuiIO &io=ImGui::GetIO();

    io.MouseWheel=(float)event.y;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Window::NewImGuiFrame() {
    ImGuiIO &io=ImGui::GetIO();

    io.DeltaTime=1/50.f;

    {
        int x,y;
        Uint32 b=SDL_GetMouseState(&x,&y);

        io.MousePos.x=(float)x;
        io.MousePos.y=(float)y;

        io.MouseDown[0]=!!(b&SDL_BUTTON_LMASK);
        io.MouseDown[1]=!!(b&SDL_BUTTON_RMASK);
        io.MouseDown[2]=!!(b&SDL_BUTTON_MMASK);
    }

    {
        SDL_Keymod m=SDL_GetModState();

        io.KeyCtrl=!!(m&KMOD_CTRL);
        io.KeyAlt=!!(m&KMOD_ALT);
        io.KeyShift=!!(m&KMOD_SHIFT);
    }

    int output_width,output_height;
    SDL_GetRendererOutputSize(m_renderer,&output_width,&output_height);

    io.DisplaySize.x=(float)output_width;
    io.DisplaySize.y=(float)output_height;

    ImGui::NewFrame();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Window::RenderImGuiDrawData() {
    int rc;
    (void)rc;

    ImDrawData *draw_data=ImGui::GetDrawData();
    if(!draw_data) {
        return;
    }

    if(!draw_data->Valid) {
        return;
    }
    for(int i=0;i<draw_data->CmdListsCount;++i) {
        ImDrawList *draw_list=draw_data->CmdLists[i];

        size_t idx_buffer_pos=0;

        SDL_Vertex *vertices=(SDL_Vertex *)&draw_list->VtxBuffer[0];
        Uint16 num_vertices=(Uint16)(draw_list->VtxBuffer.size());
        assert(draw_list->VtxBuffer.size()<=(std::numeric_limits<decltype(num_vertices)>::max)());

        for(int j=0;j<draw_list->CmdBuffer.size();++j) {
            //if(j==0) {
            //    continue;
            //}

            const ImDrawCmd &cmd=draw_list->CmdBuffer[j];

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

                ASSERT(idx_buffer_pos<=INT_MAX);
                const uint16_t *indices=&draw_list->IdxBuffer[(int)idx_buffer_pos];
                assert(idx_buffer_pos+cmd.ElemCount<=(size_t)draw_list->IdxBuffer.size());

                rc=SDL_RenderGeometry(m_renderer,texture,vertices,num_vertices,indices,(int)cmd.ElemCount,NULL);
                ASSERT(rc==0);

                idx_buffer_pos+=cmd.ElemCount;
            }
        }
    }
    rc=SDL_RenderSetClipRect(m_renderer,NULL);
    ASSERT(rc==0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class EventType>
static void HandleEvent(const EventType &event,void (Window::*mfn)(const EventType &)) {
    auto &&it=g_all_windows.find(event.windowID);
    if(it==g_all_windows.end()) {
        return;
    }

    (it->second->*mfn)(event);
}

int main(int argc,char *argv[]) {
    (void)argc,(void)argv;

    (void)&FatalError;

#ifdef _MSC_VER
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG)|_CRTDBG_LEAK_CHECK_DF);
    //_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG)|_CRTDBG_CHECK_ALWAYS_DF|);
    //_crtBreakAlloc=191;
#endif

    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0) {
        FatalSDLError("SDL_Init");
    }

    SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengles2");

    g_update_window_event_type=SDL_RegisterEvents(1);
    SDL_AddTimer(20,&HandleUpdateWindowTimer,NULL);

    SDL_StartTextInput();

    NewWindow();

    for(;;) {
        SDL_Event event;
        if(!SDL_WaitEvent(&event)) {
            FatalSDLError("SDL_WaitEvent");
        }

        switch(event.type) {
        case SDL_QUIT:
            goto done;

        case SDL_WINDOWEVENT:
            HandleEvent(event.window,&Window::HandleWindowEvent);
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
            HandleEvent(event.key,&Window::HandleKeyEvent);
            break;

        case SDL_TEXTINPUT:
            HandleEvent(event.text,&Window::HandleTextInputEvent);
            break;

        case SDL_MOUSEWHEEL:
            HandleEvent(event.wheel,&Window::HandleMouseWheelEvent);
            break;

        default:
            if(event.type==g_update_window_event_type) {
                for(auto &&it:g_all_windows) {
                    Window *window=it.second;

                    window->Update();
                    window->Render();
                }
            }
            break;
        }
    }
done:;

    //ImGui::Shutdown();
    SDL_Quit();

    return 0;
}
