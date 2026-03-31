#include <shared/system.h>
#include "keys.h"
#include <shared/debug.h>
#include <string.h>
#include <SDL.h>
#include <beeb/type.h>

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

static KeySymKeyCombos g_key_sym_key_combos[128];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const KeySymKeyCombos *GetKeySymKeyCombosForKeySym(BeebKeySym beeb_sym) {
    ASSERT(beeb_sym >= 0 && (int)beeb_sym < 128);

    return &g_key_sym_key_combos[beeb_sym];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const KeyCombo *GetKeyComboForType(const KeySymKeyCombos *combos, BBCMicroTypeID type_id) {
    switch (type_id) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case BBCMicroTypeID_B:
    case BBCMicroTypeID_BPlus:
    case BBCMicroTypeID_Master:
        return &combos->bbc;

    case BBCMicroTypeID_MasterCompact:
        return &combos->compact;

    case BBCMicroTypeID_Electron:
        return &combos->electron;
    }
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
            name += GetPCKeyModifierEnumName(mask);
            name += "-";
        }
    }

    name += SDL_GetKeyName((SDL_Keycode)(keycode & ~PCKeyModifier_All));

    return name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SetKey(KeyCombo KeySymKeyCombos::*mptr, BeebKeySym sym, BeebKey key, BeebMetaKeyState shift_state, BeebMetaKeyState ctrl_state, BeebMetaKeyState func_state) {
    if (sym >= 0) {
        KeyCombo *c = &(g_key_sym_key_combos[sym].*mptr);

        c->key = key;
        c->shift_state = shift_state;
        c->ctrl_state = ctrl_state;
        c->func_state = func_state;
    }
}

static void SetSinglePurposeKey(KeyCombo KeySymKeyCombos::*mptr, BeebKeySym sym, BeebKey key) {
    ASSERT(sym >= 0);
    SetKey(mptr, sym, key, BeebMetaKeyState_Any, BeebMetaKeyState_Any, BeebMetaKeyState_Any);
}

static void SetMultiPurposeKey(KeyCombo KeySymKeyCombos::*mptr, BeebKeySym unshifted_sym, BeebKeySym shifted_sym, BeebKeySym ctrled_sym, BeebKeySym funced_sym, BeebKey key) {
    ASSERT(unshifted_sym >= 0);
    ASSERT(shifted_sym >= 0 || ctrled_sym >= 0 || funced_sym >= 0);
    SetKey(mptr, unshifted_sym, key, BeebMetaKeyState_Off, BeebMetaKeyState_Off, BeebMetaKeyState_Off);
    SetKey(mptr, shifted_sym, key, BeebMetaKeyState_On, BeebMetaKeyState_Off, BeebMetaKeyState_Off);

    // CTRL+ and FUNC+ on Electron don't care about the Shift state.
    SetKey(mptr, ctrled_sym, key, BeebMetaKeyState_Any, BeebMetaKeyState_On, BeebMetaKeyState_Any);
    SetKey(mptr, funced_sym, key, BeebMetaKeyState_Any, BeebMetaKeyState_Any, BeebMetaKeyState_On);
}

#define SET_ORDINARY_BBC_KEY(NAME) (SetSinglePurposeKey(&KeySymKeyCombos::bbc, BeebKeySym_##NAME, BeebKey_##NAME))
#define SET_SHIFTABLE_BBC_KEY(NAME, SHIFTED) (SetMultiPurposeKey(&KeySymKeyCombos::bbc, BeebKeySym_##NAME, BeebKeySym_##SHIFTED, BeebKeySym_None, BeebKeySym_None, BeebKey_##NAME))

