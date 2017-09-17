#ifndef HEADER_A54A7518B6494AC4BF71C370A1BA6040//-*- mode:c++ -*-
#define HEADER_A54A7518B6494AC4BF71C370A1BA6040

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <string>
#include <vector>
#include "keys.h"

struct SDL_Texture;
struct SDL_Renderer;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern const ImVec4 &DISABLED_BUTTON_COLOUR;
extern const ImVec4 &DISABLED_BUTTON_HOVERED_COLOUR;
extern const ImVec4 &DISABLED_BUTTON_ACTIVE_COLOUR;

extern const ImGuiStyle IMGUI_DEFAULT_STYLE;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// "stuff"? Unfortunately all the good names were already taken.

class ImGuiStuff {
public:
    explicit ImGuiStuff(SDL_Renderer *renderer);
    ~ImGuiStuff();

    ImGuiStuff(const ImGuiStuff &)=delete;
    ImGuiStuff &operator=(const ImGuiStuff &)=delete;

    ImGuiStuff(ImGuiStuff &&)=delete;
    ImGuiStuff &operator=(ImGuiStuff &&)=delete;

    // bool parameter, yum.
    bool Init();

    void NewFrame(bool got_mouse_focus);
    void Render();

    // Temporary (?) fix for disappearing mousewheel messages.
    void SetMouseWheel(int delta);

    void SetKeyDown(uint32_t scancode,bool state);
    void AddInputCharactersUTF8(const char *text);

    bool WantCaptureMouse() const;
    bool WantCaptureKeyboard() const;
    bool WantTextInput() const;
protected:
private:
    SDL_Renderer *m_renderer=nullptr;
    ImGuiContext *m_context=nullptr;
    struct SDL_Texture *m_font_texture=nullptr;
    uint64_t m_last_new_frame_ticks=0;
    int m_next_wheel=0;
    ImFontAtlas *m_original_font_atlas=nullptr;
    bool m_want_capture_mouse=false;
    bool m_want_capture_keyboard=false;
    bool m_want_text_input=false;
    std::string m_imgui_ini_path;
    std::string m_imgui_log_txt_path;

