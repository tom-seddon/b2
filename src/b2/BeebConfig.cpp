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

    auto &&result=std::make_shared<BeebRomData>();

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

        config.beeb_type=BBCMicroType_B;
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

        config.beeb_type=BBCMicroType_BPlus;
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

            config.beeb_type=BBCMicroType_Master;
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

            config.nvram_contents.resize(50);

            // *CONFIGURE settings:
            config.nvram_contents[5]=0xC9;//LANG 12; FS 9
            config.nvram_contents[6]=0xFF;//INSERT 0 ... INSERT 7
            config.nvram_contents[7]=0xFF;//INSERT 8 ... INSERT 15
            config.nvram_contents[10]=0x17;//MODE 7; SHADOW 0; TV 0 1
            config.nvram_contents[11]=0x80;//FLOPPY
            config.nvram_contents[12]=55;//DELAY 55
            config.nvram_contents[13]=3;//REPEAT 3
            config.nvram_contents[14]=0x00;
            config.nvram_contents[15]=0x00;
            config.nvram_contents[16]=2;//LOUD

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
