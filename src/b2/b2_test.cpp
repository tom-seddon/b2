#include <shared/system.h>
#include <shared/system_specific.h>
#include <shared/CommandLineParser.h>
#include <string>
#include <vector>
#include <regex>
#include <set>
#include <shared/testing.h>
#include "misc.h"
#include "SymbolTable.h"
#include <shared/log.h>
#include <beeb/type.h>
#include "JobQueue.h"
#include <string.h>
#include "b2.h"
#include "load_save.h"
#ifdef IMGUI_ENABLE_TEST_ENGINE
#include <imgui_test_engine/imgui_te_engine.h>
#include <imgui_test_engine/imgui_te_context.h>
#endif
#include "dear_imgui.h"
#include <shared/debug.h>
#include <shared/path.h>
#include "discs.h"
#if SYSTEM_OSX
#include <unistd.h>
#endif
#include "BeebWindow.h"
#include <shared/strings.h>
#include <shared/file_io.h>
#include <beeb/DiscGeometry.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Test {
  public:
    Test() = default;
    virtual ~Test() = 0;

    // hidden tests don't appear in the ctest_check_log list, but can be run
    // manually.
    //
    // default impl returns false.
    virtual bool IsHidden() const;

    virtual std::string GetFullName() const = 0;

    virtual void Run() = 0;

#ifdef IMGUI_ENABLE_TEST_ENGINE
    virtual void RegisterDearImGuiTest(ImGuiTestEngine *test_engine);
    virtual void DearImGuiTestFunc(ImGuiTestContext *ctx);
#endif
  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Test::~Test() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Test::IsHidden() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef IMGUI_ENABLE_TEST_ENGINE
