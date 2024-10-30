#ifndef __INC_CORE_H
#define __INC_CORE_H

#include <map>
#include "../beeb/include/beeb/type.h"
#include "../beeb/include/beeb/BBCMicroParasiteType.h"
#include "../beeb/include/beeb/DiscInterface.h"

static const float VOLUMES_TABLE[] = {
    0.00000f,
    0.03981f,
    0.05012f,
    0.06310f,
    0.07943f,
    0.10000f,
    0.12589f,
    0.15849f,
    0.19953f,
    0.25119f,
    0.31623f,
    0.39811f,
    0.50119f,
    0.63096f,
    0.79433f,
    1.00000f,
};


#define B2_SAMPLE_RATE 250000
#define B2_SAMPLE_RATE_FLOAT 250000.0
#define B2_SNAPSHOT_SIZE 262144

#define B2_LIBRETRO_SCREEN_WIDTH 768
#define B2_LIBRETRO_SCREEN_HEIGHT 576

#define B2_MAX_USERS 2
#define B2_MESSAGE_DISPLAY_FRAMES 6*50

#if __GNUC__
#define printflike __attribute__((format (printf, 1, 2)))
#else
#define printflike
#endif
#include <stdbool.h>

extern void log_fatal(const char *fmt, ...) printflike;
extern void log_error(const char *fmt, ...) printflike;
extern void log_warn(const char *fmt, ...) printflike;
extern void log_info(const char *fmt, ...) printflike;
//extern void log_info_OUTPUT(const char *fmt, ...) printflike;

// If the debugging compilation option is enabled a real function will
// be available to log debug messages.  If the debugging compilation
// optionis disabled we use a static inline empty function to make the
// debug calls disappear but in a way that does not generate warnings
// about unused variables etc.

extern void log_debug(const char *fmt, ...) printflike;
extern void log_dump(const char *prefix, uint8_t *data, size_t size);
extern void log_bitfield(const char *fmt, unsigned value, const char **names);

extern int autoboot;
extern bool sound_ddnoise, sound_tape;

