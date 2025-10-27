#include <shared/system.h>
#include <shared/CommandLineParser.h>
#include <memory>
#include <vector>
#include <set>
#include <map>
#include <regex>
#include <shared/path.h>
#include <shared/log.h>
#include <shared/testing.h>
#include <beeb/BBCMicro.h>
#include <beeb/sound.h>
#include <string>
#include <beeb/DiscImage.h>
#include <shared/debug.h>
#include <beeb/SaveTrace.h>
#include <beeb/TVOutput.h>
#include <beeb/MemoryDiscImage.h>
#include <shared/sha1.h>
#include <shared/file_io.h>
#include <shared/strings.h>
#include <inttypes.h>
#include <beeb/DiscGeometry.h>
#include <beeb/HardDiskImage.h>

#include <shared/enum_decl.h>
#include "test_beeb.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "test_beeb.inl"
#include <shared/enum_end.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <shared/pushwarn_case_fallthrough.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STB_IMAGE_IMPLEMENTATION

#ifdef _MSC_VER
#pragma warning(disable : 4244) //OPERATOR: conversion from TYPE to TYPE, possible loss of data
#endif

#include <stb_image.h>

#include <shared/popwarn.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifndef b2_SOURCE_DIR
#error
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetPathForStandardROM(StandardROM rom) {
    switch (rom) {
    default:
        TEST_FAIL("%s: unsupported StandardROM: %d (%s)", __func__, rom, GetStandardROMEnumName(rom));

    case StandardROM_OS12:
        return "OS12.ROM";

    case StandardROM_BPlusMOS:
        return "B+MOS.rom";

    case StandardROM_BASIC2:
        return "BASIC2.ROM";

    case StandardROM_Acorn1770DFS:
        return "acorn/DFS-2.26.rom";
    case StandardROM_WatfordDDFS_DDB2:
        return "watford/DDFS-1.53.rom";
    case StandardROM_WatfordDDFS_DDB3:
        return "watford/DDFS-1.54T.rom";
    case StandardROM_OpusDDOS:
        return "opus/OPUS-DDOS-3.45.rom";
    case StandardROM_OpusChallenger:
        return "opus/challenger-1.01.rom";

    case StandardROM_MOS320_ADFS:
        return "M128/3.20/adfs.rom";
    case StandardROM_MOS320_BASIC4:
        return "M128/3.20/basic4.rom";
    case StandardROM_MOS320_DFS:
        return "M128/3.20/dfs.rom";
    case StandardROM_MOS320_EDIT:
        return "M128/3.20/edit.rom";
    case StandardROM_MOS320_MOS:
        return "M128/3.20/mos.rom";
    case StandardROM_MOS320_TERMINAL:
        return "M128/3.20/terminal.rom";
    case StandardROM_MOS320_VIEW:
        return "M128/3.20/view.rom";
    case StandardROM_MOS320_VIEWSHEET:
        return "M128/3.20/viewsht.rom";

    case StandardROM_MOS350_ADFS:
        return "M128/3.50/adfs.rom";
    case StandardROM_MOS350_BASIC4:
        return "M128/3.50/basic4.rom";
    case StandardROM_MOS350_DFS:
        return "M128/3.50/dfs.rom";
    case StandardROM_MOS350_EDIT:
        return "M128/3.50/edit.rom";
    case StandardROM_MOS350_MOS:
        return "M128/3.50/mos.rom";
    case StandardROM_MOS350_TERMINAL:
        return "M128/3.50/terminal.rom";
    case StandardROM_MOS350_VIEW:
        return "M128/3.50/view.rom";
    case StandardROM_MOS350_VIEWSHEET:
        return "M128/3.50/viewsht.rom";

    case StandardROM_MasterTurboParasite:
        return "MasterTurboParasite.rom";
    case StandardROM_TUBE110:
        return "TUBE110.rom";

    case StandardROM_MOS500_ADFS:
        return "MCompact/5.00/adfs.rom";
    case StandardROM_MOS500_BASIC4:
        return "MCompact/5.00/basic4.rom";
    case StandardROM_MOS500_UTILS:
        return "MCompact/5.00/utils.rom";
    case StandardROM_MOS500_MOS:
        return "MCompact/5.00/mos.rom";

    case StandardROM_MOS510_ADFS:
        return "MCompact/5.10/adfs.rom";
    case StandardROM_MOS510_BASIC4:
        return "MCompact/5.10/basic4.rom";
    case StandardROM_MOS510_UTILS:
        return "MCompact/5.10/utils.rom";
    case StandardROM_MOS510_MOS:
        return "MCompact/5.10/mos.rom";

    case StandardROM_MOSI510C_ADFS:
        return "MCompact/I5.10C/adfs.rom";
    case StandardROM_MOSI510C_BASIC4:
        return "MCompact/I5.10C/basic4.rom";
    case StandardROM_MOSI510C_UTILS:
        return "MCompact/I5.10C/utils.rom";
    case StandardROM_MOSI510C_MOS:
        return "MCompact/I5.10C/mos.rom";

    case StandardROM_MOS511i_ADFS:
        return "MCompact/5.11i/adfs.rom";
    case StandardROM_MOS511i_BASIC4:
        return "MCompact/5.11i/basic4.rom";
    case StandardROM_MOS511i_UTILS:
        return "MCompact/5.11i/utils.rom";
    case StandardROM_MOS511i_MOS:
        return "MCompact/5.11i/mos.rom";
    case StandardROM_MOS511i_ARABIC:
        return "MCompact/5.11i/arabic.rom";
    case StandardROM_MOS511i_INTERNATIONAL:
        return "MCompact/5.11i/international.rom";
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct TestBBCType {
    const DiscInterface *disc_interface = nullptr;
    BBCMicroParasiteType parasite_type = BBCMicroParasiteType_None;
    bool configure_extube = false; //Default for test Master is INTUBE.
    bool configure_notube = false; //Default for test Master is TUBE.
    bool configure_hard = false;   //Default for test Master is FLOPPY.
    bool video_nula = false;
    bool scsi = false;
    bool mmfs = false;

    static_assert(ROMType_16KB == 0);
    ROMType rom_types[16] = {};
    std::string rom_paths[16];
    bool is_ram[16] = {};
    StandardROM os_rom = StandardROM_None; //used to infer the BBCMicroTypeID
    std::string os_path;
    std::string parasite_os_path;

    TestBBCType WithSecondProcessor(BBCMicroParasiteType parasite_type) const;
    TestBBCType WithConfigureEXTUBE() const;
    TestBBCType WithConfigureNOTUBE() const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TestBBCType TestBBCType::WithSecondProcessor(BBCMicroParasiteType parasite_type_) const {
    TEST_EQ_II(this->parasite_type, BBCMicroParasiteType_None);

    TestBBCType type = *this;
    type.parasite_type = parasite_type_;

    switch (type.parasite_type) {
    default:
        break;

    case BBCMicroParasiteType_External3MHz6502:
        type.parasite_os_path = GetPathForStandardROM(StandardROM_TUBE110);
        break;

    case BBCMicroParasiteType_MasterTurbo:
        type.parasite_os_path = GetPathForStandardROM(StandardROM_MasterTurboParasite);
        break;
    }

    return type;
}

static TestBBCType WithFlagSet(const TestBBCType *src, bool TestBBCType::*flag_mptr) {
    TestBBCType type = *src;

    type.*flag_mptr = true;

    return type;
}

TestBBCType TestBBCType::WithConfigureEXTUBE() const {
    return WithFlagSet(this, &TestBBCType::configure_extube);
}

TestBBCType TestBBCType::WithConfigureNOTUBE() const {
    return WithFlagSet(this, &TestBBCType::configure_notube);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void InitROM(TestBBCType *type, uint8_t bank, StandardROM rom) {
    TEST_GE_UU(bank, 0);
    TEST_LE_UU(bank, 15);
    TEST_TRUE(type->rom_paths[bank].empty());
    if (rom != StandardROM_None) {
        type->rom_paths[bank] = GetPathForStandardROM(rom);
    }
}

static void InitROMs(TestBBCType *type,
                     StandardROM os,
                     StandardROM romf = StandardROM_None,
                     StandardROM rome = StandardROM_None,
                     StandardROM romd = StandardROM_None,
                     StandardROM romc = StandardROM_None,
                     StandardROM romb = StandardROM_None,
                     StandardROM roma = StandardROM_None,
                     StandardROM rom9 = StandardROM_None,
                     StandardROM rom8 = StandardROM_None,
                     StandardROM rom7 = StandardROM_None,
                     StandardROM rom6 = StandardROM_None,
                     StandardROM rom5 = StandardROM_None,
                     StandardROM rom4 = StandardROM_None,
                     StandardROM rom3 = StandardROM_None,
                     StandardROM rom2 = StandardROM_None,
                     StandardROM rom1 = StandardROM_None,
                     StandardROM rom0 = StandardROM_None) {
    type->os_rom = os;
    type->os_path = GetPathForStandardROM(os);

    InitROM(type, 0xf, romf);
    InitROM(type, 0xe, rome);
    InitROM(type, 0xd, romd);
    InitROM(type, 0xc, romc);
    InitROM(type, 0xb, romb);
    InitROM(type, 0xa, roma);
    InitROM(type, 0x9, rom9);
    InitROM(type, 0x8, rom8);
    InitROM(type, 0x7, rom7);
    InitROM(type, 0x6, rom6);
    InitROM(type, 0x5, rom5);
    InitROM(type, 0x4, rom4);
    InitROM(type, 0x3, rom3);
    InitROM(type, 0x2, rom2);
    InitROM(type, 0x1, rom1);
    InitROM(type, 0x0, rom0);
}

static TestBBCType GetBTapeType() {
    TestBBCType type;

    InitROMs(&type, StandardROM_OS12, StandardROM_BASIC2);

    return type;
}

static TestBBCType GetBPlusType() {
    TestBBCType type;

    InitROMs(&type, StandardROM_BPlusMOS, StandardROM_BASIC2, StandardROM_Acorn1770DFS);

    type.disc_interface = &DISC_INTERFACE_ACORN_1770;

    return type;
}

static TestBBCType GetBBCBDiskType(const DiscInterface *disc_interface) {
    TestBBCType type = GetBTapeType();

    type.disc_interface = disc_interface;

    InitROM(&type, 14, type.disc_interface->fs_rom);

    return type;
}

static TestBBCType GetMasterType() {
    TestBBCType type;

    type.disc_interface = &DISC_INTERFACE_MASTER128;

    for (uint8_t bank = 4; bank < 8; ++bank) {
        type.is_ram[bank] = true;
    }

    return type;
}

static TestBBCType GetMasterMOS320Type() {
    TestBBCType type = GetMasterType();

    InitROMs(&type, StandardROM_MOS320_MOS, StandardROM_MOS320_TERMINAL, StandardROM_MOS320_VIEW, StandardROM_MOS320_ADFS, StandardROM_MOS320_BASIC4, StandardROM_MOS320_EDIT, StandardROM_MOS320_VIEWSHEET, StandardROM_MOS320_DFS);

    return type;
}

static TestBBCType GetMasterMOS350Type() {
    TestBBCType type = GetMasterType();

    InitROMs(&type, StandardROM_MOS350_MOS, StandardROM_MOS350_TERMINAL, StandardROM_MOS350_VIEW, StandardROM_MOS350_ADFS, StandardROM_MOS350_BASIC4, StandardROM_MOS350_EDIT, StandardROM_MOS350_VIEWSHEET, StandardROM_MOS350_DFS);

    return type;
}

static TestBBCType GetMasterCompactMOS500Type() {
    TestBBCType type = GetMasterType();

    InitROMs(&type, StandardROM_MOS500_MOS, StandardROM_MOS500_UTILS, StandardROM_MOS500_BASIC4, StandardROM_MOS500_ADFS);

    return type;
}

static TestBBCType GetMasterCompactMOS510Type() {
    TestBBCType type = GetMasterType();

    InitROMs(&type, StandardROM_MOS510_MOS, StandardROM_MOS510_UTILS, StandardROM_MOS510_BASIC4, StandardROM_MOS510_ADFS);

    return type;
}

static TestBBCType GetMasterCompactMOSI510CType() {
    TestBBCType type = GetMasterType();

    InitROMs(&type, StandardROM_MOSI510C_MOS, StandardROM_MOSI510C_UTILS, StandardROM_MOSI510C_BASIC4, StandardROM_MOSI510C_ADFS);

    return type;
}

static TestBBCType GetMasterCompactMOS511iType() {
    TestBBCType type = GetMasterType();

    InitROMs(&type, StandardROM_MOS511i_MOS, StandardROM_MOS511i_UTILS, StandardROM_MOS511i_BASIC4, StandardROM_MOS511i_ADFS);
    InitROM(&type, 8, StandardROM_MOS511i_ARABIC);
    InitROM(&type, 2, StandardROM_MOS511i_INTERNATIONAL);

    return type;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BBCMicroTypeID GetBBCMicroTypeID(const TestBBCType &type) {
    switch (type.os_rom) {
    default:
        TEST_FAIL("%s: unknown OS ROM: %d (%s)", __func__, type.os_rom, GetStandardROMEnumName(type.os_rom));
        break;

    case StandardROM_OS12:
        return BBCMicroTypeID_B;

    case StandardROM_BPlusMOS:
        return BBCMicroTypeID_BPlus;

    case StandardROM_MOS320_MOS:
    case StandardROM_MOS350_MOS:
        return BBCMicroTypeID_Master;

    case StandardROM_MOS500_MOS:
    case StandardROM_MOS510_MOS:
    case StandardROM_MOSI510C_MOS:
    case StandardROM_MOS511i_MOS:
        return BBCMicroTypeID_MasterCompact;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint32_t GetBBCMicroInitFlags(const TestBBCType &type) {
    uint32_t init_flags = 0;

    if (type.video_nula) {
        init_flags |= BBCMicroInitFlag_VideoNuLA;
    }

    // The B/B+/Master OS don't expect missing serial hardware!
    if (HasSerial(GetBBCMicroTypeID(type))) {
        init_flags |= BBCMicroInitFlag_Serial;
    }

    if (type.scsi) {
        init_flags |= BBCMicroInitFlag_SCSI;
    }

    if (type.mmfs) {
        init_flags |= BBCMicroInitFlag_MMFS;
    }

    return init_flags;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<uint8_t> GetNVRAMContents(const TestBBCType &type) {
    BBCMicroTypeID type_id = GetBBCMicroTypeID(type);
    switch (type_id) {
    default:
        TEST_FAIL("%s: unknown BBCMicroTypeID: %d (%s)", __func__, type_id, GetBBCMicroTypeIDEnumName(type_id));
        // fall through
    case BBCMicroTypeID_B:
        [[fallthrough]];
    case BBCMicroTypeID_BPlus:
        return {};

    case BBCMicroTypeID_Master:
        {
            std::vector<uint8_t> nvram(50);

            nvram[5] = 0xC9;        // 5 - LANG 12; FS 9
            nvram[6] = 0xFF;        // 6 - INSERT 0 ... INSERT 7
            nvram[7] = 0xFF;        // 7 - INSERT 8 ... INSERT 15
            nvram[8] = 0x00;        // 8
            nvram[9] = 0x00;        // 9
            nvram[10] = 0x17;       //10 - MODE 7; SHADOW 0; TV 0 1
            nvram[11] = 0x80;       //11 - FLOPPY
            nvram[12] = 55;         //12 - DELAY 55
            nvram[13] = 0x03;       //13 - REPEAT 3
            nvram[14] = 0x00;       //14
            nvram[15] = 1 << 5 | 1; //15 - PRINT 1; TUBE
            nvram[16] = 0x02;       //16 - LOUD; INTUBE

            if (type.configure_extube) {
                nvram[16] |= 4;
            }

            if (type.configure_notube) {
                nvram[15] &= ~1u;
            }

            if (type.scsi) {
                nvram[11] &= ~0x80u;
            }

            return nvram;
        }
        break;

    case BBCMicroTypeID_MasterCompact:
        {
            TEST_FALSE(type.configure_extube);
            TEST_FALSE(type.configure_notube);
            TEST_FALSE(type.scsi);

            std::vector<uint8_t> nvram(128);

            nvram[5] = 0xED;        // 5 - LANG 14; FS 13
            nvram[6] = 0xFF;        // 6 - INSERT 0 ... INSERT 7
            nvram[7] = 0xFF;        // 7 - INSERT 8 ... INSERT 15
            nvram[8] = 0x00;        // 8
            nvram[9] = 0x00;        // 9
            nvram[10] = 0x17;       //10 - MODE 7; SHADOW 0; TV 0 1
            nvram[11] = 0xC0;       //11 - FLOPPY; NODIR
            nvram[12] = 55;         //12 - DELAY 55
            nvram[13] = 0x03;       //13 - REPEAT 3
            nvram[14] = 0x00;       //14
            nvram[15] = 1 << 5 | 1; //15 - PRINT 1; TUBE
            nvram[16] = 0x02;       //16 - LOUD
            nvram[17] = 0x00;       //17 - unused?
            nvram[18] = 0x00;       //18 - joystick settings
            nvram[19] = 0x00;       //19 - country code

            // Additional flag to indicate contents are valid.
            //
            // Values for this are $b0 for MOS 5.00/MOS 5.10 or $b2 for MOS I5.10C/MOS
            // 5.11.
            switch (type.os_rom) {
            default:
                TEST_FAIL("%s: unknown Compact-type OS: %d (%s)", __func__, type.os_rom, GetStandardROMEnumName(type.os_rom));
                break;

            case StandardROM_MOS500_MOS:
            case StandardROM_MOS510_MOS:
                nvram[127] = 0xb0;
                break;

            case StandardROM_MOSI510C_MOS:
            case StandardROM_MOS511i_MOS:
                nvram[127] = 0xb2;
                break;
            }

            return nvram;
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestBBCMicro : public BBCMicro {
  public:
    std::string oswrch_output;
    std::string spool_output;
    std::string spool_output_name;

#if BBCMICRO_DEBUGGER
    class Writer {
      public:
        M6502Word addr = {};

        Writer(const Writer &) = default;

        void Addb(uint8_t a);
        void Addbw(uint8_t a, uint16_t b);
        void Addbb(uint8_t a, uint8_t b);

      protected:
      private:
        Writer() = default;

        TestBBCMicro *m_bbc = nullptr;
        uint32_t m_dso = 0;

        friend class TestBBCMicro;
    };
#endif

    explicit TestBBCMicro(const TestBBCType &type, const HardDiskImageSet &hard_disk_images = {}, std::string mmfs_image_path = "");

    void StartCaptureOSWRCH();
    void StopCaptureOSWRCH();

    void LoadFile(const std::string &path, uint32_t addr);
    void LoadDiskImage(int drive, const std::string &path);

    bool RunUntilOSWORD0(double max_num_seconds);

    // return value is video output.
    std::vector<uint32_t> RunForNFrames(size_t num_frames);

    void Paste(std::string text);

    uint32_t Update1();

    double GetSpeed() const;

    // flags to be used when code writes to $fc10.
    uint32_t GetTestTraceFlags() const;
    void SetTestTraceFlags(uint32_t flags);

    void SaveTestTrace(const std::string &stem);

#if BBCMICRO_DEBUGGER
    Writer GetWriter(uint16_t addr);
#endif

    uint8_t MustFindOpcode(const char *mnemonic) const;
    uint8_t MustFindOpcode(const char *mnemonic, M6502AddrMode mode) const;

    void SetBytes(M6502Word addr, const std::vector<uint8_t> &data);
    std::vector<uint8_t> GetBytes(M6502Word addr, size_t num_bytes) const;

  protected:
    void GotOSWRCH();
    virtual bool GotOSCLI(); //true=handled, false=ok to pass on to real OSCLI
  private:
    bool m_spooling = false;
    size_t m_oswrch_capture_count = 0;
    uint64_t m_num_ticks = 0;
    CycleCount m_num_cycles = {0};
#if BBCMICRO_TRACE
    std::shared_ptr<Trace> m_test_trace;
    uint32_t m_trace_flags = 0;
#else
    const uint32_t m_trace_flags = 0;
#endif

    void LoadParasiteOS(const std::string &name);

    static uint8_t ReadTestCommand(void *context, M6502Word addr);
    static void WriteTestCommand(void *context, M6502Word addr, uint8_t value);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(OUTPUT, "", &log_printer_stdout_and_debugger, true);
LOG_DEFINE(BBC_OUTPUT, "", &log_printer_stdout_and_debugger, true);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr uint16_t WRCHV = 0x20e;
static constexpr uint16_t WORDV = 0x20c;
static constexpr uint16_t CLIV = 0x208;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetOutputFileName(const std::string &path) {
    return PathJoined(BBC_TESTS_OUTPUT_FOLDER, path);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void CopyFile(const std::string &src_path, const std::string &dest_path) {
    std::vector<uint8_t> data;
    TEST_TRUE(LoadFile(&data, src_path, nullptr));

    TEST_TRUE(SaveFile(data, dest_path, nullptr, SaveFlag_CreateFolder));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SaveTextOutput2(const std::string &contents,
                            const std::string &test_name,
                            const std::string &type,
                            const std::string &suffix) {
    SaveTextFile(contents, GetOutputFileName(test_name + "." + type + "_" + suffix), nullptr, SaveFlag_CreateFolder);
    SaveTextFile(contents, GetOutputFileName(type + "/" + test_name + "." + suffix), nullptr, SaveFlag_CreateFolder);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetPrintable(const std::string &bbc_output) {
    std::string r;

    for (char c : bbc_output) {
        switch (c) {
        case 0:
        case 7:
        case 13:
            // ignore.
            break;

        case '`':
            r += u8"\u00a3"; //POUND SIGN
            break;

        default:
            if (c < 32 || c >= 127) {
                char tmp[100];
                snprintf(tmp, sizeof tmp, "`%02x`", c);
                r += tmp;
            } else {
                [[fallthrough]];
            case 10:
                r.push_back(c);
            }
            break;
        }
    }

    return r;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SaveTextOutput(const std::string &output, const std::string &test_name, const std::string &type) {
    std::string printable_output = GetPrintable(output);
    SaveTextOutput2(GetPrintable(output), test_name, type, "ascii.txt");
    SaveTextOutput2(output, test_name, type, "raw.dat");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Handle escaping of BeebLink names. See https://github.com/tom-seddon/beeblink/blob/5803109faeb6035de294d3f221c24a4f75ca963d/server/beebfs.ts#L63
//
// This isn't done very cleverly, but it only has to work with the test
// files...

static std::string GetBeebLinkChar(char c) {
    switch (c) {
    case '/':
    case '<':
    case '>':
    case ':':
    case '"':
    case '\\':
    case '|':
    case '?':
    case '*':
    case ' ':
    case '.':
    case '#':
    escape:
        return strprintf("#%02x", (unsigned)c);

    default:
        if (c < 32) {
            goto escape;
        } else if (c > 126) {
            goto escape;
        } else {
            return std::string(1, c);
        }
    }
}

static std::string GetBeebLinkName(const std::string &name) {
    TEST_EQ_SS(name.substr(1, 1), ".");

    std::string beeblink_name = GetBeebLinkChar(name[0]) + ".";

    for (size_t i = 2; i < name.size(); ++i) {
        beeblink_name += GetBeebLinkChar(name[i]);
    }

    return beeblink_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetTestFileName(const std::string &beeblink_volume_path,
                            const std::string &beeblink_drive,
                            const std::string &name) {
    return PathJoined(beeblink_volume_path,
                      beeblink_drive,
                      GetBeebLinkName(name));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::shared_ptr<const std::array<uint8_t, 16384>> LoadOSROM(const std::string &name) {
    std::string path = PathJoined(b2_SOURCE_DIR, "etc/roms", name);

    std::vector<uint8_t> data;
    TEST_TRUE(LoadFile(&data, path, nullptr));

    auto rom = std::make_shared<std::array<uint8_t, 16384>>();

    TEST_LE_UU(data.size(), rom->size());
    memcpy(rom->data(), data.data(), data.size());
    //for (size_t i = 0; i < data.size(); ++i) {
    //    (*rom)[i] = data[i];
    //}

    return rom;
}

static std::shared_ptr<const std::vector<uint8_t>> LoadSidewaysROM(const std::string &name) {
    std::shared_ptr<const std::array<uint8_t, 16384>> rom = LoadOSROM(name);
    return std::make_shared<std::vector<uint8_t>>(rom->begin(), rom->end());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void TestBBCMicro::Writer::Addb(uint8_t a) {
    m_bbc->DebugSetBytes(this->addr, m_dso, false, &a, 1);
    ++this->addr.w;
}
#endif

#if BBCMICRO_DEBUGGER
void TestBBCMicro::Writer::Addbw(uint8_t a, uint16_t b) {
    this->Addb(a);
    this->Addb(b & 0xff);
    this->Addb(b >> 8);
}
#endif

#if BBCMICRO_DEBUGGER
void TestBBCMicro::Writer::Addbb(uint8_t a, uint8_t b) {
    this->Addb(a);
    this->Addb(b);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TestBBCMicro::TestBBCMicro(const TestBBCType &type, const HardDiskImageSet &hard_disk_images, std::string mmfs_image_path)
    : BBCMicro(CreateBBCMicroType(GetBBCMicroTypeID(type), type.rom_types, BBCMicroTypeFlag_ROMBoard),
               type.disc_interface,
               type.parasite_type,
               GetNVRAMContents(type),
               nullptr,
               GetBBCMicroInitFlags(type),
               nullptr,
               hard_disk_images,
               std::move(mmfs_image_path),
               {0}) {
#if BBCMICRO_TRACE
    m_trace_flags = (BBCMicroTraceFlag_RTC |
                     BBCMicroTraceFlag_1770 |
                     BBCMicroTraceFlag_SystemVIA |
                     BBCMicroTraceFlag_UserVIA |
                     BBCMicroTraceFlag_VideoULA |
                     BBCMicroTraceFlag_SN76489);
#endif

    this->SetTeletextDimFlash(true);

    this->SetXFJIO(0xfc10, &ReadTestCommand, this, &WriteTestCommand, this);

    this->SetOSROM(LoadOSROM(type.os_path));
    for (uint8_t bank = 0; bank < 16; ++bank) {
        if (type.rom_paths[bank].empty()) {
            if (type.is_ram[bank]) {
                this->SetSidewaysRAM(bank, nullptr);
            }
        } else {
            TEST_FALSE(type.is_ram[bank]);
            this->SetSidewaysROM(bank, LoadSidewaysROM(type.rom_paths[bank]), type.rom_types[bank]);
        }
    }

    if (!type.parasite_os_path.empty()) {
        this->LoadParasiteOS(type.parasite_os_path);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::StartCaptureOSWRCH() {
    ++m_oswrch_capture_count;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::StopCaptureOSWRCH() {
    ASSERT(m_oswrch_capture_count > 0);
    --m_oswrch_capture_count;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::LoadFile(const std::string &path, uint32_t addr) {
    std::vector<uint8_t> contents;
    TEST_TRUE(::LoadFile(&contents, path, nullptr));

    if (this->GetParasiteType() == BBCMicroParasiteType_None || (addr & 0xffff0000) == 0xffff0000) {
        addr &= 0xffff;
        TEST_LE_UU(addr + contents.size(), 0x8000);
        for (size_t i = 0; i < contents.size(); ++i) {
            this->TestSetByte((uint16_t)(addr + i), contents[i]);
        }
    } else {
        for (size_t i = 0; i < contents.size(); ++i) {
            this->TestSetParasiteByte((uint16_t)(addr + i), contents[i]);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::shared_ptr<MemoryDiscImage> LoadDiskImage(const std::string &path) {
    std::vector<uint8_t> contents;
    TEST_TRUE(::LoadFile(&contents, path, nullptr));

    DiscGeometry geometry;
    TEST_TRUE(FindDiscGeometryFromFileDetails(&geometry, path.c_str(), contents.size(), nullptr));

    std::shared_ptr<MemoryDiscImage> image = MemoryDiscImage::LoadFromBuffer(path, "file", contents.data(), contents.size(), geometry, nullptr);
    TEST_NON_NULL(image);

    return image;
}

void TestBBCMicro::LoadDiskImage(int drive, const std::string &path) {

    this->SetDiscImage(drive, ::LoadDiskImage(path));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TestBBCMicro::RunUntilOSWORD0(double max_num_seconds) {
    const uint8_t *ram = this->GetRAM();
    const M6502 *cpu = this->GetM6502();

    CycleCount max_num_cycles = {(uint64_t)(max_num_seconds * CYCLES_PER_SECOND)};

    uint64_t start_ticks = GetCurrentTickCount();

    bool hit_osword0 = false;

    CycleCount num_cycles = {0};
    while (num_cycles.n < max_num_cycles.n) {

        uint32_t update_result = this->Update1();
        ++num_cycles.n;

        if (update_result & BBCMicroUpdateResultFlag_Host) {
            if (M6502_IsAboutToExecute(cpu)) {
                if (cpu->abus.b.l == ram[WORDV + 0] &&
                    cpu->abus.b.h == ram[WORDV + 1] &&
                    cpu->a == 0) {
                    hit_osword0 = true;
                    break;
                }
            }
        }
    }

    m_num_ticks += GetCurrentTickCount() - start_ticks;

    TEST_LE_UU(num_cycles.n, max_num_cycles.n);

    return hit_osword0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> TestBBCMicro::RunForNFrames(size_t num_frames) {
    TVOutput tv;

    VideoDataUnitCount version;
    const uint32_t *pixels = tv.GetTexturePixels(&version);

    SoundDataUnit temp_sound_data_unit;
    constexpr size_t MAX_NUM_VIDEO_DATA_UNITS = 1000;
    VideoDataUnit video_data_units[MAX_NUM_VIDEO_DATA_UNITS];

    size_t num_frames_got = 0;
    size_t video_data_unit_index = 0;

    while (num_frames_got < num_frames) {
        for (size_t i = 0; i < 1024; ++i) {
            uint32_t update_result = this->Update(&video_data_units[video_data_unit_index],
                                                  &temp_sound_data_unit);

            if (update_result & BBCMicroUpdateResultFlag_VideoUnit) {
                ++video_data_unit_index;
                if (video_data_unit_index >= MAX_NUM_VIDEO_DATA_UNITS) {
                    tv.Update(video_data_units, video_data_unit_index);
                    video_data_unit_index = 0;
                }
            }
        }

        tv.Update(video_data_units, video_data_unit_index);
        video_data_unit_index = 0;

        VideoDataUnitCount new_version;
        pixels = tv.GetTexturePixels(&new_version);
        if (new_version.n != version.n) {
            version = new_version;
            ++num_frames_got;
        }
    }

    std::vector<uint32_t> result(pixels, pixels + TV_TEXTURE_WIDTH * TV_TEXTURE_HEIGHT);
    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::Paste(std::string text) {
    this->StartPaste(std::make_shared<std::string>(std::move(text)));

    uint64_t start_ticks = GetCurrentTickCount();

    while (this->IsPasting()) {
        this->Update1();
    }

    m_num_ticks += GetCurrentTickCount() - start_ticks;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t TestBBCMicro::Update1() {
    VideoDataUnit temp_video_data_unit;
    SoundDataUnit temp_sound_data_unit;
    uint32_t update_result = this->Update(&temp_video_data_unit,
                                          &temp_sound_data_unit);

    ++m_num_cycles.n;

    if (update_result & BBCMicroUpdateResultFlag_Host) {
        const M6502 *cpu = this->GetM6502();

        if (M6502_IsAboutToExecute(cpu)) {
            const uint8_t *ram = this->GetRAM();

            if (cpu->abus.b.l == ram[WRCHV + 0] && cpu->abus.b.h == ram[WRCHV + 1]) {
                this->GotOSWRCH();
            } else if (cpu->abus.b.l == ram[CLIV + 0] && cpu->abus.b.h == ram[CLIV + 1]) {
                if (this->GotOSCLI()) {
                    this->TestRTS();
                }
            }
        }
    }

    return update_result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double TestBBCMicro::GetSpeed() const {
    double num_seconds = GetSecondsFromTicks(m_num_ticks);
    return m_num_cycles.n / (num_seconds * CYCLES_PER_SECOND);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t TestBBCMicro::GetTestTraceFlags() const {
    return m_trace_flags;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::SetTestTraceFlags(uint32_t flags) {
#if BBCMICRO_TRACE
    m_trace_flags = flags;
#else
    (void)flags;
    // not available...
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
static bool SaveTraceData(const void *data, size_t num_bytes, void *context) {
    return fwrite(data, 1, num_bytes, (FILE *)context) == num_bytes;
}
#endif

void TestBBCMicro::SaveTestTrace(const std::string &stem) {
    (void)stem;

#if BBCMICRO_TRACE
    if (!m_test_trace) {
        this->StopTrace(&m_test_trace);
    }

    if (!!m_test_trace) {
        std::string path = GetOutputFileName(strprintf("%s.trace.txt", stem.c_str()));
        LOGF(OUTPUT, "Saving trace to: %s\n", path.c_str());
        FILE *f = fopen(path.c_str(), "wb"); //always save with Unix-type line endings
        TEST_NON_NULL(f);

        ::SaveTrace(m_test_trace,
                    TraceOutputFlags_Cycles | TraceOutputFlags_AbsoluteCycles | TraceOutputFlags_RegisterNames,
                    &SaveTraceData,
                    f,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr);

        fclose(f);
        f = nullptr;
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
TestBBCMicro::Writer TestBBCMicro::GetWriter(uint16_t addr) {
    Writer writer;

    writer.m_bbc = this;
    writer.addr.w = addr;

    return writer;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t TestBBCMicro::MustFindOpcode(const char *mnemonic) const {
    const M6502 *cpu = this->GetM6502();

    int opcode = -1;
    for (int i = 0; i < 256; ++i) {
        const M6502DisassemblyInfo *di = &cpu->config->disassembly_info[i];

        if (strcasecmp(di->mnemonic, mnemonic) == 0) {
            if (opcode >= 0) {
                TEST_FAIL("ambiguous by-name opcode search for: %s", mnemonic);
            }

            opcode = i;
        }
    }

    return (uint8_t)opcode;
}

uint8_t TestBBCMicro::MustFindOpcode(const char *mnemonic, M6502AddrMode mode) const {
    const M6502 *cpu = this->GetM6502();

    for (int i = 0; i < 256; ++i) {
        const M6502DisassemblyInfo *di = &cpu->config->disassembly_info[i];

        if (di->mode == mode && strcasecmp(di->mnemonic, mnemonic) == 0) {
            return (uint8_t)i;
        }
    }

    TEST_FAIL("opcode not found: mnemonic=%s; mode=%d", mnemonic, mode);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::SetBytes(M6502Word addr_, const std::vector<uint8_t> &bytes) {
    M6502Word addr = addr_;

    for (uint8_t byte : bytes) {
        ASSERT(addr.w < 0x8000);
        this->TestSetByte(addr.w++, byte);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> TestBBCMicro::GetBytes(M6502Word addr_, size_t num_bytes) const {
    std::vector<uint8_t> bytes;
    const uint8_t *ram = this->GetRAM();

    M6502Word addr = addr_;

    for (size_t i = 0; i < num_bytes; ++i) {
        ASSERT(addr.w < 0x8000);
        bytes.push_back(ram[addr.w++]);
    }

    return bytes;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::LoadParasiteOS(const std::string &name) {
    std::string path = PathJoined(b2_SOURCE_DIR, "etc/roms", name);

    std::vector<uint8_t> data;
    TEST_TRUE(::LoadFile(&data, path, nullptr));

    auto rom = std::make_shared<std::array<uint8_t, 4096>>();

    TEST_LE_UU(data.size(), rom->size());
    for (size_t i = 0; i < data.size(); ++i) {
        (*rom)[rom->size() - data.size() + i] = data[i];
    }

    this->SetParasiteOS(rom);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t TestBBCMicro::ReadTestCommand(void *context, M6502Word addr) {
    (void)context, (void)addr;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::WriteTestCommand(void *context, M6502Word addr, uint8_t value) {
    (void)addr;
    auto m = (TestBBCMicro *)context;
    (void)m;

    if (value == 0) {
#if BBCMICRO_TRACE
        // Stop trace.
        std::shared_ptr<Trace> tmp;
        m->StopTrace(&tmp);
        if (!!tmp) {
            m->m_test_trace = tmp;
        }
#endif
    } else if (value == 1) {
#if BBCMICRO_TRACE
        m->StartTrace(m->m_trace_flags, 256 * 1048576);
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::GotOSWRCH() {
    const M6502 *cpu = this->GetM6502();
    auto c = (char)cpu->a;

    if (m_oswrch_capture_count > 0) {
        if (c == 8) {
            if (!this->oswrch_output.empty()) {
                this->oswrch_output.pop_back();
            }
        } else {
            this->oswrch_output.push_back(c);
        }
    }

    if (m_spooling) {
        this->spool_output.push_back(c);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TestBBCMicro::GotOSCLI() {
    const M6502 *cpu = this->GetM6502();
    const uint8_t *ram = this->GetRAM();

    M6502Word addr;
    addr.b.l = cpu->x;
    addr.b.h = cpu->y;
    // can't be in the sideways area
    TEST_FALSE(addr.w >= 0x8000 && addr.w <= 0xc000);

    std::string str;
    for (size_t i = 0; i < 256; ++i) {
        if (ram[addr.w] == 13) {
            break;
        }

        str.push_back((char)ram[addr.w]);
        ++addr.w;
    }

    std::string::size_type cmd_begin = str.find_first_not_of("* ");
    if (cmd_begin != std::string::npos) {
        std::string::size_type cmd_end = str.find_first_of(" ", cmd_begin);
        std::string cmd = str.substr(cmd_begin, cmd_end - cmd_begin);

        std::string::size_type args_begin = str.find_first_not_of(" ", cmd_end);
        if (args_begin == std::string::npos) {
            args_begin = str.size();
        }
        std::string args = str.substr(args_begin);
        LOGF(OUTPUT, "command: ``%s''\n", cmd.c_str());
        LOGF(OUTPUT, "args: ``%s''\n", args.c_str());
        if (cmd == "SPOOL") {
            if (!args.empty()) {
                ASSERT(!m_spooling);
                m_spooling = true;
                spool_output_name = args;
                return true;
            } else {
                ASSERT(m_spooling);
                m_spooling = false;
                return true;
            }
        }
    }

    LOGF(OUTPUT, "ignoring OSCLI: ``%s''\n", str.c_str());

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestSpooledOutput(const TestBBCMicro &bbc,
                       const std::string &beeblink_volume_path,
                       const std::string &beeblink_drive,
                       const std::string &test_name) {
    TEST_FALSE(bbc.spool_output.empty());
    if (!bbc.spool_output.empty()) {
        {
            LOGF(BBC_OUTPUT, "Spooled: ");
            LOGI(BBC_OUTPUT);
            LOG_STR(BBC_OUTPUT, GetPrintable(bbc.spool_output).c_str());
            LOG(BBC_OUTPUT).EnsureBOL();
        }

        std::vector<uint8_t> wanted_results;
        TEST_TRUE(LoadFile(&wanted_results,
                           GetTestFileName(beeblink_volume_path,
                                           beeblink_drive,
                                           bbc.spool_output_name),
                           nullptr));

        std::string wanted_output(wanted_results.begin(), wanted_results.end());

        {
            LOGF(BBC_OUTPUT, "Wanted: ");
            LOGI(BBC_OUTPUT);
            LOG_STR(BBC_OUTPUT, GetPrintable(wanted_output).c_str());
            LOG(BBC_OUTPUT).EnsureBOL();
        }

        SaveTextOutput(wanted_output, test_name, "wanted");
        SaveTextOutput(bbc.spool_output, test_name, "got");

        TEST_EQ_SS(bbc.spool_output, wanted_output);
        //LOGF(OUTPUT,"Match: %d\n",wanted_output==bbc.tspool_output);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool g_infer_wanted_images = false;

void RunImageTest(const std::string &wanted_png_src_path,
                  const std::string &png_name,
                  TestBBCMicro *beeb) {
    std::vector<uint32_t> got_image = beeb->RunForNFrames(3);

    // The emulator doesn't bother to fill in the alpha channel. Also, all the
    // pixels are the wrong way round for stb_image, which wants
    // DXGI_FORMAT_R8G8B8A8_UNORM.
    for (uint32_t &pixel : got_image) {
        uint32_t r = (uint8_t)(pixel >> 16);
        uint32_t g = (uint8_t)(pixel >> 8);
        uint32_t b = (uint8_t)(pixel >> 0);

        pixel = r << 0 | g << 8 | b << 16 | 0xffu << 24;
    }

    std::string got_png_path = GetOutputFileName(png_name + ".got.png");

    // Unlike the test_common stuff, stbi_write_png won't create the folder.
    TEST_TRUE(PathCreateFolder(PathGetFolder(got_png_path)));

    TEST_TRUE(stbi_write_png(got_png_path.c_str(),
                             TV_TEXTURE_WIDTH,
                             TV_TEXTURE_HEIGHT,
                             4,
                             got_image.data(),
                             TV_TEXTURE_WIDTH * 4));

    // Put a copy of the wanted PNG in the output folder, so it's accessible.
    // The differences PNG isn't always illuminating.
    std::string wanted_png_path = GetOutputFileName(png_name + ".wanted.png");
    std::vector<uint8_t> wanted_png_data;
    bool got_wanted_png_data = LoadFile(&wanted_png_data, wanted_png_src_path, nullptr);
    if (!got_wanted_png_data) {
        if (g_infer_wanted_images) {
            TEST_TRUE(LoadFile(&wanted_png_data, got_png_path, nullptr));
            TEST_TRUE(SaveFile(wanted_png_data, wanted_png_src_path, nullptr, SaveFlag_CreateFolder));
        } else {
            TEST_TRUE(got_wanted_png_data);
        }
    }
    TEST_TRUE(SaveFile(wanted_png_data, wanted_png_path, nullptr, SaveFlag_CreateFolder));

    int wanted_width, wanted_height;
    unsigned char *wanted_data = stbi_load(wanted_png_path.c_str(),
                                           &wanted_width,
                                           &wanted_height,
                                           nullptr,
                                           4);
    TEST_NON_NULL(wanted_data);
    TEST_EQ_II(wanted_width, TV_TEXTURE_WIDTH);
    TEST_EQ_II(wanted_height, TV_TEXTURE_HEIGHT);

    bool any_differences = false;
    std::vector<uint32_t> differences;
    for (size_t i = 0; i < got_image.size(); ++i) {
        uint32_t pixel = 0xff000000u;

        uint32_t got_rgb = got_image[i] & 0x00ffffff;
        uint32_t wanted_rgb = ((uint32_t)wanted_data[i * 4 + 0] << 0 |
                               (uint32_t)wanted_data[i * 4 + 1] << 8 |
                               (uint32_t)wanted_data[i * 4 + 2] << 16);

        if (got_rgb != wanted_rgb) {
            pixel |= got_rgb ^ wanted_rgb;
            //pixel|=0x00ffffff;
            any_differences = true;
        } else {
            pixel |= got_rgb >> 1 & 0x007f7f7f;
        }

        differences.push_back(pixel);
    }

    std::string differences_png_path = GetOutputFileName(png_name + ".differences.png");
    TEST_TRUE(stbi_write_png(differences_png_path.c_str(),
                             TV_TEXTURE_WIDTH,
                             TV_TEXTURE_HEIGHT,
                             4,
                             differences.data(),
                             TV_TEXTURE_WIDTH * 4));
    TEST_FALSE(any_differences);

    free(wanted_data), wanted_data = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetBeebLinkVolumePath() {
    return PathJoined(b2_SOURCE_DIR, "etc", "b2_tests");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Test {
  public:
    Test() = default;
    virtual ~Test() = 0;

    virtual std::string GetFullName() const = 0;
    virtual void Run() = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Test::~Test() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class StandardTest : public Test {
  public:
    StandardTest(const std::string &name, const TestBBCType type, const char *drive = "0")
        : m_type(std::move(type))
        , m_drive(drive)
        , m_name(name) {
    }

    std::string GetFullName() const override {
        return "standard." + m_name;
    }

    void Run() override {
        //RunStandardTest(GetBeebLinkVolumePath().c_str(),
        //                m_drive.c_str(),
        //                m_name.c_str(),
        //                m_bbc_micro_type,
        //                0,
        //                0);

        TestBBCMicro bbc(m_type);

        //{
        //    uint32_t trace_flags = bbc.GetTestTraceFlags();

        //    trace_flags &= ~clear_trace_flags;
        //    trace_flags |= set_trace_flags;

        //    bbc.SetTestTraceFlags(trace_flags);
        //}

        bbc.StartCaptureOSWRCH();
        bbc.RunUntilOSWORD0(10.0);
        //bbc.StartTrace(0, 256 * 1024 * 1024);

        //if (m_type.os_rom == StandardROM_MOS320_MOS) {
        //    bbc.Paste("*ROMS\r");
        //    bbc.RunUntilOSWORD0(20.0);
        //}

        // Putting PAGE at $1900 makes it easier to replicate the same
        // conditions on a real BBC B with DFS.
        //
        // (Most tests don't depend on the value of PAGE, but the T.TIMINGS
        // output is affected by it.)
        bbc.LoadFile(GetTestFileName(GetBeebLinkVolumePath(),
                                     m_drive,
                                     std::string("T.") + m_name),
                     0x1900);
        bbc.Paste("PAGE=&1900\rOLD\rRUN\r");
        bbc.RunUntilOSWORD0(20.0);

        {
            LOGF(BBC_OUTPUT, "All Output: ");
            LOGI(BBC_OUTPUT);
            LOG_STR(BBC_OUTPUT, GetPrintable(bbc.oswrch_output).c_str());
            LOG(BBC_OUTPUT).EnsureBOL();
        }

        std::string stem = strprintf("%s.%s.%s",
                                     this->GetFullName().c_str(),
                                     GetBBCMicroTypeIDEnumName(GetBBCMicroTypeID(m_type)),
                                     GetStandardROMEnumName(m_type.os_rom));

        TEST_TRUE(SaveTextFile(bbc.oswrch_output,
                               GetOutputFileName(strprintf("%s.all_output.txt", stem.c_str())),
                               nullptr,
                               SaveFlag_CreateFolder));

        bbc.SaveTestTrace(stem);

        TestSpooledOutput(bbc,
                          GetBeebLinkVolumePath(),
                          m_drive,
                          stem);

        LOGF(OUTPUT, "Speed: ~%.3fx\n", bbc.GetSpeed());
    }

  protected:
  private:
    TestBBCType m_type;
    std::string m_drive;
    std::string m_name;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class KevinEdwardsTest : public Test {
  public:
    KevinEdwardsTest(const std::string &name, std::string pre_paste_text)
        : m_file_name(name)
        , m_pre_paste_text(std::move(pre_paste_text)) {
    }

    std::string GetFullName() const override {
        return "kevin_edwards." + m_file_name;
    }

    void Run() override {
        bool save_trace = false; //TODO...

        TestBBCMicro bbc(GetBTapeType());

        bbc.SetTestTraceFlags(0);
        bbc.StartCaptureOSWRCH();
        bbc.RunUntilOSWORD0(10.0);
        bbc.LoadFile(GetTestFileName(GetBeebLinkVolumePath(),
                                     "2",
                                     "$." + m_file_name),
                     0x3000);

        TEST_LT_UU(m_pre_paste_text.size(), 250);
        bbc.Paste(m_pre_paste_text);

        const M6502 *cpu = bbc.GetM6502();

        bool good = false;

        uint64_t num_cycles = 0;
        uint64_t max_num_cycles = 100 * 1000 * 1000;
        while (num_cycles < max_num_cycles) {
            if (M6502_IsAboutToExecute(cpu)) {
                if (cpu->abus.w == 0xfff4 &&
                    cpu->a == 200 &&
                    cpu->x == 3) {
                    break;
                } else if (cpu->abus.w == 0xe00) {
                    good = true;
                    break;
                }
            }

            bbc.Update1();
            ++num_cycles;
        }

        if (save_trace) {
            // Not super useful... trace is rather large and utterly impenetrable.
            bbc.SaveTestTrace(this->GetFullName());
        }

        TEST_LT_UU(num_cycles, max_num_cycles);
        TEST_TRUE(good);
    }

  protected:
  private:
    std::string m_file_name;
    std::string m_pre_paste_text;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class dp111TimingTest : public Test {
  public:
    dp111TimingTest(const std::string &name, TestBBCType type)
        : m_ssd_stem(name)
        , m_type(std::move(type)) {
    }

    std::string GetFullName() const override {
        return "dp111." + m_ssd_stem;
    }

    void Run() override {
        std::string ssd_path = PathJoined(b2_SOURCE_DIR, "submodules/6502Timing", m_ssd_stem + ".ssd");

        TestBBCMicro bbc(m_type);

        bbc.StartCaptureOSWRCH();
        bbc.LoadDiskImage(0, ssd_path.c_str());
        bbc.RunUntilOSWORD0(10.0);
        bbc.Paste("*CAT\r");
        bbc.Paste("*RUN 6502tim\r");
        bbc.SetXFJIO(0xfcd0, nullptr, nullptr, &WriteFailureCount, this);
        bbc.RunUntilOSWORD0(20.0);

        {
            LOGF(BBC_OUTPUT, "All Output: ");
            LOGI(BBC_OUTPUT);
            LOG_STR(BBC_OUTPUT, GetPrintable(bbc.oswrch_output).c_str());
            LOG(BBC_OUTPUT).EnsureBOL();
        }

        printf("Number of failures: %d\n", m_num_failures);
        TEST_EQ_II(m_num_failures, 0);
    }

  protected:
  private:
    std::string m_ssd_stem;
    TestBBCType m_type;
    int m_num_failures = -1;

    static void WriteFailureCount(void *context, M6502Word addr, uint8_t value) {
        (void)addr;
        auto this_ = (dp111TimingTest *)context;

        this_->m_num_failures = value;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TubeTest : public Test {
  public:
    TubeTest(std::string name, std::string file_name, TestBBCType type, uint32_t load_addr, std::string pre_paste_text, std::string post_paste_text)
        : m_name(std::move(name))
        , m_file_name(std::move(file_name))
        , m_type(std::move(type))
        , m_load_addr(load_addr)
        , m_pre_paste_text(std::move(pre_paste_text))
        , m_post_paste_text(post_paste_text) {
    }

    std::string GetFullName() const override {
        return "tube." + m_name;
    }

    void Run() override {
        TestBBCMicro bbc(m_type);

        //bbc.StartTrace(0, 256 * 1024 * 1024);
        bbc.StartCaptureOSWRCH();
        bbc.RunUntilOSWORD0(10.0);
        bbc.LoadFile(GetTestFileName(GetBeebLinkVolumePath(),
                                     "4",
                                     "$." + m_file_name),
                     m_load_addr);
        //bbc.SaveTestTrace("test_tube");

        if (!m_pre_paste_text.empty()) {
            bbc.Paste(m_pre_paste_text);
        }

        bbc.Paste("RUN\r");

        bbc.RunUntilOSWORD0(200);

        if (!m_post_paste_text.empty()) {
            bbc.Paste(m_post_paste_text);
            bbc.RunUntilOSWORD0(10);
        }

        {
            LOGF(BBC_OUTPUT, "All Output: ");
            LOGI(BBC_OUTPUT);
            LOG_STR(BBC_OUTPUT, GetPrintable(bbc.oswrch_output).c_str());
            LOG(BBC_OUTPUT).EnsureBOL();
        }

        TestSpooledOutput(bbc,
                          GetBeebLinkVolumePath(),
                          "4",
                          this->GetFullName());
    }

  protected:
  private:
    std::string m_name;
    std::string m_file_name;
    TestBBCType m_type;
    uint32_t m_load_addr;
    std::string m_pre_paste_text;
    std::string m_post_paste_text;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TeletextTest : public Test {
  public:
    TeletextTest(const std::string &volume_name, const std::string &name, uint32_t load_addr, std::string paste_text, std::string png_name)
        : m_volume_name(volume_name)
        , m_stem(name)
        , m_load_addr(load_addr)
        , m_paste_text(std::move(paste_text))
        , m_png_name(std::move(png_name)) {
    }

    std::string GetFullName() const override {
        return "teletext." + m_stem;
    }

    void Run() override {
        std::string beeblink_volume_path = PathJoined(b2_SOURCE_DIR, "etc", m_volume_name);

        TestBBCMicro bbc(GetBTapeType());

        bbc.RunUntilOSWORD0(10.0);

        bbc.LoadFile(GetTestFileName(beeblink_volume_path, "0", "$." + m_stem),
                     m_load_addr);

        if (!m_paste_text.empty()) {
            bbc.Paste(m_paste_text);
            bbc.RunUntilOSWORD0(10.0);
        }

        RunImageTest(PathJoined(beeblink_volume_path, m_png_name),
                     m_volume_name + "." + m_stem,
                     &bbc);
    }

  protected:
  private:
    std::string m_volume_name;
    std::string m_stem;
    uint32_t m_load_addr;
    std::string m_paste_text;
    std::string m_png_name;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VideoULAModeTest : public Test {
  public:
    VideoULAModeTest(bool clock, bool flash, uint8_t mode, bool nula, bool nula_logical_mode, uint8_t nula_attribute_mode, bool nula_text_attribute_mode)
        : m_clock(clock)
        , m_flash(flash)
        , m_mode(mode)
        , m_nula(nula)
        , m_nula_logical_mode(nula_logical_mode)
        , m_nula_attribute_mode(nula_attribute_mode)
        , m_nula_text_attribute_mode(nula_text_attribute_mode) {
        if (!nula) {
            TEST_FALSE(m_nula_logical_mode);
            TEST_EQ_UU(m_nula_attribute_mode, 0);
        } else {
            TEST_LT_UU(m_nula_attribute_mode, 4);
        }
    }

    std::string GetFullName() const override {
        std::string name;
        name += this->GetDeviceName();
        name += ".ulamode.C" + std::to_string((int)m_clock) + ".F" + std::to_string((int)m_flash) + ".M" + std::to_string(m_mode);
        if (m_nula) {
            name += ".L" + std::to_string((int)m_nula_logical_mode);
            name += ".A" + std::to_string(m_nula_attribute_mode);
            name += ".T" + std::to_string((int)m_nula_text_attribute_mode);
        }

        return name;
    }

    void Run() override {
        TestBBCType type = GetMasterMOS320Type();
        type.video_nula = m_nula;
        //TestBBCMicroArgs args;
        //if (m_nula) {
        //    args.flags |= TestBBCMicroFlags_VideoNuLA;
        //}
        TestBBCMicro bbc(type); //TestBBCMicroType_Master128MOS320, args);

        bbc.RunUntilOSWORD0(10.0);

        bbc.LoadFile(GetTestFileName(GetBeebLinkVolumePath(), "5", "$.ULAMODE"), 0xe00);

        std::string paste_text = "C%=" + std::to_string(m_clock) + "\rF%=" + std::to_string(m_flash) + "\rM%=" + std::to_string(m_mode) + "\r";

        if (m_nula) {
            if (m_nula_logical_mode) {
                paste_text += "?&FE22=&11\r";
            }

            paste_text += strprintf("?&FE22=&%02X\r", 0x60 | m_nula_attribute_mode);

            if (m_nula_text_attribute_mode) {
                paste_text += "?&FE22=&71\r";
            }
        }

        paste_text += "OLD\rRUN\r";

        bbc.Paste(paste_text);

        bbc.RunUntilOSWORD0(10);

        std::string wanted_image_path = PathJoined(b2_SOURCE_DIR, "etc", "b2_tests/5/wanted_images/" + this->GetDeviceName() + ".ulamode/" + this->GetFullName() + ".png");
        RunImageTest(wanted_image_path,
                     this->GetFullName(),
                     &bbc);
    }

  protected:
  private:
    bool m_clock = false;
    bool m_flash = false;
    uint8_t m_mode = 0;
    bool m_nula = false;
    bool m_nula_logical_mode = false;
    uint8_t m_nula_attribute_mode = 0;
    bool m_nula_text_attribute_mode = false;

    std::string GetDeviceName() const {
        if (m_nula) {
            return "video_nula";
        } else {
            return "video_ula";
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VideoNuLADetectTest : public Test {
  public:
    VideoNuLADetectTest(std::string full_name, bool has_nula, std::string paste_text, bool should_detect)
        : m_full_name(std::move(full_name))
        , m_has_nula(has_nula)
        , m_paste_text(std::move(paste_text))
        , m_should_detect(should_detect) {
    }

    std::string GetFullName() const override {
        return m_full_name;
    }

    void Run() override {
        TestBBCType type = GetBTapeType();
        type.video_nula = m_has_nula;
        TestBBCMicro bbc(type);

        bbc.StartCaptureOSWRCH();
        bbc.RunUntilOSWORD0(10.0);

        bbc.LoadFile(GetTestFileName(GetBeebLinkVolumePath(), "5", "$.DETECTNULA"), 0x1900);

        if (!m_paste_text.empty()) {
            bbc.Paste(m_paste_text);
            bbc.RunUntilOSWORD0(10.0);
        }

        bbc.Paste("OLD\rRUN\r");
        bbc.RunUntilOSWORD0(10.0);

        const uint8_t *ram = bbc.GetRAM();
        TEST_EQ_II(m_should_detect, ram[0x8f] != 0);
    }

  protected:
  private:
    std::string m_full_name;
    bool m_has_nula;
    std::string m_paste_text;
    bool m_should_detect;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VideoNuLATest : public Test {
  public:
    VideoNuLATest(std::string category2, std::string basic_name, std::vector<std::pair<std::string, std::string>> vars)
        : m_category2(std::move(category2))
        , m_basic_name(std::move(basic_name))
        , m_vars(std::move(vars)) {
    }

    std::string GetFullName() const override {
        std::string name = this->GetNameStem();
        for (const std::pair<std::string, std::string> &name_and_value : m_vars) {
            name += "." + name_and_value.first + name_and_value.second;
        }
        return name;
    }

    void Run() override {
        TestBBCType type = GetBTapeType();
        type.video_nula = true;
        TestBBCMicro bbc(type);

        bbc.RunUntilOSWORD0(10.0);

        bbc.LoadFile(GetTestFileName(GetBeebLinkVolumePath(), "5", m_basic_name), 0xe00);

        std::string paste;
        for (const std::pair<std::string, std::string> &name_and_value : m_vars) {
            paste += name_and_value.first + "%=" + name_and_value.second + "\r";
        }
        paste += "OLD\rRUN\r";

        bbc.Paste(paste);

        bbc.RunUntilOSWORD0(10);

        std::string png_name = this->GetFullName();
        std::string wanted_image_path = PathJoined(b2_SOURCE_DIR, "etc", "b2_tests/5/wanted_images/" + this->GetNameStem() + "/" + png_name + ".png");
        RunImageTest(wanted_image_path, png_name, &bbc);
    }

  protected:
  private:
    std::string m_category2;
    std::string m_basic_name;
    std::vector<std::pair<std::string, std::string>> m_vars;

    std::string GetNameStem() const {
        return "video_nula." + m_category2;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
class DebuggerTestBreakpointsB : public Test {
  public:
    DebuggerTestBreakpointsB(std::string name, TestBBCType type, uint8_t host_io_flags_for_breakpoint, bool write)
        : m_name(std::move(name))
        , m_type(std::move(type))
        , m_host_io_flags_for_breakpoint(host_io_flags_for_breakpoint)
        , m_write(write) {
        TEST_EQ_UU(m_host_io_flags_for_breakpoint & ~7u, 0);
    }

    std::string GetFullName() const override {
        return m_name;
    }

    void Run() override {
        this->TestIO(0xfe02, true);
        this->TestIO(0xfc00, !(m_host_io_flags_for_breakpoint & HostIOFlag_IFJ));
        this->TestIO(0xfee0, !(m_host_io_flags_for_breakpoint & HostIOFlag_ITU));
    }

  protected:
  private:
    std::string m_name;
    TestBBCType m_type;
    uint8_t m_host_io_flags_for_breakpoint = 0;
    bool m_write = false;
    bool m_verbose = false;

    void TestIO(uint16_t addr, bool should_succeed) {
        if (m_host_io_flags_for_breakpoint & HostIOFlag_TST) {
            should_succeed = false;
        }

        printf("host_io_flags_for_breakpoint=%d addr=0x%x write=%s: should_succeed=%s\n", m_host_io_flags_for_breakpoint, addr, BOOL_STR(m_write), BOOL_STR(should_succeed));
        TestBBCMicro bbc(m_type);
        TEST_TRUE(bbc.GetTypeID() == BBCMicroTypeID_B || bbc.GetTypeID() == BBCMicroTypeID_BPlus);
        bbc.SetDebugState(std::make_shared<BBCMicro::DebugState>());
        if (m_verbose) {
            bbc.StartCaptureOSWRCH();
        }

        bbc.RunUntilOSWORD0(10.0);

        uint8_t opcode;
        uint8_t break_flag;
        BBCMicroHaltReason halt_reason;
        if (m_write) {
            opcode = bbc.MustFindOpcode("sta", M6502AddrMode_ABS);
            break_flag = BBCMicroByteDebugFlag_BreakWrite;
            halt_reason = BBCMicroHaltReason_Write;
        } else {
            opcode = bbc.MustFindOpcode("lda", M6502AddrMode_ABS);
            break_flag = BBCMicroByteDebugFlag_BreakRead;
            halt_reason = BBCMicroHaltReason_Read;
        }
        uint8_t rts = bbc.MustFindOpcode("rts");

        bbc.DebugSetReadByteDebugFlags({(uint16_t)(FIRST_IO_BIG_PAGE_INDEX.i + m_host_io_flags_for_breakpoint)}, addr, break_flag);

        TestBBCMicro::Writer w = bbc.GetWriter(0x70);
        w.Addbw(opcode, addr);
        w.Addb(rts);

        bbc.Paste("CALL &70\r");

        bbc.RunUntilOSWORD0(10.0);

        if (m_verbose) {
            LOGF(BBC_OUTPUT, "All Output: ");
            LOGI(BBC_OUTPUT);
            LOG_STR(BBC_OUTPUT, GetPrintable(bbc.oswrch_output).c_str());
            LOG(BBC_OUTPUT).EnsureBOL();
        }

        std::shared_ptr<const BBCMicro::DebugState> debug = bbc.GetDebugState();
        TEST_NON_NULL(debug);
        if (should_succeed) {
            TEST_NE_II(bbc.DebugGetHaltReason(), BBCMicroHaltReason_None);
            TEST_EQ_UU(debug->halt_reason, halt_reason);
            TEST_GT_II(debug->halt_addr, 0);
            TEST_EQ_UU((unsigned)debug->halt_addr, addr);
        } else {
            TEST_EQ_II(bbc.DebugGetHaltReason(), BBCMicroHaltReason_None);
        }
    }
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
class DebuggerTestBreakpointsMaster : public Test {
  public:
    DebuggerTestBreakpointsMaster(std::string name, TestBBCType type, uint8_t host_io_flags_for_breakpoint, uint8_t host_io_flags_for_system, bool write)
        : m_name(std::move(name))
        , m_type(std::move(type))
        , m_host_io_flags_for_breakpoint(host_io_flags_for_breakpoint)
        , m_host_io_flags_for_system(host_io_flags_for_system)
        , m_write(write) {
        TEST_EQ_UU(m_host_io_flags_for_breakpoint & ~7u, 0u);
        TEST_EQ_UU(m_host_io_flags_for_system & ~7u, 0u);
    }

    std::string GetFullName() const override {
        return m_name;
    }

    void Run() override {
        bool sys_tst = !!(m_host_io_flags_for_system & HostIOFlag_TST);
        bool bp_tst = !!(m_host_io_flags_for_breakpoint & HostIOFlag_TST);

        bool should_succeed_sheila;

        if (!sys_tst && !bp_tst) {
            // BP is in IO; R=access IO, W=access IO; bp always hit.
            should_succeed_sheila = true;
        } else if (!sys_tst && bp_tst) {
            // BP is in ROM; R=access IO, W=access IO; bp never hit.
            should_succeed_sheila = false;
        } else if (sys_tst && !bp_tst) {
            // BP is in IO; R=access ROM, W=access IO; bp hit for writes
            should_succeed_sheila = !!m_write;
        } else if (sys_tst && bp_tst) {
            // BP is in ROM; R=access ROM, W=access IO; bp hit for reads
            should_succeed_sheila = !m_write;
        } else {
            ASSERT(false); //(in)sanity check
            should_succeed_sheila = false;
        }

        this->TestIO(0xfe02, should_succeed_sheila);

        bool sys_ifj = !!(m_host_io_flags_for_system & HostIOFlag_IFJ);
        bool bp_ifj = !!(m_host_io_flags_for_breakpoint & HostIOFlag_IFJ);

        bool should_succeed_fj;

        if (!sys_tst && !bp_tst) {
            // BP is in IO; R=access IO, W=access IO; bp hit if IFJ matches.
            should_succeed_fj = sys_ifj == bp_ifj;
        } else if (!sys_tst && bp_tst) {
            // BP is in ROM; R=access IO, W=access IO; bp never hit.
            should_succeed_fj = false;
        } else if (sys_tst && !bp_tst) {
            // BP is in IO; R=access ROM, W=access IO; bp hit for writes if IFJ matches
            should_succeed_fj = m_write && sys_ifj == bp_ifj;
        } else if (sys_tst && bp_tst) {
            // BP is in ROM; R=access ROM, W=access IO; bp hit for reads
            should_succeed_fj = !m_write;
        } else {
            ASSERT(false); //(in)sanity check
            should_succeed_fj = false;
        }

        this->TestIO(0xfc00, should_succeed_fj);

        bool sys_itu = !!(m_host_io_flags_for_system & HostIOFlag_ITU);
        bool bp_itu = !!(m_host_io_flags_for_breakpoint & HostIOFlag_ITU);

        bool should_succeed_tube;
        if (!sys_tst && !bp_tst) {
            // BP is in IO; R=access IO, W=access IO; bp hit if IFJ matches.
            should_succeed_tube = sys_itu == bp_itu;
        } else if (!sys_tst && bp_tst) {
            // BP is in ROM; R=access IO, W=access IO; bp never hit.
            should_succeed_tube = false;
        } else if (sys_tst && !bp_tst) {
            // BP is in IO; R=access ROM, W=access IO; bp hit for writes if IFJ matches
            should_succeed_tube = m_write && sys_itu == bp_itu;
        } else if (sys_tst && bp_tst) {
            // BP is in ROM; R=access ROM, W=access IO; bp hit for reads
            should_succeed_tube = !m_write;
        } else {
            ASSERT(false); //(in)sanity check
            should_succeed_tube = false;
        }

        this->TestIO(0xfee0, should_succeed_tube);
    }

  protected:
  private:
    std::string m_name;
    TestBBCType m_type;
    uint8_t m_host_io_flags_for_breakpoint = 0;
    uint8_t m_host_io_flags_for_system = 0;
    bool m_write = false;
    bool m_trace = false;

    std::string GetDescription(uint8_t f) {
        std::string s;

        s += f & HostIOFlag_TST ? "TST" : "___";
        s += "|";
        s += f & HostIOFlag_IFJ ? "IFJ" : "XFJ";
        s += "|";
        s += f & HostIOFlag_ITU ? "ITU" : "XTU";
        s += " (" + std::to_string(f) + ")";

        return s;
    }

    void TestIO(uint16_t addr, bool should_succeed) {
        printf("bp=%s sys=%s addr=0x%x write=%s: should_succeed=%s\n", GetDescription(m_host_io_flags_for_breakpoint).c_str(), GetDescription(m_host_io_flags_for_system).c_str(), addr, BOOL_STR(m_write), BOOL_STR(should_succeed));

        TestBBCMicro bbc(m_type);
        bbc.SetDebugState(std::make_shared<BBCMicro::DebugState>());
        TEST_TRUE(bbc.GetTypeID() == BBCMicroTypeID_Master || bbc.GetTypeID() == BBCMicroTypeID_MasterCompact);
        bbc.RunUntilOSWORD0(10.0);

        const uint8_t php = bbc.MustFindOpcode("php");
        const uint8_t plp = bbc.MustFindOpcode("plp");
        const uint8_t pha = bbc.MustFindOpcode("pha");
        const uint8_t pla = bbc.MustFindOpcode("pla");
        const uint8_t sei = bbc.MustFindOpcode("sei");
        const uint8_t rts = bbc.MustFindOpcode("rts");
        const uint8_t and_imm = bbc.MustFindOpcode("and", M6502AddrMode_IMM);
        const uint8_t ora_imm = bbc.MustFindOpcode("ora", M6502AddrMode_IMM);
        const uint8_t lda_abs = bbc.MustFindOpcode("lda", M6502AddrMode_ABS);
        const uint8_t sta_abs = bbc.MustFindOpcode("sta", M6502AddrMode_ABS);
        //const uint8_t lda_imm = bbc.MustFindOpcode("lda", M6502AddrMode_IMM);
        const uint8_t opcode = m_write ? sta_abs : lda_abs;

        TestBBCMicro::Writer w = bbc.GetWriter(0x70);

        w.Addb(php);
        w.Addb(sei);
        w.Addbw(lda_abs, 0xfe34);
        w.Addb(pha);
        w.Addbb(and_imm, (uint8_t)~0x70);                  //clear ITU+IFJ+TSTS
        w.Addbb(ora_imm, m_host_io_flags_for_system << 4); //they're the same layout as the ACCCON bits
        w.Addbw(sta_abs, 0xfe34);
        w.Addbw(opcode, addr);
        w.Addb(pla);
        w.Addbw(sta_abs, 0xfe34);
        w.Addb(plp);
        w.Addb(rts);

        ASSERT(w.addr.w <= 0x90);

        bbc.DebugSetReadByteDebugFlags({(uint16_t)(FIRST_IO_BIG_PAGE_INDEX.i + m_host_io_flags_for_breakpoint)},
                                       addr,
                                       m_write ? BBCMicroByteDebugFlag_BreakWrite : BBCMicroByteDebugFlag_BreakRead);

        if (m_trace) {
            bbc.StartTrace(0, 256 * 1024 * 1024);
        }

        bbc.Paste("CALL&70\r");
        bbc.RunUntilOSWORD0(10.0);

        if (m_trace) {
            bbc.SaveTestTrace(m_name + "." + strprintf("%04x", addr));
        }

        std::shared_ptr<const BBCMicro::DebugState> debug = bbc.GetDebugState();
        TEST_NON_NULL(debug);
        if (should_succeed) {
            TEST_NE_II(bbc.DebugGetHaltReason(), BBCMicroHaltReason_None);
            TEST_EQ_UU(debug->halt_reason, m_write ? BBCMicroHaltReason_Write : BBCMicroHaltReason_Read);
            TEST_GT_II(debug->halt_addr, 0);
            TEST_EQ_UU((unsigned)debug->halt_addr, addr);
        } else {
            TEST_EQ_II(bbc.DebugGetHaltReason(), BBCMicroHaltReason_None);
        }
    }
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetPrinterBufferDataString(const PrinterBuffer &printer_buffer) {
    std::vector<uint8_t> data = printer_buffer.GetData();
    data.push_back(0); //don't mind me...

    return std::string((char *)data.data());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class PrinterTest : public Test {
  public:
    PrinterTest(std::string name, TestBBCType type)
        : m_name(std::move(name))
        , m_type(std::move(type)) {
    }

    std::string GetFullName() const override {
        return m_name;
    }

    void Run() override {
        TestBBCMicro bbc(m_type);
        bbc.RunUntilOSWORD0(10.0);

        PrinterBuffer printer_buffer;

        bbc.SetPrinterEnabled(true);
        bbc.SetPrinterBuffer(&printer_buffer);

        bbc.Paste("*FX6\rVDU 2:PRINT \"PRINTER TEST\":VDU 3\r");

        bbc.RunUntilOSWORD0(10.0);

        TEST_EQ_SS(GetPrinterBufferDataString(printer_buffer), "PRINTER TEST\n\r");
    }

  protected:
  private:
    std::string m_name;
    TestBBCType m_type;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DiskAccessTest : public Test {
  public:
    DiskAccessTest(std::string name, TestBBCType type, FSType fs_type, int master_acccon_io_flags = -1)
        : m_name(std::move(name))
        , m_type(std::move(type))
        , m_fs_type(fs_type)
        , m_master_acccon_io_flags(master_acccon_io_flags) {
        if (m_master_acccon_io_flags >= 0) {
            TEST_EQ_UU((unsigned)m_master_acccon_io_flags & ~3u, 0u);
        }
    }

    std::string GetFullName() const override {
        return m_name;
    }

    void Run() override {
        this->InitDiskImage();

        std::vector<uint8_t> random_data[2];
        for (size_t i = 0; i < 2; ++i) {
            TEST_TRUE(LoadFile(&random_data[i], PathJoined(b2_SOURCE_DIR, "etc/tests/random." + std::to_string(i)) + ".dat", nullptr));
            TEST_EQ_UU(random_data[i].size(), 8192);
            random_data[i].resize(random_data[i].size() - 200); //make it not an exact multiple of sector or track size
        }

        {
            TestBBCMicro bbc(m_type, this->GetHardDiskImageSet(), this->GetMMFSImagePath());

            TestFailFnAdder fn_adder;
            if (m_verbose) {
                bbc.StartCaptureOSWRCH();

                fn_adder.Add([&bbc](const TestFailArgs *) {
                    PrintCapturedOutput(bbc, "failed save");
                });
            }

            bbc.SetDiscImage(0, this->GetFloppyDiskImage());
            //bbc.LoadDiskImage(0, PathJoined(b2_SOURCE_DIR, "etc/discs", m_blank_disk_image_name));
            bbc.RunUntilOSWORD0(10.0);

            this->Start(&bbc);

            bbc.SetBytes(ADDRESS, random_data[0]);
            bbc.Paste(strprintf("*SAVE TEST %04X+%04zX\r", ADDRESS.w, random_data[0].size()));
            bbc.RunUntilOSWORD0(10.0);

            if (m_fs_type == FSType_DFS) {
                bbc.SetBytes(ADDRESS, random_data[1]);
                bbc.Paste(strprintf("*SAVE :2.TEST2 %04X+%04zX\r", ADDRESS.w, random_data[1].size()));
                bbc.RunUntilOSWORD0(10.0);
            }

            if (m_fs_type == FSType_ADFS) {
                bbc.Paste("*DISMOUNT\r");
                bbc.RunUntilOSWORD0(10.0);
            }

            if (m_verbose) {
                PrintCapturedOutput(bbc, "successful save");
            }
        }

        //disc_image->SaveToFile("C:\\temp\\agh.dsd", nullptr);

        this->TestLoad("TEST", random_data[0]);
        if (m_fs_type == FSType_DFS) {
            this->TestLoad(":2.TEST2", random_data[1]);
        }
    }

  protected:
    static constexpr M6502Word ADDRESS = {0x2000};
    std::string m_name;
    TestBBCType m_type;
    const FSType m_fs_type;
    int m_master_acccon_io_flags = -1;
    bool m_verbose = true;

    virtual void InitDiskImage() = 0;

    virtual std::shared_ptr<DiscImage> GetFloppyDiskImage() const {
        return {};
    }

    virtual std::shared_ptr<HardDiskImage> GetHardDiskImage() const {
        return {};
    }

    virtual std::string GetMMFSImagePath() const {
        return "";
    }

  private:
    HardDiskImageSet GetHardDiskImageSet() const {
        HardDiskImageSet set;

        set.images[0] = this->GetHardDiskImage();

        return set;
    }

    static void PrintCapturedOutput(const TestBBCMicro &bbc, const char *step) {
        LOGF(BBC_OUTPUT, "All %s output: ", step);
        LOGI(BBC_OUTPUT);
        LOG_STR(BBC_OUTPUT, GetPrintable(bbc.oswrch_output).c_str());
        LOG(BBC_OUTPUT).EnsureBOL();
    }

    void Start(TestBBCMicro *bbc) {
        std::string stuff;

        stuff += "MODE 7\r"; //make room for test data

        // There's no check that the setting makes sense. The caller just has to
        // supply -1 when inappropriate.
        if (m_master_acccon_io_flags >= 0) {
            stuff += strprintf("?&FE34=(?&FE34 AND &%02X) OR &%02X\r", (uint8_t)~(3 << 4), m_master_acccon_io_flags << 4);
        }

        switch (m_fs_type) {
        default:
            TEST_FAIL("%s: unknown FS type: %d (%s)", __func__, m_fs_type, GetFSTypeEnumName(m_fs_type));
            break;

        case FSType_DFS:
            stuff += "*DISK\r";
            break;

        case FSType_ADFS:
            stuff += "*ADFS\r";

            // Always do an explicit *MOUNT, as NODIR is the config default for
            // MOS 5.00+.
            stuff += "*MOUNT 0\r";
            break;

        case FSType_MMFS:
            stuff += "*MMFS\r";
            break;
        }

        //stuff += "*FX6\r";

        bbc->Paste(stuff);
        bbc->RunUntilOSWORD0(10.0);
    }

    void TestLoad(const std::string &file_name, const std::vector<uint8_t> &random_data) {
        TestBBCMicro bbc(m_type, this->GetHardDiskImageSet(), this->GetMMFSImagePath());

        TestFailFnAdder fn_adder;
        if (m_verbose) {
            bbc.StartCaptureOSWRCH();

            fn_adder.Add([&bbc, name = this->GetFullName()](const TestFailArgs *) {
                //bbc.SaveTestTrace(name);
                PrintCapturedOutput(bbc, "failed load");
            });
        }

        ////bbc.SetPrinterBuffer(&printer_buffer);
        ////bbc.SetPrinterEnabled(true);
        //TEST_NON_NULL(disc_image);
        bbc.SetDiscImage(0, this->GetFloppyDiskImage());

        bbc.RunUntilOSWORD0(10.0);

        this->Start(&bbc);

        size_t extra_size = 100;

        // OS 1.20 skips clearing the first byte in every page, so overwrite the
        // memory with known data. (Since b2's behaviour might change if
        // https://github.com/tom-seddon/b2/issues/49 ever gets fixed)
        std::vector<uint8_t> clear_data(random_data.size() + extra_size, 0);
        for (size_t i = 0; i < random_data.size(); ++i) {
            clear_data[i] = 0xff;
        }
        bbc.SetBytes(ADDRESS, clear_data);

        bbc.Paste("*LOAD " + file_name + "\r");

        //bbc.StartTrace(BBCMicroTraceFlag_1770, 1024 * 1024 * 1024);

        bbc.RunUntilOSWORD0(10.0);

        std::vector<uint8_t> wanted_data = random_data;
        wanted_data.resize(wanted_data.size() + extra_size);

        std::vector<uint8_t> got_data = bbc.GetBytes(ADDRESS, wanted_data.size());

        TEST_EQ_AA(got_data.data(), wanted_data.data(), wanted_data.size());

        if (m_verbose) {
            PrintCapturedOutput(bbc, "successful load");
        }
    }

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class FloppyDiskAccessTest : public DiskAccessTest {
  public:
    FloppyDiskAccessTest(std::string name, TestBBCType type, FSType fs_type, std::string blank_disk_image_name, int master_acccon_io_flags = -1)
        : DiskAccessTest(std::move(name), std::move(type), fs_type, master_acccon_io_flags)
        , m_blank_disk_image_name(std::move(blank_disk_image_name)) {
    }

  protected:
    void InitDiskImage() override {
        TEST_NULL(m_disk_image);

        std::string path = PathJoined(b2_SOURCE_DIR, "etc/discs", m_blank_disk_image_name);

        m_disk_image = LoadDiskImage(path);
    }

    std::shared_ptr<DiscImage> GetFloppyDiskImage() const override {
        return m_disk_image;
    }

  private:
    std::string m_blank_disk_image_name;
    std::shared_ptr<MemoryDiscImage> m_disk_image;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MMFSDiskAccessTest : public DiskAccessTest {
  public:
    MMFSDiskAccessTest(std::string name, TestBBCType type, int master_acccon_io_flags = -1)
        : DiskAccessTest(std::move(name), std::move(type), FSType_MMFS, master_acccon_io_flags) {

        TEST_FALSE(m_type.mmfs);
        m_type.mmfs = true;

        TEST_EQ_II(m_type.rom_types[0], ROMType_16KB);
        TEST_TRUE(m_type.rom_paths[0].empty());

        std::string rom_name;
        if (IsMasterSeries(GetBBCMicroTypeID(m_type))) {
            rom_name = "MAMMFS.rom";
        } else {
            rom_name = "MMFS.rom";
        }

        // ROM path is relative to etc/roms... bit of a bodge needed here.
        m_type.rom_paths[0] = PathJoined("../mmfs_1_59_20250720_1149/MMFS/M/", rom_name);
    }

  protected:
    void InitDiskImage() override {
        std::string src_path = PathJoined(b2_SOURCE_DIR, "etc/discs", "mmfs_test.mmb");
        std::string dest_path = this->GetMMFSImagePath();
        CopyFile(src_path, dest_path);
    }

    std::string GetMMFSImagePath() const override {
        return GetOutputFileName(this->GetFullName() + ".mmfs_test.mmb");
    }

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HardDiskAccessTest : public DiskAccessTest {
  public:
    HardDiskAccessTest(std::string name, TestBBCType type, int master_acccon_io_flags)
        : DiskAccessTest(std::move(name), std::move(type), FSType_ADFS, master_acccon_io_flags) {
        m_type.scsi = true;
    }

  protected:
    std::string GetNameStem() const {
        return GetOutputFileName(this->GetFullName() + ".hd");
    }

    void InitDiskImage() override {
        std::string src_stem = PathJoined(b2_SOURCE_DIR, "etc/discs", "10MB");
        std::string dest_stem = this->GetNameStem();

        this->CopyHardDiskFile(src_stem, dest_stem, ".dat");
        this->CopyHardDiskFile(src_stem, dest_stem, ".dsc");
    }

    std::shared_ptr<HardDiskImage> GetHardDiskImage() const override {
        std::shared_ptr<HardDiskImage> image = HardDiskImage::CreateForFile(this->GetNameStem() + ".dat", nullptr);
        TEST_NON_NULL(image);
        return image;
    }

  private:
    void CopyHardDiskFile(const std::string &src_stem, const std::string &dest_stem, const std::string &ext) {
        CopyFile(src_stem + ext, dest_stem + ext);
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

#define DEBUGGER_ONLY(T) T

#else

class PlaceholderDebuggerTest : public Test {
  public:
    template <class... Types>
    PlaceholderDebuggerTest(std::string name, Types...)
        : m_name(std::move(name)) {
    }

    std::string GetFullName() const override {
        return m_name;
    }

    void Run() override {
        // ...
    }

  protected:
  private:
    std::string m_name;
};

#define DEBUGGER_ONLY(T) PlaceholderDebuggerTest

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    bool verbose = false;
    std::vector<std::string> test_name_strs;
    std::vector<std::regex> test_name_regexes;
    bool list = false;
    bool list_for_check_ctest_log = false;
    bool infer_wanted_images = false;
    bool wip = false;
    bool reverse = false;
};

static Options GetOptions(int argc, char *argv[]) {
    CommandLineParser p;

    Options options;

    bool help;
    p.AddHelpOption(&help);

    std::vector<std::string> test_name_patterns;

    p.AddOption('v', "verbose").SetIfPresent(&options.verbose).Help("be more verbose");
    p.AddOption('t', "test").Meta("TEST").AddArgToList(&options.test_name_strs).Help("run test(s) matching TEST, a case-insensitive string");
    p.AddOption('T', "test-pattern").Meta("TEST").AddArgToList(&test_name_patterns).Help("run test(s) matching TEST, a case-insensitive glob pattern");
    p.AddOption('l', "list").SetIfPresent(&options.list).Help("list all test names");
    p.AddOption('l', "list-for-check_ctest_log").SetIfPresent(&options.list_for_check_ctest_log).Help("list all test names, formatted for the benefit of check_ctest_log");
    p.AddOption(0, "infer-wanted-images").SetIfPresent(&options.infer_wanted_images).Help("wanted images may not exist if one doesn't, assume the got image is the right one, and copy it to the wanted image path");
    p.AddOption(0, "wip").SetIfPresent(&options.wip).Help("include WIP tests that aren't finished or passing yet");

    // intended for use when adding new tests, in conjunction with -T, on the
    // basis that the last one added is the most likely to fail.
    p.AddOption(0, "reverse").SetIfPresent(&options.reverse).Help("work through the test list in reverse order");

    if (!p.Parse(argc, argv)) {
        exit(1);
    }

    if (help) {
        exit(0);
    }

    for (const std::string &test_name_pattern : test_name_patterns) {
        std::string test_name_regex_str;
        for (char c : test_name_pattern) {
            if (isdigit(c) || isalpha(c) || c == '_') {
                test_name_regex_str.push_back(c);
            } else if (c == '.') {
                test_name_regex_str += "\\.";
            } else if (c == '*') {
                test_name_regex_str += ".*";
            } else {
                fprintf(stderr, "FATAL: unsupported pattern: %s\n", test_name_pattern.c_str());
                exit(1);
            }
        }

        if (options.verbose) {
            printf("Regex: %s\n", test_name_regex_str.c_str());
        }

        std::regex re;
        try {
            re = std::regex(std::regex(test_name_regex_str, std::regex_constants::icase | std::regex_constants::extended));
        } catch (const std::regex_error &e) {
            fprintf(stderr, "FATAL: error in regex: %s\nFATAL: %s\n", test_name_regex_str.c_str(), e.what());
            exit(1);
        }

        options.test_name_regexes.push_back(std::move(re));
    }

    return options;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    Options options = GetOptions(argc, argv);

    std::vector<std::unique_ptr<Test>> all_tests;
    all_tests.push_back(std::make_unique<StandardTest>("VTIMERS", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.AC1", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.AC2", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.AC3", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.AC4", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.AC5", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.AC6", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.AC7", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.C1", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.C2", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.C3", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.C4", GetMasterMOS320Type()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.C5", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.I1", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.I2", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.PB2", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.PB7", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.T11", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.T21", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.T22", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VIA.PB6", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("TIMINGS", GetBTapeType()));
    all_tests.push_back(std::make_unique<StandardTest>("VTIMEOU", GetMasterMOS320Type()));
    all_tests.push_back(std::make_unique<StandardTest>("VPOLL", GetMasterMOS320Type()));
    all_tests.push_back(std::make_unique<StandardTest>("NOP1", GetMasterMOS320Type(), "1"));
    all_tests.push_back(std::make_unique<KevinEdwardsTest>("Alien8", "P%=&900:[OPT 2:LDX #4:LDY #0:.loop LDA &3000,Y:STA &C00,Y:INY:BNE loop:INC loop+2:INC loop+5:DEX:BNE loop:LDA #1:STA &FC10:JMP &CEA:]\rCALL &900\r"));
    all_tests.push_back(std::make_unique<KevinEdwardsTest>("Nightsh", "P%=&3900:[OPT3:LDX #9:LDY #0:.loop LDA &3000,Y:STA &700,Y:INY:BNE loop:INC loop+2:INC loop+5:DEX:BNE loop:LDA #&40:STA0:LDA #&F:STA 1:LDA #1:STA &FC10:JMP &F01:]\rCALL &3900\r"));
    all_tests.push_back(std::make_unique<KevinEdwardsTest>("Jetman", "P%=&380:[OPT 3:LDX #10:LDY #0:.loop LDA &3000,Y:STA &600,Y:INY:BNE loop:INC loop+2:INC loop+5:DEX:BNE loop:LDA #&1F:STA 0:LDA #&F:STA 1:LDA #1:STA &FC10:JMP &F01:]\rCALL &380\r"));
    all_tests.push_back(std::make_unique<dp111TimingTest>("6502timing", GetBBCBDiskType(&DISC_INTERFACE_ACORN_1770)));
    all_tests.push_back(std::make_unique<dp111TimingTest>("6502timing1M", GetBBCBDiskType(&DISC_INTERFACE_ACORN_1770)));
    all_tests.push_back(std::make_unique<dp111TimingTest>("65C12timing", GetMasterMOS320Type()));
    all_tests.push_back(std::make_unique<dp111TimingTest>("65C12timing1M", GetMasterMOS320Type()));
    //all_tests.push_back(std::make_unique<TubeTest>("xtu_prst", "PRST", TestBBCMicroType_Master128MOS320WithExternal3MHz6502, TestBBCMicroFlags_ConfigureNoTube, 0xffff1900, "PAGE=&1900\rOLD\r!&70=&C4FF3AD5\rI%=FALSE\r", ""));
    //all_tests.push_back(std::make_unique<TubeTest>("itu_prst", "PRST", TestBBCMicroType_Master128MOS320WithMasterTurbo, TestBBCMicroFlags_ConfigureNoTube, 0xffff1900, "PAGE=&1900\rOLD\r!&70=&C4FF3AD5\rI%=TRUE\r", ""));
    //all_tests.push_back(std::make_unique<TubeTest>("xtu_r124", "R124", TestBBCMicroType_Master128MOS320WithExternal3MHz6502, TestBBCMicroFlags_ConfigureExTube, 0x800, "OLD\r*SPOOL X.R124\r", "*SPOOL\r"));
    //all_tests.push_back(std::make_unique<TubeTest>("xtu_r3", "R3", TestBBCMicroType_Master128MOS320WithExternal3MHz6502, 0, 0x800, "OLD\r*SPOOL X.R3\r", "*SPOOL\r"));

    all_tests.push_back(std::make_unique<TubeTest>("xtu_prst",
                                                   "PRST",
                                                   GetMasterMOS320Type().WithSecondProcessor(BBCMicroParasiteType_External3MHz6502).WithConfigureNOTUBE(),
                                                   0xffff1900,
                                                   "PAGE=&1900\rOLD\r!&70=&C4FF3AD5\rI%=FALSE\r",
                                                   ""));
    all_tests.push_back(std::make_unique<TubeTest>("itu_prst",
                                                   "PRST",
                                                   GetMasterMOS320Type().WithSecondProcessor(BBCMicroParasiteType_MasterTurbo).WithConfigureNOTUBE(),
                                                   0xffff1900,
                                                   "PAGE=&1900\rOLD\r!&70=&C4FF3AD5\rI%=TRUE\r",
                                                   ""));
    all_tests.push_back(std::make_unique<TubeTest>("xtu_r124",
                                                   "R124",
                                                   GetMasterMOS320Type().WithSecondProcessor(BBCMicroParasiteType_External3MHz6502).WithConfigureEXTUBE(),
                                                   0x800,
                                                   "OLD\r*SPOOL X.R124\r",
                                                   "*SPOOL\r"));
    all_tests.push_back(std::make_unique<TubeTest>("xtu_r3",
                                                   "R3",
                                                   GetMasterMOS320Type().WithSecondProcessor(BBCMicroParasiteType_External3MHz6502),
                                                   0x800,
                                                   "OLD\r*SPOOL X.R3\r",
                                                   "*SPOOL\r"));

    all_tests.push_back(std::make_unique<TeletextTest>("teletest_v1", "ENGTEST", 0x7c00, "", "engtest.png"));
    all_tests.push_back(std::make_unique<TeletextTest>("teletest_v1", "RED", 0x7c00, "", "red.png"));
    all_tests.push_back(std::make_unique<TeletextTest>("teletest_v1", "TELETST", 0xe00, "OLD\rRUN\r", "teletst.png"));
    all_tests.push_back(std::make_unique<TeletextTest>("taliadon_test", "TEST", 0xe00, "OLD\rRUN\r", "taliadon_test.png"));

    for (int nula = 0; nula < 2; ++nula) {
        for (int nula_logical = 0; nula_logical < 2; ++nula_logical) {
            for (uint8_t nula_attribute_mode = 0; nula_attribute_mode < 4; ++nula_attribute_mode) {
                for (int nula_text_attribute_mode = 0; nula_text_attribute_mode < 2; ++nula_text_attribute_mode) {
                    if (!nula) {
                        if (nula_logical || nula_attribute_mode > 0 || nula_text_attribute_mode) {
                            // Irrelevant cases. The ordinary Video ULA only has one
                            // palette, no attribute mode, etc.
                            continue;
                        }
                    } else {
                        if (nula_attribute_mode == 2 || nula_attribute_mode == 3 || nula_text_attribute_mode) {
                            // TODO...
                            continue;
                        }
                    }

                    for (int clock = 0; clock < 2; ++clock) {
                        for (int flash = 0; flash < 2; ++flash) {
                            for (uint8_t mode = 0; mode < 4; ++mode) {
                                all_tests.push_back(std::make_unique<VideoULAModeTest>(clock != 0,
                                                                                       flash != 0,
                                                                                       mode,
                                                                                       nula != 0,
                                                                                       nula_logical != 0,
                                                                                       nula_attribute_mode,
                                                                                       nula_text_attribute_mode != 0));
                            }
                        }
                    }
                }
            }
        }
    }

    for (int m = 0; m < 9; ++m) {
        for (int o = 0; o < 16; ++o) {
            auto test = new VideoNuLATest("scroll1", "N.2-SCROLL-1", {{"M", std::to_string(m)}, {"O", strprintf("%02d", o)}});
            all_tests.push_back(std::unique_ptr<VideoNuLATest>(test));
        }
    }

    for (int m = 0; m < 9; ++m) {
        for (int n = 0; n < 16; ++n) {
            auto test = new VideoNuLATest("blank1", "N.3-BLANK-1", {{"M", std::to_string(m)}, {"N", strprintf("%02d", n)}});
            all_tests.push_back(std::unique_ptr<VideoNuLATest>(test));
        }
    }

    all_tests.push_back(std::make_unique<VideoNuLADetectTest>("video_ula.detect_nula", false, "", false));
    all_tests.push_back(std::make_unique<VideoNuLADetectTest>("video_nula.detect_nula.enabled", false, "", false));
    all_tests.push_back(std::make_unique<VideoNuLADetectTest>("video_nula.detect_nula.disabled", false, "?&FE22=&50\r", false));

    for (uint8_t host_io_flags_for_breakpoint = 0; host_io_flags_for_breakpoint < 8; ++host_io_flags_for_breakpoint) {
        for (int write = 0; write < 2; ++write) {
            std::string suffix = ".BP" + std::to_string(host_io_flags_for_breakpoint) + "." + (write ? "w" : "r");
            all_tests.push_back(std::make_unique<DEBUGGER_ONLY(DebuggerTestBreakpointsB)>("debug.bp.b" + suffix,
                                                                                          GetBTapeType(),
                                                                                          host_io_flags_for_breakpoint,
                                                                                          !!write));
            all_tests.push_back(std::make_unique<DEBUGGER_ONLY(DebuggerTestBreakpointsB)>("debug.bp.bplus" + suffix,
                                                                                          GetBPlusType(),
                                                                                          host_io_flags_for_breakpoint,
                                                                                          !!write));
        }
    }

    for (uint8_t host_io_flags_for_breakpoint = 0; host_io_flags_for_breakpoint < 8; ++host_io_flags_for_breakpoint) {
        for (uint8_t host_io_flags_for_system = 0; host_io_flags_for_system < 8; ++host_io_flags_for_system) {
            for (int write = 0; write < 2; ++write) {
                all_tests.push_back(std::make_unique<DEBUGGER_ONLY(DebuggerTestBreakpointsMaster)>((std::string("debug.bp.master128") +
                                                                                                    ".BP" + std::to_string(host_io_flags_for_breakpoint) +
                                                                                                    ".SYS" + std::to_string(host_io_flags_for_system) +
                                                                                                    "." + (write ? "w" : "r")),
                                                                                                   GetMasterMOS320Type(),
                                                                                                   host_io_flags_for_breakpoint,
                                                                                                   host_io_flags_for_system,
                                                                                                   !!write));
            }
        }
    }

    all_tests.push_back(std::make_unique<PrinterTest>("printer.os120", GetBTapeType()));
    all_tests.push_back(std::make_unique<PrinterTest>("printer.os200", GetBPlusType()));
    all_tests.push_back(std::make_unique<PrinterTest>("printer.mos320", GetMasterMOS320Type()));
    all_tests.push_back(std::make_unique<PrinterTest>("printer.mos350", GetMasterMOS350Type()));
    all_tests.push_back(std::make_unique<PrinterTest>("printer.mos500", GetMasterCompactMOS500Type()));
    all_tests.push_back(std::make_unique<PrinterTest>("printer.mos510", GetMasterCompactMOS510Type()));
    all_tests.push_back(std::make_unique<PrinterTest>("printer.mos511i", GetMasterCompactMOS511iType()));
    all_tests.push_back(std::make_unique<PrinterTest>("printer.mosI510c", GetMasterCompactMOSI510CType()));

    all_tests.push_back(std::make_unique<FloppyDiskAccessTest>("disk.floppy.b.sd.acorndfs", GetBBCBDiskType(&DISC_INTERFACE_ACORN_1770), FSType_DFS, "80.dsd"));
    all_tests.push_back(std::make_unique<FloppyDiskAccessTest>("disk.floppy.b.sd.watford.ddb2", GetBBCBDiskType(&DISC_INTERFACE_WATFORD_DDB2), FSType_DFS, "80.dsd"));
    all_tests.push_back(std::make_unique<FloppyDiskAccessTest>("disk.floppy.b.sd.watford.ddb3", GetBBCBDiskType(&DISC_INTERFACE_WATFORD_DDB3), FSType_DFS, "80.dsd"));
    all_tests.push_back(std::make_unique<FloppyDiskAccessTest>("disk.floppy.b.sd.opus", GetBBCBDiskType(&DISC_INTERFACE_OPUS), FSType_DFS, "80.dsd"));
    all_tests.push_back(std::make_unique<FloppyDiskAccessTest>("disk.floppy.b.sd.challenger", GetBBCBDiskType(&DISC_INTERFACE_CHALLENGER_512K), FSType_DFS, "80.dsd"));

    all_tests.push_back(std::make_unique<FloppyDiskAccessTest>("disk.floppy.b.dd.watford.ddb2", GetBBCBDiskType(&DISC_INTERFACE_WATFORD_DDB2), FSType_DFS, "blank_wddfs_disc.31files.ddd"));
    all_tests.push_back(std::make_unique<FloppyDiskAccessTest>("disk.floppy.b.dd.watford.ddb3", GetBBCBDiskType(&DISC_INTERFACE_WATFORD_DDB3), FSType_DFS, "blank_wddfs_disc.31files.ddd"));
    all_tests.push_back(std::make_unique<FloppyDiskAccessTest>("disk.floppy.b.dd.opus", GetBBCBDiskType(&DISC_INTERFACE_OPUS), FSType_DFS, "blank_ddos_disc.ddd"));
    all_tests.push_back(std::make_unique<FloppyDiskAccessTest>("disk.floppy.b.dd.challenger", GetBBCBDiskType(&DISC_INTERFACE_CHALLENGER_512K), FSType_DFS, "blank_ddos_disc.ddd"));

    all_tests.push_back(std::make_unique<FloppyDiskAccessTest>("disk.floppy.bplus", GetBPlusType(), FSType_DFS, "80.dsd"));

    for (int io_flags = 0; io_flags < 4; ++io_flags) {
        all_tests.push_back(std::make_unique<FloppyDiskAccessTest>(strprintf("disk.floppy.master.%d.mos320.dfs", io_flags), GetMasterMOS320Type(), FSType_DFS, "80.dsd", io_flags));
        all_tests.push_back(std::make_unique<FloppyDiskAccessTest>(strprintf("disk.floppy.master.%d.mos320.adfs", io_flags), GetMasterMOS320Type(), FSType_ADFS, "adl.adl", io_flags));
        all_tests.push_back(std::make_unique<FloppyDiskAccessTest>(strprintf("disk.floppy.master.%d.mos350.dfs", io_flags), GetMasterMOS350Type(), FSType_DFS, "80.dsd", io_flags));
        all_tests.push_back(std::make_unique<FloppyDiskAccessTest>(strprintf("disk.floppy.master.%d.mos350.adfs", io_flags), GetMasterMOS350Type(), FSType_ADFS, "adl.adl", io_flags));
        all_tests.push_back(std::make_unique<FloppyDiskAccessTest>(strprintf("disk.floppy.compact.%d.mos500.adfs", io_flags), GetMasterCompactMOS500Type(), FSType_ADFS, "adl.adl", io_flags));
        all_tests.push_back(std::make_unique<FloppyDiskAccessTest>(strprintf("disk.floppy.compact.%d.mos510.adfs", io_flags), GetMasterCompactMOS510Type(), FSType_ADFS, "adl.adl", io_flags));
        all_tests.push_back(std::make_unique<FloppyDiskAccessTest>(strprintf("disk.floppy.compact.%d.mosI510C.adfs", io_flags), GetMasterCompactMOSI510CType(), FSType_ADFS, "adl.adl", io_flags));
        all_tests.push_back(std::make_unique<FloppyDiskAccessTest>(strprintf("disk.floppy.compact.%d.mos511i.adfs", io_flags), GetMasterCompactMOS511iType(), FSType_ADFS, "adl.adl", io_flags));
    }

    // SCSI doesn't apply when IFJ.
    for (uint8_t io_flags : std::vector<uint8_t>{0, HostIOFlag_ITU}) {
        all_tests.push_back(std::make_unique<HardDiskAccessTest>(strprintf("disk.hard.master.%d.mos320.adfs", io_flags), GetMasterMOS320Type(), io_flags));
        all_tests.push_back(std::make_unique<HardDiskAccessTest>(strprintf("disk.hard.master.%d.mos350.adfs", io_flags), GetMasterMOS350Type(), io_flags));
    }

    all_tests.push_back(std::make_unique<MMFSDiskAccessTest>(strprintf("disk.mmfs.b"), GetBTapeType()));
    all_tests.push_back(std::make_unique<MMFSDiskAccessTest>(strprintf("disk.mmfs.bplus"), GetBPlusType()));
    for (int io_flags = 0; io_flags < 4; ++io_flags) {
        all_tests.push_back(std::make_unique<MMFSDiskAccessTest>(strprintf("disk.mmfs.master.%d.mos320", io_flags), GetMasterMOS320Type(), io_flags));
        all_tests.push_back(std::make_unique<MMFSDiskAccessTest>(strprintf("disk.mmfs.master.%d.mos350", io_flags), GetMasterMOS350Type(), io_flags));
        all_tests.push_back(std::make_unique<MMFSDiskAccessTest>(strprintf("disk.mmfs.compact.%d.mos500", io_flags), GetMasterCompactMOS500Type(), io_flags));
        all_tests.push_back(std::make_unique<MMFSDiskAccessTest>(strprintf("disk.mmfs.compact.%d.mos510", io_flags), GetMasterCompactMOS510Type(), io_flags));
        all_tests.push_back(std::make_unique<MMFSDiskAccessTest>(strprintf("disk.mmfs.compact.%d.mosI510C", io_flags), GetMasterCompactMOSI510CType(), io_flags));
        all_tests.push_back(std::make_unique<MMFSDiskAccessTest>(strprintf("disk.mmfs.compact.%d.mos511i", io_flags), GetMasterCompactMOS511iType(), io_flags));
    }

    std::set<std::string> names;
    for (const std::unique_ptr<Test> &test : all_tests) {
        names.insert(test->GetFullName());
    }
    TEST_EQ_UU(names.size(), all_tests.size());

    if (options.list) {
        for (const std::string &name : names) {
            printf("%s\n", name.c_str());
        }

        return 0;
    }

    if (options.list_for_check_ctest_log) {
        for (const std::string &name : names) {
            printf("2fcf9707-9498-4a03-9b27-ef501fa2fbb6:test_beeb.%s\n", name.c_str());
        }

        return 0;
    }

    g_infer_wanted_images = options.infer_wanted_images;

    bool ran_any_tests = false;

    for (size_t test_index = 0; test_index < all_tests.size(); ++test_index) {
        std::unique_ptr<Test> &test = options.reverse ? all_tests[all_tests.size() - 1 - test_index] : all_tests[test_index];
        bool run = options.test_name_regexes.empty() && options.test_name_strs.empty();

        if (!run) {
            for (const std::regex &test_name_regex : options.test_name_regexes) {
                if (std::regex_match(test->GetFullName(), test_name_regex)) {
                    run = true;
                    break;
                }
            }
        }

        if (!run) {
            for (const std::string &test_name_str : options.test_name_strs) {
                if (strcasecmp(test->GetFullName().c_str(), test_name_str.c_str()) == 0) {
                    run = true;
                    break;
                }
            }
        }

        if (!run) {
            if (options.verbose) {
                printf("skipping test: %s\n", test->GetFullName().c_str());
            }
            continue;
        }

        if (options.verbose) {
            printf("starting test: %s\n", test->GetFullName().c_str());
        }
        printf("ea73a8dc-2d1a-43bc-ae41-078e441e53c5:test_beeb.%s\n", test->GetFullName().c_str());

        uint64_t start_ticks = GetCurrentTickCount();

        test->Run();
        ran_any_tests = true;

        uint64_t end_ticks = GetCurrentTickCount();

        if (options.verbose) {
            printf("test finished: %s (took %.3f seconds)\n", test->GetFullName().c_str(), GetSecondsFromTicks(end_ticks - start_ticks));
        }
    }

    TEST_TRUE(ran_any_tests);
}
