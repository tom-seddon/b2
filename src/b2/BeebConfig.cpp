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

static std::shared_ptr<BeebRomData> LoadROM(
    const std::string &file_name,
    Messages *msg)
{
    std::vector<uint8_t> data;
    if(!LoadFile(&data,file_name,msg)) {
        return nullptr;
    }

    if(data.size()>ROM_SIZE) {
        msg->e.f(
            "ROM too large (%zu bytes; max: %zu bytes): %s\n",
            data.size(),
            ROM_SIZE,
            file_name.c_str());
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetROMPath(const std::string &file_name) {
    return GetAssetPath("roms",file_name);
}

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

        config.os_file_name="OS12.ROM";
        config.roms[15].file_name="BASIC2.ROM";
        config.roms[14].file_name=config.disc_interface->default_fs_rom;
        config.roms[13].writeable=true;

        g_default_configs.push_back(std::move(config));
    }

    // B+/B+128
    {
        BeebConfig config;

        config.name="B+";
        config.disc_interface=&DISC_INTERFACE_ACORN_1770;

        config.type=&BBC_MICRO_TYPE_B_PLUS;
        config.os_file_name="B+MOS.rom";
        config.roms[15].file_name="BASIC2.ROM";
        config.roms[14].file_name=config.disc_interface->default_fs_rom;

        g_default_configs.push_back(config);

        config.name="B+128";

        config.roms[0].writeable=true;
        config.roms[1].writeable=true;
        config.roms[12].writeable=true;
        config.roms[13].writeable=true;

        g_default_configs.push_back(config);
    }

    // Master 128
    {
        BeebConfig config;

        for(std::string version:{"3.20","3.50"}) {
            config.name="Master 128 (MOS "+version+")";
            config.disc_interface=&DISC_INTERFACE_MASTER128;

            config.type=&BBC_MICRO_TYPE_MASTER;
            config.os_file_name=PathJoined("m128",version,"mos.rom");
            config.roms[15].file_name=PathJoined("m128",version,"terminal.rom");
            config.roms[14].file_name=PathJoined("m128",version,"view.rom");
            config.roms[13].file_name=PathJoined("m128",version,"adfs.rom");
            config.roms[12].file_name=PathJoined("m128",version,"basic4.rom");
            config.roms[11].file_name=PathJoined("m128",version,"edit.rom");
            config.roms[10].file_name=PathJoined("m128",version,"viewsht.rom");
            config.roms[9].file_name=PathJoined("m128",version,"dfs.rom");
            config.roms[7].writeable=true;
            config.roms[6].writeable=true;
            config.roms[5].writeable=true;
            config.roms[4].writeable=true;

            g_default_configs.push_back(config);
        }
    }

    for(BeebConfig &config:g_default_configs) {
        ASSERT(!config.os_file_name.empty());
        config.os_file_name=GetROMPath(config.os_file_name);

        for(size_t i=0;i<16;++i) {
            if(!config.roms[i].file_name.empty()) {
                config.roms[i].file_name=GetROMPath(config.roms[i].file_name);
            }
        }
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

    dest->os=LoadROM(dest->config.os_file_name,msg);
    if(!dest->os) {
        return false;
    }

    for(int i=0;i<16;++i) {
        if(!dest->config.roms[i].file_name.empty()) {
            dest->roms[i]=LoadROM(dest->config.roms[i].file_name,msg);
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