    friend class ImGuiContextSetter;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ImGuiContextSetter {
public:
    explicit ImGuiContextSetter(ImGuiStuff *stuff);
    ~ImGuiContextSetter();

    ImGuiContextSetter(const ImGuiContextSetter &)=delete;
    ImGuiContextSetter &operator=(const ImGuiContextSetter &)=delete;

    ImGuiContextSetter(ImGuiContextSetter &&)=delete;
    ImGuiContextSetter &operator=(ImGuiContextSetter &&)=delete;
protected:
private:
    ImGuiContext *m_old_imgui_context;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ImGuiIDPusher {
public:
    explicit ImGuiIDPusher(const char* str_id);
    ImGuiIDPusher(const char* str_id_begin,const char* str_id_end);
    explicit ImGuiIDPusher(const void* ptr_id);
    explicit ImGuiIDPusher(int int_id);
    ~ImGuiIDPusher();
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ImGuiStyleColourPusher {
public:
    ImGuiStyleColourPusher();
    ImGuiStyleColourPusher(ImGuiCol idx0,const ImVec4& col0);
    ImGuiStyleColourPusher(ImGuiCol idx0,const ImVec4& col0,ImGuiCol idx1,const ImVec4& col1);
    ImGuiStyleColourPusher(ImGuiCol idx0,const ImVec4& col0,ImGuiCol idx1,const ImVec4& col1,ImGuiCol idx2,const ImVec4& col2);

    ~ImGuiStyleColourPusher();

    // returns *this.
    ImGuiStyleColourPusher &Push(ImGuiCol idx,const ImVec4& col);

    // standard things...
    void PushDisabledButtonColours();
    void PushDefaultButtonColours();

    void PushDefault(ImGuiCol idx0,ImGuiCol idx1=ImGuiCol_COUNT,ImGuiCol idx2=ImGuiCol_COUNT,ImGuiCol idx3=ImGuiCol_COUNT);

    ImGuiStyleColourPusher(const ImGuiStyleColourPusher &)=delete;
    ImGuiStyleColourPusher &operator=(const ImGuiStyleColourPusher &)=delete;

    ImGuiStyleColourPusher(ImGuiStyleColourPusher &&);
    ImGuiStyleColourPusher &operator=(ImGuiStyleColourPusher &&)=delete;
protected:
private:
    int m_num_pushes=0;

    void PushDefaultInternal(ImGuiCol idx);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// (std::vector<std::string> *)idx
//bool ImGuiGetListBoxItemFromStringVector(void *data,int idx,const char **out_text);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// lame name.
ImRect MakeImRectFromPosAndSize(const ImVec2 &pos,const ImVec2 &size);

// changes position, preserving size.
void SetImRectPosX(ImRect *rect,float x);
void SetImRectPosY(ImRect *rect,float y);
void SetImRectPos(ImRect *rect,const ImVec2 &pos);
void TranslateImRect(ImRect *rect,const ImVec2 &delta);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiInputText(std::string *new_str,const char *name,const std::string &old_str);

// A collapsing header that you can't collapse.
void ImGuiHeader(const char *str);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The LED colour is the ImGuiCol_CheckMark style colour.
void ImGuiLED(bool on,const char *str);
void ImGuiLEDv(bool on,const char *fmt,va_list v);
void ImGuiLEDf(bool on,const char *fmt,...) PRINTF_LIKE(2,3);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class T>
static bool ImGuiRadioButton(const char *label,T *value,T button_value) {
    int tmp=*value;

    bool result=ImGui::RadioButton(label,&tmp,button_value);

    *value=(T)tmp;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiBeginFlag(const char *label,uint32_t *open,uint32_t open_mask,ImGuiWindowFlags flags=0);

bool ImGuiMenuItemFlag(const char* label,const char* shortcut,uint32_t *selected,uint32_t selected_mask,bool enabled=true);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiButton(const char *label,bool enabled=true);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class ObjectTypePtr,class ObjectType,class ResultType,class ArgumentType>
void ImGuiSliderGetSet(const char *label,
    ObjectTypePtr &&object,
    ResultType (ObjectType::*get_mfn)() const,
    void (ObjectType::*set_mfn)(ArgumentType),
    float mini,
    float maxi,
    const char *display_format)
{
    auto value=static_cast<float>((object->*get_mfn)());

    if(ImGui::SliderFloat(label,&value,mini,maxi,display_format)) {
        (object->*set_mfn)(value);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr float DEFAULT_PLOT_SCALE_MIN=FLT_MAX;
static constexpr float DEFAULT_PLOT_SCALE_MAX=FLT_MAX;
static const ImVec2 DEFAULT_PLOT_GRAPH_SIZE(0.f,0.f);
static const ImVec2 DEFAULT_PLOT_MARKERS(0.f,0.f);

void ImGuiPlotLines(const char* label,const float* values,int values_count,int values_offset=0,const char* overlay_text=NULL,float scale_min=DEFAULT_PLOT_SCALE_MIN,float scale_max=DEFAULT_PLOT_SCALE_MAX,ImVec2 graph_size=ImVec2(0,0),ImVec2 markers=DEFAULT_PLOT_MARKERS,int stride=sizeof(float));
void ImGuiPlotLines(const char* label,float (*values_getter)(void* data,int idx),void* data,int values_count,int values_offset=0,const char* overlay_text=NULL,float scale_min=DEFAULT_PLOT_SCALE_MIN,float scale_max=DEFAULT_PLOT_SCALE_MAX,ImVec2 graph_size=DEFAULT_PLOT_GRAPH_SIZE,ImVec2 markers=DEFAULT_PLOT_MARKERS);
void ImGuiPlotHistogram(const char* label,const float* values,int values_count,int values_offset=0,const char* overlay_text=NULL,float scale_min=DEFAULT_PLOT_SCALE_MIN,float scale_max=DEFAULT_PLOT_SCALE_MAX,ImVec2 graph_size=ImVec2(0,0),ImVec2 markers=DEFAULT_PLOT_MARKERS,int stride=sizeof(float));
void ImGuiPlotHistogram(const char* label,float (*values_getter)(void* data,int idx),void* data,int values_count,int values_offset=0,const char* overlay_text=NULL,float scale_min=DEFAULT_PLOT_SCALE_MIN,float scale_max=DEFAULT_PLOT_SCALE_MAX,ImVec2 graph_size=DEFAULT_PLOT_GRAPH_SIZE,ImVec2 markers=DEFAULT_PLOT_MARKERS);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t ImGuiGetPressedKeycode();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
