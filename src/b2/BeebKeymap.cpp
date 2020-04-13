#include <shared/system.h>
#include "BeebKeymap.h"
#include "keymap.h"
#include <SDL.h>
#include "keys.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define SHIFT(K) ((K)|PCKeyModifier_Shift)

static const BeebKeymap::Mapping g_keysym_common[]={
    {SDLK_F10,BeebKeySym_f0},
    {SDLK_F1,BeebKeySym_f1},
    {SDLK_F2,BeebKeySym_f2},
    {SDLK_F3,BeebKeySym_f3},
    {SDLK_F4,BeebKeySym_f4},
    {SDLK_F5,BeebKeySym_f5},
    {SDLK_F6,BeebKeySym_f6},
    {SDLK_F7,BeebKeySym_f7},
    {SDLK_F8,BeebKeySym_f8},
    {SDLK_F9,BeebKeySym_f9},
    {SDLK_ESCAPE,BeebKeySym_Escape},
    {SDLK_1,BeebKeySym_1},
    {SHIFT(SDLK_1),BeebKeySym_ExclamationMark},
    {SDLK_2,BeebKeySym_2},
    {SDLK_3,BeebKeySym_3},
    {SDLK_4,BeebKeySym_4},
    {SHIFT(SDLK_4),BeebKeySym_Dollar},
    {SDLK_5,BeebKeySym_5},
    {SHIFT(SDLK_5),BeebKeySym_Percent},
    {SDLK_6,BeebKeySym_6},
    {SHIFT(SDLK_7),BeebKeySym_Ampersand},
    {SDLK_7,BeebKeySym_7},
    {SDLK_QUOTE,BeebKeySym_Apostrophe},
    {SDLK_8,BeebKeySym_8},
    {SHIFT(SDLK_9),BeebKeySym_LeftBracket},
    {SDLK_9,BeebKeySym_9},
    {SHIFT(SDLK_0),BeebKeySym_RightBracket},
    {SDLK_0,BeebKeySym_0},
    {SDLK_MINUS,BeebKeySym_Minus},
    {SDLK_EQUALS,BeebKeySym_Equals},
    {SHIFT(SDLK_6),BeebKeySym_Caret},
    {SDLK_TAB,BeebKeySym_Tab},
    {SDLK_q,BeebKeySym_Q},
    {SDLK_w,BeebKeySym_W},
    {SDLK_e,BeebKeySym_E},
    {SDLK_r,BeebKeySym_R},
    {SDLK_t,BeebKeySym_T},
    {SDLK_y,BeebKeySym_Y},
    {SDLK_u,BeebKeySym_U},
    {SDLK_i,BeebKeySym_I},
    {SDLK_o,BeebKeySym_O},
    {SDLK_p,BeebKeySym_P},
    {SDLK_LEFTBRACKET,BeebKeySym_LeftSquareBracket},
    {SHIFT(SDLK_LEFTBRACKET),BeebKeySym_LeftCurlyBracket},
    {SHIFT(SDLK_MINUS),BeebKeySym_Underline},
    {SDLK_CAPSLOCK,BeebKeySym_CapsLock},
    {SDLK_LCTRL,BeebKeySym_Ctrl},{SDLK_RCTRL,BeebKeySym_Ctrl},
    {SDLK_a,BeebKeySym_A},
    {SDLK_s,BeebKeySym_S},
    {SDLK_d,BeebKeySym_D},
    {SDLK_f,BeebKeySym_F},
    {SDLK_g,BeebKeySym_G},
    {SDLK_h,BeebKeySym_H},
    {SDLK_j,BeebKeySym_J},
    {SDLK_k,BeebKeySym_K},
    {SDLK_l,BeebKeySym_L},
    {SDLK_SEMICOLON,BeebKeySym_Semicolon},
    {SHIFT(SDLK_EQUALS),BeebKeySym_Plus},
    {SHIFT(SDLK_SEMICOLON),BeebKeySym_Colon},
    {SHIFT(SDLK_8),BeebKeySym_Star},
    {SDLK_RIGHTBRACKET,BeebKeySym_RightSquareBracket},
    {SHIFT(SDLK_RIGHTBRACKET),BeebKeySym_RightCurlyBracket},
    {SDLK_RETURN,BeebKeySym_Return},
    //{SDLK_UNKNOWN,BeebKeySym_ShiftLock},//??
    {SDLK_LSHIFT,BeebKeySym_Shift},{SDLK_RSHIFT,BeebKeySym_Shift},
    {SDLK_z,BeebKeySym_Z},
    {SDLK_x,BeebKeySym_X},
    {SDLK_c,BeebKeySym_C},
    {SDLK_v,BeebKeySym_V},
    {SDLK_b,BeebKeySym_B},
    {SDLK_n,BeebKeySym_N},
    {SDLK_m,BeebKeySym_M},
    {SDLK_COMMA,BeebKeySym_Comma},
    {SHIFT(SDLK_COMMA),BeebKeySym_LessThan},
    {SDLK_PERIOD,BeebKeySym_Stop},
    {SHIFT(SDLK_PERIOD),BeebKeySym_GreaterThan},
    {SDLK_SLASH,BeebKeySym_Slash},
    {SHIFT(SDLK_SLASH),BeebKeySym_QuestionMarke},
    {SDLK_BACKSPACE,BeebKeySym_Delete},
    {SDLK_END,BeebKeySym_Copy},
    {SDLK_UP,BeebKeySym_Up},
    {SDLK_DOWN,BeebKeySym_Down},
    {SDLK_LEFT,BeebKeySym_Left},
    {SDLK_RIGHT,BeebKeySym_Right},
    {SDLK_KP_PLUS,BeebKeySym_KeypadPlus},
    {SDLK_KP_MINUS,BeebKeySym_KeypadMinus},
    {SDLK_KP_DIVIDE,BeebKeySym_KeypadSlash},
    {SDLK_KP_MULTIPLY,BeebKeySym_KeypadStar},
    {SDLK_KP_7,BeebKeySym_Keypad7},
    {SDLK_KP_8,BeebKeySym_Keypad8},
    {SDLK_KP_9,BeebKeySym_Keypad9},
    {SDLK_KP_HASH,BeebKeySym_KeypadHash},
    {SDLK_KP_4,BeebKeySym_Keypad4},
    {SDLK_KP_5,BeebKeySym_Keypad5},
    {SDLK_KP_6,BeebKeySym_Keypad6},
    {SDLK_KP_BACKSPACE,BeebKeySym_KeypadDelete},
    {SDLK_KP_1,BeebKeySym_Keypad1},
    {SDLK_KP_2,BeebKeySym_Keypad2},
    {SDLK_KP_3,BeebKeySym_Keypad3},
    {SDLK_KP_COMMA,BeebKeySym_KeypadComma},
    {SDLK_KP_0,BeebKeySym_Keypad0},
    {SDLK_KP_PERIOD,BeebKeySym_KeypadStop},
    {SDLK_KP_ENTER,BeebKeySym_KeypadReturn},
    {SDLK_SPACE,BeebKeySym_Space},
    {SDLK_F11,BeebKeySym_Break,},

    {}
};