void Test::RegisterDearImGuiTest(ImGuiTestEngine *test_engine) {
    (void)test_engine;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef IMGUI_ENABLE_TEST_ENGINE
void Test::DearImGuiTestFunc(ImGuiTestContext *ctx) {
    (void)ctx;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DearImGuiTest : public Test, public AppHandler {
  public:
    bool IsHeadless() const override {
        return !ms_interactive;
    }

    static void Interactive() {
        ms_interactive = true;
    }

    std::vector<std::string> GetCommandLineArgs() const override {
        std::vector<std::string> argv;
        argv.push_back("<<placeholder>>");
        argv.insert(argv.end(), m_args.begin(), m_args.end());
        return argv;
    }

    bool GetConfigFolder(std::string *config_folder) const override {
        if (config_folder) {
            *config_folder = PathJoined(CONFIG_FOLDER, this->GetFullName());
        }
        return true;
    }

    virtual int GetRequestedHttpServerListenPort() const override {
        // Let the OS choose. Don't have multiple instances fight.
        return 0;
    }

    void SetActualHttpServerListenPort(int port) override {
        m_http_port = port;
    }

    int GetLaunchRequestHttpServerPort() const override {
        return m_http_port;
    }

    bool GetAssetsFolder(std::string *assets_folder) const override {
        *assets_folder = ASSETS_FOLDER;
        return true;
    }

#ifdef IMGUI_ENABLE_TEST_ENGINE
    bool IsDearImGuiTestEngineEnabled() const override {
        return true;
    }

    void DearImGuiTestEngineWasCreated(BeebWindow *beeb_window, ImGuiStuff *imgui_stuff) override {
        (void)beeb_window;
        ImGuiTestEngine *test_engine = imgui_stuff->GetTestEngine();
        TEST_NON_NULL(test_engine);
        this->RegisterDearImGuiTest(test_engine);
        TEST_EQ_PP(m_test_engine, test_engine);
        TEST_NON_NULL(m_test);
        ImGuiTestEngine_QueueTest(m_test_engine, m_test);
    }

    void RegisterDearImGuiTest(ImGuiTestEngine *test_engine) override {
        m_test = IM_REGISTER_TEST(test_engine, "b2", nullptr);
        m_test->SetOwnedName(this->GetFullName().c_str());
        m_test->TestFunc = [this](ImGuiTestContext *ctx) {
            this->DearImGuiTestFunc(ctx);
        };
        m_test_engine = test_engine;
    }
#endif

    bool HandleSelectorDialogOpen(std::string *result, const Guid &guid) override {
        SelectorResults *results = &m_selector_results_by_guid[guid];
        TEST_LT_UU(results->next_result_index, results->results.size());
        *result = results->results[results->next_result_index];
        ++results->next_result_index;
        return true;
    }

    bool ShouldQuitWhenTestQueueEmpty() const override {
        if (ms_interactive) {
            // Keep running. See what happens. Quit manually if you want the test to continue.
            return false;
        } else {
            // Quit.
            return true;
        }
    }

  protected:
    void AddSelectorResult(const Guid &guid, std::string result) {
        m_selector_results_by_guid[guid].results.push_back(std::move(result));
    }

    [[nodiscard]] int Run2() {
        std::string config_folder;
        TEST_TRUE(this->GetConfigFolder(&config_folder));

        // Create config folder if it doesn't exist.
        if (!PathIsFolderOnDisk(config_folder)) {
            PathCreateFolder(config_folder);
        }

        // Clear out contents of config folder.
        PathGlob(config_folder, [](const std::string &path, bool is_folder) -> void {
            TEST_FALSE(is_folder);
#if SYSTEM_OSX || SYSTEM_LINUX
            int rc = unlink(path.c_str());
            TEST_EQ_II(rc, 0);
#elif SYSTEM_WINDOWS
            TEST_TRUE(DeleteFileW(GetWideString(path).c_str()));
#else
#error //TODO...
#endif
        });

        int result = b2_main(this);
        return result;
    }

    std::vector<std::string> m_args; //excludes argv[0]
  private:
    struct SelectorResults {
        std::vector<std::string> results;
        size_t next_result_index = 0;
    };

    ImGuiTestEngine *m_test_engine = nullptr;
    ImGuiTest *m_test = nullptr;
    int m_http_port = 0;
    std::map<Guid, SelectorResults> m_selector_results_by_guid;

    static bool ms_interactive;
};

// TODO: some better mechanism for this, surely.
bool DearImGuiTest::ms_interactive = false;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class NullTest : public Test {
  public:
    NullTest(std::string full_name)
        : m_full_name(std::move(full_name)) {
    }
    std::string GetFullName() const override {
        return m_full_name;
    }
    void Run() override {
    }

  protected:
  private:
    const std::string m_full_name;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestUTF8 : public Test {
  public:
    std::string GetFullName() const override {
        return "misc.utf8";
    }

    void Run() override {
        // Test cases from the Wikipedia page: https://en.wikipedia.org/wiki/UTF-8

        TEST_EQ_SS(GetUTF8StringForCodePoint(0x24), "\x24");
        TEST_EQ_SS(GetUTF8StringForCodePoint(0xa3), "\xc2\xa3");
        TEST_EQ_SS(GetUTF8StringForCodePoint(0x418), "\xd0\x98");
        TEST_EQ_SS(GetUTF8StringForCodePoint(0x939), "\xe0\xa4\xb9");
        TEST_EQ_SS(GetUTF8StringForCodePoint(0x20ac), "\xe2\x82\xac");
        TEST_EQ_SS(GetUTF8StringForCodePoint(0xd55c), "\xed\x95\x9c");
        TEST_EQ_SS(GetUTF8StringForCodePoint(0x10348), "\xf0\x90\x8d\x88");
        TEST_EQ_SS(GetUTF8StringForCodePoint(0x1096b3), "\xf4\x89\x9a\xb3");
        TEST_EQ_SS(GetUTF8StringForCodePoint(0x2825f), "\xf0\xa8\x89\x9f");
        TEST_EQ_SS(GetUTF8StringForCodePoint(0x5450), "\xe5\x91\x90");
        TEST_EQ_SS(GetUTF8StringForCodePoint(0x35c2), "\xe3\x97\x82");
        TEST_EQ_SS(GetUTF8StringForCodePoint(0x8d8a), "\xe8\xb6\x8a");

        {
            std::string ascii;
            GetBBCASCIIFromISO8859_1(&ascii, {'`', 0xa3});
            TEST_EQ_SS(ascii, "``");
        }

        const std::vector<uint8_t> annoying_chars = {'`', '|', '\\', '{', '[', ']', '}', '^', '_'};
        {
            TEST_EQ_SS(GetUTF8FromBBCASCII(annoying_chars, BBCUTF8ConvertMode_PassThrough, false), "`|\\{[]}^_");
            TEST_EQ_SS(GetUTF8FromBBCASCII(annoying_chars, BBCUTF8ConvertMode_OnlyGBP, false), "\xc2\xa3|\\{[]}^_");
        }

        {
            std::string utf8 = GetUTF8FromBBCASCII(annoying_chars, BBCUTF8ConvertMode_SAA5050, false);
            std::string ascii;

            uint32_t bad_codepoint;
            const uint8_t *bad_char_start;
            int bad_char_len;
            TEST_TRUE(GetBBCASCIIFromUTF8(&ascii, std::vector<uint8_t>(utf8.begin(), utf8.end()), &bad_codepoint, &bad_char_start, &bad_char_len));
            TEST_EQ_SS(ascii, std::string(annoying_chars.begin(), annoying_chars.end()));
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

static const char TEST_DATA_1[] =
    "label1_1=$8000\n"
    "label2_1=$8001\n"
    "label3_1=$8000\n"
    "ambiguous=$1000\n"
    "file1_only=$2001\n";

static const char TEST_DATA_2[] =
    "label1_2=$8000\n"
    "label2_2=$8001\n"
    "label3_2=$8000\n"
    "ambiguous=$1001\n"
    "file2_only=$2002\n";

static const char TASS_LABELS_TEST_DATA[] =
    "label1 = $8000\n"
    "string=     \"hello\"\n"
    "yes=true\n"
    "no=false\n"
    "label2 = 1234\n"
    "label3 := 999\n";

static const char BEEBASM_TEST_DATA[] = "[{'.SPARE1':0L,'.SPARE2':2L,'.irqTmp':3L,'.runLenCnt':4L,'.joystickEnabledFlag':5L,'.snowWindow':6L,'.packedTileTable':10L,'.itemExtra':16L,'.itemTile':17L,'.itemID':18L,'.itemX':19L,'.itemY':20L}]";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define TEST_EQ_SYMBOL_S(GOT_NAME_PTR, WANTED_NAME) \
    BEGIN_MACRO {                                   \
        TEST_NON_NULL((GOT_NAME_PTR));              \
        TEST_EQ_SS(*(GOT_NAME_PTR), WANTED_NAME);   \
    }                                               \
    END_MACRO

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char TASS_LABELS_INCLUSIVE_0[] =
    "label1 = $8000\n"
    "main0=0\n";

static const char TASS_LABELS_INCLUSIVE_1[] =
    "label2=$8000\n"
    "main1=1\n";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestSymbolTable : public Test {
  public:
    std::string GetFullName() const override {
        return "b2.SymbolTable";
    }

    void Run() override {

        SymbolTable::SymbolParserRegistry::InitializeBuiltinParsers();

        this->TestMultiSymbolTableStuff();
        this->TestBeebAsmStuff();
        this->TestTassLabelsStuff();
        this->TestInclusiveMode();

        // BBCMICRO_DEBUGGER is controlled entirely at the C++ level, so the test will
        // run in all configurations. So, if no debugger, do nothing, and succeeed.
    }

  protected:
  private:
    static size_t MustFindFileIndex(const SymbolTable &st, const std::string &file_path) {
        for (size_t i = 0; i < st.GetNumFiles(); ++i) {
            const SymbolFile *file = st.GetFileByIndex(i);
            if (file->file_path == file_path) {
                return i;
            }
        }

        TEST_FAIL("couldn't find expected symbol file: %s", file_path.c_str());
    }

    static std::shared_ptr<const BBCMicroType> CreateTestBBCMicroType() {
        ROMType rom_types[16];
        for (int i = 0; i < 16; ++i) {
            rom_types[i] = ROMType_16KB;
        }

        std::shared_ptr<const BBCMicroType> type = CreateBBCMicroType(BBCMicroTypeID_B, rom_types, BBCMicroTypeFlag_ROMBoard);
        return type;
    }

    void TestMultiSymbolTableStuff() {
        const SymbolTable::SymbolParser *acme_parser = SymbolTable::SymbolParserRegistry::FindParserByFormatName("ACME");
        TEST_NON_NULL(acme_parser);

        SymbolTable st;

        TEST_TRUE(st.LoadFromString(TEST_DATA_1, "1", acme_parser, nullptr));

        size_t file1_index = MustFindFileIndex(st, "1");

        TEST_TRUE(st.LoadFromString(TEST_DATA_2, "2", acme_parser, nullptr));

        size_t file2_index = MustFindFileIndex(st, "2");

        std::shared_ptr<const BBCMicroType> type = CreateTestBBCMicroType();

        uint16_t addr;
        uint32_t dso;

        // 1=enabled, 2=enabled

        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_1");

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "file1_only"));
        TEST_EQ_UU(addr, 0x2001);

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "file2_only"));
        TEST_EQ_UU(addr, 0x2002);

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "ambiguous"));
        TEST_EQ_UU(addr, 0x1000);

        st.EnableFile(file1_index, false);

        // 1=disabled, 2=enabled

        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_2");

        TEST_FALSE(st.GetAddressForSymbol(&addr, &dso, type, "file1_only"));

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "file2_only"));
        TEST_EQ_UU(addr, 0x2002);

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "ambiguous"));
        TEST_EQ_UU(addr, 0x1001);

        st.EnableFile(file1_index, true);

        // 1=enabled, 2=enabled

        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_1");

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "file1_only"));
        TEST_EQ_UU(addr, 0x2001);

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "file2_only"));
        TEST_EQ_UU(addr, 0x2002);

        st.MoveFile(file2_index, file1_index);

        // 2=enabled, 1=enabled

        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_2");

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "ambiguous"));
        TEST_EQ_UU(addr, 0x1001);
    }

    void TestBeebAsmStuff() {
        const SymbolTable::SymbolParser *beebasm_parser = SymbolTable::SymbolParserRegistry::FindParserByFormatName("BeebAsm");
        TEST_NON_NULL(beebasm_parser);

        SymbolTable st;

        TEST_TRUE(st.LoadFromString(BEEBASM_TEST_DATA, "1", beebasm_parser, nullptr));

        std::shared_ptr<const BBCMicroType> type = CreateTestBBCMicroType();

        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0, 0, type), "SPARE1");
        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(20, 0, type), "itemY");

        uint16_t addr;
        uint32_t dso;

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "snowWindow"));
        TEST_EQ_UU(addr, 6);
    }

    void TestTassLabelsStuff() {
        const SymbolTable::SymbolParser *tass_labels_parser = SymbolTable::SymbolParserRegistry::FindParserByFormatName("64tass_labels");
        TEST_NON_NULL(tass_labels_parser);

        SymbolTable st;

        TEST_TRUE(st.LoadFromString(TASS_LABELS_TEST_DATA, "1", tass_labels_parser, nullptr));

        std::shared_ptr<const BBCMicroType> type = CreateTestBBCMicroType();

        TEST_EQ_UU(st.GetSymbolCount(), 3); //should have ignored bools and strings
        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label1");
        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(1234, 0, type), "label2");

        uint16_t addr;
        uint32_t dso;

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "label1"));
        TEST_EQ_UU(addr, 0x8000);

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "label2"));
        TEST_EQ_UU(addr, 1234);
    }

    void TestInclusiveMode() {
        const SymbolTable::SymbolParser *tass_labels_parser = SymbolTable::SymbolParserRegistry::FindParserByFormatName("64tass_labels");
        TEST_NON_NULL(tass_labels_parser);

        SymbolTable st;
        TEST_TRUE(st.LoadFromString(TASS_LABELS_INCLUSIVE_0, "0", tass_labels_parser, nullptr));
        size_t index0 = MustFindFileIndex(st, "0");
        TEST_TRUE(st.LoadFromString(TASS_LABELS_INCLUSIVE_1, "1", tass_labels_parser, nullptr));
        size_t index1 = MustFindFileIndex(st, "1");

        st.SetFileAddressSuffixMode(index0, SymbolFileAddressSuffixMode_Inclusive);
        st.SetFileAddressSuffixes(index0, {"0"});
        st.SetFileAddressSuffixMode(index1, SymbolFileAddressSuffixMode_Inclusive);
        st.SetFileAddressSuffixes(index1, {"1"});

        std::shared_ptr<const BBCMicroType> type = CreateTestBBCMicroType();

        const std::string *str;

        str = st.GetSymbolNameForAddress(0x8000, BBCMicroDebugStateOverride_OverrideROM | 0, type);
        TEST_NON_NULL(str);
        TEST_EQ_SS(*str, "label1");

        str = st.GetSymbolNameForAddress(0x8000, BBCMicroDebugStateOverride_OverrideROM | 1, type);
        TEST_NON_NULL(str);
        TEST_EQ_SS(*str, "label2");

        for (uint8_t rom = 0; rom < 16; ++rom) {
            uint32_t dso = BBCMicroDebugStateOverride_OverrideROM | rom;
            // (rom==0 || rom==1 were tested above)
            if (rom > 2) {
                TestFailFnAdder adder([rom](const TestFailArgs *) {
                    LOGF(TESTING, "rom=%u\n", rom);
                });
                str = st.GetSymbolNameForAddress(0x8000, dso, type);
                TEST_NULL(str);
            }

            TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0, dso, type), "main0");
            TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(1, dso, type), "main1");
        }
    }
};

