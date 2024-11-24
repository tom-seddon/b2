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
#include <beeb/DiscImage.h>
#include <shared/sha1.h>
#include <shared/file_io.h>

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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STB_IMAGE_IMPLEMENTATION
#ifdef _MSC_VER
#pragma warning(disable : 4244) //OPERATOR: conversion from TYPE to TYPE, possible loss of data
#endif

#include <stb_image.h>

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

struct TestBBCMicroArgs {
    uint32_t flags = 0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestBBCMicro : public BBCMicro {
  public:
    std::string oswrch_output;
    std::string spool_output;
    std::string spool_output_name;

    // Flags are a combination of TestBBCMicroFlags
    explicit TestBBCMicro(TestBBCMicroType type, const TestBBCMicroArgs &args = {});

    void StartCaptureOSWRCH();
    void StopCaptureOSWRCH();

    void LoadFile(const std::string &path, uint32_t addr);
    void LoadSSD(int drive, const std::string &path);

    void RunUntilOSWORD0(double max_num_seconds);

    // return value is video output.
    std::vector<uint32_t> RunForNFrames(size_t num_frames);

    void Paste(std::string text);

    uint32_t Update1();

    double GetSpeed() const;

    // flags to be used when code writes to $fc10.
    uint32_t GetTestTraceFlags() const;
    void SetTestTraceFlags(uint32_t flags);

    void SaveTestTrace(const std::string &stem);

  protected:
    void GotOSWRCH();
    virtual bool GotOSCLI(); //true=handled, false=ok to pass on to real OSCLI
  private:
    bool m_spooling = false;
    size_t m_oswrch_capture_count = 0;
    size_t m_video_data_unit_idx = 0;
    std::vector<VideoDataUnit> m_video_data_units;
    SoundDataUnit m_temp_sound_data_unit;
    uint64_t m_num_ticks = 0;
    CycleCount m_num_cycles = {0};
#if BBCMICRO_TRACE
    std::shared_ptr<Trace> m_test_trace;
    uint32_t m_trace_flags = 0;
#else
    const uint32_t m_trace_flags = 0;
#endif

    void LoadROMsB();
    void LoadROMsBPlus();
    void LoadROMsMaster(const std::string &version);
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

// 1 unit = 2 bytes
//
// 21 bits = 4 MBytes, approx 1 second
// 22 bits = 8 MBytes, approx 2 seconds
// 23 bits = 16 MBytes, approx 4 seconds
// 24 bits = 32 MBytes, approx 8 seconds
static constexpr size_t NUM_VIDEO_DATA_UNITS_LOG2 = 24;

static constexpr size_t NUM_VIDEO_DATA_UNITS = 1 << NUM_VIDEO_DATA_UNITS_LOG2;
static constexpr size_t VIDEO_DATA_UNIT_INDEX_MASK = (1 << NUM_VIDEO_DATA_UNITS_LOG2) - 1;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string strprintfv(const char *fmt, va_list v) {
    char *str;
    if (vasprintf(&str, fmt, v) == -1) {
        // Better suggestions welcome... please.
        return std::string("vasprintf failed - ") + strerror(errno) + " (" + std::to_string(errno) + ")";
    } else {
        std::string result(str);

        free(str);
        str = NULL;

        return result;
    }
}

std::string PRINTF_LIKE(1, 2) strprintf(const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    std::string result = strprintfv(fmt, v);
    va_end(v);

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveFileInternal(const void *contents, size_t contents_size, const std::string &path, const char *mode) {
    if (!PathCreateFolder(PathGetFolder(path))) {
        return false;
    }

    FILE *f = fopen(path.c_str(), mode);
    if (!f) {
        return false;
    }

    size_t n = fwrite(contents, 1, contents_size, f);

    fclose(f);
    f = nullptr;

    if (n != contents_size) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveTextFile(const std::string &contents, const std::string &path) {
    return SaveFileInternal(contents.data(),
                            contents.size(),
                            path,
                            "wt");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetOutputFileName(const std::string &path) {
    return PathJoined(BBC_TESTS_OUTPUT_FOLDER, path);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SaveTextOutput2(const std::string &contents,
                            const std::string &test_name,
                            const std::string &type,
                            const std::string &suffix) {
    SaveTextFile(contents, GetOutputFileName(test_name + "." + type + "_" + suffix));
    SaveTextFile(contents, GetOutputFileName(type + "/" + test_name + "." + suffix));
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

class TestDiscImage : public DiscImage {
  public:
    TestDiscImage(std::string name, std::vector<uint8_t> contents);

    std::shared_ptr<DiscImage> Clone() const override;
    std::string GetHash() const override;
    std::string GetName() const override;
    std::string GetLoadMethod() const override;
    std::string GetDescription() const override;
    std::vector<FileDialogFilter> GetFileDialogFilters() const override;
    bool SaveToFile(const std::string &file_name, const LogSet &logs) const override;
    bool Read(uint8_t *value,
              uint8_t side,
              uint8_t track,
              uint8_t sector,
              size_t offset) const override;
    bool Write(uint8_t side,
               uint8_t track,
               uint8_t sector,
               size_t offset,
               uint8_t value) override;
    void Flush() override;
    bool GetDiscSectorSize(size_t *size,
                           uint8_t side,
                           uint8_t track,
                           uint8_t sector,
                           bool double_density) const override;
    bool IsWriteProtected() const override;

  protected:
  private:
    std::string m_name;
    std::vector<uint8_t> m_contents;

    bool GetByteIndex(size_t *index,
                      uint8_t side,
                      uint8_t track,
                      uint8_t sector,
                      size_t offset) const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TestDiscImage::TestDiscImage(std::string name, std::vector<uint8_t> contents)
    : m_name(std::move(name))
    , m_contents(std::move(contents)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DiscImage> TestDiscImage::Clone() const {
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string TestDiscImage::GetHash() const {
    char hash_str[SHA1::DIGEST_STR_SIZE];
    SHA1::HashBuffer(nullptr, hash_str, m_contents.data(), m_contents.size());

    return hash_str;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string TestDiscImage::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string TestDiscImage::GetLoadMethod() const {
    return "test";
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string TestDiscImage::GetDescription() const {
    return "";
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<FileDialogFilter> TestDiscImage::GetFileDialogFilters() const {
    return {};
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TestDiscImage::SaveToFile(const std::string &file_name, const LogSet &logs) const {
    (void)file_name, (void)logs;

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TestDiscImage::Read(uint8_t *value,
                         uint8_t side,
                         uint8_t track,
                         uint8_t sector,
                         size_t offset) const {
    size_t index;
    if (!this->GetByteIndex(&index, side, track, sector, offset)) {
        return false;
    }

    if (index >= m_contents.size()) {
        *value = 0;
    } else {
        *value = m_contents[index];
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TestDiscImage::Write(uint8_t side,
                          uint8_t track,
                          uint8_t sector,
                          size_t offset,
                          uint8_t value) {
    size_t index;
    if (!this->GetByteIndex(&index, side, track, sector, offset)) {
        return false;
    }

    if (index >= m_contents.size()) {
        m_contents.resize(index + 1);
    }

    m_contents[index] = value;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestDiscImage::Flush() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TestDiscImage::GetDiscSectorSize(size_t *size,
                                      uint8_t side,
                                      uint8_t track,
                                      uint8_t sector,
                                      bool double_density) const {
    if (double_density) {
        return false;
    }

    if (!this->GetByteIndex(nullptr, side, track, sector, 0)) {
        return false;
    }

    *size = 256;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TestDiscImage::IsWriteProtected() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TestDiscImage::GetByteIndex(size_t *index,
                                 uint8_t side,
                                 uint8_t track,
                                 uint8_t sector,
                                 size_t offset) const {
    if (side != 0) {
        return false;
    }

    if (track > 80) {
        return false;
    }

    if (sector > 10) {
        return false;
    }

    if (offset > 256) {
        return false;
    }

    if (index) {
        *index = (track * 10u + sector) * 256u + offset;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::shared_ptr<const std::array<uint8_t, 16384>> LoadOSROM(const std::string &name) {
    std::string path = PathJoined(ROMS_FOLDER, name);

    std::vector<uint8_t> data;
    TEST_TRUE(LoadFile(&data, path, nullptr));

    auto rom = std::make_shared<std::array<uint8_t, 16384>>();

    TEST_LE_UU(data.size(), rom->size());
    for (size_t i = 0; i < data.size(); ++i) {
        (*rom)[i] = data[i];
    }

    return rom;
}

static std::shared_ptr<const std::vector<uint8_t>> LoadSidewaysROM(const std::string &name) {
    std::shared_ptr<const std::array<uint8_t, 16384>> rom = LoadOSROM(name);
    return std::make_shared<std::vector<uint8_t>>(rom->begin(), rom->end());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BBCMicroTypeID GetBBCMicroTypeID(TestBBCMicroType type, uint32_t) {
    switch (type) {
    default:
        TEST_FAIL("%s: unknown TestBBCMicroType", __func__);
        // fall through
    case TestBBCMicroType_BTape:
    case TestBBCMicroType_BAcorn1770DFS:
        return BBCMicroTypeID_B;

    case TestBBCMicroType_BPlusTape:
        return BBCMicroTypeID_BPlus;

    case TestBBCMicroType_Master128MOS320:
    case TestBBCMicroType_Master128MOS350:
    case TestBBCMicroType_Master128MOS320WithMasterTurbo:
    case TestBBCMicroType_Master128MOS320WithExternal3MHz6502:
        return BBCMicroTypeID_Master;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const DiscInterface *GetDiscInterface(TestBBCMicroType type, uint32_t) {
    switch (type) {
    default:
        TEST_FAIL("%s: unknown TestBBCMicroType", __func__);
        // fall through
    case TestBBCMicroType_BTape:
    case TestBBCMicroType_BPlusTape:
        return nullptr;

    case TestBBCMicroType_BAcorn1770DFS:
        return &DISC_INTERFACE_ACORN_1770;

    case TestBBCMicroType_Master128MOS320:
    case TestBBCMicroType_Master128MOS350:
    case TestBBCMicroType_Master128MOS320WithMasterTurbo:
    case TestBBCMicroType_Master128MOS320WithExternal3MHz6502:
        return &DISC_INTERFACE_MASTER128;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BBCMicroParasiteType GetBBCMicroParasiteType(TestBBCMicroType type, uint32_t) {
    switch (type) {
    default:
        TEST_FAIL("%s: unknown TestBBCMicroType", __func__);
        // fall through
    case TestBBCMicroType_BTape:
    case TestBBCMicroType_BPlusTape:
    case TestBBCMicroType_BAcorn1770DFS:
    case TestBBCMicroType_Master128MOS320:
    case TestBBCMicroType_Master128MOS350:
        return BBCMicroParasiteType_None;

    case TestBBCMicroType_Master128MOS320WithMasterTurbo:
        return BBCMicroParasiteType_MasterTurbo;

    case TestBBCMicroType_Master128MOS320WithExternal3MHz6502:
        return BBCMicroParasiteType_External3MHz6502;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<uint8_t> GetNVRAMContents(TestBBCMicroType type, uint32_t flags) {
    switch (type) {
    default:
        TEST_FAIL("%s: unknown TestBBCMicroType", __func__);
        // fall through
    case TestBBCMicroType_BTape:
    case TestBBCMicroType_BPlusTape:
    case TestBBCMicroType_BAcorn1770DFS:
        return {};

    case TestBBCMicroType_Master128MOS320:
    case TestBBCMicroType_Master128MOS350:
    case TestBBCMicroType_Master128MOS320WithMasterTurbo:
    case TestBBCMicroType_Master128MOS320WithExternal3MHz6502:
        {
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
            nvram[16] = 0x02; //16 - LOUD; INTUBE

            if (flags & TestBBCMicroFlags_ConfigureExTube) {
                nvram[16] |= 4;
            }

            if (flags & TestBBCMicroFlags_ConfigureNoTube) {
                nvram[15] &= ~1u;
            }

            return nvram;
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static_assert(ROMType_16KB == 0);
static const ROMType DEFAULT_ROM_TYPES[16] = {};

TestBBCMicro::TestBBCMicro(TestBBCMicroType type, const TestBBCMicroArgs &args)
    : BBCMicro(CreateBBCMicroType(GetBBCMicroTypeID(type, args.flags), DEFAULT_ROM_TYPES),
               GetDiscInterface(type, args.flags),
               GetBBCMicroParasiteType(type, args.flags),
               GetNVRAMContents(type, args.flags),
               nullptr,
               0,
               nullptr,
               {0}) {
#if BBCMICRO_TRACE
    m_trace_flags = (BBCMicroTraceFlag_RTC |
                     BBCMicroTraceFlag_1770 |
                     BBCMicroTraceFlag_SystemVIA |
                     BBCMicroTraceFlag_UserVIA |
                     BBCMicroTraceFlag_VideoULA |
                     BBCMicroTraceFlag_SN76489);
#endif

    switch (type) {
    default:
        TEST_FAIL("unknown TestBBCMicroType");
        break;

    case TestBBCMicroType_BTape:
        this->LoadROMsB();
        break;

    case TestBBCMicroType_BAcorn1770DFS:
        this->LoadROMsB();
        this->SetSidewaysROM(14, LoadSidewaysROM("acorn/DFS-2.26.rom"), ROMType_16KB);
        break;

    case TestBBCMicroType_BPlusTape:
        this->LoadROMsBPlus();
        break;

    case TestBBCMicroType_Master128MOS320:
        this->LoadROMsMaster("3.20");
        break;

    case TestBBCMicroType_Master128MOS350:
        this->LoadROMsMaster("3.50");
        break;

    case TestBBCMicroType_Master128MOS320WithMasterTurbo:
        this->LoadROMsMaster("3.20");
        this->LoadParasiteOS("MasterTurboParasite.rom");
        break;

    case TestBBCMicroType_Master128MOS320WithExternal3MHz6502:
        this->LoadROMsMaster("3.20");
        this->LoadParasiteOS("TUBE110.rom");
        break;
    }

    this->SetXFJIO(0xfc10, &ReadTestCommand, this, &WriteTestCommand, this);

    m_video_data_units.resize(NUM_VIDEO_DATA_UNITS);
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

void TestBBCMicro::LoadSSD(int drive, const std::string &path) {
    std::vector<uint8_t> contents;
    TEST_TRUE(::LoadFile(&contents, path, nullptr));
    this->SetDiscImage(drive, std::make_shared<TestDiscImage>(path, std::move(contents)));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::RunUntilOSWORD0(double max_num_seconds) {
    const uint8_t *ram = this->GetRAM();
    const M6502 *cpu = this->GetM6502();

    CycleCount max_num_cycles = {(uint64_t)(max_num_seconds * CYCLES_PER_SECOND)};

    uint64_t start_ticks = GetCurrentTickCount();

    CycleCount num_cycles = {0};
    while (num_cycles.n < max_num_cycles.n) {

        uint32_t update_result = this->Update1();
        ++num_cycles.n;

        if (update_result & BBCMicroUpdateResultFlag_Host) {
            if (M6502_IsAboutToExecute(cpu)) {
                if (cpu->abus.b.l == ram[WORDV + 0] &&
                    cpu->abus.b.h == ram[WORDV + 1] &&
                    cpu->a == 0) {
                    break;
                }
            }
        }
    }

    m_num_ticks += GetCurrentTickCount() - start_ticks;

    TEST_LE_UU(num_cycles.n, max_num_cycles.n);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> TestBBCMicro::RunForNFrames(size_t num_frames) {
    TVOutput tv;

    uint64_t version;
    const uint32_t *pixels = tv.GetTexturePixels(&version);

    size_t num_frames_got = 0;

    while (num_frames_got < num_frames) {
        size_t a = m_video_data_unit_idx;

        for (size_t i = 0; i < 1024; ++i) {
            uint32_t update_result = this->Update(&m_video_data_units[m_video_data_unit_idx],
                                                  &m_temp_sound_data_unit);

            if (update_result & BBCMicroUpdateResultFlag_VideoUnit) {
                ++m_video_data_unit_idx;
                if (m_video_data_unit_idx > VIDEO_DATA_UNIT_INDEX_MASK) {
                    tv.Update(&m_video_data_units[a], m_video_data_unit_idx - a);
                    m_video_data_unit_idx = 0;
                    a = m_video_data_unit_idx;
                }
            }
        }

        tv.Update(&m_video_data_units[a], m_video_data_unit_idx - a);

        uint64_t new_version;
        pixels = tv.GetTexturePixels(&new_version);
        if (new_version > version) {
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
    uint32_t update_result = this->Update(&m_video_data_units[m_video_data_unit_idx],
                                          &m_temp_sound_data_unit);

    ++m_num_cycles.n;

    if (update_result & BBCMicroUpdateResultFlag_VideoUnit) {
        ++m_video_data_unit_idx;
        m_video_data_unit_idx &= VIDEO_DATA_UNIT_INDEX_MASK;
    }

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
        FILE *f = fopen(path.c_str(), "wt");
        TEST_NON_NULL(f);

        ::SaveTrace(m_test_trace,
                    TraceOutputFlags_Cycles | TraceOutputFlags_AbsoluteCycles | TraceOutputFlags_RegisterNames,
                    &SaveTraceData,
                    f,
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

void TestBBCMicro::LoadROMsB() {
    this->SetOSROM(LoadOSROM("OS12.ROM"));
    this->SetSidewaysROM(15, LoadSidewaysROM("BASIC2.ROM"), ROMType_16KB);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::LoadROMsBPlus() {
    this->SetOSROM(LoadOSROM("B+MOS.ROM"));
    this->SetSidewaysROM(15, LoadSidewaysROM("BASIC2.ROM"), ROMType_16KB);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::LoadROMsMaster(const std::string &version) {
    this->SetOSROM(LoadOSROM(PathJoined("M128", version, "mos.rom")));
    this->SetSidewaysROM(15, LoadSidewaysROM(PathJoined("M128", version, "terminal.rom")), ROMType_16KB);
    this->SetSidewaysROM(14, LoadSidewaysROM(PathJoined("M128", version, "view.rom")), ROMType_16KB);
    this->SetSidewaysROM(13, LoadSidewaysROM(PathJoined("M128", version, "adfs.rom")), ROMType_16KB);
    this->SetSidewaysROM(12, LoadSidewaysROM(PathJoined("M128", version, "basic4.rom")), ROMType_16KB);
    this->SetSidewaysROM(11, LoadSidewaysROM(PathJoined("M128", version, "edit.rom")), ROMType_16KB);
    this->SetSidewaysROM(10, LoadSidewaysROM(PathJoined("M128", version, "viewsht.rom")), ROMType_16KB);
    this->SetSidewaysROM(9, LoadSidewaysROM(PathJoined("M128", version, "dfs.rom")), ROMType_16KB);

    for (uint8_t i = 4; i < 8; ++i) {
        this->SetSidewaysRAM(i, nullptr);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::LoadParasiteOS(const std::string &name) {
    std::string path = PathJoined(ROMS_FOLDER, name);

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

void RunImageTest(const std::string &wanted_png_src_path,
                  const std::string &png_name,
                  TestBBCMicro *beeb) {
    std::vector<uint32_t> got_image = beeb->RunForNFrames(10);

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
    TEST_TRUE(LoadFile(&wanted_png_data, wanted_png_src_path, nullptr));
    TEST_TRUE(SaveFile(wanted_png_data, wanted_png_path, nullptr));

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
    const std::string name;

    Test(std::string category, std::string name_)
        : name(category + "." + name_) {
    }

    virtual ~Test() = 0;

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
    StandardTest(const std::string &name, TestBBCMicroType bbc_micro_type_ = TestBBCMicroType_BTape, const char *drive = "0")
        : Test("standard", name)
        , m_bbc_micro_type(bbc_micro_type_)
        , m_drive(drive)
        , m_name(name) {
    }

    void Run() override {
        //RunStandardTest(GetBeebLinkVolumePath().c_str(),
        //                m_drive.c_str(),
        //                m_name.c_str(),
        //                m_bbc_micro_type,
        //                0,
        //                0);

        TestBBCMicro bbc(m_bbc_micro_type);

        //{
        //    uint32_t trace_flags = bbc.GetTestTraceFlags();

        //    trace_flags &= ~clear_trace_flags;
        //    trace_flags |= set_trace_flags;

        //    bbc.SetTestTraceFlags(trace_flags);
        //}

        bbc.StartCaptureOSWRCH();
        bbc.RunUntilOSWORD0(10.0);
        //bbc.SetTestTraceFlags(bbc.GetTestTraceFlags()|BBCMicroTraceFlag_EveryMemoryAccess);

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

        std::string stem = strprintf("%s.%s", this->name.c_str(), GetTestBBCMicroTypeEnumName(m_bbc_micro_type));

        TEST_TRUE(SaveTextFile(bbc.oswrch_output,
                               GetOutputFileName(strprintf("%s.all_output.txt", stem.c_str()))));

        bbc.SaveTestTrace(stem);

        TestSpooledOutput(bbc,
                          GetBeebLinkVolumePath(),
                          m_drive,
                          stem);

        LOGF(OUTPUT, "Speed: ~%.3fx\n", bbc.GetSpeed());
    }

  protected:
  private:
    TestBBCMicroType m_bbc_micro_type;
    std::string m_drive;
    std::string m_name;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class KevinEdwardsTest : public Test {
  public:
    KevinEdwardsTest(const std::string &name, std::string pre_paste_text)
        : Test("kevin_edwards", name)
        , m_file_name(name)
        , m_pre_paste_text(std::move(pre_paste_text)) {
    }

    void Run() override {
        bool save_trace = false; //TODO...

        TestBBCMicro bbc(TestBBCMicroType_BTape);

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
            bbc.SaveTestTrace(name);
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
    dp111TimingTest(const std::string &name, TestBBCMicroType bbc_micro_type)
        : Test("dp111", name)
        , m_ssd_stem(name)
        , m_bbc_micro_type(bbc_micro_type) {
    }

    void Run() override {
        std::string ssd_path = PathJoined(b2_SOURCE_DIR, "submodules/6502Timing", m_ssd_stem + ".ssd");

        TestBBCMicro bbc(m_bbc_micro_type);

        bbc.StartCaptureOSWRCH();
        bbc.LoadSSD(0, ssd_path.c_str());
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
    TestBBCMicroType m_bbc_micro_type;
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
    TubeTest(std::string name, std::string file_name, TestBBCMicroType bbc_micro_type, uint32_t test_bbc_micro_flags, uint32_t load_addr, std::string pre_paste_text, std::string post_paste_text)
        : Test("tube", std::move(name))
        , m_file_name(std::move(file_name))
        , m_bbc_micro_type(bbc_micro_type)
        , m_test_bbc_micro_flags(test_bbc_micro_flags)
        , m_load_addr(load_addr)
        , m_pre_paste_text(std::move(pre_paste_text))
        , m_post_paste_text(post_paste_text) {
    }

    void Run() override {
        TestBBCMicroArgs args;
        args.flags = m_test_bbc_micro_flags;

        TestBBCMicro bbc(m_bbc_micro_type, args);

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
                          this->name);
    }

  protected:
  private:
    std::string m_file_name;
    TestBBCMicroType m_bbc_micro_type;
    uint32_t m_test_bbc_micro_flags;
    uint32_t m_load_addr;
    std::string m_pre_paste_text;
    std::string m_post_paste_text;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TeletextTest : public Test {
  public:
    TeletextTest(const std::string &name, uint32_t load_addr, std::string paste_text, std::string png_name)
        : Test("teletext", name)
        , m_stem(name)
        , m_load_addr(load_addr)
        , m_paste_text(std::move(paste_text))
        , m_png_name(std::move(png_name)) {
    }

    void Run() override {
        std::string beeblink_volume_path = PathJoined(b2_SOURCE_DIR, "etc", "teletest_v1");

        TestBBCMicro bbc(TestBBCMicroType_BTape);

        bbc.RunUntilOSWORD0(10.0);

        bbc.LoadFile(GetTestFileName(beeblink_volume_path, "0", "$." + m_stem),
                     m_load_addr);

        if (!m_paste_text.empty()) {
            bbc.Paste(m_paste_text);
            bbc.RunUntilOSWORD0(10.0);
        }

        RunImageTest(PathJoined(beeblink_volume_path, m_png_name),
                     m_stem,
                     &bbc);
    }

  protected:
  private:
    std::string m_stem;
    uint32_t m_load_addr;
    std::string m_paste_text;
    std::string m_png_name;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define M(T, ...) std::make_unique<T>(__VA_ARGS__)
static const std::unique_ptr<Test> ALL_TESTS[] = {
    M(StandardTest, "VTIMERS"),
    M(StandardTest, "VIA.AC1"),
    M(StandardTest, "VIA.AC2"),
    M(StandardTest, "VIA.AC3"),
    M(StandardTest, "VIA.AC4"),
    M(StandardTest, "VIA.AC5"),
    M(StandardTest, "VIA.AC6"),
    M(StandardTest, "VIA.AC7"),
    M(StandardTest, "VIA.C1"),
    M(StandardTest, "VIA.C2"),
    M(StandardTest, "VIA.C3"),
    // VIA.C4 isn't Master-specific, but it uses ASL $xxxx,X, which behaves
    // differently on CMOS CPUs, and the output in the repo comes from a Master 128.
    M(StandardTest, "VIA.C4", TestBBCMicroType_Master128MOS320),
    M(StandardTest, "VIA.I1"),
    M(StandardTest, "VIA.I2"),
    M(StandardTest, "VIA.PB2"),
    M(StandardTest, "VIA.PB7"),
    M(StandardTest, "VIA.T11"),
    M(StandardTest, "VIA.T21"),
    M(StandardTest, "VIA.T22"),
    M(StandardTest, "VIA.PB6"),
    M(StandardTest, "TIMINGS"),
    M(StandardTest, "VTIMEOU", TestBBCMicroType_Master128MOS320),
    M(StandardTest, "VPOLL", TestBBCMicroType_Master128MOS320),
    M(KevinEdwardsTest, "Alien8", "P%=&900:[OPT 2:LDX #4:LDY #0:.loop LDA &3000,Y:STA &C00,Y:INY:BNE loop:INC loop+2:INC loop+5:DEX:BNE loop:LDA #1:STA &FC10:JMP &CEA:]\rCALL &900\r"),
    M(KevinEdwardsTest, "Nightsh", "P%=&3900:[OPT3:LDX #9:LDY #0:.loop LDA &3000,Y:STA &700,Y:INY:BNE loop:INC loop+2:INC loop+5:DEX:BNE loop:LDA #&40:STA0:LDA #&F:STA 1:LDA #1:STA &FC10:JMP &F01:]\rCALL &3900\r"),
    M(KevinEdwardsTest, "Jetman", "P%=&380:[OPT 3:LDX #10:LDY #0:.loop LDA &3000,Y:STA &600,Y:INY:BNE loop:INC loop+2:INC loop+5:DEX:BNE loop:LDA #&1F:STA 0:LDA #&F:STA 1:LDA #1:STA &FC10:JMP &F01:]\rCALL &380\r"),
    M(dp111TimingTest, "6502timing", TestBBCMicroType_BAcorn1770DFS),
    M(dp111TimingTest, "6502timing1M", TestBBCMicroType_BAcorn1770DFS),
    M(dp111TimingTest, "65C12timing", TestBBCMicroType_Master128MOS320),
    M(dp111TimingTest, "65C12timing1M", TestBBCMicroType_Master128MOS320),
    M(TubeTest, "xtu_prst", "PRST", TestBBCMicroType_Master128MOS320WithExternal3MHz6502, TestBBCMicroFlags_ConfigureNoTube, 0xffff1900, "PAGE=&1900\rOLD\r!&70=&C4FF3AD5\rI%=FALSE\r", ""),
    M(TubeTest, "itu_prst", "PRST", TestBBCMicroType_Master128MOS320WithMasterTurbo, TestBBCMicroFlags_ConfigureNoTube, 0xffff1900, "PAGE=&1900\rOLD\r!&70=&C4FF3AD5\rI%=TRUE\r", ""),
    M(TubeTest, "xtu_r124", "R124", TestBBCMicroType_Master128MOS320WithExternal3MHz6502, TestBBCMicroFlags_ConfigureExTube, 0x800, "OLD\r*SPOOL X.R124\r", "*SPOOL\r"),
    M(TubeTest, "xtu_r3", "R3", TestBBCMicroType_Master128MOS320WithExternal3MHz6502, 0, 0x800, "OLD\r*SPOOL X.R3\r", "*SPOOL\r"),
    M(TeletextTest, "ENGTEST", 0x7c00, "", "engtest.png"),
    M(TeletextTest, "RED", 0x7c00, "", "red.png"),
    M(TeletextTest, "TELETST", 0xe00, "OLD\rRUN\r", "teletst.png"),
};
#undef M

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    std::vector<std::string> test_name_strs;
    std::vector<std::regex> test_name_regexes;
    bool list = false;
};

static Options GetOptions(int argc, char *argv[]) {
    CommandLineParser p;

    Options options;

    bool help;
    p.AddHelpOption(&help);

    std::vector<std::string> test_name_regex_strs;

    p.AddOption('t', "test").Meta("TEST").AddArgToList(&options.test_name_strs).Help("run test(s) matching TEST, a case-insensitive string");
    p.AddOption('T', "test-regexp").Meta("TEST").AddArgToList(&test_name_regex_strs).Help("run test(s) matching TEST, a case-insensitive regular expression");
    p.AddOption('l', "list").SetIfPresent(&options.list).Help("list all test names");

    if (!p.Parse(argc, argv)) {
        exit(1);
    }

    if (help) {
        exit(0);
    }

    for (const std::string &test_name_regex_str : test_name_regex_strs) {
        try {
            options.test_name_regexes.push_back(std::regex(test_name_regex_str, std::regex_constants::icase | std::regex_constants::extended));
        } catch (const std::regex_error &e) {
            fprintf(stderr, "FATAL: error in regex: %s\nFATAL: %s\n", test_name_regex_str.c_str(), e.what());
            exit(1);
        }
    }

    return options;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    Options options = GetOptions(argc, argv);

    if (options.list) {
        std::set<std::string> names;

        for (const std::unique_ptr<Test> &test : ALL_TESTS) {
            names.insert(test->name);
        }

        for (const std::string &name : names) {
            printf("%s\n", name.c_str());
        }
    }

    for (const std::unique_ptr<Test> &test : ALL_TESTS) {
        bool run = options.test_name_regexes.empty() && options.test_name_strs.empty();

        if (!run) {
            for (const std::regex &test_name_regex : options.test_name_regexes) {
                if (std::regex_match(test->name, test_name_regex)) {
                    run = true;
                    break;
                }
            }
        }

        if (!run) {
            for (const std::string &test_name_str : options.test_name_strs) {
                if (strcasecmp(test->name.c_str(), test_name_str.c_str()) == 0) {
                    run = true;
                    break;
                }
            }
        }

        if (!run) {
            printf("skipping test: %s\n", test->name.c_str());
            continue;
        }

        printf("running test: %s\n", test->name.c_str());

        test->Run();
    }
}