const std::multimap<std::string, std::string> multidisk_replacements = {
{"Tape 1 of 2 Side A" , "Tape 2 of 2 Side B"},
{"Tape 1 of 4 Side A" , "Tape 2 of 4 Side B"},
{"Tape 1 of 4 Side A" , "Tape 3 of 4 Side A"},
{"Tape 1 of 4 Side A" , "Tape 4 of 4 Side B"},
{"Tape 1 of"      , "Tape 2 of"},
{"Tape 1 of"      , "Tape 3 of"},
{"Tape 1 of"      , "Tape 4 of"},
{"Tape 1 of"      , "Tape 5 of"},
{"Tape 1 of"      , "Tape 6 of"},
{"Tape 1 of"      , "Tape 7 of"},
{"Tape 1 of"      , "Tape 8 of"},
{"Tape 1 of"      , "Tape 9 of"},
{"Tape 1 of"      , "Tape 10 of"},
{"Disk 1 Side A"  , "Disk 1 Side B"},
{"Disk 1 Side A"  , "Disk 2 Side A"},
{"Disk 1 Side A"  , "Disk 2 Side B"},
{"Disk 1 Side A"  , "Disk 3 Side A"},
{"Disk 1 Side A"  , "Disk 3 Side B"},
{"Disk 1A"        , "Disk 1B"},
{"Disk 1A"        , "Disk 2A"},
{"Disk 1A"        , "Disk 2B"},
{"Disk 1A"        , "Disk 3A"},
{"Disk 1A"        , "Disk 3B"},
{"Disk 1 of"      , "Disk 2 of"},
{"Disk 1 of"      , "Disk 3 of"},
{"Disk 1 of"      , "Disk 4 of"},
{"Disk 1 of"      , "Disk 5 of"},
{"Disk 1 of"      , "Disk 6 of"},
{"Side 1A"        , "Side 1B"},
{"Side 1A"        , "Side 2A"},
{"Side 1A"        , "Side 2B"},
{"Side 1A"        , "Side 3A"},
{"Side 1A"        , "Side 3B"},
{"Side 1A"        , "Side 4A"},
{"Side 1A"        , "Side 4B"},
{"Side A"         , "Side B"},
};
struct machine_type {
   const char * name;
   const std::array<unsigned char, 16384> * os_standard_rom;
   const BBCMicroType * type;
   const DiscInterfaceDef * disc_interface;
   const std::array<unsigned char, 16384> * rom_array[16];
   bool ext_mem;
   bool beeblink;
   bool adji;
   BBCMicroParasiteType parasite_type;
   const char * parasite_os;
   const char * nvram_type;
   const char * nvram;
};
#define MACHINE_TYPES_COUNT 8
machine_type machine_types[] = {
   {  
      "B/Acorn 1770",
      &OS12_ROM,
      &BBC_MICRO_TYPE_B,
      &DISC_INTERFACE_ACORN_1770,
      {
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &writeable_ROM,
                &Acorn1770DFS_ROM,
                &BASIC2_ROM,
      },
      false,
      false,
      false,
      BBCMicroParasiteType_None,
      nullptr,
      "Unknown",
      ""
   },
   {  
      "B/Watford 1770 (DDB2)",
      &OS12_ROM,
      &BBC_MICRO_TYPE_B,
      &DISC_INTERFACE_WATFORD_DDB2,
      {
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &writeable_ROM,
                &WatfordDDFS_DDB2_ROM,
                &BASIC2_ROM,
      },
      false,
      false,
      false,
      BBCMicroParasiteType_None,
      nullptr,
      "Unknown",
      ""
   },      
   {  
      "B/Watford 1770 (DDB3)",
      &OS12_ROM,
      &BBC_MICRO_TYPE_B,
      &DISC_INTERFACE_WATFORD_DDB3,
      {
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &writeable_ROM,
                &WatfordDDFS_DDB3_ROM,
                &BASIC2_ROM,
      },
      false,
      false,
      false,
      BBCMicroParasiteType_None,
      nullptr,
      "Unknown",
      ""
   },
   {
      "B/Opus 1770",
      &OS12_ROM,
      &BBC_MICRO_TYPE_B,
      &DISC_INTERFACE_OPUS,
      {
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &writeable_ROM,
                &OpusDDOS_ROM,
                &BASIC2_ROM,
      },
      false,
      false,
      false,
      BBCMicroParasiteType_None,
      nullptr,
      "Unknown",
      ""
   },
   {
      "B/Opus CHALLENGER 256K",
      &OS12_ROM,
      &BBC_MICRO_TYPE_B,
      &DISC_INTERFACE_CHALLENGER_256K,
      {
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &writeable_ROM,
                &OpusChallenger_ROM,
                &BASIC2_ROM,
      },
      false,
      false,
      false,
      BBCMicroParasiteType_None,
      nullptr,
      "Unknown",
      ""
   },
   {
      "B/Opus CHALLENGER 512K",
      &OS12_ROM,
      &BBC_MICRO_TYPE_B,
      &DISC_INTERFACE_CHALLENGER_512K,
      {
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &writeable_ROM,
                &OpusChallenger_ROM,
                &BASIC2_ROM,
      },
      false,
      false,
      false,
      BBCMicroParasiteType_None,
      nullptr,
      "Unknown",
      ""
   },
   {
      "B+",
      &BPlusMOS_ROM,
      &BBC_MICRO_TYPE_B_PLUS,
      &DISC_INTERFACE_ACORN_1770,
      {
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &Acorn1770DFS_ROM,
                &BASIC2_ROM,
      },
      false,
      false,
      false,
      BBCMicroParasiteType_None,
      nullptr,
      "Unknown",
      ""
   },
   {
      "B+128",
      &BPlusMOS_ROM,
      &BBC_MICRO_TYPE_B_PLUS,
      &DISC_INTERFACE_ACORN_1770,
      {
                &writeable_ROM,
                &writeable_ROM,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &writeable_ROM,
                &writeable_ROM,
                &Acorn1770DFS_ROM,
                &BASIC2_ROM,
      },
      false,
      false,
      false,
      BBCMicroParasiteType_None,
      nullptr,
      "Unknown",
      ""
   },
};
// TODO: support the rest of the types
/*
        {
            "name": "Master 128 (MOS 3.20)",
            "os": {
                "standard_rom": "MOS320_MOS"
            },
            "type": "Master",
            "disc_interface": "Master 128",
            "roms": [
                null,
                null,
                null,
                null,
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                null,
                {
                    "standard_rom": "MOS320_DFS"
                },
                {
                    "standard_rom": "MOS320_VIEWSHEET"
                },
                {
                    "standard_rom": "MOS320_EDIT"
                },
                {
                    "standard_rom": "MOS320_BASIC4"
                },
                {
                    "standard_rom": "MOS320_ADFS"
                },
                {
                    "standard_rom": "MOS320_VIEW"
                },
                {
                    "standard_rom": "MOS320_TERMINAL"
                }
            ],
            "ext_mem": false,
            "beeblink": false,
            "adji": false,
            "parasite_type": "None",
            "parasite_os": null,
            "nvram_type": "Master128",
            "nvram": "0000000000c9ffff000017803703000102000000000000000000000000000000000000000000000000000000000000000000"
        },
        {
            "name": "Master 128 (MOS 3.50)",
            "os": {
                "standard_rom": "MOS350_MOS"
            },
            "type": "Master",
            "disc_interface": "Master 128",
            "roms": [
                null,
                null,
                null,
                null,
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                null,
                {
                    "standard_rom": "MOS350_DFS"
                },
                {
                    "standard_rom": "MOS350_VIEWSHEET"
                },
                {
                    "standard_rom": "MOS350_EDIT"
                },
                {
                    "standard_rom": "MOS350_BASIC4"
                },
                {
                    "standard_rom": "MOS350_ADFS"
                },
                {
                    "standard_rom": "MOS350_VIEW"
                },
                {
                    "standard_rom": "MOS350_TERMINAL"
                }
            ],
            "ext_mem": false,
            "beeblink": false,
            "adji": false,
            "parasite_type": "None",
            "parasite_os": null,
            "nvram_type": "Master128",
            "nvram": "0000000000c9ffff000017803703000102000000000000000000000000000000000000000000000000000000000000000000"
        },
        {
            "name": "Master Turbo (MOS 3.20)",
            "os": {
                "standard_rom": "MOS320_MOS"
            },
            "type": "Master",
            "disc_interface": "Master 128",
            "roms": [
                null,
                null,
                null,
                null,
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                null,
                {
                    "standard_rom": "MOS320_DFS"
                },
                {
                    "standard_rom": "MOS320_VIEWSHEET"
                },
                {
                    "standard_rom": "MOS320_EDIT"
                },
                {
                    "standard_rom": "MOS320_BASIC4"
                },
                {
                    "standard_rom": "MOS320_ADFS"
                },
                {
                    "standard_rom": "MOS320_VIEW"
                },
                {
                    "standard_rom": "MOS320_TERMINAL"
                }
            ],
            "ext_mem": false,
            "beeblink": false,
            "adji": false,
            "parasite_type": "MasterTurbo",
            "parasite_os": {
                "standard_rom": "MasterTurboParasite"
            },
            "nvram_type": "Master128",
            "nvram": "0000000000c9ffff000017803703000102000000000000000000000000000000000000000000000000000000000000000000"
        },
        {
            "name": "Master Turbo (MOS 3.50)",
            "os": {
                "standard_rom": "MOS350_MOS"
            },
            "type": "Master",
            "disc_interface": "Master 128",
            "roms": [
                null,
                null,
                null,
                null,
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                null,
                {
                    "standard_rom": "MOS350_DFS"
                },
                {
                    "standard_rom": "MOS350_VIEWSHEET"
                },
                {
                    "standard_rom": "MOS350_EDIT"
                },
                {
                    "standard_rom": "MOS350_BASIC4"
                },
                {
                    "standard_rom": "MOS350_ADFS"
                },
                {
                    "standard_rom": "MOS350_VIEW"
                },
                {
                    "standard_rom": "MOS350_TERMINAL"
                }
            ],
            "ext_mem": false,
            "beeblink": false,
            "adji": false,
            "parasite_type": "MasterTurbo",
            "parasite_os": {
                "standard_rom": "MasterTurboParasite"
            },
            "nvram_type": "Master128",
            "nvram": "0000000000c9ffff000017803703000102000000000000000000000000000000000000000000000000000000000000000000"
        },
        {
            "name": "B/Acorn 1770 + 6502 second processor",
            "os": {
                "standard_rom": "OS12"
            },
            "type": "B",
            "disc_interface": "Acorn 1770",
            "roms": [
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                {
                    "writeable": true
                },
                {
                    "standard_rom": "Acorn1770DFS"
                },
                {
                    "standard_rom": "BASIC2"
                }
            ],
            "ext_mem": false,
            "beeblink": false,
            "adji": false,
            "parasite_type": "External3MHz6502",
            "parasite_os": {
                "standard_rom": "TUBE110"
            },
            "nvram_type": "Unknown"
        },
        {
            "name": "Master Compact (MOS 5.00)",
            "os": {
                "standard_rom": "MOS500_MOS"
            },
            "type": "MasterCompact",
            "disc_interface": "Master 128",
            "roms": [
                null,
                null,
                null,
                null,
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                null,
                null,
                null,
                null,
                null,
                {
                    "standard_rom": "MOS500_ADFS"
                },
                {
                    "standard_rom": "MOS500_BASIC4"
                },
                {
                    "standard_rom": "MOS500_UTILS"
                }
            ],
            "ext_mem": false,
            "beeblink": false,
            "adji": false,
            "parasite_type": "None",
            "parasite_os": null,
            "nvram_type": "MasterCompact",
            "nvram": "0000000000edffff000017c037030001020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000b0"
        },
        {
            "name": "Master Compact (MOS 5.10)",
            "os": {
                "standard_rom": "MOS510_MOS"
            },
            "type": "MasterCompact",
            "disc_interface": "Master 128",
            "roms": [
                null,
                null,
                null,
                null,
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                null,
                null,
                null,
                null,
                null,
                {
                    "standard_rom": "MOS510_ADFS"
                },
                {
                    "standard_rom": "MOS510_BASIC4"
                },
                {
                    "standard_rom": "MOS510_UTILS"
                }
            ],
            "ext_mem": false,
            "beeblink": false,
            "adji": false,
            "parasite_type": "None",
            "parasite_os": null,
            "nvram_type": "MasterCompact",
            "nvram": "0000000000edffff000017c037030001020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000b0"
        },
        {
            "name": "Olivetti PC 128 S",
            "os": {
                "standard_rom": "MOSI510C_MOS"
            },
            "type": "MasterCompact",
            "disc_interface": "Master 128",
            "roms": [
                null,
                null,
                null,
                null,
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                {
                    "writeable": true
                },
                null,
                null,
                null,
                null,
                null,
                {
                    "standard_rom": "MOSI510C_ADFS"
                },
                {
                    "standard_rom": "MOSI510C_BASIC4"
                },
                {
                    "standard_rom": "MOSI510C_UTILS"
                }
            ],
            "ext_mem": false,
            "beeblink": false,
            "adji": false,
            "parasite_type": "None",
            "parasite_os": null,
            "nvram_type": "PC128S",
            "nvram": "0000000000edffff0000f9e320080a2c800003040000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000b2"
        }
*/

#endif