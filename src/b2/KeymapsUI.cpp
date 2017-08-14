#include <shared/system.h>
#include "KeymapsUI.h"
#include "BeebWindows.h"
#include "dear_imgui.h"
#include "keys.h"
#include <SDL.h>
#include <shared/debug.h>
#include "keymap.h"
#include "BeebWindow.h"
#include <IconsFontAwesome.h>
#include "BeebKeymap.h"

#include <shared/enum_decl.h>
#include "KeymapsUI_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "KeymapsUI_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char KeymapsUI::NOT_EDITABLE_ICON[]=ICON_FA_LOCK;
const char KeymapsUI::SCANCODES_KEYMAP_ICON[]=ICON_FA_KEYBOARD_O;
const char KeymapsUI::KEYSYMS_KEYMAP_ICON[]=ICON_FA_FONT;

static const char SHORTCUT_KEYCODES_POPUP[]="shortcut_keycodes";
static const char PC_SCANCODES_POPUP[]="pc_scancodes";
static const char PC_KEYCODES_POPUP[]="pc_keycodes";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

KeymapsUI::KeymapsUI() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

KeymapsUI::~KeymapsUI() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Keycap {
    BeebKey key;
    BeebKeySym unshifted_sym,shifted_sym;
    //const char *unshifted,*shifted;
    int width_in_halves;
    KeyColour colour;
};

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