#define SET_ORDINARY_ELECTRON_KEY(NAME) (SetSinglePurposeKey(&KeySymKeyCombos::electron, BeebKeySym_##NAME, BeebKey_##NAME))
#define SET_SHIFTABLE_ELECTRON_KEY(NAME, SHIFTED) (SetMultiPurposeKey(&KeySymKeyCombos::electron, BeebKeySym_##NAME, BeebKeySym_##SHIFTED, BeebKeySym_None, BeebKeySym_None, BeebKey_##NAME))
#define SET_SHIFTABLE_CTRLABLE_ELECTRON_KEY(NAME, SHIFTED, CTRLED) (SetMultiPurposeKey(&KeySymKeyCombos::electron, BeebKeySym_##NAME, BeebKeySym_##SHIFTED, BeebKeySym_##CTRLED, BeebKeySym_None, BeebKey_##NAME))
#define SET_SHIFTABLE_FUNCABLE_ELECTRON_KEY(NAME, SHIFTED, FUNCED) (SetMultiPurposeKey(&KeySymKeyCombos::electron, BeebKeySym_##NAME, BeebKeySym_##SHIFTED, BeebKeySym_None, BeebKeySym_##FUNCED, BeebKey_##NAME))

struct KeyComboTableInitialiser {
    KeyComboTableInitialiser() {
        SET_ORDINARY_BBC_KEY(f0);
        SET_ORDINARY_BBC_KEY(f1);
        SET_ORDINARY_BBC_KEY(f2);
        SET_ORDINARY_BBC_KEY(f3);
        SET_ORDINARY_BBC_KEY(f4);
        SET_ORDINARY_BBC_KEY(f5);
        SET_ORDINARY_BBC_KEY(f6);
        SET_ORDINARY_BBC_KEY(f7);
        SET_ORDINARY_BBC_KEY(f8);
        SET_ORDINARY_BBC_KEY(f9);
        SET_ORDINARY_BBC_KEY(Escape);
        SET_SHIFTABLE_BBC_KEY(1, ExclamationMark);
        SET_SHIFTABLE_BBC_KEY(2, Quotes);
        SET_SHIFTABLE_BBC_KEY(3, Hash);
        SET_SHIFTABLE_BBC_KEY(4, Dollar);
        SET_SHIFTABLE_BBC_KEY(5, Percent);
        SET_SHIFTABLE_BBC_KEY(6, Ampersand);
        SET_SHIFTABLE_BBC_KEY(7, Apostrophe);
        SET_SHIFTABLE_BBC_KEY(8, LeftBracket);
        SET_SHIFTABLE_BBC_KEY(9, RightBracket);
        SET_ORDINARY_BBC_KEY(0);
        SET_SHIFTABLE_BBC_KEY(Minus, Equals);
        SET_SHIFTABLE_BBC_KEY(Caret, Tilde);
        SET_SHIFTABLE_BBC_KEY(Backslash, Pipe);
        SET_ORDINARY_BBC_KEY(Tab);
        SET_ORDINARY_BBC_KEY(Q);
        SET_ORDINARY_BBC_KEY(W);
        SET_ORDINARY_BBC_KEY(E);
        SET_ORDINARY_BBC_KEY(R);
        SET_ORDINARY_BBC_KEY(T);
        SET_ORDINARY_BBC_KEY(Y);
        SET_ORDINARY_BBC_KEY(U);
        SET_ORDINARY_BBC_KEY(I);
        SET_ORDINARY_BBC_KEY(O);
        SET_ORDINARY_BBC_KEY(P);
        SET_ORDINARY_BBC_KEY(At);
        SET_SHIFTABLE_BBC_KEY(LeftSquareBracket, LeftCurlyBracket);
        SET_SHIFTABLE_BBC_KEY(Underline, Pound);
        SET_ORDINARY_BBC_KEY(CapsLock);
        SET_ORDINARY_BBC_KEY(Ctrl);
        SET_ORDINARY_BBC_KEY(A);
        SET_ORDINARY_BBC_KEY(S);
        SET_ORDINARY_BBC_KEY(D);
        SET_ORDINARY_BBC_KEY(F);
        SET_ORDINARY_BBC_KEY(G);
        SET_ORDINARY_BBC_KEY(H);
        SET_ORDINARY_BBC_KEY(J);
        SET_ORDINARY_BBC_KEY(K);
        SET_ORDINARY_BBC_KEY(L);
        SET_SHIFTABLE_BBC_KEY(Semicolon, Plus);
        SET_SHIFTABLE_BBC_KEY(Colon, Star);
        SET_SHIFTABLE_BBC_KEY(RightSquareBracket, RightCurlyBracket);
        SET_ORDINARY_BBC_KEY(Return);
        SET_ORDINARY_BBC_KEY(ShiftLock);
        SET_ORDINARY_BBC_KEY(Shift);
        SET_ORDINARY_BBC_KEY(Z);
        SET_ORDINARY_BBC_KEY(X);
        SET_ORDINARY_BBC_KEY(C);
        SET_ORDINARY_BBC_KEY(V);
        SET_ORDINARY_BBC_KEY(B);
        SET_ORDINARY_BBC_KEY(N);
        SET_ORDINARY_BBC_KEY(M);
        SET_SHIFTABLE_BBC_KEY(Comma, LessThan);
        SET_SHIFTABLE_BBC_KEY(Stop, GreaterThan);
        SET_SHIFTABLE_BBC_KEY(Slash, QuestionMarke);
        SET_ORDINARY_BBC_KEY(Delete);
        SET_ORDINARY_BBC_KEY(Copy);
        SET_ORDINARY_BBC_KEY(Up);
        SET_ORDINARY_BBC_KEY(Down);
        SET_ORDINARY_BBC_KEY(Left);
        SET_ORDINARY_BBC_KEY(Right);
        SET_ORDINARY_BBC_KEY(KeypadPlus);
        SET_ORDINARY_BBC_KEY(KeypadMinus);
        SET_ORDINARY_BBC_KEY(KeypadSlash);
        SET_ORDINARY_BBC_KEY(KeypadStar);
        SET_ORDINARY_BBC_KEY(Keypad7);
        SET_ORDINARY_BBC_KEY(Keypad8);
        SET_ORDINARY_BBC_KEY(Keypad9);
        SET_ORDINARY_BBC_KEY(KeypadHash);
        SET_ORDINARY_BBC_KEY(Keypad4);
        SET_ORDINARY_BBC_KEY(Keypad5);
        SET_ORDINARY_BBC_KEY(Keypad6);
        SET_ORDINARY_BBC_KEY(KeypadDelete);
        SET_ORDINARY_BBC_KEY(Keypad1);
        SET_ORDINARY_BBC_KEY(Keypad2);
        SET_ORDINARY_BBC_KEY(Keypad3);
        SET_ORDINARY_BBC_KEY(KeypadComma);
        SET_ORDINARY_BBC_KEY(Keypad0);
        SET_ORDINARY_BBC_KEY(KeypadStop);
        SET_ORDINARY_BBC_KEY(KeypadReturn);
        SET_ORDINARY_BBC_KEY(Space);
        SET_ORDINARY_BBC_KEY(Break);

        // The Master Compact is same as the Master 128, plus two spot hacks.
        for (int i = 0; i < 128; ++i) {
            g_key_sym_key_combos[i].compact = g_key_sym_key_combos[i].bbc;
        }

        SetSinglePurposeKey(&KeySymKeyCombos::compact, BeebKeySym_CompactSpecialKey, BeebKey_At);
        SetMultiPurposeKey(&KeySymKeyCombos::compact, BeebKeySym_0, BeebKeySym_At, BeebKeySym_None, BeebKeySym_None, BeebKey_0);

        // The Electron is its own special thing.
        SET_ORDINARY_ELECTRON_KEY(Escape);
        SET_SHIFTABLE_FUNCABLE_ELECTRON_KEY(1, ExclamationMark, 1);
        SET_SHIFTABLE_FUNCABLE_ELECTRON_KEY(2, Quotes, 2);
        SET_SHIFTABLE_FUNCABLE_ELECTRON_KEY(3, Hash, 3);
        SET_SHIFTABLE_FUNCABLE_ELECTRON_KEY(4, Dollar, 4);
        SET_SHIFTABLE_FUNCABLE_ELECTRON_KEY(5, Percent, 5);
        SET_SHIFTABLE_FUNCABLE_ELECTRON_KEY(6, Ampersand, 6);
        SET_SHIFTABLE_FUNCABLE_ELECTRON_KEY(7, Apostrophe, 7);
        SET_SHIFTABLE_FUNCABLE_ELECTRON_KEY(8, LeftBracket, 8);
        SET_SHIFTABLE_FUNCABLE_ELECTRON_KEY(9, RightBracket, 9);
        SET_SHIFTABLE_FUNCABLE_ELECTRON_KEY(0, At, 0);
        SET_SHIFTABLE_ELECTRON_KEY(Minus, Equals);
        SET_SHIFTABLE_CTRLABLE_ELECTRON_KEY(Left, Caret, Tilde);
        SET_SHIFTABLE_CTRLABLE_ELECTRON_KEY(Right, Pipe, Backslash);
        SET_ORDINARY_ELECTRON_KEY(Break);

        // Caps Lock is an oddity, as Func is not a keysym key.
        SetKey(&KeySymKeyCombos::electron, BeebKeySym_CapsLock, BeebKey_CapsLock, BeebMetaKeyState_On, BeebMetaKeyState_Any, BeebMetaKeyState_Any);
        SET_ORDINARY_ELECTRON_KEY(Q);
        SET_ORDINARY_ELECTRON_KEY(W);
        SET_ORDINARY_ELECTRON_KEY(E);
        SET_ORDINARY_ELECTRON_KEY(R);
        SET_ORDINARY_ELECTRON_KEY(T);
        SET_ORDINARY_ELECTRON_KEY(Y);
        SET_ORDINARY_ELECTRON_KEY(U);
        SET_ORDINARY_ELECTRON_KEY(I);
        SET_ORDINARY_ELECTRON_KEY(O);
        SET_ORDINARY_ELECTRON_KEY(P);
        SET_SHIFTABLE_CTRLABLE_ELECTRON_KEY(Up, Pound, LeftCurlyBracket);
        SET_SHIFTABLE_CTRLABLE_ELECTRON_KEY(Down, Underline, RightCurlyBracket);
        SET_SHIFTABLE_CTRLABLE_ELECTRON_KEY(Copy, LeftSquareBracket, RightSquareBracket);

        SET_ORDINARY_ELECTRON_KEY(Ctrl);
        SET_ORDINARY_ELECTRON_KEY(A);
        SET_ORDINARY_ELECTRON_KEY(S);
        SET_ORDINARY_ELECTRON_KEY(D);
        SET_ORDINARY_ELECTRON_KEY(F);
        SET_ORDINARY_ELECTRON_KEY(G);
        SET_ORDINARY_ELECTRON_KEY(H);
        SET_ORDINARY_ELECTRON_KEY(J);
        SET_ORDINARY_ELECTRON_KEY(K);
        SET_ORDINARY_ELECTRON_KEY(L);
        SET_SHIFTABLE_ELECTRON_KEY(Semicolon, Plus);
        SET_SHIFTABLE_ELECTRON_KEY(Colon, Star);
        SET_ORDINARY_ELECTRON_KEY(Return);

        SET_ORDINARY_ELECTRON_KEY(Shift);
        SET_ORDINARY_ELECTRON_KEY(Z);
        SET_ORDINARY_ELECTRON_KEY(X);
        SET_ORDINARY_ELECTRON_KEY(C);
        SET_ORDINARY_ELECTRON_KEY(V);
        SET_ORDINARY_ELECTRON_KEY(B);
        SET_ORDINARY_ELECTRON_KEY(N);
        SET_ORDINARY_ELECTRON_KEY(M);
        SET_SHIFTABLE_ELECTRON_KEY(Comma, LessThan);
        SET_SHIFTABLE_ELECTRON_KEY(Stop, GreaterThan);
        SET_SHIFTABLE_ELECTRON_KEY(Slash, QuestionMarke);
        SET_ORDINARY_ELECTRON_KEY(Delete);

        SET_ORDINARY_ELECTRON_KEY(Space);
    }
};

static const KeyComboTableInitialiser g_key_combo_table_initialiser;
