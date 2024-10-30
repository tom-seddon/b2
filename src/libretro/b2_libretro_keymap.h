#ifndef KEYMAP_H__
#define KEYMAP_H__
#include "libretro.h"
const std::map<unsigned, BeebKey> beeb_libretro_keymap = {
// Straightforward mappings: letters and numbers
    {RETROK_a,        BeebKey_A},
    {RETROK_b,        BeebKey_B},
    {RETROK_c,        BeebKey_C},
    {RETROK_d,        BeebKey_D},
    {RETROK_e,        BeebKey_E},
    {RETROK_f,        BeebKey_F},
    {RETROK_g,        BeebKey_G},
    {RETROK_h,        BeebKey_H},
    {RETROK_i,        BeebKey_I},
    {RETROK_j,        BeebKey_J},
    {RETROK_k,        BeebKey_K},
    {RETROK_l,        BeebKey_L},
    {RETROK_m,        BeebKey_M},
    {RETROK_n,        BeebKey_N},
    {RETROK_o,        BeebKey_O},
    {RETROK_p,        BeebKey_P},
    {RETROK_q,        BeebKey_Q},
    {RETROK_r,        BeebKey_R},
    {RETROK_s,        BeebKey_S},
    {RETROK_t,        BeebKey_T},
    {RETROK_u,        BeebKey_U},
    {RETROK_v,        BeebKey_V},
    {RETROK_w,        BeebKey_W},
    {RETROK_x,        BeebKey_X},
    {RETROK_y,        BeebKey_Y},
    {RETROK_z,        BeebKey_Z},
    {RETROK_0,        BeebKey_0},
    {RETROK_1,        BeebKey_1},
    {RETROK_2,        BeebKey_2},
    {RETROK_3,        BeebKey_3},
    {RETROK_4,        BeebKey_4},
    {RETROK_5,        BeebKey_5},
    {RETROK_6,        BeebKey_6},
    {RETROK_7,        BeebKey_7},
    {RETROK_8,        BeebKey_8},
    {RETROK_9,        BeebKey_9},
// F-row
    {RETROK_F10,      BeebKey_f0},        // logical mapping: F0 -- PC keyboard F10
    {RETROK_F1,       BeebKey_f1},
    {RETROK_F2,       BeebKey_f2},
    {RETROK_F3,       BeebKey_f3},
    {RETROK_F4,       BeebKey_f4},
    {RETROK_F5,       BeebKey_f5},
    {RETROK_F6,       BeebKey_f6},
    {RETROK_F7,       BeebKey_f7},
    {RETROK_F8,       BeebKey_f8},
    {RETROK_F9,       BeebKey_f9},
    {RETROK_F11,      BeebKey_Break},     // positional mapping: Break  -- PC keyboard F11
    {RETROK_PAUSE,    BeebKey_Break},     // logical mapping:    Break  -- PC keyboard Pause/Break
// Row 1
    {RETROK_ESCAPE,   BeebKey_Escape},    // positional mapping: Escape -- PC keyboard backquote
    {RETROK_BACKQUOTE,BeebKey_Escape},    // logical mapping: Escape    -- PC keyboard Esc
    {RETROK_MINUS,    BeebKey_Minus},
    {RETROK_EQUALS,   BeebKey_Caret},     // positional mapping: caret  -- PC keyboard =
    {RETROK_HOME,     BeebKey_Backslash}, // semi-positional mapping: backslash -- PC keyboard Home
    {RETROK_OEM_102,  BeebKey_Backslash}, // logical mapping: backslash -- PC keyboard 102. button (next to left Shift)
    {RETROK_LEFT,     BeebKey_Left},      // logical mapping: left      -- PC keyboard cursor left
    {RETROK_RIGHT,    BeebKey_Right},     // logical mapping: right     -- PC keyboard cursor right
// Row 2
    {RETROK_TAB,      BeebKey_Tab},
    {RETROK_LEFTBRACKET,BeebKey_At},      // positional mapping: @  -- PC keyboard [
    {RETROK_RIGHTBRACKET,
              BeebKey_LeftSquareBracket}, // positional mapping: [{ -- PC keyboard ]
    {RETROK_END,      BeebKey_Underline}, // semi-positional mapping: underline -- PC keyboard End
    {RETROK_UP,       BeebKey_Up},        // logical mapping: up    -- PC keyboard cursor up
    {RETROK_DOWN,     BeebKey_Down},      // logical mapping: down  -- PC keyboard cursor down
// Row 3
    {RETROK_CAPSLOCK, BeebKey_CapsLock},
    {RETROK_PAGEUP,   BeebKey_CapsLock},  // extra mapping: caps lock -- PC keyboard PgUp
    {RETROK_LCTRL,    BeebKey_Ctrl},      // logical mapping: Ctrl    -- PC keyboard left Ctrl
    {RETROK_RCTRL,    BeebKey_Ctrl},      // logical mapping: Ctrl    -- PC keyboard right Ctrl
    {RETROK_SEMICOLON,BeebKey_Semicolon},
    {RETROK_QUOTE,    BeebKey_Colon},     // positional mapping: :    -- PC keyboard '
    {RETROK_BACKSLASH,
             BeebKey_RightSquareBracket}, // positional mapping: ]}   -- PC keyboard \| (at least in ISO layout)
    {RETROK_RETURN,   BeebKey_Return},
// Row 4
    {RETROK_NUMLOCK,  BeebKey_ShiftLock}, // logical mapping (sort of): shift lock -- PC keyboard Num Lock
    {RETROK_PAGEDOWN, BeebKey_ShiftLock}, // extra mapping: shift lock -- PC keyboard PgDn
    {RETROK_LSHIFT,   BeebKey_Shift},     // note that Shift keys can not be distinguished in Beeb keyboard matrix
    {RETROK_COMMA,    BeebKey_Comma},
    {RETROK_PERIOD,   BeebKey_Stop},
    {RETROK_SLASH,    BeebKey_Slash},
    {RETROK_RSHIFT,   BeebKey_Shift},     // logical mapping: Shift  -- PC keyboard right Shift
    {RETROK_BACKSPACE,BeebKey_Delete},    // logical mapping: Delete -- PC keyboard Backspace
    {RETROK_DELETE,   BeebKey_Delete},    // logical mapping: Delete -- PC keyboard Delete
    {RETROK_INSERT,   BeebKey_Copy},      // logical mapping: Copy   -- PC keyboard Insert
// Row 5
    {RETROK_SPACE,    BeebKey_Space},
// BBC Master keypad
    {RETROK_KP0,      BeebKey_Keypad0},
    {RETROK_KP1,      BeebKey_Keypad1},
    {RETROK_KP2,      BeebKey_Keypad2},
    {RETROK_KP3,      BeebKey_Keypad3},
    {RETROK_KP4,      BeebKey_Keypad4},
    {RETROK_KP5,      BeebKey_Keypad5},
    {RETROK_KP6,      BeebKey_Keypad6},
    {RETROK_KP7,      BeebKey_Keypad7},
    {RETROK_KP8,      BeebKey_Keypad8},
    {RETROK_KP9,      BeebKey_Keypad9},
    {RETROK_KP_PERIOD,BeebKey_KeypadStop},
    {RETROK_KP_ENTER, BeebKey_KeypadReturn},
    {RETROK_KP_PLUS,  BeebKey_KeypadPlus},
    {RETROK_KP_MINUS, BeebKey_KeypadMinus},
    {RETROK_KP_DIVIDE,BeebKey_KeypadSlash},
    {RETROK_KP_MULTIPLY,BeebKey_KeypadStar},
// TODO: map the 3 extra keypad keys on the Master somewhere (delete, ', #)
};