#else

class TestSymbolTable : public NullTest {
  public:
    TestSymbolTable()
        : NullTest("b2.SymbolTable") {
    }

  protected:
  private:
};

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BlockJob : JobQueue::Job {
    void ThreadExecute() override {
        printf("blocker started.\n");

        while (!this->WasCanceled()) {
            SleepMS(100);

            //printf("f=%" PRId32 " c=%" PRId32 "\n",m_finished,m_canceled);
        }

        printf("blocker finished.\n");
    }
};

struct TestJob1 : JobQueue::Job {
    std::atomic<int32_t> *value = nullptr;

    void ThreadExecute() override {
        if (this->value) {
            ++*this->value;
        }
    }
};

class TestJobQueue : public Test {
  public:
    std::string GetFullName() const override {
        return "b2.JobQueue";
    }
    void Run() override {
        setbuf(stdout, nullptr);

        {
            JobQueue jq;

            TEST_TRUE(jq.Init(1));

            auto &&blocker = std::make_shared<BlockJob>();

            jq.AddJob(blocker);

            std::atomic<int32_t> counter{0};

            std::vector<std::shared_ptr<TestJob1>> test_jobs;
            test_jobs.resize(10);

            for (size_t i = 0; i < test_jobs.size(); ++i) {
                std::shared_ptr<TestJob1> p = std::make_shared<TestJob1>();
                p->value = &counter;

                test_jobs[i] = p;
                jq.AddJob(p);
            }

            TEST_EQ_II(counter, 0);

            printf("waiting for blocker to start...\n");

            while (!blocker->IsRunning()) {
                SleepMS(1);
            }

            TEST_FALSE(blocker->IsFinished());
            TEST_FALSE(blocker->WasCanceled());

            blocker->Cancel();

            printf("waiting for blocker to finish...\n");
            while (!blocker->IsFinished()) {
                SleepMS(1);
            }

            TEST_TRUE(blocker->WasCanceled());

            printf("waiting for test jobs to finish...\n");

            for (;;) {
                size_t num_finished = 0;

                for (auto &&test_job : test_jobs) {
                    if (test_job->IsFinished()) {
                        ++num_finished;
                    }
                }

                if (num_finished == test_jobs.size()) {
                    break;
                }

                SleepMS(1);
            }

            TEST_EQ_II(counter, (int32_t)test_jobs.size());

            printf("all done...\n");
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestFileExit : public DearImGuiTest {
  public:
    std::string GetFullName() const override {
        return "b2ui.FileExit";
    };

    bool IsHidden() const override {
        return true;
    }

    void DearImGuiTestFunc(ImGuiTestContext *ctx) override {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("File/Exit/Confirm");
    }

    void Run() override {
        TEST_EQ_II(this->Run2(), 0);
    }

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestCopyOfDisk : public DearImGuiTest {
  public:
    TestCopyOfDisk(const Disc *disk, int drive, bool in_memory)
        : m_disk(disk)
        , m_drive(drive)
        , m_in_memory(in_memory) {
    }

    std::string GetFullName() const override {
        std::string name = "b2ui.copy_of_disk." + std::to_string(m_drive) + "." + std::to_string(m_in_memory) + "." + PathGetName(m_disk->path);
        return name;
    }

    void DearImGuiTestFunc(ImGuiTestContext *ctx) override {
        TEST_FALSE(m_disk_path.empty());
        this->AddSelectorResult(NEW_DISK_IMAGE_SELECTOR_GUID, m_disk_path);
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick(strprintf("File/Drive %d/New %s/%s %s", m_drive,
                                 m_in_memory ? "in-memory disc image" : "disc image",
                                 m_disk->blank ? "Blank" : "Copy of",
                                 m_disk->name.c_str())
                           .c_str());
        TEST_TRUE(PathIsFileOnDisk(m_disk_path, nullptr, nullptr));

        std::vector<uint8_t> wanted_data;
        TEST_TRUE(LoadFile(&wanted_data, m_disk->GetAssetPath(), nullptr));

        std::vector<uint8_t> got_data;
        TEST_TRUE(LoadFile(&got_data, m_disk_path, nullptr));

        TEST_EQ_UU(got_data.size(), wanted_data.size());

        if (m_disk->geometry->adfs) {
            // The disk identifier (and therefore the checksum) will have been updated. Don't check that it's different, as the identifier is random and so there's a non-zero chance that it'll be the same. Overwrite the relevant got bytes with the wanted bytes so the disk images otherwise match.
            //
            // See RandomizeADFSDiskIdentifier.
            TEST_GE_UU(got_data.size(), 512u);

            got_data[0x1fb] = wanted_data[0x1fb];
            got_data[0x1fc] = wanted_data[0x1fc];
            got_data[511] = wanted_data[511];
        }

        TEST_EQ_AA(got_data.data(), wanted_data.data(), got_data.size());
    }

    void Run() override {
        std::string config_folder;
        TEST_TRUE(this->GetConfigFolder(&config_folder));
        m_disk_path = PathJoined(config_folder, "test." + m_disk->path);
        TEST_EQ_II(this->Run2(), 0);
    }

  protected:
  private:
    std::string m_disk_path;
    const Disc *m_disk = nullptr;
    const int m_drive = 0;
    const bool m_in_memory = false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct TestOptions {
    bool verbose = false;
    std::vector<std::string> test_name_strs;
    std::vector<std::regex> test_name_regexes;
    bool list = false;
    bool list_for_check_ctest_log = false;
    bool wip = false;
    bool reverse = false;
    bool b2 = false;
    std::vector<std::string> b2_argv;
    bool interactive = false;
};

static TestOptions GetOptions(int argc, char *argv[]) {
    CommandLineParser p;

    TestOptions options;

    bool help;
    p.AddHelpOption(&help);

    std::vector<std::string> test_name_patterns;

    p.AddOption('v', "verbose").SetIfPresent(&options.verbose).Help("be more verbose");
    p.AddOption('t', "test").Meta("TEST").AddArgToList(&options.test_name_strs).Help("run test(s) matching TEST, a case-insensitive string");
    p.AddOption('T', "test-pattern").Meta("TEST").AddArgToList(&test_name_patterns).Help("run test(s) matching TEST, a case-insensitive glob pattern");
    p.AddOption('l', "list").SetIfPresent(&options.list).Help("list all test names");
    p.AddOption('l', "list-for-check_ctest_log").SetIfPresent(&options.list_for_check_ctest_log).Help("list all test names, formatted for the benefit of check_ctest_log");
    p.AddOption(0, "wip").SetIfPresent(&options.wip).Help("include WIP tests that aren't finished or passing yet");
    p.AddOption('b', "b2").SetIfPresent(&options.b2).Help("pretend to be ordinary b2, with Dear ImGui Test Engine enabled. Tests will be available - run at own risk");
    p.AddOption('B', "b2-arg").AddArgToList(&options.b2_argv).Help("add a string, verbatim, to the b2 argv");
    p.AddOption(0, "interactive").SetIfPresent(&options.interactive).Help("if running a single Dear ImGui Test Engine test, run it in interactive mode");

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

class b2ModeAppHandler : public OrdinaryAppHandler {
  public:
    b2ModeAppHandler(int argc, char *argv[], std::vector<std::unique_ptr<Test>> *tests)
        : OrdinaryAppHandler(argc, argv)
        , m_tests(tests) {
    }

    bool IsDearImGuiTestEngineEnabled() const override {
        return true;
    }

#ifdef IMGUI_ENABLE_TEST_ENGINE
    void DearImGuiTestEngineWasCreated(BeebWindow *beeb_window, ImGuiStuff *imgui_stuff) override {
        (void)beeb_window;

        ImGuiTestEngine *test_engine = imgui_stuff->GetTestEngine();
        ASSERT(test_engine);

        for (const std::unique_ptr<Test> &test : *m_tests) {
            test->RegisterDearImGuiTest(test_engine);
        }
    }
#endif

    void MessageLoopWillStart() override {
        printf("Config path: %s\n", GetConfigPath("").c_str());
        printf("Cache path: %s\n", GetCachePath("").c_str());
        printf("Example asset path: %s\n", GetAssetPath(GAMECONTROLLER_DB_FILE_NAME).c_str());
    }

    bool GetAssetsFolder(std::string *assets_folder) const override {
        *assets_folder = ASSETS_FOLDER;
        return true;
    }

  protected:
  private:
    std::vector<std::unique_ptr<Test>> *m_tests = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void AddCopyOfDiskTests(std::vector<std::unique_ptr<Test>> *all_tests, const Disc *disks, size_t num_disks) {
    for (int drive = 0; drive < 2; ++drive) {
        for (int in_memory = 0; in_memory < 2; ++in_memory) {
            for (size_t disk_idx = 0; disk_idx < num_disks; ++disk_idx) {
                const Disc *disk = &disks[disk_idx];
                all_tests->push_back(std::make_unique<TestCopyOfDisk>(disk, drive, !!in_memory));
            }
        }
    }
}

int main(int argc, char *argv[]) {
    TestOptions options = GetOptions(argc, argv);

    std::vector<std::unique_ptr<Test>> all_tests;

    all_tests.push_back(std::make_unique<TestUTF8>());
    all_tests.push_back(std::make_unique<TestSymbolTable>());
    all_tests.push_back(std::make_unique<TestJobQueue>());
    all_tests.push_back(std::make_unique<TestFileExit>());

    AddCopyOfDiskTests(&all_tests, BLANK_DFS_DISCS, NUM_BLANK_DFS_DISCS);
    AddCopyOfDiskTests(&all_tests, BLANK_ADFS_DISCS, NUM_BLANK_ADFS_DISCS);
    AddCopyOfDiskTests(&all_tests, WELCOME_DISKS, NUM_WELCOME_DISKS);

    std::map<std::string, Test *> tests_by_name;
    for (const std::unique_ptr<Test> &test : all_tests) {
        tests_by_name[test->GetFullName()] = test.get();
    }
    TEST_EQ_UU(all_tests.size(), tests_by_name.size());

    if (options.list) {
        for (auto &&name_and_test : tests_by_name) {
            if (name_and_test.second->IsHidden()) {
                printf("(hidden) ");
            }
            printf("%s\n", name_and_test.first.c_str());
        }

        return 0;
    }

    if (options.list_for_check_ctest_log) {
        for (auto &&name_and_test : tests_by_name) {
            if (!name_and_test.second->IsHidden()) {
                printf("2fcf9707-9498-4a03-9b27-ef501fa2fbb6:b2_test: %s\n", name_and_test.first.c_str());
            }
        }

        return 0;
    }

    if (options.b2) {
        std::vector<char *> b2_argv;
        b2_argv.push_back(argv[0]);
        for (std::string &b2_arg : options.b2_argv) {
            b2_argv.push_back(b2_arg.data());
        }
        b2_argv.push_back(nullptr);

        b2ModeAppHandler app_handler((int)(b2_argv.size() - 1), b2_argv.data(), &all_tests);
        int result = b2_main(&app_handler);
        return result;
    } else {
        bool ran_any_tests = false;

        std::vector<bool> run_test;
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

            run_test.push_back(run);
        }

        TEST_EQ_UU(run_test.size(), all_tests.size());

        if (options.interactive) {
            size_t n = 0;
            for (size_t test_index = 0; test_index < all_tests.size(); ++test_index) {
                if (run_test[test_index]) {
                    ++n;
                }
            }

            // TODO: more experimentation required.
            (void)n;
            //TEST_EQ_UU(n,1);

            DearImGuiTest::Interactive();
        }

        for (size_t test_index = 0; test_index < all_tests.size(); ++test_index) {
            bool run = run_test[test_index];
            Test *test = all_tests[test_index].get();

            if (!run) {
                if (options.verbose) {
                    printf("skipping test: %s\n", test->GetFullName().c_str());
                }
                continue;
            }

            if (options.verbose) {
                printf("starting test: %s\n", test->GetFullName().c_str());
            }
            printf("ea73a8dc-2d1a-43bc-ae41-078e441e53c5:b2_test: %s\n", test->GetFullName().c_str());

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

    return 0;
}