// US-style
static const BeebKeymap::Mapping g_keysym_us[]={
    {SHIFT(SDLK_QUOTE),BeebKeySym_Quotes},
    {SHIFT(SDLK_3),BeebKeySym_Hash},
    {SHIFT(SDLK_BACKQUOTE),BeebKeySym_Tilde},
    {SDLK_BACKSLASH,BeebKeySym_Backslash},
    {SHIFT(SDLK_BACKSLASH),BeebKeySym_Pipe},
    {SHIFT(SDLK_2),BeebKeySym_At},
    {SDLK_BACKQUOTE,BeebKeySym_Pound},//` = ASCII 95, after all...
    {},
};

// UK-style
static const BeebKeymap::Mapping g_keysym_uk[]={
    {SHIFT(SDLK_2),BeebKeySym_Quotes},
    {'#',BeebKeySym_Hash},
    {SHIFT('#'),BeebKeySym_Tilde},
    {SDLK_BACKSLASH,BeebKeySym_Backslash},
    {SHIFT(SDLK_BACKSLASH),BeebKeySym_Pipe},
    {SHIFT(SDLK_QUOTE),BeebKeySym_At},
    {SHIFT(SDLK_3),BeebKeySym_Pound},
    {},
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BeebKeymap::Mapping g_scancode_common[]={
    {SDL_SCANCODE_SPACE,BeebKey_Space,},
    {SDL_SCANCODE_COMMA,BeebKey_Comma,},
    {SDL_SCANCODE_MINUS,BeebKey_Minus,},
    {SDL_SCANCODE_PERIOD,BeebKey_Stop,},
    {SDL_SCANCODE_SLASH,BeebKey_Slash,},
    {SDL_SCANCODE_0,BeebKey_0,},
    {SDL_SCANCODE_1,BeebKey_1,},
    {SDL_SCANCODE_2,BeebKey_2,},
    {SDL_SCANCODE_3,BeebKey_3,},
    {SDL_SCANCODE_4,BeebKey_4,},
    {SDL_SCANCODE_5,BeebKey_5,},
    {SDL_SCANCODE_6,BeebKey_6,},
    {SDL_SCANCODE_7,BeebKey_7,},
    {SDL_SCANCODE_8,BeebKey_8,},
    {SDL_SCANCODE_9,BeebKey_9,},
    {SDL_SCANCODE_APOSTROPHE,BeebKey_Colon,},
    {SDL_SCANCODE_SEMICOLON,BeebKey_Semicolon,},
    {SDL_SCANCODE_HOME,BeebKey_At,},
    {SDL_SCANCODE_NONUSBACKSLASH,BeebKey_At,},
    {SDL_SCANCODE_A,BeebKey_A,},
    {SDL_SCANCODE_B,BeebKey_B,},
    {SDL_SCANCODE_C,BeebKey_C,},
    {SDL_SCANCODE_D,BeebKey_D,},
    {SDL_SCANCODE_E,BeebKey_E,},
    {SDL_SCANCODE_F,BeebKey_F,},
    {SDL_SCANCODE_G,BeebKey_G,},
    {SDL_SCANCODE_H,BeebKey_H,},
    {SDL_SCANCODE_I,BeebKey_I,},
    {SDL_SCANCODE_J,BeebKey_J,},
    {SDL_SCANCODE_K,BeebKey_K,},
    {SDL_SCANCODE_L,BeebKey_L,},
    {SDL_SCANCODE_M,BeebKey_M,},
    {SDL_SCANCODE_N,BeebKey_N,},
    {SDL_SCANCODE_O,BeebKey_O,},
    {SDL_SCANCODE_P,BeebKey_P,},
    {SDL_SCANCODE_Q,BeebKey_Q,},
    {SDL_SCANCODE_R,BeebKey_R,},
    {SDL_SCANCODE_S,BeebKey_S,},
    {SDL_SCANCODE_T,BeebKey_T,},
    {SDL_SCANCODE_U,BeebKey_U,},
    {SDL_SCANCODE_V,BeebKey_V,},
    {SDL_SCANCODE_W,BeebKey_W,},
    {SDL_SCANCODE_X,BeebKey_X,},
    {SDL_SCANCODE_Y,BeebKey_Y,},
    {SDL_SCANCODE_Z,BeebKey_Z,},
    {SDL_SCANCODE_LEFTBRACKET,BeebKey_LeftSquareBracket,},
    {SDL_SCANCODE_NONUSBACKSLASH,BeebKey_Backslash,},
    {SDL_SCANCODE_BACKSLASH,BeebKey_Backslash,},
    {SDL_SCANCODE_RIGHTBRACKET,BeebKey_RightSquareBracket,},
    {SDL_SCANCODE_GRAVE,BeebKey_Caret,},
    {SDL_SCANCODE_EQUALS,BeebKey_Underline,},
    {SDL_SCANCODE_ESCAPE,BeebKey_Escape,},
    {SDL_SCANCODE_TAB,BeebKey_Tab,},
    {SDL_SCANCODE_RCTRL,BeebKey_Ctrl,},
    {SDL_SCANCODE_SCROLLLOCK,BeebKey_ShiftLock,},
    {SDL_SCANCODE_LSHIFT,BeebKey_Shift,},
    {SDL_SCANCODE_RSHIFT,BeebKey_Shift,},
    {SDL_SCANCODE_BACKSPACE,BeebKey_Delete,},
    {SDL_SCANCODE_END,BeebKey_Copy,},
    {SDL_SCANCODE_RETURN,BeebKey_Return,},
    {SDL_SCANCODE_UP,BeebKey_Up,},
    {SDL_SCANCODE_DOWN,BeebKey_Down,},
    {SDL_SCANCODE_LEFT,BeebKey_Left,},
    {SDL_SCANCODE_RIGHT,BeebKey_Right,},
    {SDL_SCANCODE_F10,BeebKey_f0,},
    {SDL_SCANCODE_F1,BeebKey_f1,},
    {SDL_SCANCODE_F2,BeebKey_f2,},
    {SDL_SCANCODE_F3,BeebKey_f3,},
    {SDL_SCANCODE_F4,BeebKey_f4,},
    {SDL_SCANCODE_F5,BeebKey_f5,},
    {SDL_SCANCODE_F6,BeebKey_f6,},
    {SDL_SCANCODE_F7,BeebKey_f7,},
    {SDL_SCANCODE_F8,BeebKey_f8,},
    {SDL_SCANCODE_F9,BeebKey_f9,},
    {SDL_SCANCODE_F11,BeebKey_Break,},

    {SDL_SCANCODE_KP_0,BeebKey_Keypad0,},
    {SDL_SCANCODE_KP_1,BeebKey_Keypad1,},
    {SDL_SCANCODE_KP_2,BeebKey_Keypad2,},
    {SDL_SCANCODE_KP_3,BeebKey_Keypad3,},
    {SDL_SCANCODE_KP_4,BeebKey_Keypad4,},
    {SDL_SCANCODE_KP_5,BeebKey_Keypad5,},
    {SDL_SCANCODE_KP_6,BeebKey_Keypad6,},
    {SDL_SCANCODE_KP_7,BeebKey_Keypad7,},
    {SDL_SCANCODE_KP_8,BeebKey_Keypad8,},
    {SDL_SCANCODE_KP_9,BeebKey_Keypad9,},
    {SDL_SCANCODE_KP_PERIOD,BeebKey_KeypadStop,},
    {SDL_SCANCODE_KP_ENTER,BeebKey_KeypadReturn},
    {SDL_SCANCODE_KP_PLUS,BeebKey_KeypadPlus},
    {SDL_SCANCODE_KP_MINUS,BeebKey_KeypadMinus},
    {SDL_SCANCODE_KP_DIVIDE,BeebKey_KeypadSlash},
    {SDL_SCANCODE_KP_MULTIPLY,BeebKey_KeypadStar},

    {},
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BeebKeymap::Mapping g_scancode_cc[]={
    {SDL_SCANCODE_CAPSLOCK,BeebKey_Ctrl,},
    {SDL_SCANCODE_LCTRL,BeebKey_CapsLock,},
    {SDL_SCANCODE_LALT,BeebKey_Ctrl,},
    {},
};

static const BeebKeymap::Mapping g_scancode_noncc[]={
    {SDL_SCANCODE_CAPSLOCK,BeebKey_CapsLock,},
    {SDL_SCANCODE_LCTRL,BeebKey_Ctrl,},
    {},
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebKeymap::BeebKeymap(std::string name,bool is_key_sym_map):
Keymap<BeebKeymapTraits>(std::move(name)),
m_is_key_sym_map(is_key_sym_map)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebKeymap::IsKeySymMap() const {
    return m_is_key_sym_map;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebKeymap::GetPreferShortcuts() const {
    return m_prefer_shortcuts;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebKeymap::SetPreferShortcuts(bool prefer_shortcuts) {
    m_prefer_shortcuts=prefer_shortcuts;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BeebKeymap CreateDefaultBeebKeymap(const char *name,
                                          bool is_key_sym_map,
                                          const std::initializer_list<const BeebKeymap::Mapping *> &list)
{
    BeebKeymap keymap(name,is_key_sym_map);

    for(const BeebKeymap::Mapping *mappings:list) {
        for(const BeebKeymap::Mapping *mapping=mappings;mapping->pc_key!=0;++mapping) {
            keymap.SetMapping(mapping->pc_key,mapping->value,true);
        }
    }

    return keymap;
}

const BeebKeymap DEFAULT_KEYMAP=CreateDefaultBeebKeymap("Default",
                                                        false,
                                                        {g_scancode_common,g_scancode_noncc});

const BeebKeymap DEFAULT_KEYMAP_CC=CreateDefaultBeebKeymap("Default (caps/ctrl)",
                                                           false,
                                                           {g_scancode_common,g_scancode_cc});

const BeebKeymap DEFAULT_KEYMAP_UK=CreateDefaultBeebKeymap("Default UK",
                                                           true,
                                                           {g_keysym_common,g_keysym_uk});

const BeebKeymap DEFAULT_KEYMAP_US=CreateDefaultBeebKeymap("Default US",
                                                           true,
                                                           {g_keysym_common,g_keysym_us});
