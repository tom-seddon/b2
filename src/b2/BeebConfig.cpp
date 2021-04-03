#include <shared/system.h>
#include "BeebConfig.h"
#include <beeb/BBCMicro.h>
#include "load_save.h"
#include <shared/log.h>
#include <shared/debug.h>
#include "Messages.h"
#include <beeb/DiscInterface.h>
#include <shared/path.h>
#include <stdlib.h>
#include "conf.h"
#include "load_save.h"
#include <beeb/DiscImage.h>
#include "misc.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::shared_ptr<BeebRomData> LoadROM(const BeebConfig::ROM &rom,Messages *msg) {
    std::vector<uint8_t> data;

    std::string path;
    if(rom.standard_rom) {
        path=rom.standard_rom->GetAssetPath();
    } else {
        path=rom.file_name;
    }

    if(!LoadFile(&data,path,msg)) {
        return nullptr;
    }

    if(data.size()>ROM_SIZE) {
        msg->e.f(
            "ROM too large (%zu bytes; max: %zu bytes): %s\n",
            data.size(),
            ROM_SIZE,
            path.c_str());
        return nullptr;
    }

    auto result=std::make_shared<BeebRomData>();

    for(size_t i=0;i<ROM_SIZE;++i) {
        if(i<data.size()) {
            (*result)[i]=data[i];
        } else {
            (*result)[i]=0;
        }
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<BeebConfig> g_default_configs;

static std::vector<uint8_t> GetDefaultMasterNVRAM() {
    std::vector<uint8_t> nvram;

    nvram.resize(50);

    nvram[5]=0xC9;// 5 - LANG 12; FS 9
    nvram[6]=0xFF;// 6 - INSERT 0 ... INSERT 7
    nvram[7]=0xFF;// 7 - INSERT 8 ... INSERT 15
    nvram[8]=0x00;// 8
    nvram[9]=0x00;// 9
    nvram[10]=0x17;//10 - MODE 7; SHADOW 0; TV 0 1
    nvram[11]=0x80;//11 - FLOPPY
    nvram[12]=55;//12 - DELAY 55
    nvram[13]=0x03;//13 - REPEAT 3
    nvram[14]=0x00;//14
    nvram[15]=0x00;//15
    nvram[16]=0x02;//16 - LOUD

    return nvram;
}

static std::vector<uint8_t> g_default_master_nvram_contents=GetDefaultMasterNVRAM();

static std::vector<std::unique_ptr<BeebConfig>> g_configs;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void InitDefaultBeebConfigs() {
    ASSERT(g_default_configs.empty());

    for(const DiscInterfaceDef *const *di_ptr=ALL_DISC_INTERFACES;*di_ptr;++di_ptr) {
        const DiscInterfaceDef *di=*di_ptr;

        BeebConfig config;

        config.name=std::string("B/")+di->name;

        config.type=&BBC_MICRO_TYPE_B;
        config.disc_interface=di;

        config.os.standard_rom=&BEEB_ROM_OS12;
        config.roms[15].standard_rom=&BEEB_ROM_BASIC2;
        config.roms[14].standard_rom=FindBeebROM(config.disc_interface->fs_rom);
        config.roms[13].writeable=true;

        g_default_configs.push_back(std::move(config));
    }

    // B+/B+128
    {
        BeebConfig config;

        config.name="B+";
        config.disc_interface=&DISC_INTERFACE_ACORN_1770;

        config.type=&BBC_MICRO_TYPE_B_PLUS;
        config.os.standard_rom=&BEEB_ROM_BPLUS_MOS;
        config.roms[15].standard_rom=&BEEB_ROM_BASIC2;
        config.roms[14].standard_rom=FindBeebROM(config.disc_interface->fs_rom);

        g_default_configs.push_back(config);

        config.name="B+128";

        config.roms[0].writeable=true;
        config.roms[1].writeable=true;
        config.roms[12].writeable=true;
        config.roms[13].writeable=true;

        g_default_configs.push_back(config);
    }

    // Master 128 MOS 3.20
    {
        BeebConfig config;

        config.name="Master 128 (MOS 3.20)";
        config.disc_interface=&DISC_INTERFACE_MASTER128;
        config.type=&BBC_MICRO_TYPE_MASTER;
        config.os.standard_rom=FindBeebROM(StandardROM_MOS320_MOS);
        config.roms[15].standard_rom=FindBeebROM(StandardROM_MOS320_TERMINAL);
        config.roms[14].standard_rom=FindBeebROM(StandardROM_MOS320_VIEW);
        config.roms[13].standard_rom=FindBeebROM(StandardROM_MOS320_ADFS);
        config.roms[12].standard_rom=FindBeebROM(StandardROM_MOS320_BASIC4);
        config.roms[11].standard_rom=FindBeebROM(StandardROM_MOS320_EDIT);
        config.roms[10].standard_rom=FindBeebROM(StandardROM_MOS320_VIEWSHEET);
        config.roms[9].standard_rom=FindBeebROM(StandardROM_MOS320_DFS);
        config.roms[7].writeable=true;
        config.roms[6].writeable=true;
        config.roms[5].writeable=true;
        config.roms[4].writeable=true;

        g_default_configs.push_back(config);
    }

    // Master 128 MOS 3.50
    {
        BeebConfig config;

        config.name="Master 128 (MOS 3.50)";
        config.disc_interface=&DISC_INTERFACE_MASTER128;
        config.type=&BBC_MICRO_TYPE_MASTER;
        config.os.standard_rom=FindBeebROM(StandardROM_MOS350_MOS);
        config.roms[15].standard_rom=FindBeebROM(StandardROM_MOS350_TERMINAL);
        config.roms[14].standard_rom=FindBeebROM(StandardROM_MOS350_VIEW);
        config.roms[13].standard_rom=FindBeebROM(StandardROM_MOS350_ADFS);
        config.roms[12].standard_rom=FindBeebROM(StandardROM_MOS350_BASIC4);
        config.roms[11].standard_rom=FindBeebROM(StandardROM_MOS350_EDIT);
        config.roms[10].standard_rom=FindBeebROM(StandardROM_MOS350_VIEWSHEET);
        config.roms[9].standard_rom=FindBeebROM(StandardROM_MOS350_DFS);
        config.roms[7].writeable=true;
        config.roms[6].writeable=true;
        config.roms[5].writeable=true;
        config.roms[4].writeable=true;

        g_default_configs.push_back(config);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumDefaultBeebConfigs() {
    return g_default_configs.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebConfig *GetDefaultBeebConfigByIndex(size_t index) {
    ASSERT(index<g_default_configs.size());

    return &g_default_configs[index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> GetDefaultNVRAMContents(const BBCMicroType *type) {
    if(type->type_id==BBCMicroTypeID_Master) {
        return g_default_master_nvram_contents;
    } else {
        return std::vector<uint8_t>();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ResetDefaultNVRAMContents(const BBCMicroType *type) {
    if(type->type_id==BBCMicroTypeID_Master) {
        g_default_master_nvram_contents=GetDefaultMasterNVRAM();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetDefaultNVRAMContents(const BBCMicroType *type,std::vector<uint8_t> nvram_contents) {
    if(type->type_id==BBCMicroTypeID_Master) {
        g_default_master_nvram_contents=std::move(nvram_contents);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebLoadedConfig::Load(
    BeebLoadedConfig *dest,
    const BeebConfig &src,
    Messages *msg)
{
    dest->config=src;

    dest->os=LoadROM(dest->config.os,msg);
    if(!dest->os) {
        return false;
    }

    for(int i=0;i<16;++i) {
        if(dest->config.roms[i].standard_rom||
           !dest->config.roms[i].file_name.empty())
        {
            dest->roms[i]=LoadROM(dest->config.roms[i],msg);
            if(!dest->roms[i]) {
                return false;
            }
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This is a bit stupid, but not a very common occurrence.
//
// Also, PCs are fast...
void BeebLoadedConfig::ReuseROMs(const BeebLoadedConfig &oth) {
    if(!!this->os&&!!oth.os) {
        if(std::equal(this->os->begin(),this->os->end(),oth.os->begin())) {
            this->os=oth.os;
        }
    }

    for(size_t i=0;i<16;++i) {
        if(!!this->roms[i]) {
            for(size_t j=0;j<16;++j) {
                if(!!oth.roms[j]) {
                    if(std::equal(this->roms[i]->begin(),this->roms[i]->end(),oth.roms[j]->begin())) {
                        this->roms[i]=oth.roms[j];
                        break;
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BeebConfig *FindBeebConfigByName(const std::string &name) {
    for(size_t i=0;i<g_configs.size();++i) {
        if(g_configs[i]->name==name) {
            return g_configs[i].get();
        }
    }

    return nullptr;
}

static void MakeNameUnique(BeebConfig *config) {
    config->name=GetUniqueName(config->name,[](const std::string &name)->const void * {
        return FindBeebConfigByName(name);
    },config);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadBeebConfigByName(BeebLoadedConfig *loaded_config,const std::string &config_name,Messages *msg) {
    const BeebConfig *config=FindBeebConfigByName(config_name);
    if(!config) {
        msg->e.f("unknown config: %s\n",config_name.c_str());
        return false;
    }

    if(!BeebLoadedConfig::Load(loaded_config,*config,msg)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void AddBeebConfig(BeebConfig config) {
    g_configs.push_back(std::make_unique<BeebConfig>(std::move(config)));

    MakeNameUnique(g_configs.back().get());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RemoveBeebConfigByIndex(size_t index) {
    ASSERT(index<g_configs.size());
    ASSERT(index<PTRDIFF_MAX);
    g_configs.erase(g_configs.begin()+(ptrdiff_t)index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebConfigDidChange(size_t index) {
    ASSERT(index<g_configs.size());

    MakeNameUnique(g_configs[index].get());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumBeebConfigs() {
    return g_configs.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebConfig *GetBeebConfigByIndex(size_t index) {
    ASSERT(index<g_configs.size());
    return g_configs[index].get();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
