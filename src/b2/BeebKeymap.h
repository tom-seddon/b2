#ifndef HEADER_84A1A75C26C943A189772563D0E0F95C// -*- mode:c++ -*-
#define HEADER_84A1A75C26C943A189772563D0E0F95C

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "keymap.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebKeymapTraits {
    typedef int8_t ValueType;
    static const int8_t TERMINATOR=-1;
};

// The mapping is SDL_Scancode<->BeebKey (for scancode mappings) or
// SDL_Keycode+PCKeyModifier<->BeebKeySym (for keysym mappings). Use
// IsKeySymMap to find out what sort of mapping this is.
//
// The keymap holds the keysym/scancode flag, but doesn't otherwise
// distinguish between the two types of map.

class BeebKeymap:
    public Keymap<BeebKeymapTraits>
{
public:
    BeebKeymap(std::string name,
               bool is_key_sym_map);

    bool IsKeySymMap() const;

    bool GetPreferShortcuts() const;
    void SetPreferShortcuts(bool prefer_shortcuts);
protected:
private:
    bool m_is_key_sym_map=false;
    bool m_prefer_shortcuts=false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern const BeebKeymap DEFAULT_KEYMAP;
extern const BeebKeymap DEFAULT_KEYMAP_CC;
extern const BeebKeymap DEFAULT_KEYMAP_UK;
extern const BeebKeymap DEFAULT_KEYMAP_US;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
