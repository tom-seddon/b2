#ifndef HEADER_A54A7518B6494AC4BF71C370A1BA6040 //-*- mode:c++ -*-
#define HEADER_A54A7518B6494AC4BF71C370A1BA6040

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

#ifdef _MSC_VER

#pragma warning(push)
#pragma warning(disable : 4458) //declaration of 'identifier' hides class member
#pragma warning(disable : 4267) //'var' : conversion from 'size_t' to 'type', possible loss of data
#pragma warning(disable : 4305) //'identifier' : truncation from 'type1' to 'type2'
#pragma warning(disable : 4100) //'identifier' : unreferenced formal parameter
#pragma warning(disable : 4800) //'type' : forcing value to bool 'true' or 'false' (performance warning)

#elif defined __GNUC__

#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"

#if (defined __GNUC__) && !(defined __clang__)
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#endif

#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined __GNUC__
#pragma GCC diagnostic pop
#endif

#include <string>
#include <vector>
#include "keys.h"

struct SDL_Texture;
struct SDL_Renderer;
struct SDL_Cursor;
class Messages;
class SelectorDialog;

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

    ImGuiStuff(const ImGuiStuff &) = delete;
    ImGuiStuff &operator=(const ImGuiStuff &) = delete;

    ImGuiStuff(ImGuiStuff &&) = delete;
    ImGuiStuff &operator=(ImGuiStuff &&) = delete;

    bool Init(ImGuiConfigFlags extra_config_flags);

    void NewFrame();

    // does ImGui::Render.
    void RenderImGui();

    // does the SDL rendering stuff.
    void RenderSDL();

    void AddMouseWheelEvent(float x, float y);
    void AddMouseButtonEvent(uint8_t button, bool state);
    void AddMouseMotionEvent(int x, int y);
    bool AddKeyEvent(uint32_t scancode, bool state);
    void AddInputCharactersUTF8(const char *text);

#if STORE_DRAWLISTS
    void DoStoredDrawListWindow();
#endif
    void DoDebugGui();

    unsigned GetFontSizePixels() const;
    void SetFontSizePixels(unsigned font_size_pixels);

    // The non-modifier key returned will be marked as no longer pressed.
    uint32_t ConsumePressedKeycode();

  protected:
  private:
    enum ConsumePressedKeycodeState {
        ConsumePressedKeycodeState_Off,
        ConsumePressedKeycodeState_Waiting,
        ConsumePressedKeycodeState_Consumed,
    };

    SDL_Renderer *m_renderer = nullptr;
    ImGuiContext *m_context = nullptr;
    SDL_Texture *m_font_texture = nullptr;
    uint64_t m_last_new_frame_ticks = 0;
    ImFontAtlas *m_original_font_atlas = nullptr;
    ImFontAtlas *m_new_font_atlas = nullptr;
    std::string m_imgui_ini_path;
    std::string m_imgui_log_txt_path;
    SDL_Cursor *m_cursors[ImGuiMouseCursor_COUNT] = {};
#if STORE_DRAWLISTS
    struct StoredDrawCmd {
        bool callback = false;
        int texture_width = 0;
        int texture_height = 0;
        unsigned num_indices = 0;
    };

    struct StoredDrawList {
        std::string name;
        std::vector<StoredDrawCmd> cmds;
    };

    std::vector<StoredDrawList> m_draw_lists;
