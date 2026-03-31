#ifndef HEADER_F189E7E3EFB04F5490038430D6E02054 // -*- mode:c++ -*-
#define HEADER_F189E7E3EFB04F5490038430D6E02054

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <beeb/keys.h>
#include <string>

enum BBCMicroTypeID : uint8_t;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "keys.inl"
#include <shared/enum_end.h>

static const uint32_t PCKeyModifier_All = PCKeyModifier_Shift | PCKeyModifier_Ctrl | PCKeyModifier_Alt | PCKeyModifier_Gui | PCKeyModifier_AltGr | PCKeyModifier_NumLock;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Handles BeebKey and BeebSpecialKey. Returns nullptr if unknown.
const char *GetBeebKeyName(BeebKey beeb_key);
BeebKey GetBeebKeyByName(const char *name);

const char *GetBeebKeySymName(BeebKeySym beeb_sym);
BeebKeySym GetBeebKeySymByName(const char *name);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct KeyCombo {
    BeebKey key = BeebKey_None;
    BeebMetaKeyState shift_state = BeebMetaKeyState_Any;

    // Only relevant for Electron.
    BeebMetaKeyState ctrl_state = BeebMetaKeyState_Any;
    BeebMetaKeyState func_state = BeebMetaKeyState_Any;
};

struct KeySymKeyCombos {
    BeebKeySym key_sym = BeebKeySym_None;

    // Applies to BBC B/B+/Master 128.
    KeyCombo bbc;

    // Applies to Master Compact/PC 128 S.
    KeyCombo compact;

    // Applies to Electron.
    KeyCombo electron;
};

const KeySymKeyCombos *GetKeySymKeyCombosForKeySym(BeebKeySym beeb_sym);
const KeyCombo *GetKeyComboForType(const KeySymKeyCombos *combos, BBCMicroTypeID type_id);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t GetPCKeyModifiersFromSDLKeymod(uint16_t mod);

// Get name of combined SDL_Keycode/PCKeyModifier value.
//
// If the SDL_Keycode part is 0, returns an empty string, ignoring any
// modifier flags.
std::string GetKeycodeName(uint32_t keycode);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