class KeymapsUIImpl:
    public KeymapsUI
{
public:
    void SetWindowDetails(BeebWindow *beeb_window) override;

    void SetCurrentBeebKeymap(const BeebKeymap *keymap) override;
    const BeebKeymap *GetCurrentBeebKeymap() const override;
    void DoImGui() override;
    bool WantsKeyboardFocus() const override;
    bool DidConfigChange() const override;
protected:
private:
    BeebWindow *m_beeb_window=nullptr;
    bool m_wants_keyboard_focus=false;
    const BeebKeymap *m_current_keymap=nullptr;
    bool m_edited=false;

    void DoScancodesList(const BeebKeymap *keymap,BeebKeymap *editable_keymap,BeebKey beeb_key);
    uint32_t GetPressedKeycode();
    template<class KeymapType>
    void DoKeySymsList(const KeymapType *keymap,KeymapType *editable_keymap,typename KeymapType::ValueType value);
    ImGuiStyleColourPusher GetColourPusherForKeycap(const BeebKeymap *keymap,int8_t keymap_key,const Keycap *keycap);
    void DoScancodeKeyboardLinePart(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const Keycap *line);
    void DoScancodeKeyboardLineParts(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const Keycap *line,const Keycap *m128_line);
    void DoKeySymButton(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const char *label,const ImVec2 &size,BeebKeySym key_sym,const Keycap *keycap);
    void DoKeySymKeyboardLineTopHalves(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const Keycap *line,BottomHalfKeycap *bottom_halves,size_t *num_bottom_halves);
    void DoKeySymKeyboardLineBottomHalves(const BeebKeymap *keymap,BeebKeymap *editable_keymap,float y,const BottomHalfKeycap *bottom_halves,size_t num_bottom_halves);
    void DoKeySymKeyboardLineParts(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const Keycap *line,const Keycap *m128_line);
    void DoKeyboardLine(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const Keycap *line,const Keycap *m128_line);

    // Returns false if keymap was deleted.
    bool DoEditKeymapGui(const BeebKeymap *keymap,BeebKeymap *editable_keymap);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<KeymapsUI> KeymapsUI::Create() {
    return std::make_unique<KeymapsUIImpl>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void KeymapsUIImpl::SetWindowDetails(BeebWindow *beeb_window) {
    m_beeb_window=beeb_window;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void KeymapsUIImpl::SetCurrentBeebKeymap(const BeebKeymap *keymap) {
    m_current_keymap=keymap;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebKeymap *KeymapsUIImpl::GetCurrentBeebKeymap() const {
    return m_current_keymap;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool KeymapsUIImpl::WantsKeyboardFocus() const {
    return m_wants_keyboard_focus;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool KeymapsUIImpl::DidConfigChange() const {
    return m_edited;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void KeymapsUIImpl::DoImGui() {
    bool any=false;

    m_wants_keyboard_focus=false;

    CommandTable::ForEachCommandTable([&](CommandTable *table) {
        ImGuiIDPusher id_pusher(table);
        std::string title=table->GetName()+" shortcuts";
        if(ImGui::CollapsingHeader(title.c_str(),"header",true,true)) {
            table->ForEachCommand([&](Command *command) {
                ImGuiIDPusher id_pusher(command);

                if(ImGui::Button(command->GetText().c_str())) {
                    ImGui::OpenPopup(SHORTCUT_KEYCODES_POPUP);
                }

                if(ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    this->DoKeySymsList(table,(CommandTable *)nullptr,command);
                    ImGui::EndTooltip();
                }

                if(ImGui::BeginPopup(SHORTCUT_KEYCODES_POPUP)) {
                    this->DoKeySymsList(table,table,command);
                    ImGui::EndPopup();
                }
            });
        }
    });

    BeebWindows::ForEachBeebKeymap([&](const BeebKeymap *keymap,BeebKeymap *editable_keymap) {
        if(any) {
            ImGui::Separator();
            any=true;
        }

        if(!this->DoEditKeymapGui(keymap,editable_keymap)) {
            return false;
        }

        return true;
    });
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *g_keysym_labels[256];

//#define K(KEY,...) {BeebKey_##KEY,__VA_ARGS__,)
//#define F(N) {BeebKey_F##N,"f" #N,0,2,KeyColour_Red}
//#define E(N,C) {BeebKey_##N,C,nullptr,2,KeyColour_Khaki}
//#define LETTER(L) {BeebKey_##L,#L}
//#define EDITKEY(N) N,0,2,2,
#define END {BeebKey_None,BeebKeySym_None,BeebKeySym_None,-1}

// BBC keyboard caps
static const Keycap g_keyboard_line1[]={
    {BeebKey_None,BeebKeySym_None,BeebKeySym_None,5},
    {BeebKey_f0,BeebKeySym_f0,BeebKeySym_None,2,KeyColour_Red},
    {BeebKey_f1,BeebKeySym_f1,BeebKeySym_None,2,KeyColour_Red},
    {BeebKey_f2,BeebKeySym_f2,BeebKeySym_None,2,KeyColour_Red},
    {BeebKey_f3,BeebKeySym_f3,BeebKeySym_None,2,KeyColour_Red},
    {BeebKey_f4,BeebKeySym_f4,BeebKeySym_None,2,KeyColour_Red},
    {BeebKey_f5,BeebKeySym_f5,BeebKeySym_None,2,KeyColour_Red},
    {BeebKey_f6,BeebKeySym_f6,BeebKeySym_None,2,KeyColour_Red},
    {BeebKey_f7,BeebKeySym_f7,BeebKeySym_None,2,KeyColour_Red},
    {BeebKey_f8,BeebKeySym_f8,BeebKeySym_None,2,KeyColour_Red},
    {BeebKey_f9,BeebKeySym_f9,BeebKeySym_None,2,KeyColour_Red},
    {BeebKey_Break,BeebKeySym_Break,BeebKeySym_None,},
    END,
};

static const Keycap g_keyboard_line2[]={
    {BeebKey_Escape,BeebKeySym_Escape,BeebKeySym_None,},
    {BeebKey_1,BeebKeySym_1,BeebKeySym_ExclamationMark},
    {BeebKey_2,BeebKeySym_2,BeebKeySym_Quotes},
    {BeebKey_3,BeebKeySym_3,BeebKeySym_Hash},
    {BeebKey_4,BeebKeySym_4,BeebKeySym_Dollar},
    {BeebKey_5,BeebKeySym_5,BeebKeySym_Percent},
    {BeebKey_6,BeebKeySym_6,BeebKeySym_Ampersand},
    {BeebKey_7,BeebKeySym_7,BeebKeySym_Apostrophe},
    {BeebKey_8,BeebKeySym_8,BeebKeySym_LeftBracket},
    {BeebKey_9,BeebKeySym_9,BeebKeySym_RightBracket},
    {BeebKey_0,BeebKeySym_0,BeebKeySym_None},
    {BeebKey_Minus,BeebKeySym_Minus,BeebKeySym_Equals},
    {BeebKey_Caret,BeebKeySym_Caret,BeebKeySym_Tilde},
    {BeebKey_Backslash,BeebKeySym_Backslash,BeebKeySym_Pipe},
    {BeebKey_Left,BeebKeySym_Left,BeebKeySym_None,2,KeyColour_Khaki},
    {BeebKey_Right,BeebKeySym_Right,BeebKeySym_None,2,KeyColour_Khaki},
    END,
};

static const Keycap g_keyboard_line3[]={
    {BeebKey_Tab,BeebKeySym_Tab,BeebKeySym_None,3},
    {BeebKey_Q,BeebKeySym_Q,BeebKeySym_None,2},
    {BeebKey_W,BeebKeySym_W,BeebKeySym_None,2},
    {BeebKey_E,BeebKeySym_E,BeebKeySym_None,2},
    {BeebKey_R,BeebKeySym_R,BeebKeySym_None,2},
    {BeebKey_T,BeebKeySym_T,BeebKeySym_None,2},
    {BeebKey_Y,BeebKeySym_Y,BeebKeySym_None,2},
    {BeebKey_U,BeebKeySym_U,BeebKeySym_None,2},
    {BeebKey_I,BeebKeySym_I,BeebKeySym_None,2},
    {BeebKey_O,BeebKeySym_O,BeebKeySym_None,2},
    {BeebKey_P,BeebKeySym_P,BeebKeySym_None,2},
    {BeebKey_At,BeebKeySym_At,BeebKeySym_None,2},
    {BeebKey_LeftSquareBracket,BeebKeySym_LeftSquareBracket,BeebKeySym_LeftCurlyBracket},
    {BeebKey_Underline,BeebKeySym_Underline,BeebKeySym_Pound},
    {BeebKey_Up,BeebKeySym_Up,BeebKeySym_None,2,KeyColour_Khaki},
    {BeebKey_Down,BeebKeySym_Down,BeebKeySym_None,2,KeyColour_Khaki},
    END,
};

static const Keycap g_keyboard_line4[]={
    {BeebKey_CapsLock,BeebKeySym_CapsLock,BeebKeySym_None},
    {BeebKey_Ctrl,BeebKeySym_Ctrl,BeebKeySym_None},
    {BeebKey_A,BeebKeySym_A,BeebKeySym_None,2},
    {BeebKey_S,BeebKeySym_S,BeebKeySym_None,2},
    {BeebKey_D,BeebKeySym_D,BeebKeySym_None,2},
    {BeebKey_F,BeebKeySym_F,BeebKeySym_None,2},
    {BeebKey_G,BeebKeySym_G,BeebKeySym_None,2},
    {BeebKey_H,BeebKeySym_H,BeebKeySym_None,2},
    {BeebKey_J,BeebKeySym_J,BeebKeySym_None,2},
    {BeebKey_K,BeebKeySym_K,BeebKeySym_None,2},
    {BeebKey_L,BeebKeySym_L,BeebKeySym_None,2},
    {BeebKey_Semicolon,BeebKeySym_Semicolon,BeebKeySym_Plus},
    {BeebKey_Colon,BeebKeySym_Colon,BeebKeySym_Star},
    {BeebKey_RightSquareBracket,BeebKeySym_RightSquareBracket,BeebKeySym_RightCurlyBracket},
    {BeebKey_Return,BeebKeySym_Return,BeebKeySym_None,4},
    END,
};

static const Keycap g_keyboard_line5[]={
    {BeebKey_ShiftLock,BeebKeySym_ShiftLock,BeebKeySym_None},
    {BeebKey_Shift,BeebKeySym_Shift,BeebKeySym_None,3},
    {BeebKey_Z,BeebKeySym_Z,BeebKeySym_None,2},
    {BeebKey_X,BeebKeySym_X,BeebKeySym_None,2},
    {BeebKey_C,BeebKeySym_C,BeebKeySym_None,2},
    {BeebKey_V,BeebKeySym_V,BeebKeySym_None,2},
    {BeebKey_B,BeebKeySym_B,BeebKeySym_None,2},
    {BeebKey_N,BeebKeySym_N,BeebKeySym_None,2},
    {BeebKey_M,BeebKeySym_M,BeebKeySym_None,2},
    {BeebKey_Comma,BeebKeySym_Comma,BeebKeySym_LessThan},
    {BeebKey_Stop,BeebKeySym_Stop,BeebKeySym_GreaterThan},
    {BeebKey_Slash,BeebKeySym_Slash,BeebKeySym_QuestionMarke,},
    {BeebKey_Shift,BeebKeySym_Shift,BeebKeySym_None,3},
    {BeebKey_Delete,BeebKeySym_Delete,BeebKeySym_None},
    {BeebKey_Copy,BeebKeySym_Copy,BeebKeySym_None,2,KeyColour_Khaki},
    END,
};

static const Keycap g_keyboard_line6[]={
    {BeebKey_None,BeebKeySym_None,BeebKeySym_None,8},
    {BeebKey_Space,BeebKeySym_Space,BeebKeySym_None,19},
    END,
};

static const Keycap g_m128_line1[]={
    {BeebKey_KeypadPlus,BeebKeySym_KeypadPlus,BeebKeySym_None,2},
    {BeebKey_KeypadMinus,BeebKeySym_KeypadMinus,BeebKeySym_None,2},
    {BeebKey_KeypadSlash,BeebKeySym_KeypadSlash,BeebKeySym_None,2},
    {BeebKey_KeypadStar,BeebKeySym_KeypadStar,BeebKeySym_None,2},
    END
};

static const Keycap g_m128_line2[]={
    {BeebKey_Keypad7,BeebKeySym_Keypad7,BeebKeySym_None,2},
    {BeebKey_Keypad8,BeebKeySym_Keypad8,BeebKeySym_None,2},
    {BeebKey_Keypad9,BeebKeySym_Keypad9,BeebKeySym_None,2},
    {BeebKey_KeypadHash,BeebKeySym_KeypadHash,BeebKeySym_None,2},
    END
};

static const Keycap g_m128_line3[]={
    {BeebKey_Keypad4,BeebKeySym_Keypad4,BeebKeySym_None,2},
    {BeebKey_Keypad5,BeebKeySym_Keypad5,BeebKeySym_None,2},
    {BeebKey_Keypad6,BeebKeySym_Keypad6,BeebKeySym_None,2},
    {BeebKey_KeypadDelete,BeebKeySym_KeypadDelete,BeebKeySym_None,2},
    END
};

static const Keycap g_m128_line4[]={
    {BeebKey_Keypad1,BeebKeySym_Keypad1,BeebKeySym_None,2},
    {BeebKey_Keypad2,BeebKeySym_Keypad2,BeebKeySym_None,2},
    {BeebKey_Keypad3,BeebKeySym_Keypad3,BeebKeySym_None,2},
    {BeebKey_KeypadStop,BeebKeySym_KeypadStop,BeebKeySym_None,2},
    END
};

static const Keycap g_m128_line5[]={
    {BeebKey_Keypad0,BeebKeySym_Keypad0,BeebKeySym_None,2},
    {BeebKey_KeypadComma,BeebKeySym_KeypadComma,BeebKeySym_None,2},
    {BeebKey_KeypadReturn,BeebKeySym_KeypadReturn,BeebKeySym_None,4,},
    END
};

#undef END

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ButtonColours {
    ImColor button,hovered,active;
};

static const ButtonColours KEY_COLOURS[]={
    {ImColor::HSV(0.f,0.f,.1f),ImColor::HSV(0.f,0.f,.3f),ImColor::HSV(0.f,0.f,.25f),},
    {ImColor::HSV(0.f,1.f,.8f),ImColor::HSV(0.f,1.f,1.f),ImColor(0.f,1.f,.9f),},
    {ImColor::HSV(.136f,.6f,.2f),ImColor::HSV(.136f,.6f,.45f),ImColor::HSV(.136f,.6f,.425f)},
};

static const ImColor PRESSED_KEY_BUTTON_COLOUR=ImColor::HSV(1/7.f,.6f,.6f);
static const ImColor PRESSED_KEY_BUTTON_HOVERED_COLOUR=ImColor::HSV(1/7.f,.7f,.7f);
static const ImColor PRESSED_KEY_BUTTON_ACTIVE_COLOUR=ImColor::HSV(1/7.f,.8f,.8f);

static const ImColor EMPTY_KEY_BUTTON_COLOUR=ImColor::HSV(.65f,.6f,.6f);
static const ImColor EMPTY_KEY_BUTTON_HOVERED_COLOUR=ImColor::HSV(.65f,.7f,.7f);
static const ImColor EMPTY_KEY_BUTTON_ACTIVE_COLOUR=ImColor::HSV(.65f,.8f,.8f);

static const ImColor PRESSED_KEY_TEXT_COLOUR=ImColor::HSV(1/7.f,.6f,.6f);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void KeymapsUIImpl::DoScancodesList(const BeebKeymap *keymap,BeebKeymap *editable_keymap,BeebKey beeb_key) {
    int sdl_keystates_size;
    const Uint8 *sdl_keystates=SDL_GetKeyboardState(&sdl_keystates_size);
    ASSERT(sdl_keystates_size>=0);

    ImGui::Text(editable_keymap?"Edit PC Keys":"PC Keys");
    ImGui::Separator();
    if(const uint32_t *scancodes=keymap->GetPCKeysForValue((int8_t)beeb_key)) {
        for(const uint32_t *scancode=scancodes;*scancode!=0;++scancode) {
            ASSERT(*scancode<SDL_NUM_SCANCODES);
            ImGuiIDPusher id_pusher((int32_t)*scancode);

            if(editable_keymap) {
                ImGuiStyleColourPusher pusher;
                pusher.PushDefaultButtonColours();

                if(ImGui::Button("x")) {
                    editable_keymap->SetMapping(*scancode,(int8_t)beeb_key,false);
                    m_edited=true;
                }
                ImGui::SameLine();
            }

            bool pc_key_pressed=*scancode<(size_t)sdl_keystates_size&&sdl_keystates[*scancode];

            ImGuiStyleColourPusher pusher;

            if(pc_key_pressed) {
                pusher.Push(ImGuiCol_Text,PRESSED_KEY_TEXT_COLOUR);
            }

            ImGui::TextUnformatted(SDL_GetScancodeName((SDL_Scancode)*scancode));
        }
    }

    if(editable_keymap) {
        ImGui::TextUnformatted("(press key to add)");
        m_wants_keyboard_focus=true;

        for(int i=0;i<sdl_keystates_size;++i) {
            if(sdl_keystates[i]) {
                editable_keymap->SetMapping((uint32_t)i,(int8_t)beeb_key,true);
                m_edited=true;
            }
        }
    }
}

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

// returns 0 if there's nothing pressed.
uint32_t KeymapsUIImpl::GetPressedKeycode() {
    int sdl_keystates_size;
    const Uint8 *sdl_keystates=SDL_GetKeyboardState(&sdl_keystates_size);
    uint32_t modifiers=GetPCKeyModifiersFromSDLKeymod((uint16_t)SDL_GetModState());

    m_wants_keyboard_focus=true;

    for(int i=0;i<sdl_keystates_size;++i) {
        if(sdl_keystates[i]) {
            if(IsModifierKey((SDL_Scancode)i)) {
                // Ignore...
            } else {
                SDL_Keycode keycode=SDL_GetKeyFromScancode((SDL_Scancode)i);
                if(keycode!=0) {
                    return (uint32_t)keycode|modifiers;
                }
            }
        }
    }

    return 0;
}

template<class KeymapType>
void KeymapsUIImpl::DoKeySymsList(const KeymapType *keymap,KeymapType *editable_keymap,typename KeymapType::ValueType value) {
    ImGui::Text(editable_keymap?"Edit PC Keys":"PC Keys");
    ImGui::Separator();
    if(const uint32_t *keycodes=keymap->GetPCKeysForValue(value)) {
        for(const uint32_t *keycode=keycodes;*keycode!=0;++keycode) {
            ImGuiIDPusher id_pusher((int)*keycode);

            if(editable_keymap) {
                if(ImGui::Button("x")) {
                    editable_keymap->SetMapping(*keycode,value,false);
                    m_edited=true;
                }
                ImGui::SameLine();
            }

            std::string name=GetKeycodeName(*keycode);

            ImGui::TextUnformatted(name.c_str());
        }
    }

    if(editable_keymap) {
        ImGui::TextUnformatted("(press key to add)");

        uint32_t keycode=this->GetPressedKeycode();
        if(keycode!=0) {
            editable_keymap->SetMapping(keycode,value,true);
            m_edited=true;
        }
    }
}

ImGuiStyleColourPusher KeymapsUIImpl::GetColourPusherForKeycap(const BeebKeymap *keymap,int8_t keymap_key,const Keycap *keycap) {
    uint8_t beeb_key_pressed=m_beeb_window->GetBeebKeyState(keycap->key);
    const uint32_t *scancodes=keymap->GetPCKeysForValue(keymap_key);

    ImGuiStyleColourPusher pusher;

    if(beeb_key_pressed) {
        pusher.Push(ImGuiCol_Button,PRESSED_KEY_BUTTON_COLOUR);
        pusher.Push(ImGuiCol_ButtonHovered,PRESSED_KEY_BUTTON_HOVERED_COLOUR);
        pusher.Push(ImGuiCol_ButtonActive,PRESSED_KEY_BUTTON_ACTIVE_COLOUR);
    } else if(!scancodes) {
        pusher.Push(ImGuiCol_Button,EMPTY_KEY_BUTTON_COLOUR);
        pusher.Push(ImGuiCol_ButtonHovered,EMPTY_KEY_BUTTON_HOVERED_COLOUR);
        pusher.Push(ImGuiCol_ButtonActive,EMPTY_KEY_BUTTON_ACTIVE_COLOUR);
    } else {
        pusher.Push(ImGuiCol_Button,KEY_COLOURS[keycap->colour].button);
        pusher.Push(ImGuiCol_ButtonHovered,KEY_COLOURS[keycap->colour].hovered);
        pusher.Push(ImGuiCol_ButtonActive,KEY_COLOURS[keycap->colour].active);
    }

    return pusher;
}

static const float KEY_WIDTH=36.f;
static const float KEY_HEIGHT=32.f;
static const float KEYPAD_X=750.f;
static const size_t MAX_NUM_KEYCAPS=100;

struct Row2Key {
    const Keycap *key;
    float x,w;
};

static ImVec2 GetKeySize(const Keycap *key) {
    float w=(float)key->width_in_halves;
    if(w==0.f) {
        w=2.f;
    }

    w*=KEY_WIDTH/2.f;

    return ImVec2(w,KEY_HEIGHT);
}

void KeymapsUIImpl::DoScancodeKeyboardLinePart(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const Keycap *line) {
    for(const Keycap *key=line;key->width_in_halves>=0;++key) {
        ImGuiIDPusher id_pusher(key);

        const ImVec2 &size=GetKeySize(key);

        if(key->key<0) {
            ImGui::InvisibleButton("",size);
        } else {
            ImGuiStyleColourPusher colour_pusher=this->GetColourPusherForKeycap(keymap,(int8_t)key->key,key);

            char tmp[100];
            const char *label;
            const char *shifted=g_keysym_labels[key->shifted_sym];
            const char *unshifted=g_keysym_labels[key->unshifted_sym];

            if(shifted&&unshifted) {
                snprintf(tmp,sizeof tmp,"%s\n%s",shifted,unshifted);
                label=tmp;
            } else {
                ASSERT(!shifted);

                if(unshifted) {
                    label=unshifted;
                } else {
                    label="";
                }
            }

            if(ImGui::Button(label,size)) {
                if(editable_keymap) {
                    ImGui::OpenPopup(PC_SCANCODES_POPUP);
                }
            }

            if(ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                this->DoScancodesList(keymap,nullptr,key->key);
                ImGui::EndTooltip();
            }

            if(ImGui::BeginPopup(PC_SCANCODES_POPUP)) {
                this->DoScancodesList(keymap,editable_keymap,key->key);
                ImGui::EndPopup();
            }
        }

        ImGui::SameLine();
    }
}

void KeymapsUIImpl::DoScancodeKeyboardLineParts(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const Keycap *line,const Keycap *m128_line) {
    this->DoScancodeKeyboardLinePart(keymap,editable_keymap,line);

    if(m128_line) {
        ImGui::SetCursorPosX(KEYPAD_X);

        this->DoScancodeKeyboardLinePart(keymap,editable_keymap,m128_line);
    }
}

void KeymapsUIImpl::DoKeySymButton(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const char *label,const ImVec2 &size,BeebKeySym key_sym,const Keycap *keycap) {
    //BeebKey key=BeebKey_None;
    //BeebShiftState shift_state;
    //GetBeebKeyComboForKeySym(&key,&shift_state,key_sym);

    ImGuiStyleColourPusher colour_pusher=this->GetColourPusherForKeycap(keymap,(int8_t)key_sym,keycap);

    if(ImGui::Button(label,size)) {
        if(editable_keymap) {
            ImGui::OpenPopup(PC_KEYCODES_POPUP);
        }
    }

    if(ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        this->DoKeySymsList(keymap,(BeebKeymap *)nullptr,(int8_t)key_sym);
        ImGui::EndTooltip();
    }

    if(ImGui::BeginPopup(PC_KEYCODES_POPUP)) {
        this->DoKeySymsList(keymap,editable_keymap,(int8_t)key_sym);
        ImGui::EndPopup();
    }
}

void KeymapsUIImpl::DoKeySymKeyboardLineTopHalves(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const Keycap *line,BottomHalfKeycap *bottom_halves,size_t *num_bottom_halves) {
    for(const Keycap *keycap=line;keycap->width_in_halves>=0;++keycap) {
        ImGuiIDPusher id_pusher(keycap);

        ImVec2 size=GetKeySize(keycap);

        if(keycap->key<0) {
            ImGui::InvisibleButton("",size);
        } else {
            const char *shifted=g_keysym_labels[keycap->shifted_sym];
            const char *unshifted=g_keysym_labels[keycap->unshifted_sym];

            if(shifted&&unshifted) {
                // Two half-height buttons.
                size.y*=.5f;

                ASSERT(*num_bottom_halves<MAX_NUM_KEYCAPS);
                bottom_halves[(*num_bottom_halves)++]={ImGui::GetCursorPosX(),size,keycap};

                ImGuiIDPusher id_pusher2(1);
                this->DoKeySymButton(keymap,editable_keymap,shifted,size,keycap->shifted_sym,keycap);
                //ImGui::Button(shifted,size);
            } else {
                // Full-height button.
                ASSERT(!shifted);

                this->DoKeySymButton(keymap,editable_keymap,unshifted,size,keycap->unshifted_sym,keycap);
            }
        }

        ImGui::SameLine();
    }
}

void KeymapsUIImpl::DoKeySymKeyboardLineBottomHalves(const BeebKeymap *keymap,BeebKeymap *editable_keymap,float y,const BottomHalfKeycap *bottom_halves,size_t num_bottom_halves) {
    for(size_t i=0;i<num_bottom_halves;++i) {
        const BottomHalfKeycap *key=&bottom_halves[i];

        ImGuiIDPusher id_pusher(key->keycap);
        ImGuiIDPusher id_pusher2(0);

        ImGui::SetCursorPos(ImVec2(key->x,y));
        this->DoKeySymButton(keymap,editable_keymap,g_keysym_labels[key->keycap->unshifted_sym],key->size,key->keycap->unshifted_sym,key->keycap);

        ImGui::SameLine();
    }
}

void KeymapsUIImpl::DoKeySymKeyboardLineParts(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const Keycap *line,const Keycap *m128_line) {
    BottomHalfKeycap keycaps[MAX_NUM_KEYCAPS];
    size_t num_keycaps=0;

    float y=ImGui::GetCursorPosY();

    this->DoKeySymKeyboardLineTopHalves(keymap,editable_keymap,line,keycaps,&num_keycaps);

    if(m128_line) {
        ImGui::SetCursorPosX(KEYPAD_X);

        this->DoKeySymKeyboardLineTopHalves(keymap,editable_keymap,m128_line,keycaps,&num_keycaps);
    }

    this->DoKeySymKeyboardLineBottomHalves(keymap,editable_keymap,y+KEY_HEIGHT*.5f,keycaps,num_keycaps);

    ImGui::SetCursorPosY(y);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void KeymapsUIImpl::DoKeyboardLine(const BeebKeymap *keymap,BeebKeymap *editable_keymap,const Keycap *line,const Keycap *m128_line) {
    if(keymap->IsKeySymMap()) {
        this->DoKeySymKeyboardLineParts(keymap,editable_keymap,line,m128_line);
    } else {
        this->DoScancodeKeyboardLineParts(keymap,editable_keymap,line,m128_line);
    }

    ImGui::NewLine();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool KeymapsUIImpl::DoEditKeymapGui(const BeebKeymap *keymap,BeebKeymap *editable_keymap) {
    ImGuiIDPusher id_pusher(keymap);

    bool is_default=false;
    if(keymap==BeebWindows::GetDefaultBeebKeymap()) {
        is_default=true;
    }

    bool is_current=false;
    if(keymap==m_current_keymap) {
        is_current=true;
    }

    std::string title=keymap->GetName().c_str();
    if(!editable_keymap) {
        title+=" ";
        title+=NOT_EDITABLE_ICON;
    }

    if(is_default) {
        title+=" ";
        title+=ICON_FA_CHECK;
    }

    title+=" ";
    if(keymap->IsKeySymMap()) {
        title+=KEYSYMS_KEYMAP_ICON;
    } else {
        title+=SCANCODES_KEYMAP_ICON;
    }

    if(ImGui::CollapsingHeader(title.c_str(),"header",true,is_current)) {
        if(editable_keymap) {
            std::string name;
            if(ImGuiInputText(&name,"Name",keymap->GetName())) {
                BeebWindows::SetBeebKeymapName(editable_keymap,name);
                m_edited=true;
            }
        } else {
            ImGui::TextUnformatted(keymap->GetName().c_str());
        }

        if(ImGui::Button("Copy")) {
            BeebWindows::AddBeebKeymap(*keymap);
            m_edited=true;
        }

        if(editable_keymap) {
            ImGui::SameLine();

            if(ImGui::Button("Delete")) {
                BeebWindows::RemoveBeebKeymap(editable_keymap);
                m_edited=true;
                return false;
            }
        }

        if(!is_current) {
            ImGui::SameLine();

            if(ImGui::Button("Select")) {
                m_current_keymap=keymap;
            }
        }

        if(!is_default) {
            ImGui::SameLine();

            if(ImGui::Button("Set default")) {
                BeebWindows::SetDefaultBeebKeymap(keymap);
                m_edited=true;
            }
        }

        {
            bool prefer=keymap->GetPreferShortcuts();
            if(ImGui::Checkbox("Prioritize command shortcuts",&prefer)) {
                if(editable_keymap) {
                    editable_keymap->SetPreferShortcuts(prefer);
                    m_edited=true;
                }
            }
        }

        this->DoKeyboardLine(keymap,editable_keymap,g_keyboard_line1,g_m128_line1);
        this->DoKeyboardLine(keymap,editable_keymap,g_keyboard_line2,g_m128_line2);
        this->DoKeyboardLine(keymap,editable_keymap,g_keyboard_line3,g_m128_line3);
        this->DoKeyboardLine(keymap,editable_keymap,g_keyboard_line4,g_m128_line4);
        this->DoKeyboardLine(keymap,editable_keymap,g_keyboard_line5,g_m128_line5);
        this->DoKeyboardLine(keymap,editable_keymap,g_keyboard_line6,nullptr);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct KeySymLabelsInitialiser {
    KeySymLabelsInitialiser() {
        g_keysym_labels[BeebKeySym_Break]="BRK";

        g_keysym_labels[BeebKeySym_None]=nullptr;
        g_keysym_labels[BeebKeySym_f0]="f0";
        g_keysym_labels[BeebKeySym_f1]="f1";
        g_keysym_labels[BeebKeySym_f2]="f2";
        g_keysym_labels[BeebKeySym_f3]="f3";
        g_keysym_labels[BeebKeySym_f4]="f4";
        g_keysym_labels[BeebKeySym_f5]="f5";
        g_keysym_labels[BeebKeySym_f6]="f6";
        g_keysym_labels[BeebKeySym_f7]="f7";
        g_keysym_labels[BeebKeySym_f8]="f8";
        g_keysym_labels[BeebKeySym_f9]="f9";
        g_keysym_labels[BeebKeySym_Escape]="ESC";
        g_keysym_labels[BeebKeySym_1]="1";
        g_keysym_labels[BeebKeySym_ExclamationMark]="!";
        g_keysym_labels[BeebKeySym_2]="2";
        g_keysym_labels[BeebKeySym_Quotes]="\"";
        g_keysym_labels[BeebKeySym_3]="3";
        g_keysym_labels[BeebKeySym_Hash]="#";
        g_keysym_labels[BeebKeySym_4]="4";
        g_keysym_labels[BeebKeySym_Dollar]="$";
        g_keysym_labels[BeebKeySym_5]="5";
        g_keysym_labels[BeebKeySym_Percent]="%";
        g_keysym_labels[BeebKeySym_6]="6";
        g_keysym_labels[BeebKeySym_Ampersand]="&";
        g_keysym_labels[BeebKeySym_7]="7";
        g_keysym_labels[BeebKeySym_Apostrophe]="'";
        g_keysym_labels[BeebKeySym_8]="8";
        g_keysym_labels[BeebKeySym_LeftBracket]="(";
        g_keysym_labels[BeebKeySym_9]="9";
        g_keysym_labels[BeebKeySym_RightBracket]=")";
        g_keysym_labels[BeebKeySym_0]="0";
        g_keysym_labels[BeebKeySym_Minus]="-";
        g_keysym_labels[BeebKeySym_Equals]="=";
        g_keysym_labels[BeebKeySym_Caret]="^";
        g_keysym_labels[BeebKeySym_Tilde]="~";
        g_keysym_labels[BeebKeySym_Backslash]="\\";
        g_keysym_labels[BeebKeySym_Pipe]="|";
        g_keysym_labels[BeebKeySym_Tab]="TAB";
        g_keysym_labels[BeebKeySym_Q]="Q";
        g_keysym_labels[BeebKeySym_W]="W";
        g_keysym_labels[BeebKeySym_E]="E";
        g_keysym_labels[BeebKeySym_R]="R";
        g_keysym_labels[BeebKeySym_T]="T";
        g_keysym_labels[BeebKeySym_Y]="Y";
        g_keysym_labels[BeebKeySym_U]="U";
        g_keysym_labels[BeebKeySym_I]="I";
        g_keysym_labels[BeebKeySym_O]="O";
        g_keysym_labels[BeebKeySym_P]="P";
        g_keysym_labels[BeebKeySym_At]="@";
        g_keysym_labels[BeebKeySym_LeftSquareBracket]="[";
        g_keysym_labels[BeebKeySym_LeftCurlyBracket]="{";
        g_keysym_labels[BeebKeySym_Underline]="_";
        g_keysym_labels[BeebKeySym_Pound]="\xc2\xa3";
        g_keysym_labels[BeebKeySym_CapsLock]="CAPS\nLOCK";
        g_keysym_labels[BeebKeySym_Ctrl]="CTRL";
        g_keysym_labels[BeebKeySym_A]="A";
        g_keysym_labels[BeebKeySym_S]="S";
        g_keysym_labels[BeebKeySym_D]="D";
        g_keysym_labels[BeebKeySym_F]="F";
        g_keysym_labels[BeebKeySym_G]="G";
        g_keysym_labels[BeebKeySym_H]="H";
        g_keysym_labels[BeebKeySym_J]="J";
        g_keysym_labels[BeebKeySym_K]="K";
        g_keysym_labels[BeebKeySym_L]="L";
        g_keysym_labels[BeebKeySym_Semicolon]=";";
        g_keysym_labels[BeebKeySym_Plus]="+";
        g_keysym_labels[BeebKeySym_Colon]=":";
        g_keysym_labels[BeebKeySym_Star]="*";
        g_keysym_labels[BeebKeySym_RightSquareBracket]="]";
        g_keysym_labels[BeebKeySym_RightCurlyBracket]="}";
        g_keysym_labels[BeebKeySym_Return]="RETURN";
        g_keysym_labels[BeebKeySym_ShiftLock]="SHF\nLOCK";
        g_keysym_labels[BeebKeySym_Shift]="SHIFT";
        g_keysym_labels[BeebKeySym_Z]="Z";
        g_keysym_labels[BeebKeySym_X]="X";
        g_keysym_labels[BeebKeySym_C]="C";
        g_keysym_labels[BeebKeySym_V]="V";
        g_keysym_labels[BeebKeySym_B]="B";
        g_keysym_labels[BeebKeySym_N]="N";
        g_keysym_labels[BeebKeySym_M]="M";
        g_keysym_labels[BeebKeySym_Comma]=",";
        g_keysym_labels[BeebKeySym_LessThan]="<";
        g_keysym_labels[BeebKeySym_Stop]=".";
        g_keysym_labels[BeebKeySym_GreaterThan]=">";
        g_keysym_labels[BeebKeySym_Slash]="/";
        g_keysym_labels[BeebKeySym_QuestionMarke]="?";
        g_keysym_labels[BeebKeySym_Delete]="DEL";
        g_keysym_labels[BeebKeySym_Copy]="COPY";
        g_keysym_labels[BeebKeySym_Up]=ICON_FA_LONG_ARROW_UP;
        g_keysym_labels[BeebKeySym_Down]=ICON_FA_LONG_ARROW_DOWN;
        g_keysym_labels[BeebKeySym_Left]=ICON_FA_LONG_ARROW_LEFT;
        g_keysym_labels[BeebKeySym_Right]=ICON_FA_LONG_ARROW_RIGHT;
        g_keysym_labels[BeebKeySym_KeypadPlus]="+";
        g_keysym_labels[BeebKeySym_KeypadMinus]="-";
        g_keysym_labels[BeebKeySym_KeypadSlash]="/";
        g_keysym_labels[BeebKeySym_KeypadStar]="*";
        g_keysym_labels[BeebKeySym_Keypad7]="7";
        g_keysym_labels[BeebKeySym_Keypad8]="8";
        g_keysym_labels[BeebKeySym_Keypad9]="9";
        g_keysym_labels[BeebKeySym_KeypadHash]="#";
        g_keysym_labels[BeebKeySym_Keypad4]="4";
        g_keysym_labels[BeebKeySym_Keypad5]="5";
        g_keysym_labels[BeebKeySym_Keypad6]="6";
        g_keysym_labels[BeebKeySym_KeypadDelete]="DEL";
        g_keysym_labels[BeebKeySym_Keypad1]="1";
        g_keysym_labels[BeebKeySym_Keypad2]="2";
        g_keysym_labels[BeebKeySym_Keypad3]="3";
        g_keysym_labels[BeebKeySym_KeypadComma]=",";
        g_keysym_labels[BeebKeySym_Keypad0]="0";
        g_keysym_labels[BeebKeySym_KeypadStop]=".";
        g_keysym_labels[BeebKeySym_KeypadReturn]="RETURN";
        g_keysym_labels[BeebKeySym_Space]="";
    }
};
static const KeySymLabelsInitialiser g_key_sym_labels_initialiser;