const std::map<std::string, BeebKey> joypad_keymap = {
// None is added separately in the code, since it should be the first and default
//    {"None",     BeebKey_None},
    {"a",        BeebKey_A},
    {"b",        BeebKey_B},
    {"c",        BeebKey_C},
    {"d",        BeebKey_D},
    {"e",        BeebKey_E},
    {"f",        BeebKey_F},
    {"g",        BeebKey_G},
    {"h",        BeebKey_H},
    {"i",        BeebKey_I},
    {"j",        BeebKey_J},
    {"k",        BeebKey_K},
    {"l",        BeebKey_L},
    {"m",        BeebKey_M},
    {"n",        BeebKey_N},
    {"o",        BeebKey_O},
    {"p",        BeebKey_P},
    {"q",        BeebKey_Q},
    {"r",        BeebKey_R},
    {"s",        BeebKey_S},
    {"t",        BeebKey_T},
    {"u",        BeebKey_U},
    {"v",        BeebKey_V},
    {"w",        BeebKey_W},
    {"x",        BeebKey_X},
    {"y",        BeebKey_Y},
    {"z",        BeebKey_Z},
    {"Space",    BeebKey_Space},
    {"0",        BeebKey_0},
    {"1",        BeebKey_1},
    {"2",        BeebKey_2},
    {"3",        BeebKey_3},
    {"4",        BeebKey_4},
    {"5",        BeebKey_5},
    {"6",        BeebKey_6},
    {"7",        BeebKey_7},
    {"8",        BeebKey_8},
    {"9",        BeebKey_9},
    {"Cursor up",BeebKey_Up},
    {"Cursor down",
                 BeebKey_Down},
    {"Cursor left",
                 BeebKey_Left},
    {"Cursor right",
                 BeebKey_Right},
    {"; (semicolon)",
                 BeebKey_Semicolon},
    {": (colon)", BeebKey_Colon},
    {"]",        BeebKey_RightSquareBracket},
    {", (comma)",BeebKey_Comma},
    {". (dot)",  BeebKey_Stop},
    {"/",        BeebKey_Slash},
    {"F0",       BeebKey_f0},
    {"F1",       BeebKey_f1},
    {"F2",       BeebKey_f2},
    {"F3",       BeebKey_f3},
    {"F4",       BeebKey_f4},
    {"F5",       BeebKey_f5},
    {"F6",       BeebKey_f6},
    {"F7",       BeebKey_f7},
    {"F8",       BeebKey_f8},
    {"F9",       BeebKey_f9},
    {"Break",    BeebKey_Break},
    {"Escape",   BeebKey_Escape},
    {"- (minus)",BeebKey_Minus},
    {"=",        BeebKey_Caret},
    {"\\ (backslash)",
                 BeebKey_Backslash},
    {"Tab",      BeebKey_Tab},
    {"@",        BeebKey_At},
    {"[",        BeebKey_LeftSquareBracket},
    {"_ (underline)",
                 BeebKey_Underline},
    {"Ctrl",     BeebKey_Ctrl},
    {"Return",   BeebKey_Return},
    {"Shift",    BeebKey_Shift},
    {"Caps Lock",BeebKey_CapsLock},
    {"Shift Lock",BeebKey_ShiftLock},
    {"Delete",   BeebKey_Delete},
    {"Copy",     BeebKey_Copy},
// Keypad not added until BBC Master support is implemented/tested
};

