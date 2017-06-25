#ifndef HEADER_EE88DCE933CB472689BE3ACA6187E3CF// -*- mode:c++ -*-
#define HEADER_EE88DCE933CB472689BE3ACA6187E3CF

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>
#include <set>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Maintains a mapping from uint32_t PC key code to int8_t BBC key code.
//
// The mapping is SDL_Scancode<->BeebKey (for scancode mappings) or
// SDL_Keycode+PCKeyModifier<->BeebKeySym (for keysym mappings). Use
// IsKeySymMap to find out what sort of mapping this is.
//
// The keymap holds the keysym/scancode flag, but doesn't otherwise
// distinguish between the two types of map.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Keymap {
public:
    struct Mapping {
        uint32_t pc_key;
        int8_t beeb_key;
    };

    static const Keymap DEFAULT;
    static const Keymap DEFAULT_CC;
    static const Keymap DEFAULT_UK;
    static const Keymap DEFAULT_US;

    Keymap();
    explicit Keymap(std::string name,bool key_sym_map);
    explicit Keymap(std::string name,bool key_sym_map,const std::initializer_list<const Mapping *> &list);

    void Reset();

    bool IsKeySymMap() const;

    std::string GetName() const;
    void SetName(std::string name);

    void SetMapping(uint32_t pc,int8_t bbc,bool state);

    // list is terminated by BeebSpecialKey_None.
    //
    // Returned pointer becomes invalid after next non-const member
    // function call.
    const int8_t *GetBeebKeysForPCKey(uint32_t keycode) const;

    // list is terminated by 0.
    //
    // Returned pointer becomes invalid after next non-const member
    // function call.
    const uint32_t *GetPCKeysForBeebKey(int8_t beeb_key) const;

    // If *keymap_ptr==this, resets *keymap_ptr to one of the default
    // keymaps.
    void WillBeDeleted(const Keymap **keymap_ptr) const;
protected:
private:
    struct BeebKeyList {
        uint32_t pc_key;
        size_t index;
    };
    struct BeebKeyListLessThanPCKey;

    struct PCKeyList {
        int8_t beeb_key;
        size_t index;
    };
    struct PCKeyListLessThanBeebKey;

    struct MappingLessThanByPCKey {
        bool operator()(const Mapping &a,const Mapping &b) const;
    };

    struct MappingLessThanByBeebKey;

    std::string m_name;
    bool m_is_key_sym_map=false;

    std::set<Mapping,MappingLessThanByPCKey> m_map;

    mutable bool m_dirty;
    mutable std::vector<BeebKeyList> m_beeb_key_lists;
    mutable std::vector<int8_t> m_all_beeb_keys;
    mutable std::vector<PCKeyList> m_pc_key_lists;
    mutable std::vector<uint32_t> m_all_pc_keys;

    bool GetIndex(size_t *index,uint32_t pc_key,int8_t beeb_key) const;
    void RebuildTables() const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
