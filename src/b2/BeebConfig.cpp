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

ROMType BeebConfig::SidewaysROM::GetROMType() const {
    if (this->standard_rom) {
        return this->standard_rom->type;
    } else {
        return this->type;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadROM2(std::vector<uint8_t> *data, const BeebConfig::ROM &rom, size_t size, Messages *msg) {
    std::string path;
    if (rom.standard_rom) {
        path = rom.standard_rom->GetAssetPath();
    } else {
        path = rom.file_name;
    }

    if (!LoadFile(data, path, *msg)) {
        return false;
    }

    if (data->size() > size) {
        msg->e.f(
            "ROM too large (%zu bytes; max: %zu bytes): %s\n",
            data->size(),
            size,
            path.c_str());
        return false;
    }

    return true;
}

template <size_t SIZE>
static std::shared_ptr<std::array<uint8_t, SIZE>> LoadOSROM(const BeebConfig::ROM &rom, Messages *msg) {
    std::vector<uint8_t> data;
    if (!LoadROM2(&data, rom, SIZE, msg)) {
        return nullptr;
    }

    auto result = std::make_shared<std::array<uint8_t, SIZE>>();
    for (size_t i = 0; i < SIZE; ++i) {
        (*result)[i] = 0;
    }

    // fill the OS ROM backwards. The vectors are at the end.
    for (size_t i = 0; i < data.size(); ++i) {
        (*result)[result->size() - data.size() + i] = data[i];
    }

    return result;
}

static std::shared_ptr<std::vector<uint8_t>> LoadSidewaysROM(const BeebConfig::SidewaysROM &rom,
                                                             Messages *msg) {
    const ROMTypeMetadata *metadata = GetROMTypeMetadata(rom.GetROMType());

    std::vector<uint8_t> data;
    if (!LoadROM2(&data, rom, metadata->num_bytes, msg)) {
        return nullptr;
    }

    // Ensure the ROM data is the right size.
    data.resize(metadata->num_bytes);

    auto result = std::make_shared<std::vector<uint8_t>>(std::move(data));
    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebConfig::ResetNVRAM() {
    switch (this->nvram_type) {
    default:
        ASSERT(false);
        // fall through
    case BeebConfigNVRAMType_None:
    BeebConfigNVRAMType_None:
        this->nvram.clear();
        break;

    case BeebConfigNVRAMType_Unknown:
        // Handle the case where the BeebConfig was set up before the nvram_type
        // field was added. (This only really causes a problem in the Master
        // Compact case, as the NVRAM contents aren't always cross-variant
        // compatible. None of the GitHub releases are affected.)
        switch (this->type_id) {
        default:
            ASSERT(false);
            // fall through
        case BBCMicroTypeID_B:
        case BBCMicroTypeID_BPlus:
            goto BeebConfigNVRAMType_None;

        case BBCMicroTypeID_Master:
            goto BeebConfigNVRAMType_Master128;

        case BBCMicroTypeID_MasterCompact:
            goto BeebConfigNVRAMType_MasterCompact;
        }
        break;

    case BeebConfigNVRAMType_Master128:
    BeebConfigNVRAMType_Master128:
        this->nvram = GetDefaultMaster128NVRAM();
        break;

    case BeebConfigNVRAMType_MasterCompact:
    BeebConfigNVRAMType_MasterCompact:
        this->nvram = GetDefaultMasterCompactNVRAM();
        break;

    case BeebConfigNVRAMType_PC128S:
        this->nvram = GetDefaultMasterCompactNVRAM();

        this->nvram[10] = 0xf9;
        this->nvram[11] = 0xe3;
        this->nvram[12] = 32;
        this->nvram[13] = 8;
        this->nvram[14] = 10;
        this->nvram[15] = 44;
        this->nvram[16] = 128;
        this->nvram[18] = 3;
        this->nvram[19] = 4;
        this->nvram[127] = 0xb2;

        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<BeebConfig> g_default_configs;

std::vector<uint8_t> GetDefaultMaster128NVRAM() {
    std::vector<uint8_t> nvram(50);

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

std::vector<uint8_t> GetDefaultMasterCompactNVRAM() {
    std::vector<uint8_t> nvram(128);

    nvram[5] = 0xED;  // 5 - LANG 14; FS 13
    nvram[6] = 0xFF;  // 6 - INSERT 0 ... INSERT 7
    nvram[7] = 0xFF;  // 7 - INSERT 8 ... INSERT 15
    nvram[8] = 0x00;  // 8
    nvram[9] = 0x00;  // 9
    nvram[10] = 0x17; //10 - MODE 7; SHADOW 0; TV 0 1
    nvram[11] = 0xC0; //11 - FLOPPY; NODIR
    nvram[12] = 55;   //12 - DELAY 55
    nvram[13] = 0x03; //13 - REPEAT 3
    nvram[14] = 0x00; //14
    nvram[15] = 0x01; //15 - TUBE
    nvram[16] = 0x02; //16 - LOUD
    nvram[17] = 0x00; //17 - unused?
    nvram[18] = 0x00; //18 - joystick settings
    nvram[19] = 0x00; //19 - country code

    // Additional flag to indicate contents are valid.
    //
    // Values for this are $b0 for MOS 5.00/MOS 5.10 or $b2 for MOS I5.10C/MOS
    // 5.11.
    nvram[127] = 0xb0;

    return nvram;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BeebConfig GetBConfig(const DiscInterface *di) {
    BeebConfig config;

    config.name = std::string("B/") + di->display_name;

    config.type_id = BBCMicroTypeID_B;
    config.disc_interface = di;

    config.os.standard_rom = &BEEB_ROM_OS12;
    config.roms[15].standard_rom = &BEEB_ROM_BASIC2;
    config.roms[14].standard_rom = FindBeebROM(config.disc_interface->fs_rom);
    config.roms[13].writeable = true;

    return config;
}

void InitDefaultBeebConfigs() {
    ASSERT(g_default_configs.empty());

    for (size_t i = 0; MODEL_B_DISC_INTERFACES[i]; ++i) {
        g_default_configs.push_back(GetBConfig(MODEL_B_DISC_INTERFACES[i]));
    }

    // B+/B+128
    {
        BeebConfig config;

        config.name = "B+";
        config.disc_interface = DISC_INTERFACE_ACORN_1770;

        config.type_id = BBCMicroTypeID_BPlus;
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
        config.disc_interface = DISC_INTERFACE_MASTER128;
        config.type_id = BBCMicroTypeID_Master;
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
        config.nvram_type = BeebConfigNVRAMType_Master128;

        g_default_configs.push_back(config);
    }

    // Master 128 MOS 3.50
    {
        BeebConfig config;

        config.name = "Master 128 (MOS 3.50)";
        config.disc_interface = DISC_INTERFACE_MASTER128;
        config.type_id = BBCMicroTypeID_Master;
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
        config.nvram_type = BeebConfigNVRAMType_Master128;

        g_default_configs.push_back(config);
    }

    // Master Turbo MOS 3.20
    {
        BeebConfig config;

        config.name = "Master Turbo (MOS 3.20)";
        config.disc_interface = DISC_INTERFACE_MASTER128;
        config.type_id = BBCMicroTypeID_Master;
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
        config.nvram_type = BeebConfigNVRAMType_Master128;

        g_default_configs.push_back(config);
    }

    // Master Turbo MOS 3.50
    {
        BeebConfig config;

        config.name = "Master Turbo (MOS 3.50)";
        config.disc_interface = DISC_INTERFACE_MASTER128;
        config.type_id = BBCMicroTypeID_Master;
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
        config.nvram_type = BeebConfigNVRAMType_Master128;

        g_default_configs.push_back(config);
    }

    // BBC B with 6502 second processor
    {
        BeebConfig config = GetBConfig(DISC_INTERFACE_ACORN_1770);

        config.name += " + 6502 second processor";
        config.parasite_type = BBCMicroParasiteType_External3MHz6502;
        config.parasite_os.standard_rom = FindBeebROM(StandardROM_TUBE110);
        config.feature_flags = BeebConfigFeatureFlag_6502SecondProcessor;

        g_default_configs.push_back(config);
    }

    // Master Compact MOS 5.00
    {
        BeebConfig config;

        config.name = "Master Compact (MOS 5.00)";
        config.disc_interface = DISC_INTERFACE_MASTER128;
        config.type_id = BBCMicroTypeID_MasterCompact;
        config.os.standard_rom = &BEEB_ROM_MOS500_MOS_ROM;
        config.roms[15].standard_rom = &BEEB_ROM_MOS500_SIDEWAYS_ROM_F;
        config.roms[14].standard_rom = &BEEB_ROM_MOS500_SIDEWAYS_ROM_E;
        config.roms[13].standard_rom = &BEEB_ROM_MOS500_SIDEWAYS_ROM_D;
        config.roms[7].writeable = true;
        config.roms[6].writeable = true;
        config.roms[5].writeable = true;
        config.roms[4].writeable = true;
        config.feature_flags = BeebConfigFeatureFlag_MasterCompact;
        config.nvram_type = BeebConfigNVRAMType_MasterCompact;

        g_default_configs.push_back(config);
    }

    // Master Compact MOS 5.10
    {
        BeebConfig config;

        config.name = "Master Compact (MOS 5.10)";
        config.disc_interface = DISC_INTERFACE_MASTER128;
        config.type_id = BBCMicroTypeID_MasterCompact;
        config.os.standard_rom = &BEEB_ROM_MOS510_MOS_ROM;
        config.roms[15].standard_rom = &BEEB_ROM_MOS510_SIDEWAYS_ROM_F;
        config.roms[14].standard_rom = &BEEB_ROM_MOS510_SIDEWAYS_ROM_E;
        config.roms[13].standard_rom = &BEEB_ROM_MOS510_SIDEWAYS_ROM_D;
        config.roms[7].writeable = true;
        config.roms[6].writeable = true;
        config.roms[5].writeable = true;
        config.roms[4].writeable = true;
        config.feature_flags = BeebConfigFeatureFlag_MasterCompact;
        config.nvram_type = BeebConfigNVRAMType_MasterCompact;

        g_default_configs.push_back(config);
    }

    // Olivetti PC 128S
    {
        BeebConfig config;

        config.name = "Olivetti PC 128 S";
        config.disc_interface = DISC_INTERFACE_MASTER128;
        config.type_id = BBCMicroTypeID_MasterCompact;
        config.os.standard_rom = &BEEB_ROM_MOSI510C_MOS_ROM;
        config.roms[15].standard_rom = &BEEB_ROM_MOSI510C_SIDEWAYS_ROM_F;
        config.roms[14].standard_rom = &BEEB_ROM_MOSI510C_SIDEWAYS_ROM_E;
        config.roms[13].standard_rom = &BEEB_ROM_MOSI510C_SIDEWAYS_ROM_D;
        config.roms[7].writeable = true;
        config.roms[6].writeable = true;
        config.roms[5].writeable = true;
        config.roms[4].writeable = true;
        config.feature_flags = BeebConfigFeatureFlag_OlivettiPC128S;
        config.nvram_type = BeebConfigNVRAMType_PC128S;

        g_default_configs.push_back(config);
    }

    for (BeebConfig &config : g_default_configs) {
        config.ResetNVRAM();
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

//std::vector<uint8_t> GetDefaultNVRAMContents(const BBCMicroType *type) {
//    switch (type->type_id) {
//    case BBCMicroTypeID_Master:
//        return g_default_master_128_nvram_contents;
//
//    case BBCMicroTypeID_MasterCompact:
//        return g_default_master_compact_nvram_contents;
//
//    default:
//        return std::vector<uint8_t>();
//    }
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void ResetDefaultNVRAMContents(const BBCMicroType *type) {
//    switch (type->type_id) {
//    default:
//        break;
//
//    case BBCMicroTypeID_Master:
//        g_default_master_128_nvram_contents = GetDefaultMaster128NVRAM();
//        break;
//
//    case BBCMicroTypeID_MasterCompact:
//        g_default_master_compact_nvram_contents = GetDefaultMasterCompactNVRAM();
//        break;
//    }
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void SetDefaultNVRAMContents(const BBCMicroType *type, std::vector<uint8_t> nvram_contents) {
//    switch (type->type_id) {
//    default:
//        break;
//
//    case BBCMicroTypeID_Master:
//        g_default_master_128_nvram_contents = std::move(nvram_contents);
//        break;
//
//    case BBCMicroTypeID_MasterCompact:
//        g_default_master_compact_nvram_contents = std::move(nvram_contents);
//        break;
//    }
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebLoadedConfig::Load(
    BeebLoadedConfig *dest,
    const BeebConfig &src,
    Messages *msg) {
    dest->config = src;

    dest->os = LoadOSROM<16384>(dest->config.os, msg);
    if (!dest->os) {
        return false;
    }

    for (int i = 0; i < 16; ++i) {
        BeebConfig::SidewaysROM *rom = &dest->config.roms[i];

        if (rom->standard_rom || !rom->file_name.empty()) {
            dest->roms[i] = LoadSidewaysROM(*rom, msg);
            if (!dest->roms[i]) {
                return false;
            }
        }
    }

    if (dest->config.parasite_os.standard_rom ||
        !dest->config.parasite_os.file_name.empty()) {
        dest->parasite_os = LoadOSROM<2048>(dest->config.parasite_os, msg);
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
