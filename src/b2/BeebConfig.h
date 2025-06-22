#ifndef HEADER_DE3F3F401AB041929180F37C8DEFC129 // -*- mode:c++ -*-
#define HEADER_DE3F3F401AB041929180F37C8DEFC129

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <memory>
#include <array>
#include <vector>
#include "roms.h"
#include <beeb/BBCMicroParasiteType.h>
#include <beeb/type.h>
#include <shared/json.h>
#include <beeb/scsi.h>

#include <shared/enum_decl.h>
#include "BeebConfig.inl"
#include <shared/enum_end.h>

class BBCMicro;
class Messages;
class DiscInterface;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The BeebConfig holds all the config info that gets saved to the
// JSON config file.

class BeebConfig {
  public:
    struct ROM {
        const BeebROM *standard_rom = nullptr;
        std::string file_name;
    };

    struct SidewaysROM : public ROM {
        bool writeable = false;

        // If this ROM is a standard ROM, the standard ROM's type takes
        // priority.
        ROMType type = ROMType_16KB;

        ROMType GetROMType() const;
    };

    std::string name;

    Enum<BBCMicroTypeID> type_id{BBCMicroTypeID_B};
    ROM os;
    SidewaysROM roms[16];
    ROM parasite_os;
    //uint8_t keyboard_links = 0;//TODO?
    const DiscInterface *disc_interface = nullptr;
    static constexpr bool DEFAULT_VIDEO_NULA = true;
    bool video_nula = DEFAULT_VIDEO_NULA;
    bool ext_mem = false;
    bool beeblink = false;
    bool adji = false;
    uint8_t adji_dip_switches = 0;
    Enum<BeebConfigNVRAMType> nvram_type{BeebConfigNVRAMType_Unknown};
    std::vector<uint8_t> nvram;
    bool mouse = false;
    bool rom_board = true;

    Enum<BBCMicroParasiteType> parasite_type{BBCMicroParasiteType_None};

    uint32_t feature_flags = 0; //combination of BeebConfigFeatureFlag

    Enum<OSROMType> os_rom_type{OSROMType_16KB};

    bool scsi = false;
    std::array<std::string, NUM_HARD_DISKS> hard_disk_dat_paths;

    void ResetNVRAM();

  protected:
  private:
};
// This only handles some of the BeebConfig properties. The remainder are dealt
// with manually, for one reason or another (usually name mismatches or
// inconvenient schema).
JSON_SERIALIZE(BeebConfig, name, video_nula, ext_mem, beeblink, adji, adji_dip_switches, nvram_type, mouse, parasite_type, os_rom_type, rom_board, scsi, hard_disk_dat_paths);

void InitDefaultBeebConfigs();

size_t GetNumDefaultBeebConfigs();
const BeebConfig *GetDefaultBeebConfigByIndex(size_t index);

std::vector<uint8_t> GetDefaultMaster128NVRAM();
std::vector<uint8_t> GetDefaultMasterCompactNVRAM();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The BeebLoadedConfig holds the actual data for a BeebConfig.
//
// For each BeebLoadedConfig::ROM: if the corresponding
// BeebConfig::ROM's writeable flag is false, contents must be
// non-null, and it points to the data to be used for that ROM by all
// BBCMicro objects referring to it.
//
// Otherwise (writeable is true), if contents is non-null, it's the
// initial data to be used for that sideways RAM bank, else the
// sideways RAM bank is initially all zero.

class BeebLoadedConfig {
  public:
    BeebConfig config;

    std::shared_ptr<const std::array<uint8_t, 16384>> os;
    std::shared_ptr<const std::vector<uint8_t>> roms[16];
    std::shared_ptr<const std::array<uint8_t, 4096>> parasite_os;
    HardDiskImageSet hard_disk_images;

    static bool Load(BeebLoadedConfig *dest, const BeebConfig &src, Messages *msg);

    void ReuseROMs(const BeebLoadedConfig &oth);

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
