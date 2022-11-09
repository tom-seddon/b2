#ifndef HEADER_DE3F3F401AB041929180F37C8DEFC129 // -*- mode:c++ -*-
#define HEADER_DE3F3F401AB041929180F37C8DEFC129

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <memory>
#include <array>
#include <vector>
#include "roms.h"

class BBCMicro;
class Messages;
struct DiscInterfaceDef;
struct BBCMicroType;

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
    };

    std::string name;

    const BBCMicroType *type = nullptr;
    ROM os;
    SidewaysROM roms[16];
    ROM parasite_os;
    uint8_t keyboard_links = 0;
    const DiscInterfaceDef *disc_interface = nullptr;
    bool video_nula = true;
    bool ext_mem = false;
    bool beeblink = false;
    bool parasite = false;

  protected:
  private:
};

void InitDefaultBeebConfigs();

size_t GetNumDefaultBeebConfigs();
const BeebConfig *GetDefaultBeebConfigByIndex(size_t index);

std::vector<uint8_t> GetDefaultNVRAMContents(const BBCMicroType *type);
void ResetDefaultNVRAMContents(const BBCMicroType *type);
void SetDefaultNVRAMContents(const BBCMicroType *type, std::vector<uint8_t> nvram_contents);

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

    std::shared_ptr<const std::array<uint8_t, 16384>> os, roms[16];
    std::shared_ptr<const std::array<uint8_t, 4096>> parasite_os;

    static bool Load(BeebLoadedConfig *dest, const BeebConfig &src, Messages *msg);

    void ReuseROMs(const BeebLoadedConfig &oth);

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
