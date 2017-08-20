#ifndef HEADER_EE88DCE933CB472689BE3ACA6187E3CF// -*- mode:c++ -*-
#define HEADER_EE88DCE933CB472689BE3ACA6187E3CF

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>
#include <set>
#include <shared/debug.h>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Maintains a mapping from uint32_t PC key code to value - int8_t BBC
// key code, or command object, or other.
//
// The mapping is SDL_Scancode<->BeebKey (for scancode mappings) or
// SDL_Keycode+PCKeyModifier<->BeebKeySym (for keysym mappings). Use
// IsKeySymMap to find out what sort of mapping this is.
//
// The keymap holds the keysym/scancode flag, but doesn't otherwise
// distinguish between the two types of map.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class TraitsType>
class Keymap {
public:
    struct Mapping {
        uint32_t pc_key;
        typename TraitsType::ValueType value;
    };

    typedef typename TraitsType::ValueType ValueType;

    Keymap():
        Keymap("",false)
    {
    }

    explicit Keymap(std::string name,bool key_sym_map):
        m_name(std::move(name)),
        m_is_key_sym_map(key_sym_map)
    {
        this->Reset();
    }

    explicit Keymap(std::string name,bool key_sym_map,const std::initializer_list<const Mapping *> &list):
        Keymap(std::move(name),key_sym_map)
    {
        for(const Mapping *mappings:list) {
            for(const Mapping *mapping=mappings;mapping->pc_key!=0;++mapping) {
                this->SetMapping(mapping->pc_key,mapping->value,true);
            }
        }
    }

    virtual ~Keymap() {
    }

    void Reset() {
        m_map.clear();
        m_dirty=true;
    }

    bool IsKeySymMap() const {
        return m_is_key_sym_map;
    }

    const std::string &GetName() const {
        return m_name;
    }

    void SetName(std::string name) {
        m_name=std::move(name);
    }

    void ClearMappingsByValue(typename TraitsType::ValueType value) {
        auto &&it=m_map.begin();

        while(it!=m_map.end()) {
            if(it->value==value) {
                it=m_map.erase(it);
                m_dirty=true;
            } else {
                ++it;
            }
        }
    }

    void SetMapping(uint32_t pc_key,typename TraitsType::ValueType value,bool state) {
        Mapping mapping{pc_key,value};
        if(state) {
            if(m_map.insert(mapping).second) {
                m_dirty=true;
            }
        } else {
            if(m_map.erase(mapping)>0) {
                m_dirty=true;
            }
        }
    }

    // list is terminated by BeebSpecialKey_None.
    //
    // Returned pointer becomes invalid after next non-const member
    // function call.
    const typename TraitsType::ValueType *GetValuesForPCKey(uint32_t pc_key) const {
        if(m_dirty) {
            this->RebuildTables();
        }

        auto &&it=std::lower_bound(m_value_lists.begin(),m_value_lists.end(),pc_key,ValueListLessThanPCKey());
        if(it==m_value_lists.end()||it->pc_key!=pc_key) {
            return nullptr;
        }

        ASSERT(it->index<m_all_values.size()-1);
        return &m_all_values[it->index];
    }

    // list is terminated by 0.
    //
    // Returned pointer becomes invalid after next non-const member
    // function call.
    const uint32_t *GetPCKeysForValue(typename TraitsType::ValueType value) const {
        if(m_dirty) {
            this->RebuildTables();
        }

        auto &&it=std::lower_bound(m_pc_key_lists.begin(),m_pc_key_lists.end(),value,PCKeyListLessThanValue());
        if(it==m_pc_key_lists.end()||it->value!=value) {
            return nullptr;
        }

        ASSERT(it->index<m_all_pc_keys.size()-1);
        return &m_all_pc_keys[it->index];
    }

    //// If *keymap_ptr==this, resets *keymap_ptr to one of the default
    //// keymaps.
    //void WillBeDeleted(const Keymap **keymap_ptr) const {
    //    if(*keymap_ptr==this) {
    //        *keymap_ptr=&DEFAULT_KEYMAP;
    //    }
    //}

    //void ForEachMapping(std::function<void(uint32_t,ValueType)> fun) const {
    //    for(auto &&it:m_map) {
    //        fun(it->pc_key,it->value);
    //    }
    //}
protected:
private:
    struct ValueList {
        uint32_t pc_key;
        size_t index;
    };

    struct ValueListLessThanPCKey {
        inline bool operator()(const ValueList &a,uint32_t b) const {
            return a.pc_key<b;
        }
    };

    struct PCKeyList {
        typename TraitsType::ValueType value;
        size_t index;
    };

    struct PCKeyListLessThanValue {
        inline bool operator()(const PCKeyList &a,typename TraitsType::ValueType b) const {
            return a.value<b;
        }
    };

    struct MappingLessThanByPCKey {
        inline bool operator()(const Mapping &a,const Mapping &b) const {
            if(a.pc_key<b.pc_key) {
                return true;
            } else if(b.pc_key<a.pc_key) {
                return false;
            }

            if(a.value<b.value) {
                return true;
            } else if(b.value<a.value) {
                return false;
            }

            return false;
        }
    };

    struct MappingLessThanByValue {
        inline bool operator()(const Mapping &a,const Mapping &b) const {
            if(a.value<b.value) {
                return true;
            } else if(b.value<a.value) {
                return false;
            }

            if(a.pc_key<b.pc_key) {
                return true;
            } else if(b.pc_key<a.pc_key) {
                return false;
            }

            return false;
        }
    };

    std::string m_name;
    bool m_is_key_sym_map=false;

    std::set<Mapping,MappingLessThanByPCKey> m_map;

    mutable bool m_dirty;
    mutable std::vector<ValueList> m_value_lists;
    mutable std::vector<typename TraitsType::ValueType> m_all_values;
    mutable std::vector<PCKeyList> m_pc_key_lists;
    mutable std::vector<uint32_t> m_all_pc_keys;

    void RebuildTables() const {
        if(!m_dirty) {
            return;
        }

        m_value_lists.clear();
        m_all_values.clear();
        m_pc_key_lists.clear();
        m_all_pc_keys.clear();

        size_t i;
        std::vector<Mapping> map(m_map.begin(),m_map.end());

        // By scancode.
        std::sort(map.begin(),map.end(),MappingLessThanByPCKey());

        i=0;
        while(i<map.size()) {
            ValueList list;

            list.pc_key=map[i].pc_key;
            list.index=m_all_values.size();

            size_t j=i;
            while(j<map.size()&&map[j].pc_key==list.pc_key) {
                m_all_values.push_back(map[j].value);
                ++j;
            }

            m_all_values.emplace_back(decltype(TraitsType::TERMINATOR)(TraitsType::TERMINATOR));
            m_value_lists.push_back(list);

            i=j;
        }

        // By BBC key.
        std::sort(map.begin(),map.end(),MappingLessThanByValue());

        i=0;
        while(i<map.size()) {
            PCKeyList list;

            list.value=map[i].value;
            list.index=m_all_pc_keys.size();

            size_t j=i;
            while(j<map.size()&&map[j].value==list.value) {
                m_all_pc_keys.push_back(map[j].pc_key);
                ++j;
            }

            m_all_pc_keys.push_back(0);
            m_pc_key_lists.push_back(list);

            i=j;
        }

        m_dirty=false;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