#endif
    ConsumePressedKeycodeState m_consume_pressed_keycode_state = ConsumePressedKeycodeState_Off;
    uint32_t m_consumed_keycode = 0;

    unsigned m_font_size_pixels = 0;
    bool m_font_dirty = true;

    ImGuiKey m_imgui_key_from_sdl_scancode[512] = {}; //512 = SDL_NUM_SCANCODES

    friend class ImGuiContextSetter;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ImGuiContextSetter {
  public:
    explicit ImGuiContextSetter(const ImGuiStuff *stuff);
    ~ImGuiContextSetter();

    ImGuiContextSetter(const ImGuiContextSetter &) = delete;
    ImGuiContextSetter &operator=(const ImGuiContextSetter &) = delete;

    ImGuiContextSetter(ImGuiContextSetter &&) = delete;
    ImGuiContextSetter &operator=(ImGuiContextSetter &&) = delete;

  protected:
  private:
    ImGuiContext *m_old_imgui_context = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ImGuiIDPusher {
  public:
    explicit ImGuiIDPusher(const char *str_id);
    ImGuiIDPusher(const char *str_id_begin, const char *str_id_end);
    explicit ImGuiIDPusher(const void *ptr_id);
    explicit ImGuiIDPusher(int int_id);
    explicit ImGuiIDPusher(uint32_t uint_id);
    ~ImGuiIDPusher();

    ImGuiIDPusher(const ImGuiIDPusher &) = delete;
    ImGuiIDPusher &operator=(const ImGuiIDPusher &) = delete;

    ImGuiIDPusher(ImGuiIDPusher &&);
    ImGuiIDPusher &operator=(ImGuiIDPusher &&) = delete;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ImGuiItemWidthPusher {
  public:
    explicit ImGuiItemWidthPusher(float width);
    ~ImGuiItemWidthPusher();

    ImGuiItemWidthPusher(const ImGuiItemWidthPusher &) = delete;
    ImGuiItemWidthPusher &operator=(const ImGuiItemWidthPusher &) = delete;

    ImGuiItemWidthPusher(ImGuiItemWidthPusher &&);
    ImGuiItemWidthPusher &operator=(ImGuiItemWidthPusher &&) = delete;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ImGuiStyleColourPusher {
  public:
    ImGuiStyleColourPusher();
    ImGuiStyleColourPusher(ImGuiCol idx0, const ImVec4 &col0);
    ImGuiStyleColourPusher(ImGuiCol idx0, const ImVec4 &col0, ImGuiCol idx1, const ImVec4 &col1);
    ImGuiStyleColourPusher(ImGuiCol idx0, const ImVec4 &col0, ImGuiCol idx1, const ImVec4 &col1, ImGuiCol idx2, const ImVec4 &col2);

    ~ImGuiStyleColourPusher();

    // returns *this.
    ImGuiStyleColourPusher &Push(ImGuiCol idx, const ImVec4 &col);

    void Pop(int count = 1);

    // standard things...
    void PushDisabledButtonColours(bool disabled = true);
    void PushDefaultButtonColours();

    void PushDefault(ImGuiCol idx0, ImGuiCol idx1 = ImGuiCol_COUNT, ImGuiCol idx2 = ImGuiCol_COUNT, ImGuiCol idx3 = ImGuiCol_COUNT);

    ImGuiStyleColourPusher(const ImGuiStyleColourPusher &) = delete;
    ImGuiStyleColourPusher &operator=(const ImGuiStyleColourPusher &) = delete;

    ImGuiStyleColourPusher(ImGuiStyleColourPusher &&);
    ImGuiStyleColourPusher &operator=(ImGuiStyleColourPusher &&) = delete;

  protected:
  private:
    int m_num_pushes = 0;

    void PushDefaultInternal(ImGuiCol idx);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ImGuiStyleVarPusher {
  public:
    ImGuiStyleVarPusher();
    ImGuiStyleVarPusher(ImGuiStyleVar idx0, float val0);
    ImGuiStyleVarPusher(ImGuiStyleVar idx0, const ImVec2 &val0);

    ~ImGuiStyleVarPusher();

    // returns *this.
    ImGuiStyleVarPusher &Push(ImGuiStyleVar idx, float val);
    ImGuiStyleVarPusher &Push(ImGuiStyleVar idx, const ImVec2 &val);

    ImGuiStyleVarPusher(const ImGuiStyleVarPusher &) = delete;
    ImGuiStyleVarPusher &operator=(const ImGuiStyleVarPusher &) = delete;

    ImGuiStyleVarPusher(ImGuiStyleVarPusher &&);
    ImGuiStyleVarPusher &operator=(ImGuiStyleVarPusher &&) = delete;

  protected:
  private:
    int m_num_pushes = 0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// (std::vector<std::string> *)idx
//bool ImGuiGetListBoxItemFromStringVector(void *data,int idx,const char **out_text);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// lame name.
ImRect MakeImRectFromPosAndSize(const ImVec2 &pos, const ImVec2 &size);

// changes position, preserving size.
void SetImRectPosX(ImRect *rect, float x);
void SetImRectPosY(ImRect *rect, float y);
void SetImRectPos(ImRect *rect, const ImVec2 &pos);
void TranslateImRect(ImRect *rect, const ImVec2 &delta);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiInputText(std::string *new_str, const char *name, const std::string &old_str);

// A collapsing header that you can't collapse.
void ImGuiHeader(const char *str);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The LED colour is the ImGuiCol_CheckMark style colour.
void ImGuiLED(bool on, const char *str);
void ImGuiLEDv(bool on, const char *fmt, va_list v);
void ImGuiLEDf(bool on, const char *fmt, ...) PRINTF_LIKE(2, 3);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
static bool ImGuiRadioButton(T *value, T button_value, const char *label) {
    int tmp = *value;

    bool result = ImGui::RadioButton(label, &tmp, button_value);

    *value = (T)tmp;

    return result;
}

template <class T>
static bool PRINTF_LIKE(3, 4) ImGuiRadioButtonf(T *value,
                                                T button_value,
                                                const char *fmt, ...) {
    char label[1000];

    va_list v;
    va_start(v, fmt);
    vsnprintf(label, sizeof label, fmt, v);
    va_end(v);

    bool result = ImGuiRadioButton(value, button_value, label);
    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//bool ImGuiBeginFlag(const char *label,uint32_t *open,uint32_t open_mask,ImGuiWindowFlags flags=0);

bool ImGuiMenuItemFlag(const char *label, const char *shortcut, uint32_t *selected, uint32_t selected_mask, bool enabled = true);

template <class T>
bool ImGuiMenuItemEnumValue(const char *label, const char *shortcut, T *value_ptr, T value) {
    bool flag = *value_ptr == value;
    if (ImGui::MenuItem(label, shortcut, &flag)) {
        *value_ptr = value;
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ImGuiButton(const char *label, bool enabled = true);

bool ImGuiConfirmButton(const char *label, bool needs_confirm = true);

bool ImGuiInputUInt(const char *label, unsigned *v, int step = 1, int step_fast = 100, ImGuiInputTextFlags flags = 0);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// like ImGui::CheckboxFlags, but for any type.
template <class FlagsType, class MaskType>
bool ImGuiCheckboxFlags(const char *label, FlagsType *flags, MaskType mask) {
    bool value = !!(*flags & mask);

    if (ImGui::Checkbox(label, &value)) {
        if (value) {
            *flags |= mask;
        } else {
            *flags &= ~mask;
        }

        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//template<class ObjectTypePtr,class ObjectType,class ResultType,class ArgumentType>
//void ImGuiSliderGetSet(const char *label,
//    ObjectTypePtr &&object,
//    ResultType (ObjectType::*get_mfn)() const,
//    void (ObjectType::*set_mfn)(ArgumentType),
//    float mini,
//    float maxi,
//    const char *display_format)
//{
//    auto value=static_cast<float>((object->*get_mfn)());
//
//    if(ImGui::SliderFloat(label,&value,mini,maxi,display_format)) {
//        (object->*set_mfn)(value);
//    }
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr float DEFAULT_PLOT_SCALE_MIN = FLT_MAX;
static constexpr float DEFAULT_PLOT_SCALE_MAX = FLT_MAX;
static const ImVec2 DEFAULT_PLOT_GRAPH_SIZE(0.f, 0.f);
static const ImVec2 DEFAULT_PLOT_MARKERS(0.f, 0.f);

void ImGuiPlotLines(const char *label,
                    const float *values,
                    int values_count,
                    int values_offset = 0,
                    const char *overlay_text = NULL,
                    float scale_min = DEFAULT_PLOT_SCALE_MIN,
                    float scale_max = DEFAULT_PLOT_SCALE_MAX,
                    ImVec2 graph_size = ImVec2(0, 0),
                    ImVec2 markers = DEFAULT_PLOT_MARKERS,
                    int stride = sizeof(float));

void ImGuiPlotLines(const char *label,
                    float (*values_getter)(void *data, int idx),
                    void *data,
                    int values_count,
                    int values_offset = 0,
                    const char *overlay_text = NULL,
                    float scale_min = DEFAULT_PLOT_SCALE_MIN,
                    float scale_max = DEFAULT_PLOT_SCALE_MAX,
                    ImVec2 graph_size = DEFAULT_PLOT_GRAPH_SIZE,
                    ImVec2 markers = DEFAULT_PLOT_MARKERS);

void ImGuiPlotHistogram(const char *label,
                        const float *values,
                        int values_count,
                        int values_offset = 0,
                        const char *overlay_text = NULL,
                        float scale_min = DEFAULT_PLOT_SCALE_MIN,
                        float scale_max = DEFAULT_PLOT_SCALE_MAX,
                        ImVec2 graph_size = ImVec2(0, 0),
                        ImVec2 markers = DEFAULT_PLOT_MARKERS,
                        int stride = sizeof(float));

void ImGuiPlotHistogram(const char *label,
                        float (*values_getter)(void *data, int idx),
                        void *data, int values_count,
                        int values_offset = 0,
                        const char *overlay_text = NULL,
                        float scale_min = DEFAULT_PLOT_SCALE_MIN,
                        float scale_max = DEFAULT_PLOT_SCALE_MAX,
                        ImVec2 graph_size = DEFAULT_PLOT_GRAPH_SIZE,
                        ImVec2 markers = DEFAULT_PLOT_MARKERS);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Shows a recent files menu, using the recent paths list for the given
// file selector dialog.
//
// If one is selected, overwrite *SELECTED_PATH with that path and return true.
//
// If none selected, return false.
bool ImGuiRecentMenu(std::string *selected_path,
                     const char *title,
                     const SelectorDialog &selector);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
