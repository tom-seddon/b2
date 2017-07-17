#include <shared/system.h>
#include "keymap.h"
#include <shared/debug.h>
#include <algorithm>
#include "conf.h"
#include "default_keymaps.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Keymap::MappingLessThanByPCKey::operator()(const Mapping &a,const Mapping &b) const {
    if(a.pc_key<b.pc_key) {
        return true;
    } else if(b.pc_key<a.pc_key) {
        return false;
    }

    if(a.beeb_key<b.beeb_key) {
        return true;
    } else if(b.beeb_key<a.beeb_key) {
        return false;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Keymap::MappingLessThanByBeebKey {
    inline bool operator()(const Mapping &a,const Mapping &b) const {
        if(a.beeb_key<b.beeb_key) {
            return true;
        } else if(b.beeb_key<a.beeb_key) {
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Keymap::BeebKeyListLessThanPCKey {
    inline bool operator()(const BeebKeyList &a,uint32_t b) const {
        return a.pc_key<b;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Keymap::PCKeyListLessThanBeebKey {
    inline bool operator()(const PCKeyList &a,uint8_t b) const {
        return a.beeb_key<b;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Keymap::Keymap():
    Keymap("",false)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Keymap::Keymap(std::string name,bool key_sym_map):
    m_name(std::move(name)),
    m_is_key_sym_map(key_sym_map)
{
    this->Reset();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Keymap::Keymap(std::string name,bool key_sym_map,const std::initializer_list<const Mapping *> &list):
    Keymap(std::move(name),key_sym_map)
{
    for(const Mapping *mappings:list) {
        for(const Mapping *mapping=mappings;mapping->pc_key!=0;++mapping) {
            this->SetMapping(mapping->pc_key,mapping->beeb_key,true);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Keymap::Reset() {
    m_map.clear();
    m_dirty=true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Keymap::IsKeySymMap() const {
    return m_is_key_sym_map;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string Keymap::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Keymap::SetName(std::string name) {
    m_name=std::move(name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Keymap::SetMapping(uint32_t pc_key,int8_t beeb_key,bool state) {
    Mapping mapping{pc_key,beeb_key};
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const int8_t *Keymap::GetBeebKeysForPCKey(uint32_t pc_key) const {
    if(m_dirty) {
        this->RebuildTables();
    }

    auto &&it=std::lower_bound(m_beeb_key_lists.begin(),m_beeb_key_lists.end(),pc_key,BeebKeyListLessThanPCKey());
    if(it==m_beeb_key_lists.end()||it->pc_key!=pc_key) {
        return nullptr;
    }

    ASSERT(it->index<m_all_beeb_keys.size()-1);
    return &m_all_beeb_keys[it->index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const uint32_t *Keymap::GetPCKeysForBeebKey(int8_t beeb_key) const {
    if(m_dirty) {
        this->RebuildTables();
    }

    auto &&it=std::lower_bound(m_pc_key_lists.begin(),m_pc_key_lists.end(),beeb_key,PCKeyListLessThanBeebKey());
    if(it==m_pc_key_lists.end()||it->beeb_key!=beeb_key) {
        return nullptr;
    }

    ASSERT(it->index<m_all_pc_keys.size()-1);
    return &m_all_pc_keys[it->index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Keymap::WillBeDeleted(const Keymap **keymap_ptr) const {
    if(*keymap_ptr==this) {
        *keymap_ptr=&DEFAULT_KEYMAP;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Keymap::RebuildTables() const {
    if(!m_dirty) {
        return;
    }

    m_beeb_key_lists.clear();
    m_all_beeb_keys.clear();
    m_pc_key_lists.clear();
    m_all_pc_keys.clear();

    size_t i;
    std::vector<Mapping> map(m_map.begin(),m_map.end());

    // By scancode.
    std::sort(map.begin(),map.end(),MappingLessThanByPCKey());

    i=0;
    while(i<map.size()) {
        BeebKeyList list;

        list.pc_key=map[i].pc_key;
        list.index=m_all_beeb_keys.size();

        size_t j=i;
        while(j<map.size()&&map[j].pc_key==list.pc_key) {
            m_all_beeb_keys.push_back(map[j].beeb_key);
            ++j;
        }

        m_all_beeb_keys.push_back(-1);
        m_beeb_key_lists.push_back(list);

        i=j;
    }

    // By BBC key.
    std::sort(map.begin(),map.end(),MappingLessThanByBeebKey());

    i=0;
    while(i<map.size()) {
        PCKeyList list;

        list.beeb_key=map[i].beeb_key;
        list.index=m_all_pc_keys.size();

        size_t j=i;
        while(j<map.size()&&map[j].beeb_key==list.beeb_key) {
            m_all_pc_keys.push_back(map[j].pc_key);
            ++j;
        }

        m_all_pc_keys.push_back(0);
        m_pc_key_lists.push_back(list);

        i=j;
    }

    m_dirty=false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
