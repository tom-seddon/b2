#include <shared/system.h>
#include "KeymapsUI.h"
#include "BeebWindows.h"
#include "dear_imgui.h"
#include "keys.h"
#include <SDL.h>
#include <shared/debug.h>
#include "keymap.h"
#include "BeebWindow.h"
#include <IconsFontAwesome5.h>
#include "BeebKeymap.h"
#include "SettingsUI.h"

#include <shared/enum_decl.h>
#include "KeymapsUI_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "KeymapsUI_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//const char KEYMAP_NOT_EDITABLE_ICON[]=ICON_FA_LOCK;
static const std::string KEYMAP_SCANCODES_SUFFIX = " " ICON_FA_KEYBOARD;
static const std::string KEYMAP_KEYSYMS_SUFFIX = " " ICON_FA_FONT;

static const char PC_SCANCODES_POPUP[] = "pc_scancodes";
static const char PC_KEYCODES_POPUP[] = "pc_keycodes";
static const char NEW_KEYMAP_POPUP[] = "new_keymap_popup";
static const char COPY_KEYMAP_POPUP[] = "copy_keymap_popup";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class KeymapType>
static void ImGuiKeySymsList(bool *edited, ImGuiStuff *imgui_stuff, KeymapType *keymap, typename KeymapType::ValueType value, bool editable) {
    if (editable) {
        ImGui::Text("Edit PC Keys");
    } else {
        ImGui::Text("PC Keys");
    }
    ImGui::Separator();
    if (const uint32_t *keycodes = keymap->GetPCKeysForValue(value)) {
        for (const uint32_t *keycode = keycodes; *keycode != 0; ++keycode) {
            ImGuiIDPusher id_pusher((int)*keycode);

            if (editable) {
                if (ImGui::Button("x")) {
                    keymap->SetMapping(*keycode, value, false);
                    *edited = true;
                }
                ImGui::SameLine();
            }

            std::string name = GetKeycodeName(*keycode);

            ImGui::TextUnformatted(name.c_str());
        }
    }

    if (editable) {
        ImGui::TextUnformatted("(press key to add)");

        uint32_t keycode = imgui_stuff->ConsumePressedKeycode();
        if (keycode != 0) {
            keymap->SetMapping(keycode, value, true);
            *edited = true;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetKeymapUIName(const BeebKeymap &keymap) {
    if (keymap.IsKeySymMap()) {
        return keymap.GetName() + KEYMAP_KEYSYMS_SUFFIX;
    } else {
        return keymap.GetName() + KEYMAP_SCANCODES_SUFFIX;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Keycap {
    int width_in_halves = -1;
    BeebKey key = BeebKey_None;
    BeebKeySym unshifted_sym = BeebKeySym_None;
    BeebKeySym shifted_sym = BeebKeySym_None;
    BeebKeySym ctrled_sym = BeebKeySym_None; //only applicable to Electron
    BeebKeySym funced_sym = BeebKeySym_None; //only applicable to Electron
    KeyColour colour = KeyColour_Default;

    Keycap() = default;
    Keycap(int width_in_halves, BeebKey key, BeebKeySym unshifted_sym, BeebKeySym shifted_sym = BeebKeySym_None, BeebKeySym ctrled_sym = BeebKeySym_None, BeebKeySym funced_sym = BeebKeySym_None);
    Keycap WithColour(KeyColour colour) const;
};

Keycap::Keycap(int width_in_halves_, BeebKey key_, BeebKeySym unshifted_sym_, BeebKeySym shifted_sym_, BeebKeySym ctrled_sym_, BeebKeySym funced_sym_)
    : width_in_halves(width_in_halves_)
    , key(key_)
    , unshifted_sym(unshifted_sym_)
    , shifted_sym(shifted_sym_)
    , ctrled_sym(ctrled_sym_)
    , funced_sym(funced_sym_) {
}

Keycap Keycap::WithColour(KeyColour colour_) const {
    Keycap keycap = *this;

    keycap.colour = colour_;

    return keycap;
}

// When drawing the keysym map, keys with shifted and unshifted parts
// want to be drawn as two half-height buttons. The ideal thing is to
// let ImGui do the layout as much as possible, by letting it position
// the full-size buttons, letting it position one of the half-size
// buttons in the pair, then calculating the position of the other
// half-size one from that. Doing that, I couldn't get anything other
// than a bit of a mess out of it; the fix/hacky solution seems to be
// to draw things one row at a time, without adjusting the cursor Y
// pos as it goes, creating a list of bottom halves to create as it
// goes. Then create the bottom half buttons on a second pass.
//
// This is kind of ugly and ideally needs revisting.
struct BottomHalfKeycap {
    float x;
    ImVec2 size;
    const Keycap *keycap;
};

// Defaults assume 13px Proggy Clean font.
struct Metrics {
    float key_height = 36.f;
    float key_width = 36.f;
    float keypad_x = 750.f;
};

struct KeymapsUIPersistentData {
    float divider_position = FLT_MAX;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(KeymapsUIPersistentData, divider_position);

class KeymapsUI : public SettingsUI {
  public:
    KeymapsUI(BeebWindow *beeb_window, ImGuiStuff *imgui_stuff);

    void DoImGui() override;
    bool OnClose() override;

  protected:
  private:
    BeebWindow *m_beeb_window = nullptr;
    bool m_wants_keyboard_focus = false;
    bool m_edited = false;
    int m_keymap_index = -1;
    Metrics m_metrics;
    ImGuiStuff *m_imgui_stuff = nullptr;
    KeymapsUIPersistentData m_persistent;
    bool m_first_update = true;

    void DoScancodesList(BeebKeymap *keymap, BeebKey beeb_key, bool editable);
    ImGuiStyleColourPusher GetColourPusherForKeycap(const BeebKeymap *keymap, int8_t keymap_key, const Keycap *keycap);
    ImVec2 GetKeySize(const Keycap *key) const;
    void DoScancodeKeyboardLinePart(BeebKeymap *keymap, const Keycap *line);
    void DoScancodeKeyboardLineParts(BeebKeymap *keymap, const Keycap *line, const Keycap *m128_line);
    void DoKeySymButton(BeebKeymap *keymap, const char *label, const ImVec2 &size, BeebKeySym key_sym, const Keycap *keycap);
    void DoKeySymKeyboardLineTopHalves(BeebKeymap *keymap, const Keycap *line, BottomHalfKeycap *bottom_halves, size_t *num_bottom_halves);
    void DoKeySymKeyboardLineBottomHalves(BeebKeymap *keymap, float y, const BottomHalfKeycap *bottom_halves, size_t num_bottom_halves);
    void DoKeySymKeyboardLineParts(BeebKeymap *keymap, const Keycap *line, const Keycap *m128_line);
    void DoKeyboardLine(BeebKeymap *keymap, const Keycap *line, const Keycap *m128_line);

    // Returns false if keymap was deleted.
    void DoEditKeymapGui();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateKeymapsUI(BeebWindow *beeb_window, ImGuiStuff *imgui_stuff) {
    return std::make_unique<KeymapsUI>(beeb_window, imgui_stuff);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

KeymapsUI::KeymapsUI(BeebWindow *beeb_window, ImGuiStuff *imgui_stuff)
    : m_beeb_window(beeb_window)
    , m_imgui_stuff(imgui_stuff) {
    this->SetDefaultSize(ImVec2(650, 450));
    this->SetPersistentData(&m_persistent);

    const BeebKeymap *keymap = m_beeb_window->GetCurrentKeymap();

    for (size_t i = 0; i < BeebWindows::GetNumBeebKeymaps(); ++i) {
        if (BeebWindows::GetBeebKeymapByIndex(i) == keymap) {
            m_keymap_index = (int)i;
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool KeymapsUI::OnClose() {
    return m_edited;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class BeebKeymapPointerType>
const BeebKeymap *ImGuiPickKeymapPopup(const char *popup_name,
                                       size_t (*get_num_keymaps_fn)(),
                                       BeebKeymapPointerType (*get_keymap_by_index_fn)(size_t)) {
    const BeebKeymap *result = nullptr;

    if (ImGui::BeginPopup(popup_name)) {
        for (size_t i = 0; i < (*get_num_keymaps_fn)(); ++i) {
            const BeebKeymap *keymap = (*get_keymap_by_index_fn)(i);
            std::string ui_name = GetKeymapUIName(*keymap);
            if (ImGui::MenuItem(ui_name.c_str())) {
                result = keymap;
            }
        }

        ImGui::EndPopup();
    }

    return result;
}

static BeebKeymap *GetBeebKeymapByIndex(int index) {
    if (index < 0) {
        return nullptr;
    } else if ((size_t)index >= BeebWindows::GetNumBeebKeymaps()) {
        return nullptr;
    } else {
        return BeebWindows::GetBeebKeymapByIndex((size_t)index);
    }
}

// Requires annoying std::string context, so that there's somewhere for the
// temporary string to live.
static const char *GetBeebKeymapNameCallback(void *context, int index) {
    auto tmp_str = (std::string *)context;

    const BeebKeymap *keymap = GetBeebKeymapByIndex(index);
    ASSERT(keymap);

    tmp_str->assign(GetKeymapUIName(*keymap));
    return tmp_str->c_str();
}

void KeymapsUI::DoImGui() {
    m_wants_keyboard_focus = false;

    ImGui::Columns(2, "keymaps");

    // Ugh. But I couldn't get the layout to work properly with a table.
    if (m_first_update) {
        ImGui::SetColumnWidth(0, m_persistent.divider_position);
    } else {
        float divider_position = ImGui::GetColumnWidth(0);
        if (divider_position != m_persistent.divider_position) {
            m_persistent.divider_position = divider_position;
            m_edited = true;
        }
    }

    if (ImGui::Button("New...")) {
        ImGui::OpenPopup(NEW_KEYMAP_POPUP);
    }

    ImGui::SameLine();

    if (ImGui::Button("Copy...")) {
        ImGui::OpenPopup(COPY_KEYMAP_POPUP);
    }

    ImGui::SameLine();

    if (ImGuiConfirmButton("Delete")) {
        BeebWindows::RemoveBeebKeymapByIndex((size_t)m_keymap_index);
        m_keymap_index = std::min(m_keymap_index, (int)BeebWindows::GetNumBeebKeymaps() - 1);
    }

    {
        ImGuiItemWidthPusher width_pusher(-1);

        float y = ImGui::GetCursorPosY();
        float h = ImGui::GetWindowHeight();
        float line_height = ImGui::GetTextLineHeightWithSpacing();

        // -1 for the height in items, as there's some kind of border that isn't
        // getting accommodated otherwise. A bit ugly.
        std::string tmp_str;
        ImGui::ListBox("##empty",
                       &m_keymap_index,
                       &GetBeebKeymapNameCallback,
                       &tmp_str,
                       (int)BeebWindows::GetNumBeebKeymaps(),
                       (int)((h - y) / line_height - 1));

        if (m_keymap_index >= 0 &&
            (size_t)m_keymap_index < BeebWindows::GetNumBeebKeymaps()) {
            const BeebKeymap *keymap = BeebWindows::GetBeebKeymapByIndex((size_t)m_keymap_index);
            m_beeb_window->SetCurrentKeymap(keymap);
        }
    }

    ImGui::NextColumn();

    ImGui::BeginChild("hello", ImVec2(0, 0), 0, ImGuiWindowFlags_HorizontalScrollbar);

    this->DoEditKeymapGui();

    ImGui::EndChild();

    ImGui::Columns(1);

    if (const BeebKeymap *keymap = ImGuiPickKeymapPopup(NEW_KEYMAP_POPUP,
                                                        &GetNumDefaultBeebKeymaps,
                                                        &GetDefaultBeebKeymapByIndex)) {
        BeebWindows::AddBeebKeymap(*keymap);
    }

    if (const BeebKeymap *keymap = ImGuiPickKeymapPopup(COPY_KEYMAP_POPUP,
                                                        &BeebWindows::GetNumBeebKeymaps,
                                                        &BeebWindows::GetBeebKeymapByIndex)) {
        BeebWindows::AddBeebKeymap(*keymap);
    }

    m_first_update = false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// BBC keyboard caps
static const Keycap g_keyboard_line1[] = {
    Keycap(5, BeebKey_None, BeebKeySym_None),
    Keycap(2, BeebKey_f0, BeebKeySym_f0).WithColour(KeyColour_Red),
    Keycap(2, BeebKey_f1, BeebKeySym_f1).WithColour(KeyColour_Red),
    Keycap(2, BeebKey_f2, BeebKeySym_f2).WithColour(KeyColour_Red),
    Keycap(2, BeebKey_f3, BeebKeySym_f3).WithColour(KeyColour_Red),
    Keycap(2, BeebKey_f4, BeebKeySym_f4).WithColour(KeyColour_Red),
    Keycap(2, BeebKey_f5, BeebKeySym_f5).WithColour(KeyColour_Red),
    Keycap(2, BeebKey_f6, BeebKeySym_f6).WithColour(KeyColour_Red),
    Keycap(2, BeebKey_f7, BeebKeySym_f7).WithColour(KeyColour_Red),
    Keycap(2, BeebKey_f8, BeebKeySym_f8).WithColour(KeyColour_Red),
    Keycap(2, BeebKey_f9, BeebKeySym_f9).WithColour(KeyColour_Red),
    Keycap(2, BeebKey_Break, BeebKeySym_Break, BeebKeySym_None),
    {},
};

static const Keycap g_keyboard_line2[] = {
    Keycap(2, BeebKey_Escape, BeebKeySym_Escape, BeebKeySym_None),
    Keycap(2, BeebKey_1, BeebKeySym_1, BeebKeySym_ExclamationMark),
    Keycap(2, BeebKey_2, BeebKeySym_2, BeebKeySym_Quotes),
    Keycap(2, BeebKey_3, BeebKeySym_3, BeebKeySym_Hash),
    Keycap(2, BeebKey_4, BeebKeySym_4, BeebKeySym_Dollar),
    Keycap(2, BeebKey_5, BeebKeySym_5, BeebKeySym_Percent),
    Keycap(2, BeebKey_6, BeebKeySym_6, BeebKeySym_Ampersand),
    Keycap(2, BeebKey_7, BeebKeySym_7, BeebKeySym_Apostrophe),
    Keycap(2, BeebKey_8, BeebKeySym_8, BeebKeySym_LeftBracket),
    Keycap(2, BeebKey_9, BeebKeySym_9, BeebKeySym_RightBracket),
    Keycap(2, BeebKey_0, BeebKeySym_0, BeebKeySym_None),
    Keycap(2, BeebKey_Minus, BeebKeySym_Minus, BeebKeySym_Equals),
    Keycap(2, BeebKey_Caret, BeebKeySym_Caret, BeebKeySym_Tilde),
    Keycap(2, BeebKey_Backslash, BeebKeySym_Backslash, BeebKeySym_Pipe),
    Keycap(2, BeebKey_Left, BeebKeySym_Left, BeebKeySym_None).WithColour(KeyColour_Khaki),
    Keycap(2, BeebKey_Right, BeebKeySym_Right, BeebKeySym_None).WithColour(KeyColour_Khaki),
    {},
};

static const Keycap g_compact_line2[] = {
    Keycap(2, BeebKey_Escape, BeebKeySym_Escape, BeebKeySym_None),
    Keycap(2, BeebKey_1, BeebKeySym_1, BeebKeySym_ExclamationMark),
    Keycap(2, BeebKey_2, BeebKeySym_2, BeebKeySym_Quotes),
    Keycap(2, BeebKey_3, BeebKeySym_3, BeebKeySym_Hash),
    Keycap(2, BeebKey_4, BeebKeySym_4, BeebKeySym_Dollar),
    Keycap(2, BeebKey_5, BeebKeySym_5, BeebKeySym_Percent),
    Keycap(2, BeebKey_6, BeebKeySym_6, BeebKeySym_Ampersand),
    Keycap(2, BeebKey_7, BeebKeySym_7, BeebKeySym_Apostrophe),
    Keycap(2, BeebKey_8, BeebKeySym_8, BeebKeySym_LeftBracket),
    Keycap(2, BeebKey_9, BeebKeySym_9, BeebKeySym_RightBracket),
    Keycap(2, BeebKey_0, BeebKeySym_0, BeebKeySym_At),
    Keycap(2, BeebKey_Minus, BeebKeySym_Minus, BeebKeySym_Equals),
    Keycap(2, BeebKey_Caret, BeebKeySym_Caret, BeebKeySym_Tilde),
    Keycap(2, BeebKey_Backslash, BeebKeySym_Backslash, BeebKeySym_Pipe),
    Keycap(2, BeebKey_Left, BeebKeySym_Left, BeebKeySym_None).WithColour(KeyColour_Khaki),
    Keycap(2, BeebKey_Right, BeebKeySym_Right, BeebKeySym_None).WithColour(KeyColour_Khaki),
    {},
};

static const Keycap g_keyboard_line3[] = {
    Keycap(3, BeebKey_Tab, BeebKeySym_Tab, BeebKeySym_None),
    Keycap(2, BeebKey_Q, BeebKeySym_Q),
    Keycap(2, BeebKey_W, BeebKeySym_W),
    Keycap(2, BeebKey_E, BeebKeySym_E),
    Keycap(2, BeebKey_R, BeebKeySym_R),
    Keycap(2, BeebKey_T, BeebKeySym_T),
    Keycap(2, BeebKey_Y, BeebKeySym_Y),
    Keycap(2, BeebKey_U, BeebKeySym_U),
    Keycap(2, BeebKey_I, BeebKeySym_I),
    Keycap(2, BeebKey_O, BeebKeySym_O),
    Keycap(2, BeebKey_P, BeebKeySym_P),
    Keycap(2, BeebKey_At, BeebKeySym_At),
    Keycap(2, BeebKey_LeftSquareBracket, BeebKeySym_LeftSquareBracket, BeebKeySym_LeftCurlyBracket),
    Keycap(2, BeebKey_Underline, BeebKeySym_Underline, BeebKeySym_Pound),
    Keycap(2, BeebKey_Up, BeebKeySym_Up, BeebKeySym_None).WithColour(KeyColour_Khaki),
    Keycap(2, BeebKey_Down, BeebKeySym_Down, BeebKeySym_None).WithColour(KeyColour_Khaki),
    {},
};

static const Keycap g_compact_line3[] = {
    Keycap(3, BeebKey_Tab, BeebKeySym_Tab, BeebKeySym_None),
    Keycap(2, BeebKey_Q, BeebKeySym_Q),
    Keycap(2, BeebKey_W, BeebKeySym_W),
    Keycap(2, BeebKey_E, BeebKeySym_E),
    Keycap(2, BeebKey_R, BeebKeySym_R),
    Keycap(2, BeebKey_T, BeebKeySym_T),
    Keycap(2, BeebKey_Y, BeebKeySym_Y),
    Keycap(2, BeebKey_U, BeebKeySym_U),
    Keycap(2, BeebKey_I, BeebKeySym_I),
    Keycap(2, BeebKey_O, BeebKeySym_O),
    Keycap(2, BeebKey_P, BeebKeySym_P),
    Keycap(2, BeebKey_At, BeebKeySym_CompactSpecialKey),
    Keycap(2, BeebKey_LeftSquareBracket, BeebKeySym_LeftSquareBracket, BeebKeySym_LeftCurlyBracket),
    Keycap(2, BeebKey_Underline, BeebKeySym_Underline, BeebKeySym_Pound),
    Keycap(2, BeebKey_Up, BeebKeySym_Up, BeebKeySym_None).WithColour(KeyColour_Khaki),
    Keycap(2, BeebKey_Down, BeebKeySym_Down, BeebKeySym_None).WithColour(KeyColour_Khaki),
    {},
};

static const Keycap g_keyboard_line4[] = {
    Keycap(2, BeebKey_CapsLock, BeebKeySym_CapsLock, BeebKeySym_None),
    Keycap(2, BeebKey_Ctrl, BeebKeySym_Ctrl, BeebKeySym_None),
    Keycap(2, BeebKey_A, BeebKeySym_A),
    Keycap(2, BeebKey_S, BeebKeySym_S),
    Keycap(2, BeebKey_D, BeebKeySym_D),
    Keycap(2, BeebKey_F, BeebKeySym_F),
    Keycap(2, BeebKey_G, BeebKeySym_G),
    Keycap(2, BeebKey_H, BeebKeySym_H),
    Keycap(2, BeebKey_J, BeebKeySym_J),
    Keycap(2, BeebKey_K, BeebKeySym_K),
    Keycap(2, BeebKey_L, BeebKeySym_L),
    Keycap(2, BeebKey_Semicolon, BeebKeySym_Semicolon, BeebKeySym_Plus),
    Keycap(2, BeebKey_Colon, BeebKeySym_Colon, BeebKeySym_Star),
    Keycap(2, BeebKey_RightSquareBracket, BeebKeySym_RightSquareBracket, BeebKeySym_RightCurlyBracket),
    Keycap(4, BeebKey_Return, BeebKeySym_Return, BeebKeySym_None),
    {},
};

static const Keycap g_keyboard_line5[] = {
    Keycap(2, BeebKey_ShiftLock, BeebKeySym_ShiftLock, BeebKeySym_None),
    Keycap(3, BeebKey_Shift, BeebKeySym_Shift),
    Keycap(2, BeebKey_Z, BeebKeySym_Z),
    Keycap(2, BeebKey_X, BeebKeySym_X),
    Keycap(2, BeebKey_C, BeebKeySym_C),
    Keycap(2, BeebKey_V, BeebKeySym_V),
    Keycap(2, BeebKey_B, BeebKeySym_B),
    Keycap(2, BeebKey_N, BeebKeySym_N),
    Keycap(2, BeebKey_M, BeebKeySym_M),
    Keycap(2, BeebKey_Comma, BeebKeySym_Comma, BeebKeySym_LessThan),
    Keycap(2, BeebKey_Stop, BeebKeySym_Stop, BeebKeySym_GreaterThan),
    Keycap(2, BeebKey_Slash, BeebKeySym_Slash, BeebKeySym_QuestionMarke),
    Keycap(3, BeebKey_Shift, BeebKeySym_Shift),
    Keycap(2, BeebKey_Delete, BeebKeySym_Delete),
    Keycap(2, BeebKey_Copy, BeebKeySym_Copy, BeebKeySym_None).WithColour(KeyColour_Khaki),
    {},
};

static const Keycap g_keyboard_line6[] = {
    Keycap(8, BeebKey_None, BeebKeySym_None, BeebKeySym_None),
    Keycap(20, BeebKey_Space, BeebKeySym_Space, BeebKeySym_None),
    {},
};

static const Keycap g_m128_line1[] = {
    Keycap(2, BeebKey_KeypadPlus, BeebKeySym_KeypadPlus, BeebKeySym_None),
    Keycap(2, BeebKey_KeypadMinus, BeebKeySym_KeypadMinus, BeebKeySym_None),
    Keycap(2, BeebKey_KeypadSlash, BeebKeySym_KeypadSlash, BeebKeySym_None),
    Keycap(2, BeebKey_KeypadStar, BeebKeySym_KeypadStar, BeebKeySym_None),
    {},
};

static const Keycap g_m128_line2[] = {
    Keycap(2, BeebKey_Keypad7, BeebKeySym_Keypad7, BeebKeySym_None),
    Keycap(2, BeebKey_Keypad8, BeebKeySym_Keypad8, BeebKeySym_None),
    Keycap(2, BeebKey_Keypad9, BeebKeySym_Keypad9, BeebKeySym_None),
    Keycap(2, BeebKey_KeypadHash, BeebKeySym_KeypadHash, BeebKeySym_None),
    {},
};

static const Keycap g_m128_line3[] = {
    Keycap(2, BeebKey_Keypad4, BeebKeySym_Keypad4, BeebKeySym_None),
    Keycap(2, BeebKey_Keypad5, BeebKeySym_Keypad5, BeebKeySym_None),
    Keycap(2, BeebKey_Keypad6, BeebKeySym_Keypad6, BeebKeySym_None),
    Keycap(2, BeebKey_KeypadDelete, BeebKeySym_KeypadDelete, BeebKeySym_None),
    {},
};

static const Keycap g_m128_line4[] = {
    Keycap(2, BeebKey_Keypad1, BeebKeySym_Keypad1, BeebKeySym_None),
    Keycap(2, BeebKey_Keypad2, BeebKeySym_Keypad2, BeebKeySym_None),
    Keycap(2, BeebKey_Keypad3, BeebKeySym_Keypad3, BeebKeySym_None),
    Keycap(2, BeebKey_KeypadStop, BeebKeySym_KeypadStop, BeebKeySym_None),
    {},
};

static const Keycap g_m128_line5[] = {
    Keycap(2, BeebKey_Keypad0, BeebKeySym_Keypad0, BeebKeySym_None),
    Keycap(2, BeebKey_KeypadComma, BeebKeySym_KeypadComma, BeebKeySym_None),
    Keycap(4, BeebKey_KeypadReturn, BeebKeySym_KeypadReturn, BeebKeySym_None),
    {},
};

#undef END

static const Keycap g_electron_line1[] = {
    Keycap(2, BeebKey_Escape, BeebKeySym_Escape),
    Keycap(2, BeebKey_1, BeebKeySym_1, BeebKeySym_ExclamationMark, BeebKeySym_None, BeebKeySym_f1),
    Keycap(2, BeebKey_2, BeebKeySym_2, BeebKeySym_Quotes, BeebKeySym_None, BeebKeySym_f2),
    Keycap(2, BeebKey_3, BeebKeySym_3, BeebKeySym_Hash, BeebKeySym_None, BeebKeySym_f3),
    Keycap(2, BeebKey_4, BeebKeySym_4, BeebKeySym_Dollar, BeebKeySym_None, BeebKeySym_f4),
    Keycap(2, BeebKey_5, BeebKeySym_5, BeebKeySym_Percent, BeebKeySym_None, BeebKeySym_f5),
    Keycap(2, BeebKey_6, BeebKeySym_6, BeebKeySym_Ampersand, BeebKeySym_None, BeebKeySym_f6),
    Keycap(2, BeebKey_7, BeebKeySym_7, BeebKeySym_Apostrophe, BeebKeySym_None, BeebKeySym_f7),
    Keycap(2, BeebKey_8, BeebKeySym_8, BeebKeySym_LeftBracket, BeebKeySym_None, BeebKeySym_f8),
    Keycap(2, BeebKey_9, BeebKeySym_9, BeebKeySym_RightBracket, BeebKeySym_None, BeebKeySym_f9),
    Keycap(2, BeebKey_0, BeebKeySym_0, BeebKeySym_At, BeebKeySym_None, BeebKeySym_f0),
    Keycap(2, BeebKey_Minus, BeebKeySym_Minus, BeebKeySym_Equals),
    Keycap(2, BeebKey_Left, BeebKeySym_Left, BeebKeySym_Caret, BeebKeySym_Tilde),
    Keycap(2, BeebKey_Right, BeebKeySym_Right, BeebKeySym_Pipe, BeebKeySym_Backslash),
    Keycap(2, BeebKey_Break, BeebKeySym_Break),
    {},
};

static const Keycap g_electron_line2[] = {
    Keycap(1, BeebKey_None, BeebKeySym_None),
    Keycap(2, BeebKey_CapsLock, BeebKeySym_CapsLock),
    Keycap(2, BeebKey_Q, BeebKeySym_Q),
    Keycap(2, BeebKey_W, BeebKeySym_W),
    Keycap(2, BeebKey_E, BeebKeySym_E),
    Keycap(2, BeebKey_R, BeebKeySym_R),
    Keycap(2, BeebKey_T, BeebKeySym_T),
    Keycap(2, BeebKey_Y, BeebKeySym_Y),
    Keycap(2, BeebKey_U, BeebKeySym_U),
    Keycap(2, BeebKey_I, BeebKeySym_I),
    Keycap(2, BeebKey_O, BeebKeySym_O),
    Keycap(2, BeebKey_P, BeebKeySym_P),
    Keycap(2, BeebKey_Up, BeebKeySym_Up, BeebKeySym_Pound, BeebKeySym_LeftCurlyBracket),
    Keycap(2, BeebKey_Down, BeebKeySym_Down, BeebKeySym_Underline, BeebKeySym_RightCurlyBracket),
    Keycap(2, BeebKey_Copy, BeebKeySym_Copy, BeebKeySym_LeftSquareBracket, BeebKeySym_RightSquareBracket),
    {},
};

static const Keycap g_electron_line3[] = {
    Keycap(2, BeebKey_None, BeebKeySym_None),
    Keycap(2, BeebKey_Ctrl, BeebKeySym_Ctrl),
    Keycap(2, BeebKey_A, BeebKeySym_A),
    Keycap(2, BeebKey_S, BeebKeySym_S),
    Keycap(2, BeebKey_D, BeebKeySym_D),
    Keycap(2, BeebKey_F, BeebKeySym_F),
    Keycap(2, BeebKey_G, BeebKeySym_G),
    Keycap(2, BeebKey_H, BeebKeySym_H),
    Keycap(2, BeebKey_J, BeebKeySym_J),
    Keycap(2, BeebKey_K, BeebKeySym_K),
    Keycap(2, BeebKey_L, BeebKeySym_L),
    Keycap(2, BeebKey_Semicolon, BeebKeySym_Semicolon, BeebKeySym_Plus),
    Keycap(2, BeebKey_Colon, BeebKeySym_Colon, BeebKeySym_Star),
    Keycap(3, BeebKey_Return, BeebKeySym_Return),
    {},
};

static const Keycap g_electron_line4[] = {
    Keycap(2, BeebKey_None, BeebKeySym_None),
    Keycap(3, BeebKey_Shift, BeebKeySym_Shift),
    Keycap(2, BeebKey_Z, BeebKeySym_Z),
    Keycap(2, BeebKey_X, BeebKeySym_X),
    Keycap(2, BeebKey_C, BeebKeySym_C),
    Keycap(2, BeebKey_V, BeebKeySym_V),
    Keycap(2, BeebKey_B, BeebKeySym_B),
    Keycap(2, BeebKey_N, BeebKeySym_N),
    Keycap(2, BeebKey_M, BeebKeySym_M),
    Keycap(2, BeebKey_Comma, BeebKeySym_Comma, BeebKeySym_LessThan),
    Keycap(2, BeebKey_Stop, BeebKeySym_Stop, BeebKeySym_GreaterThan),
    Keycap(2, BeebKey_Slash, BeebKeySym_Slash, BeebKeySym_QuestionMarke),
    Keycap(3, BeebKey_Shift, BeebKeySym_Shift),
    Keycap(2, BeebKey_Delete, BeebKeySym_Delete),
    {},
};

static const Keycap g_electron_line5[] = {
    Keycap(9, BeebKey_None, BeebKeySym_None),
    Keycap(20, BeebKey_Space, BeebKeySym_Space),
    {},
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *GetKeySymLabel(BeebKeySym sym) {
    if (sym < 0) {
        return nullptr;
    } else {
        switch (sym) {
        default:
            break;

#define L(SYM, LABEL) \
    case (SYM):       \
        return LABEL
            L(BeebKeySym_Break, "BRK");
            L(BeebKeySym_f0, "f0");
            L(BeebKeySym_f1, "f1");
            L(BeebKeySym_f2, "f2");
            L(BeebKeySym_f3, "f3");
            L(BeebKeySym_f4, "f4");
            L(BeebKeySym_f5, "f5");
            L(BeebKeySym_f6, "f6");
            L(BeebKeySym_f7, "f7");
            L(BeebKeySym_f8, "f8");
            L(BeebKeySym_f9, "f9");
            L(BeebKeySym_Escape, "ESC");
            L(BeebKeySym_1, "1");
            L(BeebKeySym_ExclamationMark, "!");
            L(BeebKeySym_2, "2");
            L(BeebKeySym_Quotes, "\"");
            L(BeebKeySym_3, "3");
            L(BeebKeySym_Hash, "#");
            L(BeebKeySym_4, "4");
            L(BeebKeySym_Dollar, "$");
            L(BeebKeySym_5, "5");
            L(BeebKeySym_Percent, "%");
            L(BeebKeySym_6, "6");
            L(BeebKeySym_Ampersand, "&");
            L(BeebKeySym_7, "7");
            L(BeebKeySym_Apostrophe, "'");
            L(BeebKeySym_8, "8");
            L(BeebKeySym_LeftBracket, "(");
            L(BeebKeySym_9, "9");
            L(BeebKeySym_RightBracket, ")");
            L(BeebKeySym_0, "0");
            L(BeebKeySym_Minus, "-");
            L(BeebKeySym_Equals, "=");
            L(BeebKeySym_Caret, "^");
            L(BeebKeySym_Tilde, "~");
            L(BeebKeySym_Backslash, "\\");
            L(BeebKeySym_Pipe, "|");
            L(BeebKeySym_Tab, "TAB");
            L(BeebKeySym_Q, "Q");
            L(BeebKeySym_W, "W");
            L(BeebKeySym_E, "E");
            L(BeebKeySym_R, "R");
            L(BeebKeySym_T, "T");
            L(BeebKeySym_Y, "Y");
            L(BeebKeySym_U, "U");
            L(BeebKeySym_I, "I");
            L(BeebKeySym_O, "O");
            L(BeebKeySym_P, "P");
            L(BeebKeySym_At, "@");
            L(BeebKeySym_LeftSquareBracket, "[");
            L(BeebKeySym_LeftCurlyBracket, "{");
            L(BeebKeySym_Underline, "_");
            L(BeebKeySym_Pound, "\xc2\xa3");
            L(BeebKeySym_CapsLock, "CAPS\nLOCK");
            L(BeebKeySym_Ctrl, "CTRL");
            L(BeebKeySym_A, "A");
            L(BeebKeySym_S, "S");
            L(BeebKeySym_D, "D");
            L(BeebKeySym_F, "F");
            L(BeebKeySym_G, "G");
            L(BeebKeySym_H, "H");
            L(BeebKeySym_J, "J");
            L(BeebKeySym_K, "K");
            L(BeebKeySym_L, "L");
            L(BeebKeySym_Semicolon, ";");
            L(BeebKeySym_Plus, "+");
            L(BeebKeySym_Colon, ":");
            L(BeebKeySym_Star, "*");
            L(BeebKeySym_RightSquareBracket, "]");
            L(BeebKeySym_RightCurlyBracket, "}");
            L(BeebKeySym_Return, "RETURN");
            L(BeebKeySym_ShiftLock, "SHF\nLOCK");
            L(BeebKeySym_Shift, "SHIFT");
            L(BeebKeySym_Z, "Z");
            L(BeebKeySym_X, "X");
            L(BeebKeySym_C, "C");
            L(BeebKeySym_V, "V");
            L(BeebKeySym_B, "B");
            L(BeebKeySym_N, "N");
            L(BeebKeySym_M, "M");
            L(BeebKeySym_Comma, ",");
            L(BeebKeySym_LessThan, "<");
            L(BeebKeySym_Stop, ".");
            L(BeebKeySym_GreaterThan, ">");
            L(BeebKeySym_Slash, "/");
            L(BeebKeySym_QuestionMarke, "?");
            L(BeebKeySym_Delete, "DEL");
            L(BeebKeySym_Copy, "COPY");
            L(BeebKeySym_Up, ICON_FA_LONG_ARROW_ALT_UP);
            L(BeebKeySym_Down, ICON_FA_LONG_ARROW_ALT_DOWN);
            L(BeebKeySym_Left, ICON_FA_LONG_ARROW_ALT_LEFT);
            L(BeebKeySym_Right, ICON_FA_LONG_ARROW_ALT_RIGHT);
            L(BeebKeySym_KeypadPlus, "+");
            L(BeebKeySym_KeypadMinus, "-");
            L(BeebKeySym_KeypadSlash, "/");
            L(BeebKeySym_KeypadStar, "*");
            L(BeebKeySym_Keypad7, "7");
            L(BeebKeySym_Keypad8, "8");
            L(BeebKeySym_Keypad9, "9");
            L(BeebKeySym_KeypadHash, "#");
            L(BeebKeySym_Keypad4, "4");
            L(BeebKeySym_Keypad5, "5");
            L(BeebKeySym_Keypad6, "6");
            L(BeebKeySym_KeypadDelete, "DEL");
            L(BeebKeySym_Keypad1, "1");
            L(BeebKeySym_Keypad2, "2");
            L(BeebKeySym_Keypad3, "3");
            L(BeebKeySym_KeypadComma, ",");
            L(BeebKeySym_Keypad0, "0");
            L(BeebKeySym_KeypadStop, ".");
            L(BeebKeySym_KeypadReturn, "RETURN");
            L(BeebKeySym_Space, "");
            L(BeebKeySym_CompactSpecialKey, ICON_FA_SQUARE);
#undef L
        }

        return "";
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ButtonColours {
    ImColor button, hovered, active;
};

static const ButtonColours KEY_COLOURS[] = {
    {
        ImColor::HSV(0.f, 0.f, .1f),
        ImColor::HSV(0.f, 0.f, .3f),
        ImColor::HSV(0.f, 0.f, .25f),
    },
    {
        ImColor::HSV(0.f, 1.f, .8f),
        ImColor::HSV(0.f, 1.f, 1.f),
        ImColor(0.f, 1.f, .9f),
    },
    {
        ImColor::HSV(.136f, .6f, .2f),
        ImColor::HSV(.136f, .6f, .45f),
        ImColor::HSV(.136f, .6f, .425f),
    },
};

static const ImColor PRESSED_KEY_BUTTON_COLOUR = ImColor::HSV(1 / 7.f, .6f, .6f);
static const ImColor PRESSED_KEY_BUTTON_HOVERED_COLOUR = ImColor::HSV(1 / 7.f, .7f, .7f);
static const ImColor PRESSED_KEY_BUTTON_ACTIVE_COLOUR = ImColor::HSV(1 / 7.f, .8f, .8f);

static const ImColor EMPTY_KEY_BUTTON_COLOUR = ImColor::HSV(.65f, .6f, .6f);
static const ImColor EMPTY_KEY_BUTTON_HOVERED_COLOUR = ImColor::HSV(.65f, .7f, .7f);
static const ImColor EMPTY_KEY_BUTTON_ACTIVE_COLOUR = ImColor::HSV(.65f, .8f, .8f);

static const ImColor PRESSED_KEY_TEXT_COLOUR = ImColor::HSV(1 / 7.f, .6f, .6f);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void KeymapsUI::DoScancodesList(BeebKeymap *keymap, BeebKey beeb_key, bool editable) {
    int sdl_keystates_size;
    const Uint8 *sdl_keystates = SDL_GetKeyboardState(&sdl_keystates_size);
    ASSERT(sdl_keystates_size >= 0);

    if (editable) {
        ImGui::Text("Edit PC Keys");
    } else {
        ImGui::Text("PC Keys");
    }

    ImGui::Separator();
    if (const uint32_t *scancodes = keymap->GetPCKeysForValue((int8_t)beeb_key)) {
        for (const uint32_t *scancode = scancodes; *scancode != 0; ++scancode) {
            ASSERT(*scancode < SDL_NUM_SCANCODES);
            ImGuiIDPusher id_pusher((int32_t)*scancode);

            ImGuiStyleColourPusher pusher;
            pusher.PushDefaultButtonColours();

            if (editable) {
                if (ImGui::Button("x")) {
                    keymap->SetMapping(*scancode, (int8_t)beeb_key, false);
                    m_edited = true;
                }
                ImGui::SameLine();
            }

            bool pc_key_pressed = *scancode < (size_t)sdl_keystates_size && sdl_keystates[*scancode];

            if (pc_key_pressed) {
                pusher.Push(ImGuiCol_Text, PRESSED_KEY_TEXT_COLOUR);
            }

            ImGui::TextUnformatted(SDL_GetScancodeName((SDL_Scancode)*scancode));
        }
    }

    if (editable) {
        ImGui::TextUnformatted("(press key to add)");
        m_wants_keyboard_focus = true;

        for (int i = 0; i < sdl_keystates_size; ++i) {
            if (sdl_keystates[i]) {
                keymap->SetMapping((uint32_t)i, (int8_t)beeb_key, true);
                m_edited = true;
            }
        }
    }
}

ImGuiStyleColourPusher KeymapsUI::GetColourPusherForKeycap(const BeebKeymap *keymap, int8_t keymap_key, const Keycap *keycap) {
    uint8_t beeb_key_pressed = m_beeb_window->GetBeebKeyState(keycap->key);
    const uint32_t *keycodes = keymap->GetPCKeysForValue(keymap_key);

    if (keycodes) {
        if (!keymap->IsKeySymMap()) {
            int sdl_keystates_size;
            const Uint8 *sdl_keystates = SDL_GetKeyboardState(&sdl_keystates_size);
            for (const uint32_t *keycode = keycodes; *keycode != 0; ++keycode) {
                if (*keycode < (uint32_t)sdl_keystates_size && sdl_keystates[*keycode]) {
                    beeb_key_pressed = true;
                    break;
                }
            }
        }
    }

    ImGuiStyleColourPusher pusher;

    if (beeb_key_pressed) {
        pusher.Push(ImGuiCol_Button, PRESSED_KEY_BUTTON_COLOUR);
        pusher.Push(ImGuiCol_ButtonHovered, PRESSED_KEY_BUTTON_HOVERED_COLOUR);
        pusher.Push(ImGuiCol_ButtonActive, PRESSED_KEY_BUTTON_ACTIVE_COLOUR);
    } else if (!keycodes) {
        pusher.Push(ImGuiCol_Button, EMPTY_KEY_BUTTON_COLOUR);
        pusher.Push(ImGuiCol_ButtonHovered, EMPTY_KEY_BUTTON_HOVERED_COLOUR);
        pusher.Push(ImGuiCol_ButtonActive, EMPTY_KEY_BUTTON_ACTIVE_COLOUR);
    } else {
        pusher.Push(ImGuiCol_Button, KEY_COLOURS[keycap->colour].button);
        pusher.Push(ImGuiCol_ButtonHovered, KEY_COLOURS[keycap->colour].hovered);
        pusher.Push(ImGuiCol_ButtonActive, KEY_COLOURS[keycap->colour].active);
    }

    return pusher;
}

//void HexEditor::GetMetrics(Metrics *metrics, const ImGuiStyle &style) {
//    (void)style;
//
//    metrics->num_addr_chars = m_handler->GetNumAddressChars();
//
//    metrics->line_height = ImGui::GetTextLineHeight();
//    metrics->glyph_width = ImGui::CalcTextSize("F").x + 1;
//
//    metrics->hex_left_x = (metrics->num_addr_chars + 2) * metrics->glyph_width;
//    metrics->hex_column_width = 2.5f * metrics->glyph_width;
//
//    metrics->ascii_left_x = metrics->hex_left_x;
//    if (this->options.hex) {
//        metrics->ascii_left_x += m_num_columns * metrics->hex_column_width + 2.f * metrics->glyph_width;
//    }
//
//    const ImVec4 &text_colour = style.Colors[ImGuiCol_Text];
//    const ImVec4 &text_disabled_colour = style.Colors[ImGuiCol_TextDisabled];
//    ImVec4 grey_colour(GetHalf(text_colour.x, text_disabled_colour.x),
//                       GetHalf(text_colour.y, text_disabled_colour.y),
//                       GetHalf(text_colour.z, text_disabled_colour.z),
//                       text_colour.w);
//    metrics->grey_colour = ImGui::ColorConvertFloat4ToU32(grey_colour);
//
//    metrics->disabled_colour = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_TextDisabled]);
//    metrics->text_colour = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]);
//}

//static const float KEY_WIDTH = 36.f;
//static const float KEY_HEIGHT = 32.f;
//static const float KEYPAD_X = 750.f;
static const size_t MAX_NUM_KEYCAPS = 100;

struct Row2Key {
    const Keycap *key;
    float x, w;
};

ImVec2 KeymapsUI::GetKeySize(const Keycap *key) const {
    float w = (float)key->width_in_halves;
    if (w == 0.f) {
        w = 2.f;
    }

    w *= m_metrics.key_width / 2.f;

    return ImVec2(w, m_metrics.key_height);
}

void KeymapsUI::DoScancodeKeyboardLinePart(BeebKeymap *keymap, const Keycap *line) {
    for (const Keycap *key = line; key->width_in_halves >= 0; ++key) {
        ImGuiIDPusher id_pusher(key);

        const ImVec2 &size = this->GetKeySize(key);

        if (key->key < 0) {
            ImGui::InvisibleButton("", size);
        } else {
            ImGuiStyleColourPusher colour_pusher = this->GetColourPusherForKeycap(keymap, (int8_t)key->key, key);

            char tmp[100];
            const char *label;
            const char *shifted = GetKeySymLabel(key->shifted_sym);
            const char *unshifted = GetKeySymLabel(key->unshifted_sym);
            const char *ctrled = GetKeySymLabel(key->ctrled_sym);
            const char *funced = GetKeySymLabel(key->funced_sym);

            // can only have 1 of FUNC or CTRL options per key.
            ASSERT(!!ctrled + !!funced <= 1);

            if (shifted && unshifted && (ctrled || funced)) {
                // blegh.
                const char *space;
                if (key->key == BeebKey_Copy) {
                    space = "";
                } else {
                    space = " ";
                }

                snprintf(tmp, sizeof tmp, "%s %s\n%s%s", shifted, ctrled ? ctrled : funced, space, unshifted);
                label = tmp;
            } else if (shifted && unshifted) {
                snprintf(tmp, sizeof tmp, "%s\n%s", shifted, unshifted);
                label = tmp;
            } else {
                ASSERT(!shifted);

                if (unshifted) {
                    label = unshifted;
                } else {
                    label = "";
                }
            }

            if (ImGui::Button(label, size)) {
                ImGui::OpenPopup(PC_SCANCODES_POPUP);
            }

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                this->DoScancodesList(keymap, key->key, false);
                ImGui::EndTooltip();
            }

            if (ImGui::BeginPopup(PC_SCANCODES_POPUP)) {
                this->DoScancodesList(keymap, key->key, true);
                ImGui::EndPopup();
            }
        }

        ImGui::SameLine();
    }
}

void KeymapsUI::DoScancodeKeyboardLineParts(BeebKeymap *keymap,
                                            const Keycap *line,
                                            const Keycap *m128_line) {
    this->DoScancodeKeyboardLinePart(keymap, line);

    if (m128_line) {
        ImGui::SetCursorPosX(m_metrics.keypad_x);

        this->DoScancodeKeyboardLinePart(keymap, m128_line);
    }
}

void KeymapsUI::DoKeySymButton(BeebKeymap *keymap, const char *label, const ImVec2 &size, BeebKeySym key_sym, const Keycap *keycap) {
    //BeebKey key=BeebKey_None;
    //BeebShiftState shift_state;
    //GetBeebKeyComboForKeySym(&key,&shift_state,key_sym);

    ImGuiStyleColourPusher colour_pusher = this->GetColourPusherForKeycap(keymap, (int8_t)key_sym, keycap);

    if (ImGui::Button(label, size)) {
        ImGui::OpenPopup(PC_KEYCODES_POPUP);
    }

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGuiKeySymsList(&m_edited, m_imgui_stuff, keymap, (int8_t)key_sym, false);
        ImGui::EndTooltip();
    }

    if (ImGui::BeginPopup(PC_KEYCODES_POPUP)) {
        ImGuiKeySymsList(&m_edited, m_imgui_stuff, keymap, (int8_t)key_sym, true);
        m_wants_keyboard_focus = true;
        ImGui::EndPopup();
    }
}

void KeymapsUI::DoKeySymKeyboardLineTopHalves(BeebKeymap *keymap,
                                              const Keycap *line,
                                              BottomHalfKeycap *bottom_halves,
                                              size_t *num_bottom_halves) {
    for (const Keycap *keycap = line; keycap->width_in_halves >= 0; ++keycap) {
        ImGuiIDPusher id_pusher(keycap);

        ImVec2 size = this->GetKeySize(keycap);

        if (keycap->key < 0) {
            ImGui::InvisibleButton("", size);
        } else {
            const char *shifted = GetKeySymLabel(keycap->shifted_sym);
            const char *ctrled = GetKeySymLabel(keycap->ctrled_sym);
            const char *unshifted = GetKeySymLabel(keycap->unshifted_sym);
            const char *funced = GetKeySymLabel(keycap->funced_sym);

            if (keycap->shifted_sym != BeebKeySym_None && keycap->unshifted_sym != BeebKeySym_None) {
                // Half-height buttons.
                size.y *= .5f;

                ASSERT(*num_bottom_halves < MAX_NUM_KEYCAPS);
                bottom_halves[(*num_bottom_halves)++] = {ImGui::GetCursorPosX(), size, keycap};

                if (shifted && !(ctrled || funced)) {
                    ImGuiIDPusher id_pusher2(1);
                    this->DoKeySymButton(keymap, shifted, size, keycap->shifted_sym, keycap);
                } else {
                    size.x *= .5f;

                    {
                        ImGuiIDPusher id_pusher2(1);
                        this->DoKeySymButton(keymap, shifted, size, keycap->shifted_sym, keycap);
                    }

                    ImGui::SameLine(0.f, 0.f);

                    {
                        ImGuiIDPusher id_pusher2(2);
                        if (ctrled) {
                            this->DoKeySymButton(keymap, ctrled, size, keycap->ctrled_sym, keycap);
                        } else {
                            this->DoKeySymButton(keymap, funced, size, keycap->funced_sym, keycap);
                        }
                    }
                }
                //ImGui::Button(shifted,size);
            } else {
                // Full-height button.
                ASSERT(!shifted);
                ASSERT(!ctrled);

                this->DoKeySymButton(keymap, unshifted, size, keycap->unshifted_sym, keycap);
            }
        }

        ImGui::SameLine();
    }
}

void KeymapsUI::DoKeySymKeyboardLineBottomHalves(BeebKeymap *keymap,
                                                 float y,
                                                 const BottomHalfKeycap *bottom_halves,
                                                 size_t num_bottom_halves) {
    for (size_t i = 0; i < num_bottom_halves; ++i) {
        const BottomHalfKeycap *key = &bottom_halves[i];

        ImGuiIDPusher id_pusher(key->keycap);
        ImGuiIDPusher id_pusher2(0);

        ImGui::SetCursorPos(ImVec2(key->x, y));
        this->DoKeySymButton(keymap,
                             GetKeySymLabel(key->keycap->unshifted_sym),
                             key->size,
                             key->keycap->unshifted_sym,
                             key->keycap);

        ImGui::SameLine();
    }
}

void KeymapsUI::DoKeySymKeyboardLineParts(BeebKeymap *keymap, const Keycap *line, const Keycap *m128_line) {
    BottomHalfKeycap keycaps[MAX_NUM_KEYCAPS];
    size_t num_keycaps = 0;

    float y = ImGui::GetCursorPosY();

    this->DoKeySymKeyboardLineTopHalves(keymap, line, keycaps, &num_keycaps);

    if (m128_line) {
        ImGui::SetCursorPosX(m_metrics.keypad_x);

        this->DoKeySymKeyboardLineTopHalves(keymap, m128_line, keycaps, &num_keycaps);
    }

    this->DoKeySymKeyboardLineBottomHalves(keymap, y + m_metrics.key_height * .5f, keycaps, num_keycaps);

    ImGui::SetCursorPosY(y);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void KeymapsUI::DoKeyboardLine(BeebKeymap *keymap, const Keycap *line, const Keycap *m128_line) {
    if (keymap->IsKeySymMap()) {
        this->DoKeySymKeyboardLineParts(keymap, line, m128_line);
    } else {
        this->DoScancodeKeyboardLineParts(keymap, line, m128_line);
    }

    ImGui::NewLine();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void KeymapsUI::DoEditKeymapGui() {
    BeebKeymap *keymap = GetBeebKeymapByIndex(m_keymap_index);
    if (!keymap) {
        return;
    }

    // The factors here are all just arbitrary numbers that make things line
    // up roughly.
    m_metrics.key_height = ImGui::GetTextLineHeight() * 2.75f;
    m_metrics.key_width = (ImGui::CalcTextSize("W").x + 5.f) * 4.f;
    m_metrics.keypad_x = m_metrics.key_width * 21;

    bool edited = false;

    ImGuiIDPusher id_pusher(keymap);

    std::string title = GetKeymapUIName(*keymap);

    {
        std::string name;
        if (ImGuiInputText(&name, "Name", keymap->GetName())) {
            keymap->SetName(std::move(name));
            edited = true;
        }
    }

    {
        bool prefer = keymap->GetPreferShortcuts();
        if (ImGui::Checkbox("Prioritize command keys", &prefer)) {
            keymap->SetPreferShortcuts(prefer);
            m_edited = true;
        }
    }

    ImGui::SetNextItemOpen(keymap->show_bbc_keyboard_ui);
    keymap->show_bbc_keyboard_ui = ImGui::CollapsingHeader("BBC B/B+/Master 128 layout", ImGuiTreeNodeFlags_NoTreePushOnOpen);
    if (keymap->show_bbc_keyboard_ui) {
        ImGuiIDPusher pusher("bbc");

        this->DoKeyboardLine(keymap, g_keyboard_line1, g_m128_line1);
        this->DoKeyboardLine(keymap, g_keyboard_line2, g_m128_line2);
        this->DoKeyboardLine(keymap, g_keyboard_line3, g_m128_line3);
        this->DoKeyboardLine(keymap, g_keyboard_line4, g_m128_line4);
        this->DoKeyboardLine(keymap, g_keyboard_line5, g_m128_line5);
        this->DoKeyboardLine(keymap, g_keyboard_line6, nullptr);
    }

    ImGui::SetNextItemOpen(keymap->show_compact_keyboard_ui);
    keymap->show_compact_keyboard_ui = ImGui::CollapsingHeader("Master Compact/PC 128 S layout", ImGuiTreeNodeFlags_NoTreePushOnOpen);
    if (keymap->show_compact_keyboard_ui) {
        ImGuiIDPusher pusher("compact");

        this->DoKeyboardLine(keymap, g_keyboard_line1, g_m128_line1);
        this->DoKeyboardLine(keymap, g_compact_line2, g_m128_line2);
        this->DoKeyboardLine(keymap, g_compact_line3, g_m128_line3);
        this->DoKeyboardLine(keymap, g_keyboard_line4, g_m128_line4);
        this->DoKeyboardLine(keymap, g_keyboard_line5, g_m128_line5);
        this->DoKeyboardLine(keymap, g_keyboard_line6, nullptr);
    }

    ImGui::SetNextItemOpen(keymap->show_electron_keyboard_ui);
    keymap->show_electron_keyboard_ui = ImGui::CollapsingHeader("Electron layout", ImGuiTreeNodeFlags_NoTreePushOnOpen);
    if (keymap->show_electron_keyboard_ui) {
        ImGuiIDPusher pusher("electron");

        this->DoKeyboardLine(keymap, g_electron_line1, nullptr);
        this->DoKeyboardLine(keymap, g_electron_line2, nullptr);
        this->DoKeyboardLine(keymap, g_electron_line3, nullptr);
        this->DoKeyboardLine(keymap, g_electron_line4, nullptr);
        this->DoKeyboardLine(keymap, g_electron_line5, nullptr);
    }

    if (edited) {
        BeebWindows::BeebKeymapDidChange((size_t)m_keymap_index);
        m_edited = true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
