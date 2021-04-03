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

// There's at least one default beeb keymap. Index 0 is always valid.
size_t GetNumDefaultBeebKeymaps();
const BeebKeymap *GetDefaultBeebKeymapByIndex(size_t index);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Ensure the keymap is not in use by any windows.
//
// The keymap list's old unique_ptr is returned. Discard when done.
std::unique_ptr<BeebKeymap> RemoveBeebKeymapByIndex(size_t index);

// AddBeebKeymap will adjust KEYMAP's name to make it unique.
//
// Returns the pointer to the keymap in the list.
BeebKeymap *AddBeebKeymap(BeebKeymap keymap);

// When necessary, name will be adjusted to make it unique.
void BeebKeymapDidChange(size_t index);

size_t GetNumBeebKeymaps();
BeebKeymap *GetBeebKeymapByIndex(size_t index);

// Keymaps are not optimised for retrieval by name.
BeebKeymap *FindBeebKeymapByName(const std::string &name);

// Returns the 0th in the list, or nullptr if the list is empty.
BeebKeymap *GetDefaultBeebKeymap();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