const std::map<std::string, unsigned> joypad_buttonmap = {
    {"Up",    RETRO_DEVICE_ID_JOYPAD_UP},
    {"Down",  RETRO_DEVICE_ID_JOYPAD_DOWN},
    {"Left",  RETRO_DEVICE_ID_JOYPAD_LEFT},
    {"Right", RETRO_DEVICE_ID_JOYPAD_RIGHT},
    {"A",     RETRO_DEVICE_ID_JOYPAD_A},
    {"B",     RETRO_DEVICE_ID_JOYPAD_B},
    {"X",     RETRO_DEVICE_ID_JOYPAD_X},
    {"Y",     RETRO_DEVICE_ID_JOYPAD_Y},
    {"Select",RETRO_DEVICE_ID_JOYPAD_SELECT},
    {"Start", RETRO_DEVICE_ID_JOYPAD_START},
    {"L",     RETRO_DEVICE_ID_JOYPAD_L},
    {"R",     RETRO_DEVICE_ID_JOYPAD_R},
    {"L2",    RETRO_DEVICE_ID_JOYPAD_L2},
    {"R2",    RETRO_DEVICE_ID_JOYPAD_R2},
    {"L3",    RETRO_DEVICE_ID_JOYPAD_L3},
    {"R3",    RETRO_DEVICE_ID_JOYPAD_R3},
};

#endif