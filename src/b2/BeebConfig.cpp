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

#include <shared/enum_def.h>
#include "BeebConfig.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <size_t MAX_SIZE, bool OS>
static std::shared_ptr<std::array<uint8_t, MAX_SIZE>> LoadROM(const BeebConfig::ROM &rom, Messages *msg) {
    std::vector<uint8_t> data;

    std::string path;
    if (rom.standard_rom) {
        path = rom.standard_rom->GetAssetPath();
    } else {
        path = rom.file_name;
    }

    if (!LoadFile(&data, path, msg)) {
        return nullptr;
    }

    if (rom.standard_rom) {
        path = rom.standard_rom->GetAssetPath();
    } else {
        path = rom.file_name;
    }

    if (!LoadFile(&data, path, msg)) {
        return nullptr;
    }

    if (data.size() > MAX_SIZE) {
        msg->e.f(
            "ROM too large (%zu bytes; max: %zu bytes): %s\n",
            data.size(),
            MAX_SIZE,
            path.c_str());
        return nullptr;
    }

    auto result = std::make_shared<std::array<uint8_t, MAX_SIZE>>();

    for (size_t i = 0; i < MAX_SIZE; ++i) {
        (*result)[i] = 0;
    }

    size_t end = OS ? MAX_SIZE : data.size();

    for (size_t i = 0; i < data.size(); ++i) {
        (*result)[end - data.size() + i] = data[i];
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<BeebConfig> g_default_configs;

static std::vector<uint8_t> GetDefaultMasterNVRAM() {
    std::vector<uint8_t> nvram;

    nvram.resize(50);

    nvram[5] = 0xC9;  // 5 - LANG 12; FS 9
    nvram[6] = 0xFF;  // 6 - INSERT 0 ... INSERT 7
    nvram[7] = 0xFF;  // 7 - INSERT 8 ... INSERT 15
    nvram[8] = 0x00;  // 8
    nvram[9] = 0x00;  // 9
    nvram[10] = 0x17; //10 - MODE 7; SHADOW 0; TV 0 1
    nvram[11] = 0x80; //11 - FLOPPY
    nvram[12] = 55;   //12 - DELAY 55
    nvram[13] = 0x03; //13 - REPEAT 3
    nvram[14] = 0x00; //14
    nvram[15] = 0x01; //15 - TUBE
    nvram[16] = 0x02; //16 - LOUD

    return nvram;
}

static std::vector<uint8_t> g_default_master_nvram_contents = GetDefaultMasterNVRAM();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BeebConfig GetBConfig(const DiscInterfaceDef *di) {
    BeebConfig config;

    config.name = std::string("B/") + di->name;

    config.type = &BBC_MICRO_TYPE_B;
    config.disc_interface = di;

    config.os.standard_rom = &BEEB_ROM_OS12;
    config.roms[15].standard_rom = &BEEB_ROM_BASIC2;
    config.roms[14].standard_rom = FindBeebROM(config.disc_interface->fs_rom);
    config.roms[13].writeable = true;

    return config;
}

void InitDefaultBeebConfigs() {
    ASSERT(g_default_configs.empty());

    for (const DiscInterfaceDef *const *di_ptr = ALL_DISC_INTERFACES; *di_ptr; ++di_ptr) {
        g_default_configs.push_back(GetBConfig(*di_ptr));
    }

    // B+/B+128
    {
        BeebConfig config;

        config.name = "B+";
        config.disc_interface = &DISC_INTERFACE_ACORN_1770;

        config.type = &BBC_MICRO_TYPE_B_PLUS;
        config.os.standard_rom = &BEEB_ROM_BPLUS_MOS;
        config.roms[15].standard_rom = &BEEB_ROM_BASIC2;
        config.roms[14].standard_rom = FindBeebROM(config.disc_interface->fs_rom);

        g_default_configs.push_back(config);

        config.name = "B+128";

        config.roms[0].writeable = true;
        config.roms[1].writeable = true;
        config.roms[12].writeable = true;
        config.roms[13].writeable = true;

        g_default_configs.push_back(config);
    }

    // Master 128 MOS 3.20
    {
        BeebConfig config;

        config.name = "Master 128 (MOS 3.20)";
        config.disc_interface = &DISC_INTERFACE_MASTER128;
        config.type = &BBC_MICRO_TYPE_MASTER;
        config.os.standard_rom = FindBeebROM(StandardROM_MOS320_MOS);
        config.roms[15].standard_rom = FindBeebROM(StandardROM_MOS320_TERMINAL);
        config.roms[14].standard_rom = FindBeebROM(StandardROM_MOS320_VIEW);
        config.roms[13].standard_rom = FindBeebROM(StandardROM_MOS320_ADFS);
        config.roms[12].standard_rom = FindBeebROM(StandardROM_MOS320_BASIC4);
        config.roms[11].standard_rom = FindBeebROM(StandardROM_MOS320_EDIT);
        config.roms[10].standard_rom = FindBeebROM(StandardROM_MOS320_VIEWSHEET);
        config.roms[9].standard_rom = FindBeebROM(StandardROM_MOS320_DFS);
        config.roms[7].writeable = true;
        config.roms[6].writeable = true;
        config.roms[5].writeable = true;
        config.roms[4].writeable = true;

        g_default_configs.push_back(config);
    }

    // Master 128 MOS 3.50
    {
        BeebConfig config;

        config.name = "Master 128 (MOS 3.50)";
        config.disc_interface = &DISC_INTERFACE_MASTER128;
        config.type = &BBC_MICRO_TYPE_MASTER;
        config.os.standard_rom = FindBeebROM(StandardROM_MOS350_MOS);
        config.roms[15].standard_rom = FindBeebROM(StandardROM_MOS350_TERMINAL);
        config.roms[14].standard_rom = FindBeebROM(StandardROM_MOS350_VIEW);
        config.roms[13].standard_rom = FindBeebROM(StandardROM_MOS350_ADFS);
        config.roms[12].standard_rom = FindBeebROM(StandardROM_MOS350_BASIC4);
        config.roms[11].standard_rom = FindBeebROM(StandardROM_MOS350_EDIT);
        config.roms[10].standard_rom = FindBeebROM(StandardROM_MOS350_VIEWSHEET);
        config.roms[9].standard_rom = FindBeebROM(StandardROM_MOS350_DFS);
        config.roms[7].writeable = true;
        config.roms[6].writeable = true;
        config.roms[5].writeable = true;
        config.roms[4].writeable = true;

        g_default_configs.push_back(config);
    }

    // Master Turbo MOS 3.20
    {
        BeebConfig config;

        config.name = "Master Turbo (MOS 3.20)";
        config.disc_interface = &DISC_INTERFACE_MASTER128;
        config.type = &BBC_MICRO_TYPE_MASTER;
        config.os.standard_rom = FindBeebROM(StandardROM_MOS320_MOS);
        config.roms[15].standard_rom = FindBeebROM(StandardROM_MOS320_TERMINAL);
        config.roms[14].standard_rom = FindBeebROM(StandardROM_MOS320_VIEW);
        config.roms[13].standard_rom = FindBeebROM(StandardROM_MOS320_ADFS);
        config.roms[12].standard_rom = FindBeebROM(StandardROM_MOS320_BASIC4);
        config.roms[11].standard_rom = FindBeebROM(StandardROM_MOS320_EDIT);
        config.roms[10].standard_rom = FindBeebROM(StandardROM_MOS320_VIEWSHEET);
        config.roms[9].standard_rom = FindBeebROM(StandardROM_MOS320_DFS);
        config.roms[7].writeable = true;
        config.roms[6].writeable = true;
        config.roms[5].writeable = true;
        config.roms[4].writeable = true;
        config.parasite_type = BBCMicroParasiteType_MasterTurbo;
        config.parasite_os.standard_rom = FindBeebROM(StandardROM_MasterTurboParasite);
        config.feature_flags = BeebConfigFeatureFlag_MasterTurbo;

        g_default_configs.push_back(config);
    }

    // Master Turbo MOS 3.50
    {
        BeebConfig config;

        config.name = "Master Turbo (MOS 3.50)";
        config.disc_interface = &DISC_INTERFACE_MASTER128;
        config.type = &BBC_MICRO_TYPE_MASTER;
        config.os.standard_rom = FindBeebROM(StandardROM_MOS350_MOS);
        config.roms[15].standard_rom = FindBeebROM(StandardROM_MOS350_TERMINAL);
        config.roms[14].standard_rom = FindBeebROM(StandardROM_MOS350_VIEW);
        config.roms[13].standard_rom = FindBeebROM(StandardROM_MOS350_ADFS);
        config.roms[12].standard_rom = FindBeebROM(StandardROM_MOS350_BASIC4);
        config.roms[11].standard_rom = FindBeebROM(StandardROM_MOS350_EDIT);
        config.roms[10].standard_rom = FindBeebROM(StandardROM_MOS350_VIEWSHEET);
        config.roms[9].standard_rom = FindBeebROM(StandardROM_MOS350_DFS);
        config.roms[7].writeable = true;
        config.roms[6].writeable = true;
        config.roms[5].writeable = true;
        config.roms[4].writeable = true;
        config.parasite_type = BBCMicroParasiteType_MasterTurbo;
        config.parasite_os.standard_rom = FindBeebROM(StandardROM_MasterTurboParasite);
        config.feature_flags = BeebConfigFeatureFlag_MasterTurbo;

        g_default_configs.push_back(config);
    }

    // BBC B with 6502 second processor
    {
        BeebConfig config = GetBConfig(&DISC_INTERFACE_ACORN_1770);

        config.name += " + 6502 second processor";
        config.parasite_type = BBCMicroParasiteType_External3MHz6502;
        config.parasite_os.standard_rom = FindBeebROM(StandardROM_TUBE110);
        config.feature_flags = BeebConfigFeatureFlag_6502SecondProcessor;

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
    ASSERT(index < g_default_configs.size());

    return &g_default_configs[index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> GetDefaultNVRAMContents(const BBCMicroType *type) {
    if (type->type_id == BBCMicroTypeID_Master) {
        return g_default_master_nvram_contents;
    } else {
        return std::vector<uint8_t>();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ResetDefaultNVRAMContents(const BBCMicroType *type) {
    if (type->type_id == BBCMicroTypeID_Master) {
        g_default_master_nvram_contents = GetDefaultMasterNVRAM();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetDefaultNVRAMContents(const BBCMicroType *type, std::vector<uint8_t> nvram_contents) {
    if (type->type_id == BBCMicroTypeID_Master) {
        g_default_master_nvram_contents = std::move(nvram_contents);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebLoadedConfig::Load(
    BeebLoadedConfig *dest,
    const BeebConfig &src,
    Messages *msg) {
    dest->config = src;

    dest->os = LoadROM<16384, true>(dest->config.os, msg);
    if (!dest->os) {
        return false;
    }

    for (int i = 0; i < 16; ++i) {
        if (dest->config.roms[i].standard_rom ||
            !dest->config.roms[i].file_name.empty()) {
            dest->roms[i] = LoadROM<16384, false>(dest->config.roms[i], msg);
            if (!dest->roms[i]) {
                return false;
            }
        }
    }

    if (dest->config.parasite_os.standard_rom ||
        !dest->config.parasite_os.file_name.empty()) {
        dest->parasite_os = LoadROM<4096, true>(dest->config.parasite_os, msg);
        if (!dest->parasite_os) {
            return false;
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
    if (!!this->os && !!oth.os) {
        if (std::equal(this->os->begin(), this->os->end(), oth.os->begin())) {
            this->os = oth.os;
        }
    }

    for (size_t i = 0; i < 16; ++i) {
        if (!!this->roms[i]) {
            for (size_t j = 0; j < 16; ++j) {
                if (!!oth.roms[j]) {
                    if (std::equal(this->roms[i]->begin(), this->roms[i]->end(), oth.roms[j]->begin())) {
                        this->roms[i] = oth.roms[j];
                        break;
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
