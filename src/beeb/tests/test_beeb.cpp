#include <shared/system.h>
#include "test_common.h"
#include <shared/CommandLineParser.h>
#include <memory>
#include <vector>
#include <set>
#include <map>
#include <regex>
#include <shared/path.h>
#include <shared/log.h>
#include <shared/testing.h>

#ifndef b2_SOURCE_DIR
#error
#endif

LOG_EXTERN(BBC_OUTPUT);

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

    virtual ~Test() = 0 {
    }

    virtual void Run() = 0;

  protected:
  private:
};

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
        bool save_trace = false;//TODO...

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
    TeletextTest(const std::string &name, uint32_t load_addr, std::string paste_text)
        : Test("teletext", name)
        , m_stem(name)
        , m_load_addr(load_addr)
        , m_paste_text(std::move(paste_text)) {
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

        RunImageTest(PathJoined(beeblink_volume_path, m_stem + ".png"),
                     m_stem,
                     &bbc);
    }

  protected:
  private:
    std::string m_stem;
    uint32_t m_load_addr;
    std::string m_paste_text;
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
    M(TeletextTest, "engtest", 0x7c00, ""),
    M(TeletextTest, "red", 0x7c00, ""),
    M(TeletextTest, "teletst", 0xe00, "OLD\rRUN\r"),
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
