#include <shared/system.h>
#include "keys.h"
#include <shared/debug.h>
#include <string.h>
#include <SDL.h>

#include <shared/enum_def.h>
#include "keys.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
static T GetKeyByName(const char *name, const char *(*get_name_fn)(T)) {
    for (int i = 0; i < 128; ++i) {
        if (const char *n = (*get_name_fn)(static_cast<T>(i))) {
            if (strcmp(n, name) == 0) {
                return static_cast<T>(i);
            }
        }
    }

    return (T)-1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *GetBeebKeyName(BeebKey beeb_key) {
    const char *name = GetBeebKeyEnumName(beeb_key);
    if (name[0] == '?') {
        return nullptr;
    } else {
        return name;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebKey GetBeebKeyByName(const char *name) {
    return GetKeyByName(name, &GetBeebKeyName);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *GetBeebKeySymName(BeebKeySym beeb_sym) {
    const char *name = GetBeebKeySymEnumName(beeb_sym);
    if (name[0] == '?') {
        return nullptr;
    } else {
        return name;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebKeySym GetBeebKeySymByName(const char *name) {
    return GetKeyByName(name, &GetBeebKeySymName);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct KeyCombo {
    BeebKey beeb_key;
    BeebShiftState shift_state;
};

// indexed by BeebKeySym
static KeyCombo g_key_combo_table[128];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetBeebKeyComboForKeySym(BeebKey *beeb_key, BeebShiftState *shift_state, BeebKeySym beeb_sym) {
    ASSERT(beeb_sym >= 0 && (int)beeb_sym < 128);

    const KeyCombo *combo = &g_key_combo_table[beeb_sym];

    if (combo->beeb_key < 0) {
        return false;
    }

    *beeb_key = combo->beeb_key;
    *shift_state = combo->shift_state;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t GetPCKeyModifiersFromSDLKeymod(uint16_t mod) {
    uint32_t modifiers = 0;

    if (mod & KMOD_SHIFT) {
        modifiers |= PCKeyModifier_Shift;
    }

    if (mod & KMOD_CTRL) {
        modifiers |= PCKeyModifier_Ctrl;
    }

    if (mod & KMOD_ALT) {
        modifiers |= PCKeyModifier_Alt;
    }

    if (mod & KMOD_GUI) {
        modifiers |= PCKeyModifier_Gui;
    }

    if (mod & KMOD_MODE) {
        modifiers |= PCKeyModifier_AltGr;
    }

    return modifiers;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetKeycodeName(uint32_t keycode) {
    std::string name;

    if (keycode == 0) {
        return std::string();
    }

    for (uint32_t mask = PCKeyModifier_Begin; mask != PCKeyModifier_End; mask <<= 1) {
        if (keycode & mask) {
            name += GetPCKeyModifierEnumName((int)mask);
            name += "-";
        }
    }

    name += SDL_GetKeyName((SDL_Keycode)(keycode & ~PCKeyModifier_All));

    return name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define K(NAME) (g_key_combo_table[BeebKeySym_##NAME] = {BeebKey_##NAME, BeebShiftState_Any})
#define K2(NAME, SHIFTED)                                                              \
    BEGIN_MACRO {                                                                      \
        g_key_combo_table[BeebKeySym_##NAME] = {BeebKey_##NAME, BeebShiftState_Off};   \
        g_key_combo_table[BeebKeySym_##SHIFTED] = {BeebKey_##NAME, BeebShiftState_On}; \
    }                                                                                  \
    END_MACRO

struct KeyComboTableInitialiser {
    KeyComboTableInitialiser() {
        for (size_t i = 0; i < 128; ++i) {
            g_key_combo_table[i] = {BeebKey_None, BeebShiftState_Any};
        }

        K(f0);
        K(f1);
        K(f2);
        K(f3);
        K(f4);
        K(f5);
        K(f6);
        K(f7);
        K(f8);
        K(f9);
        K(Escape);
        K2(1, ExclamationMark);
        K2(2, Quotes);
        K2(3, Hash);
        K2(4, Dollar);
        K2(5, Percent);
        K2(6, Ampersand);
        K2(7, Apostrophe);
        K2(8, LeftBracket);
        K2(9, RightBracket);
        K(0);
        K2(Minus, Equals);
        K2(Caret, Tilde);
        K2(Backslash, Pipe);
        K(Tab);
        K(Q);
        K(W);
        K(E);
        K(R);
        K(T);
        K(Y);
        K(U);
        K(I);
        K(O);
        K(P);
        K(At);
        K2(LeftSquareBracket, LeftCurlyBracket);
        K2(Underline, Pound);
        K(CapsLock);
        K(Ctrl);
        K(A);
        K(S);
        K(D);
        K(F);
        K(G);
        K(H);
        K(J);
        K(K);
        K(L);
        K2(Semicolon, Plus);
        K2(Colon, Star);
        K2(RightSquareBracket, RightCurlyBracket);
        K(Return);
        K(ShiftLock);
        K(Shift);
        K(Z);
        K(X);
        K(C);
        K(V);
        K(B);
        K(N);
        K(M);
        K2(Comma, LessThan);
        K2(Stop, GreaterThan);
        K2(Slash, QuestionMarke);
        K(Delete);
        K(Copy);
        K(Up);
        K(Down);
        K(Left);
        K(Right);
        K(KeypadPlus);
        K(KeypadMinus);
        K(KeypadSlash);
        K(KeypadStar);
        K(Keypad7);
        K(Keypad8);
        K(Keypad9);
        K(KeypadHash);
        K(Keypad4);
        K(Keypad5);
        K(Keypad6);
        K(KeypadDelete);
        K(Keypad1);
        K(Keypad2);
        K(Keypad3);
        K(KeypadComma);
        K(Keypad0);
        K(KeypadStop);
        K(KeypadReturn);
        K(Space);
        K(Break);
    }
};

#undef K2
#undef K

static const KeyComboTableInitialiser g_key_combo_table_initialiser;
