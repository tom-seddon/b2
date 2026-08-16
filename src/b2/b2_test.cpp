#include <shared/system.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "json.h"
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
#include "BeebWindows.h"
#include "BeebThread.h"
#include <inttypes.h>
#include <beeb/uef.h>
#include "http_api.h"
#include <http/HTTPClient.h>
#include <http/http.h>
#include <uv.h>
#include <variant>
#include <initializer_list>

// the b2 code includes the stb_image_write implementation.
#include <stb_image_write.h>
//#include <stb_image.h>

#include <shared/enum_decl.h>
#include "b2_test.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "b2_test.inl"
#include <shared/enum_end.h>

#ifndef TRANSIENT_DATA_FOLDER
#error
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// don't try to call these "stdout" or "stderr" - on VC++, the name expands to
// something that isn't an identifier.
LOG_DEFINE(std_out, "", &log_printer_stdout);
LOG_DEFINE(std_err, "", &log_printer_stderr);

static const LogSet g_stdio_logs(LOG(std_out), LOG(std_err), LOG(std_err));

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// covering any tests that could be run both ways.
static bool g_interactive = false;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Relevant for any tests that have multiple versions, one per MOS type. This is
// typically overkill but it does ensure that no surprising MOS-specific issues
// are introduced.
struct MOSType {
    // standard suffix for the test names.
    std::string suffix;

    // default config that includes this MOS version.
    std::string default_config_name;

    // expected version name, as reported by *FX0.
    std::string expected_os_version;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool HandleGetConfigOverrideFolder(std::string *folder, const std::string &name) {
    if (folder) {
        *folder = PathJoined(TRANSIENT_DATA_FOLDER, name);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool HandleGetCacheOverrideFolder(std::string *folder, const std::string &name) {
    if (folder) {
        *folder = PathJoined(TRANSIENT_DATA_FOLDER, name, "cache");
    }

    return true;
}

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
    virtual void RegisterDearImGuiTest(ImGuiTestEngine *test_engine, BeebWindow *beeb_window);
    virtual void DearImGuiTestFunc(ImGuiTestContext *ctx, BeebWindow *beeb_window);
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
void Test::RegisterDearImGuiTest(ImGuiTestEngine *test_engine, BeebWindow *beeb_window) {
    (void)test_engine, (void)beeb_window;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef IMGUI_ENABLE_TEST_ENGINE
void Test::DearImGuiTestFunc(ImGuiTestContext *ctx, BeebWindow *beeb_window) {
    (void)ctx, (void)beeb_window;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Still some work required - e.g., if a test needs to create a second window, what then?

class DearImGuiTest : public Test, public AppHandler {
  public:
    static constexpr ImVec2 FIXED_DISPLAY_SIZE{1024.f, 768.f};

    std::string GetProductName() const override {
        return DEFAULT_PRODUCT_NAME;
    }

#if SYSTEM_OSX
    std::string GetFrameName() const override {
        return "b2_DearImGuiTest";
    }
#endif

    bool IsHeadless() const override {
        return !g_interactive;
    }

    bool IsHighDPIEnabled() const override {
        // The test engine seems to have trouble with display scales. And it's
        // hard to pick the right (scale-dependent) size on Windows until the
        // window is already created. So for test purposes, just disable it.
        //
        // This intentionally also affects --interactive.
        return false;
    }

    bool IsSoundEnabled() const override {
        // This intentionally also affects --interactive.
        return false;
    }

    std::vector<std::string> GetCommandLineArgs() const override {
        std::vector<std::string> argv;
        argv.push_back("<<placeholder>>");
        argv.insert(argv.end(), m_args.begin(), m_args.end());
        return argv;
    }

    bool GetConfigOverrideFolder(std::string *folder) const override {
        return HandleGetConfigOverrideFolder(folder, this->GetFullName());
    }

    bool GetCacheOverrideFolder(std::string *folder) const override {
        return HandleGetCacheOverrideFolder(folder, this->GetFullName());
    }

    bool GetFixedDisplaySize(ImVec2 *display_size) const override {
        *display_size = FIXED_DISPLAY_SIZE;

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

    uint32_t GetUIFlags() const override {
        // The popups can interfere with the test engine, so just don't bother
        // display them.
        return UIFlag_HideAllPopups;
    }

#ifdef IMGUI_ENABLE_TEST_ENGINE
    // Whether to auto-run the test if not in headless mode.
    virtual bool ShouldAutoRunTest() const {
        return false;
    }

    bool IsDearImGuiTestEngineEnabled() const override {
        return true;
    }

    void DearImGuiTestEngineDidBecomeReady(BeebWindow *beeb_window, ImGuiStuff *imgui_stuff) override {
        ImGuiTestEngine *test_engine = imgui_stuff->GetTestEngine();
        TEST_NON_NULL(test_engine);
        this->RegisterDearImGuiTest(test_engine, beeb_window);
        TEST_EQ_PP(m_test_engine, test_engine);
        TEST_NON_NULL(m_test);

        ImGuiTestEngineIO *io = &ImGuiTestEngine_GetIO(test_engine);
        io->ConfigLogToTTY = true;
        io->ConfigLogToDebugger = true;
        io->ConfigBreakOnError = true;
        //io->ConfigRunSpeed = ImGuiTestRunSpeed_Cinematic;

        if (this->IsHeadless() || this->ShouldAutoRunTest()) {
            ImGuiTestEngine_QueueTest(m_test_engine, m_test);
        }
    }

    void RegisterDearImGuiTest(ImGuiTestEngine *test_engine, BeebWindow *beeb_window) override {
        m_test = IM_REGISTER_TEST(test_engine, "b2", nullptr);
        m_test->SetOwnedName(this->GetFullName().c_str());
        m_test->TestFunc = [this, beeb_window](ImGuiTestContext *ctx) {
            this->DearImGuiTestFunc(ctx, beeb_window);
            m_test_was_run = true;
        };
        TEST_NULL(m_test_engine);
        m_test_engine = test_engine;
    }
#endif

    bool HandleSelectorDialogOpen(std::string *result, const Guid &guid) override {
        auto &&it = m_selector_results_by_guid.find(guid);
        if (it == m_selector_results_by_guid.end()) {
            TEST_FALSE(this->IsHeadless());
            TEST_TRUE(m_allow_interactive_selector_dialogs);
            return false;
        }

        SelectorResults *results = &m_selector_results_by_guid[guid];
        TEST_TRUE(results->got_next_result);
        *result = std::move(results->next_result);
        results->got_next_result = false;
        return true;
    }

    void SetSelectorDialogResult(const Guid &guid, const std::string &result) override {
        SelectorResults *results = &m_selector_results_by_guid[guid];
        TEST_FALSE(results->got_last_result);
        results->got_last_result = true;
        results->last_result = result;
    }

    bool ShouldQuitWhenTestQueueEmpty() const override {
        if (this->IsHeadless()) {
            // Quit.
            return true;
        } else {
            // Keep running. See what happens. Quit manually if you want the test to continue.
            return false;
        }
    }

    virtual bool ShouldClearConfigFolder() const {
        return true;
    }

    bool IsFlashingCursorAlwaysVisible() const override {
        return false;
    }

  protected:
    std::string GetLastSelectorDialogResult(const Guid &guid) {
        SelectorResults *results = &m_selector_results_by_guid[guid];
        TEST_TRUE(results->got_last_result);
        results->got_last_result = false;
        return std::move(results->last_result);
    }

    void SetNextSelectorDialogResult(const Guid &guid, std::string result) {
        SelectorResults *results = &m_selector_results_by_guid[guid];
        TEST_FALSE(results->got_next_result);
        results->next_result = std::move(result);
        results->got_next_result = true;

        // reset any previous result.
        results->got_last_result = false;
        results->last_result.clear();
    }

    [[nodiscard]] int Run2() {
        // Get custom config folder. Don't continue if using the default, as files will be deleted.
        std::string config_folder;
        TEST_TRUE(this->GetConfigOverrideFolder(&config_folder));

        if (!PathIsFolderOnDisk(config_folder)) {
            PathCreateFolder(config_folder);
        }

        if (this->ShouldClearConfigFolder()) {
            // Clear out contents of config folder.
            PathGlob(config_folder, [](const std::string &path, bool is_folder) -> void {
                if (is_folder) {
                    // Ignore any folders. They're (probably) the cache folder.
                    // Though it doesn't really matter either way, as b2 only uses
                    // files immediately under the config folder.
                } else {
                    TEST_TRUE(PathDeleteFile(path));
                }
            });
        }

        int result = b2_main(this);
        if (this->IsHeadless()) {
            TEST_TRUE(m_test_was_run);
        } else {
            // In interactive mode, the user is under no obligation to run the test.
        }
        return result;
    }

    std::vector<std::string> m_args; //excludes argv[0]
    ImGuiTestEngine *m_test_engine = nullptr;

    // if true, allow interactive selector dialogs: if no result set for the
    // dialog, and running interactively, just pop it up and let the thing
    // happen.
    //
    // TODO: this mechanism hasn't really been thought through thoroughly and
    // will/should probably change.
    bool m_allow_interactive_selector_dialogs = false;

  private:
    struct SelectorResults {
        // upcoming result set by SetNextSelectorDialogResult. If set, next use
        // of this dialog will return this string.
        std::string next_result;
        bool got_next_result = false;

        // result from last use of this dialog, whether set automatically or
        // interactively.
        std::string last_result;
        bool got_last_result = false;
    };

    //BeebWindow *m_beeb_window = nullptr;
    ImGuiTest *m_test = nullptr;
    int m_http_port = 0;
    std::map<Guid, SelectorResults> m_selector_results_by_guid;
    bool m_test_was_run = false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Yielder {
  public:
    explicit Yielder(ImGuiTestContext *ctx, BeebWindow *beeb_window, DearImGuiTest *test)
        : m_ctx(ctx)
        , m_beeb_window(beeb_window)
        , m_test(test) {
        this->Reset();
    }

    void Reset() {
        m_start_ticks = GetCurrentTickCount();
    }

    void Yield() {
        if (IsDebuggerAttached()) {
            // Ignore timeout in this case.
        } else if (GetSecondsFromTicks(GetCurrentTickCount() - m_start_ticks) > m_time_limit_seconds) {
            std::string config_folder;
            TEST_TRUE(m_test->GetConfigOverrideFolder(&config_folder));

            SDLUniquePtr<SDL_Surface> display_data = m_beeb_window->GetDisplayData(false, g_stdio_logs);
            TEST_NON_NULL(display_data.get());

            std::vector<uint8_t> png_data;
            TEST_TRUE(SaveSDLSurfaceToPNGData(&png_data, display_data.get(), g_stdio_logs));

            std::string image_path = PathJoined(config_folder, "timeout." + m_test->GetFullName() + ".png");
            TEST_TRUE(SaveFile(png_data, image_path, &g_stdio_logs));

            TEST_FALSE(true);
        }
        m_ctx->Yield();
    }

    void
    YieldUntilMessageQueueEmpty() {
        std::shared_ptr<BeebThread> beeb_thread = m_beeb_window->GetBeebThread();

        while (beeb_thread->AreNonTimingMessagesPending()) {
            this->Yield();
        }
    }

  protected:
  private:
    ImGuiTestContext *const m_ctx = nullptr;
    BeebWindow *const m_beeb_window = nullptr;
    DearImGuiTest *const m_test = nullptr;
    uint64_t m_start_ticks = 0;

    // TODO: make it configurable. Default test engine watchdog starts moaning at 30 seconds.
    const double m_time_limit_seconds = 30.f;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Paste text and wait for the paste to complete.
static void PasteAndWait(Yielder *yielder, const std::shared_ptr<BeebThread> &beeb_thread, const std::string &text_) {
    std::vector<uint8_t> text;
    for (char c : text_) {
        TEST_TRUE((c >= 32 && c < 127) || c == 13);
        text.push_back((uint8_t)c);
    }

    std::atomic<bool> pasted_status = false;
    beeb_thread->Send(std::make_shared<BeebThread::StartPasteMessage>(text, 0),
                      [&pasted_status](const char *failure_reason, const char *failure_text) -> void {
                          (void)failure_text;

                          TEST_NULL(failure_reason);
                          pasted_status = true;
                      });

    int state = 0;
    yielder->Reset();
    while (!pasted_status) {
        yielder->Yield();

        switch (state) {
        case 0:
            if (beeb_thread->IsPasting()) {
                state = 1;
            }
            break;

        case 1:
            if (!beeb_thread->IsPasting()) {
                state = 2;
            }
            break;

        default:
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CountOSWORD0s : public OSWORD0Callback {
  public:
    uint64_t GetNumOSWORD0s() const {
        return m_num_osword_0s.load(std::memory_order_acquire);
    }

    bool ThreadOnOSWORD0(BeebThread *, bool) override {
        m_num_osword_0s.fetch_add(1, std::memory_order_acq_rel);

        return true;
    }

  protected:
  private:
    std::atomic<uint64_t> m_num_osword_0s{0};
};

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
            std::vector<uint8_t> wanted_ascii = {'`', '`'};

            std::vector<uint8_t> got_ascii;
            GetBBCASCIIFromISO8859_1(&got_ascii, {'`', 0xa3});

            TEST_EQ_UU(got_ascii.size(), wanted_ascii.size());
            TEST_EQ_AA(got_ascii.data(), wanted_ascii.data(), got_ascii.size());
        }

        const std::vector<uint8_t> annoying_chars = {'`', '|', '\\', '{', '[', ']', '}', '^', '_'};
        {
            TEST_EQ_SS(GetUTF8FromBBCASCII(annoying_chars, BBCUTF8ConvertMode_PassThrough, false), "`|\\{[]}^_");
            TEST_EQ_SS(GetUTF8FromBBCASCII(annoying_chars, BBCUTF8ConvertMode_OnlyGBP, false), "\xc2\xa3|\\{[]}^_");
        }

        {
            std::string utf8 = GetUTF8FromBBCASCII(annoying_chars, BBCUTF8ConvertMode_SAA5050, false);
            std::vector<uint8_t> bbc_ascii;

            int32_t bad_codepoint;
            size_t bad_char_start;
            int bad_char_len;
            TEST_TRUE(GetBBCASCIIFromUTF8(&bbc_ascii, utf8, &bad_codepoint, &bad_char_start, &bad_char_len));

            TEST_EQ_UU(bbc_ascii.size(), annoying_chars.size());
            TEST_EQ_AA(bbc_ascii.data(), annoying_chars.data(), bbc_ascii.size());
        }

        {
            std::vector<uint8_t> vdu14 = {'A', 14, 'B'};
            TEST_EQ_SS(GetUTF8FromBBCASCII(vdu14, BBCUTF8ConvertMode_PassThrough, false), "AB");

            std::vector<uint8_t> vdu15 = {'A', 15, 'B'};
            TEST_EQ_SS(GetUTF8FromBBCASCII(vdu15, BBCUTF8ConvertMode_PassThrough, false), "AB");
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

static const char TASS_DUMPED_LABELS_TEST_DATA_1[] =
    "../../dependencies/beeb/include/common.s65:396:1: WD177x.status = 0\n"
    "../../dependencies/beeb/include/common.s65:190:1: tube_256_byte_parasite_to_host = 6\n"
    "fdload.s65:244:1: fdload_prepare = 3466\n"
    "../../dependencies/beeb/include/common.s65:167:1: key_numpad_plus = $3a\n"
    "../../dependencies/beeb/include/common.s65:34:1: osbget = address($ffd7)\n"
    "framework_bank.loader.s65:64:1: framework_bank_exports = $badd\n"
    "zx02_decomp.s65:29:1: jsr_get_src_byte_addrs := [3156,3178,3249]\n"
    "zx02_decomp.s65:2:1: zx02_decomp_enable_multi_part = true\n";

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
        this->TestTassDumpedLabelsStuff();
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

    void TestTassDumpedLabelsStuff() {
        const SymbolTable::SymbolParser *tass_dumped_labels_parser = SymbolTable::SymbolParserRegistry::FindParserByFormatName("64tass_dumped_labels");
        TEST_NON_NULL(tass_dumped_labels_parser);

        SymbolTable st;

        TEST_TRUE(st.LoadFromString(TASS_DUMPED_LABELS_TEST_DATA_1, "1", tass_dumped_labels_parser, nullptr));

        std::shared_ptr<const BBCMicroType> type = CreateTestBBCMicroType();

        TEST_EQ_UU(st.GetSymbolCount(), 6);

        uint16_t addr;
        uint32_t dso;

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "osbget"));
        TEST_EQ_UU(addr, 0xffd7);

        TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "WD177x.status"));
        TEST_EQ_UU(addr, 0);
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

    void DearImGuiTestFunc(ImGuiTestContext *ctx, BeebWindow *beeb_window) override {
        (void)beeb_window;

        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("###file/###exit/###confirm");
    }

    void Run() override {
        TEST_EQ_II(this->Run2(), 0);
    }

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestStbImageUTF8 : public Test {
  public:
    std::string GetFullName() const override {
        return "misc.stb_image_utf8";
    }

    void Run() override {
#if SYSTEM_WINDOWS

        std::string image_path_utf8 = PathJoined(TRANSIENT_DATA_FOLDER, this->GetFullName(), (const char *)u8"\u00A3.png");
        PathCreateFolder(PathGetFolder(image_path_utf8));

        TEST_EQ_SS(GetUTF8String(GetWideString(image_path_utf8)), image_path_utf8);
        TEST_NE_SS(GetByteString(GetWideString(image_path_utf8), CP_THREAD_ACP), image_path_utf8);

        int width = 100, height = 100;

        std::vector<uint8_t> png_data;
        for (int i = 0; i < width * height * 4; ++i) {
            png_data.push_back(0xff);
        }

        TEST_TRUE(stbi_write_png(image_path_utf8.c_str(), width, height, 4, png_data.data(), width * 4));

        std::vector<uint8_t> data;
        TEST_TRUE(LoadFile(&data, image_path_utf8, nullptr, 0));

#else

        // not much point. Everything will be UTF8-friendly already.

#endif
    }

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestLoadPossiblyGzippedFile : public Test {
  public:
    std::string GetFullName() const override {
        return "misc.LoadPossiblyGzippedFile";
    }

    void Run() override {
        std::string names[] = {
            "Acornsoft Desk Diary (198x)(Acornsoft)",
            "Acornsoft Forth (1983)(Acornsoft)",
            "Acornsoft Forth (1983)(Acornsoft)[a]",
            "Acornsoft Zeichenbrett (198x)(Acornsoft)",
        };

        std::vector<uint8_t> wanted_concatenated_data;

        std::vector<uint8_t> got_concatenated_data;

        for (const std::string &name : names) {
            std::string stem = PathJoined(b2_SOURCE_DIR, "etc/tests/uef/" + name);

            std::vector<uint8_t> wanted_data;
            TEST_TRUE(LoadFile(&wanted_data, stem + ".uncompressed.uef", nullptr));
            wanted_concatenated_data.insert(wanted_concatenated_data.end(), wanted_data.begin(), wanted_data.end());

            std::vector<uint8_t> got_data;
            TEST_TRUE(LoadPossiblyGzippedFile(&got_data, stem + ".uef", nullptr));
            got_concatenated_data.insert(got_concatenated_data.end(), got_data.begin(), got_data.end());

            TEST_EQ_UU(got_data.size(), wanted_data.size());
            TEST_EQ_AA(got_data.data(), wanted_data.data(), got_data.size());
        }

        TEST_TRUE(DecompressGzip(&got_concatenated_data));

        TEST_EQ_UU(got_concatenated_data.size(), wanted_concatenated_data.size());
        TEST_EQ_AA(got_concatenated_data.data(), wanted_concatenated_data.data(), got_concatenated_data.size());
    }

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This test doesn't currently do anything particularly useful, but it does at
// least exercise some code, so it's not hidden.
class TestLoadUEF : public Test {
  public:
    std::string GetFullName() const override {
        return "b2.LoadUEF";
    }

    void Run() override {
        std::string path = PathJoined(b2_SOURCE_DIR, "etc/tests/uef/Acornsoft Desk Diary (198x)(Acornsoft).uef");

        std::vector<uint8_t> data;
        TEST_TRUE(LoadPossiblyGzippedFile(&data, path, nullptr));

        UEFReader uef_reader;

        TEST_TRUE(uef_reader.Load(data, path));

        std::set<uint16_t> chunk_ids;
        for (size_t i = 0; i < uef_reader.GetNumChunks(); ++i) {
            chunk_ids.insert(uef_reader.GetChunkByIndex(i).id);
        }
    }

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// see https://github.com/frida/glib/blob/81b631758fe5c665ade9d869554148f6160fe681/glib/tests/base64.c

static const char *const ok_100_encode_strs[] = {
    "AA==",
    "AAE=",
    "AAEC",
    "AAECAw==",
    "AAECAwQ=",
    "AAECAwQF",
    "AAECAwQFBg==",
    "AAECAwQFBgc=",
    "AAECAwQFBgcI",
    "AAECAwQFBgcICQ==",
    "AAECAwQFBgcICQo=",
    "AAECAwQFBgcICQoL",
    "AAECAwQFBgcICQoLDA==",
    "AAECAwQFBgcICQoLDA0=",
    "AAECAwQFBgcICQoLDA0O",
    "AAECAwQFBgcICQoLDA0ODw==",
    "AAECAwQFBgcICQoLDA0ODxA=",
    "AAECAwQFBgcICQoLDA0ODxAR",
    "AAECAwQFBgcICQoLDA0ODxAREg==",
    "AAECAwQFBgcICQoLDA0ODxAREhM=",
    "AAECAwQFBgcICQoLDA0ODxAREhMU",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFQ==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRY=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYX",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGA==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBk=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBka",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGw==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxw=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHg==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8g",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gIQ==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISI=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIj",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJA==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCU=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUm",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJw==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJyg=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygp",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKg==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKis=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKiss",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLQ==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4v",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMA==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDE=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEy",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMw==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Ng==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OQ==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PA==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+Pw==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0A=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BB",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQg==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkM=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNE",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERQ==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUY=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZH",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSA==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSEk=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElK",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKSw==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0w=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xN",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTg==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk8=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9Q",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUQ==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVI=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJT",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVA==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFU=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVW",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWVw==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1g=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZ",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWg==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWls=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltc",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXQ==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV4=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5f",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYA==",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGE=",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFi",
    "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiYw==",
    NULL};

class TestBase64 : public Test {
  public:
    std::string GetFullName() const override {
        return "misc.base64";
    }

    void Run() override {
        // test encode/becode.
        for (size_t i = 0; ok_100_encode_strs[i]; ++i) {
            size_t n = i + 1;
            std::vector<uint8_t> data;
            for (size_t j = 0; j < n; ++j) {
                data.push_back((uint8_t)j);
            }

            std::string str = Base64Encode(data);
            TEST_EQ_SS(str, ok_100_encode_strs[i]);

            std::vector<uint8_t> data2;
            TEST_TRUE(Base64Decode(&data2, ok_100_encode_strs[i], nullptr));

            TEST_EQ_UU(data2.size(), data.size());
            TEST_EQ_AA(data2.data(), data.data(), data2.size());
        }
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

    void DearImGuiTestFunc(ImGuiTestContext *ctx, BeebWindow *beeb_window) override {
        (void)beeb_window;

        if (!m_disk_path.empty()) {
            this->SetNextSelectorDialogResult(NEW_DISK_IMAGE_SELECTOR_GUID, m_disk_path);
        }

        ctx->SetRef("##MainMenuBar");

        std::string path = "###file/###drive" + std::to_string(m_drive) + "/";
        if (m_in_memory) {
            path += "###new_memory/";
        } else {
            path += "###new_file/";
        }
        path += "###" + m_disk->name;

        ctx->MenuClick(path.c_str());

        m_disk_path = this->GetLastSelectorDialogResult(NEW_DISK_IMAGE_SELECTOR_GUID);
        TEST_FALSE(m_disk_path.empty());

        TEST_TRUE(PathIsFileOnDisk(m_disk_path, nullptr, nullptr));

        std::vector<uint8_t> wanted_data;
        TEST_TRUE(LoadFile(&wanted_data, m_disk->GetAssetPath(), nullptr));

        std::vector<uint8_t> got_data;
        TEST_TRUE(LoadFile(&got_data, m_disk_path, nullptr));

        TEST_EQ_UU(got_data.size(), wanted_data.size());

        if (m_disk->geometry->adfs) {
            // The disk identifier (and therefore the checksum) will have been
            // updated. Don't check that it's different, as the identifier is
            // random and so there's a non-zero chance that it'll be the same.
            // Overwrite the relevant got bytes with the wanted bytes so the
            // disk images otherwise match.
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
        TEST_TRUE(this->GetConfigOverrideFolder(&config_folder));
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

class TestLoadZippedDisk : public DearImGuiTest {
  public:
    TestLoadZippedDisk(std::string zip_path, std::string disk_path)
        : m_zip_path(std::move(zip_path))
        , m_disk_path(std::move(disk_path)) {
    }

    std::string GetFullName() const override {
        return "b2ui.load_zipped_disk." + PathWithoutExtension(PathGetName(m_zip_path));
    }

    void DearImGuiTestFunc(ImGuiTestContext *ctx, BeebWindow *beeb_window) override {
        std::shared_ptr<BeebThread> beeb_thread = beeb_window->GetBeebThread();

        Yielder yielder(ctx, beeb_window, this);

        TEST_TRUE(PathIsFileOnDisk(m_zip_path, nullptr, nullptr));
        if (!m_disk_path.empty()) {
            TEST_TRUE(PathIsFileOnDisk(m_disk_path, nullptr, nullptr));
        }

        {
            UniqueLock<Mutex> lock;
            TEST_NULL(beeb_thread->GetDiscImage(&lock, 0));
        }

        {
            UniqueLock<Mutex> lock;
            TEST_NULL(beeb_thread->GetDiscImage(&lock, 1));
        }

        this->SetNextSelectorDialogResult(OPEN_DISK_IMAGE_SELECTOR_GUID, m_zip_path);

        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("###file/###drive0/###open_memory");
        yielder.YieldUntilMessageQueueEmpty();

        if (m_disk_path.empty()) {
            {
                UniqueLock<Mutex> lock;
                TEST_NULL(beeb_thread->GetDiscImage(&lock, 0));
            }

            {
                UniqueLock<Mutex> lock;
                TEST_NULL(beeb_thread->GetDiscImage(&lock, 1));
            }
        } else {
            {
                UniqueLock<Mutex> lock;
                TEST_NON_NULL(beeb_thread->GetDiscImage(&lock, 0));
            }

            {
                UniqueLock<Mutex> lock;
                TEST_NULL(beeb_thread->GetDiscImage(&lock, 1));
            }

            this->SetNextSelectorDialogResult(SAVE_DISK_IMAGE_COPY_SELECTOR_GUID, m_disk_copy_path);

            ctx->SetRef("##MainMenuBar");
            ctx->MenuClick("###file/###drive0/###save_copy_as");
            yielder.YieldUntilMessageQueueEmpty();

            TEST_TRUE(PathIsFileOnDisk(m_disk_copy_path, nullptr, nullptr));
        }
    }

    void Run() override {
        if (!m_disk_path.empty()) {
            std::string config_folder;
            TEST_TRUE(this->GetConfigOverrideFolder(&config_folder));

            m_disk_copy_path = PathJoined(config_folder, "test." + PathGetName(m_disk_path));
        }

        TEST_EQ_II(this->Run2(), 0);
    }

  protected:
  private:
    std::string m_zip_path;
    std::string m_disk_path;
    std::string m_disk_copy_path;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Test HTTP API. When the message loop starts, indicating the HTTP server is
// ready, a background thread is started that calls Thread. Use this to do
// blocking HTTP client calls and check the results.
//
// Set args->test_was_run to true once done.
//
// Once Thread returns, the program will quit.
class TestHTTPAPI : public Test, public AppHandler {
  public:
    TestHTTPAPI() = default;

    std::string GetProductName() const override {
        return DEFAULT_PRODUCT_NAME;
    }

#if SYSTEM_OSX
    std::string GetFrameName() const override {
        return "";
    }
#endif

    bool IsHighDPIEnabled() const override {
        return false;
    }

    std::vector<std::string> GetCommandLineArgs() const override {
        return {"<<placeholder>>"};
    }

    void Run() override {
        int result = b2_main(this);
        TEST_TRUE(m_thread.joinable());
        m_thread_args.stop_thread.store(true, std::memory_order_release);
        m_thread.join();
        TEST_TRUE(m_thread_args.test_was_run.load(std::memory_order_acquire));
        TEST_EQ_II(result, 0);
    }

    bool IsHeadless() const override {
        return !g_interactive;
    }

    bool IsSoundEnabled() const override {
        // This intentionally also affects --interactive.
        return false;
    }

    bool GetConfigOverrideFolder(std::string *folder) const override {
        return HandleGetConfigOverrideFolder(folder, this->GetFullName());
    }

    bool GetCacheOverrideFolder(std::string *folder) const override {
        return HandleGetCacheOverrideFolder(folder, this->GetFullName());
    }

    bool GetFixedDisplaySize(ImVec2 *display_size) const override {
        (void)display_size;

        return true;
    }

    virtual int GetRequestedHttpServerListenPort() const override {
        // Let the OS choose. Don't have multiple instances fight.
        return 0;
    }

    void SetActualHttpServerListenPort(int port) override {
        TEST_LE_II(m_thread_args.http_port, 0);
        m_thread_args.http_port = port;
    }

    int GetLaunchRequestHttpServerPort() const override {
        return m_thread_args.http_port;
    }

    void MessageLoopWillStart() override {
        TEST_GT_II(m_thread_args.http_port, 0);
        TEST_FALSE(m_thread.joinable());
        TEST_FALSE(m_thread_args.test_was_run.load(std::memory_order_acquire));
        TEST_FALSE(m_thread_args.stop_thread.load(std::memory_order_acquire));
        m_thread = std::thread([this]() -> void {
            this->Thread(&m_thread_args);

            m_thread_args.test_was_run.store(true, std::memory_order_release);

            SDL_Event event = {};
            event.type = SDL_QUIT;
            SDL_PushEvent(&event);
        });
    }

    uint32_t GetUIFlags() const override {
        return UIFlag_HideAllPopups;
    }

    bool IsDearImGuiTestEngineEnabled() const override {
        return false;
    }

    bool ShouldQuitWhenTestQueueEmpty() const override {
        return false;
    }

    bool HandleSelectorDialogOpen(std::string *, const Guid &) override {
        return false;
    }

    bool IsFlashingCursorAlwaysVisible() const override {
        return false;
    }

  protected:
    struct ThreadArgs {
        int http_port = -1;
        std::atomic<bool> test_was_run{false};
        std::atomic<bool> stop_thread{false};
    };

    virtual void Thread(ThreadArgs *args) = 0;

    //    int GetActualHttpServerListenPort()const{
    //        TEST_GT_II(m_thread_args.http_port,0);
    //        return m_thread_args.http_port;
    //    }

  private:
    std::string m_name;
    std::thread m_thread;
    ThreadArgs m_thread_args;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
template <class T>
static HTTPRequest GetHTTPRequestForSingleApiRequest(std::string url, std::string type, const T &body) {
    HTTPRequest http_request;

    ApiMultipleRequests api_multiple_requests;

    {
        ApiRequest api_request;
        api_request.type = std::move(type);
        api_request.args = body;

        api_multiple_requests.requests.push_back(std::move(api_request));
    }

    http_request.url = std::move(url);
    http_request.method = "POST";
    http_request.content_type = HTTP_JSON_CONTENT_TYPE;
    http_request.body = SaveJSONData(api_multiple_requests);

    return http_request;
}
#endif

#if BBCMICRO_DEBUGGER
template <class T>
static T GetSingleApiResultFromHTTPResponse(const HTTPResponse &http_response) {
    TEST_EQ_SS(http_response.content_type, HTTP_JSON_CONTENT_TYPE);

    ApiMultipleResponses api_response;
    TEST_TRUE(LoadJSONData(&api_response, http_response.content, &g_stdio_logs));

    TEST_EQ_UU(api_response.responses.size(), 1);

    T api_result;
    std::string exc_what;
    TEST_TRUE(LoadJSON(&api_result, api_response.responses[0].result, &exc_what));

    return api_result;
}
#endif

#if BBCMICRO_DEBUGGER
static std::vector<std::string> GetLines(std::string str) {
    std::vector<std::string> lines;
    ForEachLine(str, [&lines](const std::string_view &line) -> bool {
        lines.push_back(std::string(line));
        return true;
    });
    return lines;
}
#endif

static const BeebConfig *MustGetDefaultConfigByName(const std::string &name) {
    for (size_t i = 0; i < GetNumDefaultBeebConfigs(); ++i) {
        const BeebConfig *default_config = GetDefaultBeebConfigByIndex(i);

        if (default_config->name == name) {
            return default_config;
        }
    }

    TEST_FAIL("default config not found: %s", name.c_str());
}

class TestHTTPConfig : public TestHTTPAPI {
  public:
    explicit TestHTTPConfig(MOSType mos_type)
        : m_mos_type(std::move(mos_type)) {
    }

    std::string GetFullName() const override {
        return "b2.http.config." + m_mos_type.suffix;
    }

  protected:
    void Thread(ThreadArgs *args) override {
#if BBCMICRO_DEBUGGER
        std::unique_ptr<HTTPClient> client = CreateHTTPClient();
        client->SetLogs(&g_stdio_logs);
        client->SetVerbose(true);

        std::string url = strprintf("http://localhost:%d/api", args->http_port);

        ApiConfigArgs config_args;
        config_args.base_default_config = m_mos_type.default_config_name;
        config_args.wait_for_osword_0 = true;

        {
            HTTPResponse http_response;
            int status = client->SendRequest(GetHTTPRequestForSingleApiRequest(url, API_REQUEST_TYPE_CONFIG, config_args), &http_response);
            TEST_EQ_II(status, 200);
        }

        {
            HTTPResponse http_response;
            int status = client->SendRequest(GetHTTPRequestForSingleApiRequest(url, API_REQUEST_TYPE_START_CAPTURE_OSWRCH, nullptr), &http_response);
            TEST_EQ_II(status, 200);
        }

        {
            HTTPResponse http_response;
            ApiPasteArgs paste_args;
            TEST_TRUE(GetBBCASCIIFromUTF8(&paste_args.input.bytes, "REM DUMMY LINE\rREM TIME=0:REPEAT:UNTILTIME>200\r*FX0\r", nullptr, nullptr, nullptr));
            paste_args.wait_for_osword_0 = true;
            int status = client->SendRequest(GetHTTPRequestForSingleApiRequest(url, API_REQUEST_TYPE_PASTE, paste_args), &http_response);
            TEST_EQ_II(status, 200);
        }

        {
            HTTPResponse http_response;
            int status = client->SendRequest(GetHTTPRequestForSingleApiRequest(url, API_REQUEST_TYPE_STOP_CAPTURE_OSWRCH, nullptr), &http_response);
            TEST_EQ_II(status, 200);

            ApiStopCaptureOSWRCHResult result = GetSingleApiResultFromHTTPResponse<ApiStopCaptureOSWRCHResult>(http_response);
            //printf("got %zu\n", result.output.bytes.size());

            std::vector<std::string> lines = GetLines(GetUTF8FromBBCASCII(result.output.bytes, BBCUTF8ConvertMode_PassThrough, true));
            TEST_GE_UU(lines.size(), 2u);
            TEST_EQ_SS(lines[lines.size() - 2], m_mos_type.expected_os_version);
            TEST_EQ_SS(lines[lines.size() - 1], ">");
        }
#else
        (void)args;
#endif
    }

  private:
    MOSType m_mos_type;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestHTTPPasteOSWORD0Timeout : public TestHTTPAPI {
  public:
    explicit TestHTTPPasteOSWORD0Timeout(MOSType mos_type)
        : m_mos_type(std::move(mos_type)) {
    }

    std::string GetFullName() const override {
        return "b2.http.paste.osword_0_timeout." + m_mos_type.suffix;
    }

  protected:
    void Thread(ThreadArgs *args) override {
#if BBCMICRO_DEBUGGER
        std::unique_ptr<HTTPClient> client = CreateHTTPClient();
        client->SetLogs(&g_stdio_logs);
        client->SetVerbose(true);

        std::string url = strprintf("http://localhost:%d/api", args->http_port);

        {
            ApiConfigArgs config_args;
            config_args.base_default_config = m_mos_type.default_config_name;
            config_args.wait_for_osword_0 = true;

            HTTPResponse http_response;
            int status = client->SendRequest(GetHTTPRequestForSingleApiRequest(url, API_REQUEST_TYPE_CONFIG, config_args), &http_response);
            TEST_EQ_II(status, 200);
        }

        {
            ApiPasteArgs paste_args;
            TEST_TRUE(GetBBCASCIIFromUTF8(&paste_args.input.bytes, "TIME=0:REPEAT:UNTILTIME>100\r", nullptr, nullptr, nullptr));
            paste_args.wait_for_osword_0 = true;
            paste_args.wait_for_osword_0_timeout_seconds = .5;

            HTTPResponse http_response;
            int status = client->SendRequest(GetHTTPRequestForSingleApiRequest(url, API_REQUEST_TYPE_PASTE, paste_args), &http_response);
            TEST_EQ_II(status, 500);
            ApiFailureResult result = GetSingleApiResultFromHTTPResponse<ApiFailureResult>(http_response);
            TEST_EQ_SS(result.reason, "timeout");
        }
#else
        (void)args;
#endif
    }

  private:
    MOSType m_mos_type;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestHTTPConfigOSWORD0Timeout : public TestHTTPAPI {
  public:
    explicit TestHTTPConfigOSWORD0Timeout(MOSType mos_type)
        : m_mos_type(std::move(mos_type)) {
    }

    std::string GetFullName() const override {
        return "b2.http.config.osword_0_timeout." + m_mos_type.suffix;
    }

  protected:
    void Thread(ThreadArgs *args) override {
#if BBCMICRO_DEBUGGER
        std::unique_ptr<HTTPClient> client = CreateHTTPClient();
        client->SetLogs(&g_stdio_logs);
        client->SetVerbose(true);

        std::string url = strprintf("http://localhost:%d/api", args->http_port);

        // path to any old language ROM that doesn't do an OSWORD 0 in good time...
        std::string problem_rom_path = PathJoined(b2_SOURCE_DIR, "etc/tests/roms/Wordwise Plus v1.49 [variant 5].rom");

        {
            ApiConfigArgs config_args;
            config_args.base_default_config = m_mos_type.default_config_name;
            config_args.wait_for_osword_0 = true;
            config_args.wait_for_osword_0_timeout_seconds = .5;

            const BeebConfig *default_config = MustGetDefaultConfigByName(config_args.base_default_config);

            if (default_config->nvram.empty()) {
                // Problem ROM should replace BASIC.
                int8_t basic_bank = -1;
                for (int8_t bank = 15; bank >= 0; --bank) {
                    if (const BeebROM *beeb_rom = default_config->roms[bank].standard_rom) {
                        if (beeb_rom->rom == StandardROM_BASIC2) {
                            basic_bank = bank;
                            break;
                        }
                    }
                }

                TEST_GE_II(basic_bank, 0);

                ApiSidewaysROM rom;
                rom.bank = (uint8_t)basic_bank;
                rom.contents.path = problem_rom_path;

                config_args.sideways_roms.push_back(rom);
            } else {
                // Problem ROM should go in bank 8. Also fix up the LANG setting.
                ApiSidewaysROM rom;
                rom.bank = 8;
                rom.contents.path = problem_rom_path;

                config_args.sideways_roms.push_back(rom);

                // LANG setting is top 4 bits of byte +5.
                {
                    ApiConfigNVRAMByte byte;

                    byte.index = 5;
                    byte.mask = 0x0f;
                    byte.value = rom.bank << 4;

                    config_args.nvram_bytes.push_back(byte);
                }
            }

            HTTPResponse http_response;
            int status = client->SendRequest(GetHTTPRequestForSingleApiRequest(url, API_REQUEST_TYPE_CONFIG, config_args), &http_response);
            TEST_EQ_II(status, 500);
            ApiFailureResult result = GetSingleApiResultFromHTTPResponse<ApiFailureResult>(http_response);
            TEST_EQ_SS(result.reason, "timeout");
        }
#else
        (void)args;
#endif
    }

  private:
    MOSType m_mos_type;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestHTTPBRKTracking : public TestHTTPAPI {
  public:
    explicit TestHTTPBRKTracking(bool do_brk)
        : m_do_brk(do_brk) {
    }

    std::string GetFullName() const override {
        return strprintf("b2.http.track_brk.%d", m_do_brk);
    }

  protected:
    void Thread(ThreadArgs *thread_args) override {
#if BBCMICRO_DEBUGGER
        std::unique_ptr<HTTPClient> client = CreateHTTPClient();
        client->SetLogs(&g_stdio_logs);
        client->SetVerbose(true);

        std::string url = strprintf("http://localhost:%d/api", thread_args->http_port);

        ApiMultipleRequests requests;

        {
            ApiConfigArgs args;
            args.base_default_config = "B/Acorn 1770";
            args.wait_for_osword_0 = true;
            args.wait_for_osword_0_timeout_seconds = 10.;

            ApiRequest request;
            request.type = API_REQUEST_TYPE_CONFIG;
            request.args = args;

            requests.requests.push_back(std::move(request));
        }

        {
            ApiRequest request;
            request.type = API_REQUEST_TYPE_START_COUNTING_BRKS;
            requests.requests.push_back(std::move(request));
        }

        if (m_do_brk) {
            ApiPasteArgs args;
            args.input.bytes = {'S', 'T', 'O', 'P', '\r'};
            args.wait_for_osword_0 = true;

            ApiRequest request;
            request.type = API_REQUEST_TYPE_PASTE;
            request.args = std::move(args);

            requests.requests.push_back(std::move(request));
        }

        {
            ApiStopCountingBRKsArgs args;
            args.expected_brk_count = m_do_brk ? 1 : 0;

            ApiRequest request;
            request.type = API_REQUEST_TYPE_STOP_COUNTING_BRKS;
            request.args = std::move(args);

            requests.requests.push_back(std::move(request));
        }

        HTTPRequest http_request;
        http_request.url = url;
        http_request.method = "POST";
        http_request.content_type = HTTP_JSON_CONTENT_TYPE;
        http_request.body = SaveJSONData(requests);

        HTTPResponse http_response;
        int status = client->SendRequest(http_request, &http_response);
        TEST_EQ_II(status, 200);

#else
        (void)thread_args;
#endif
    }

  private:
    bool m_do_brk = false;
};

//template <class T>
//static T GetSingleApiResultFromHTTPResponse(const HTTPResponse &http_response) {
//    TEST_EQ_SS(http_response.content_type, HTTP_JSON_CONTENT_TYPE);
//
//    ApiMultipleResponses api_response;
//    TEST_TRUE(LoadJSONData(&api_response, http_response.content, &g_stdio_logs));
//
//    TEST_EQ_UU(api_response.responses.size(), 1);
//
//    T api_result;
//    std::string exc_what;
//    TEST_TRUE(LoadJSON(&api_result, api_response.responses[0].result, &exc_what));
//
//    return api_result;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestHTTPPeek : public TestHTTPAPI {
  public:
    TestHTTPPeek() = default;

    std::string GetFullName() const override {
        return "b2.http.peek";
    }

  protected:
    void Thread(ThreadArgs *thread_args) override {
#if BBCMICRO_DEBUGGER
        std::unique_ptr<HTTPClient> client = CreateHTTPClient();
        client->SetLogs(&g_stdio_logs);
        client->SetVerbose(true);

        std::string url = strprintf("http://localhost:%d/api", thread_args->http_port);

        ApiMultipleRequests requests;

        {
            ApiConfigArgs args;
            args.base_default_config = "B/Acorn 1770";
            args.wait_for_osword_0 = true;
            args.wait_for_osword_0_timeout_seconds = 10.;

            ApiRequest request;
            request.type = API_REQUEST_TYPE_CONFIG;
            request.args = args;

            requests.requests.push_back(std::move(request));
        }

        {
            ApiPeekArgs args;
            args.begin = 0x8000;
            args.size = 0x4000;
            args.suffix = "f";

            ApiRequest request;
            request.type = API_REQUEST_TYPE_PEEK;
            request.args = args;

            requests.requests.push_back(std::move(request));
        }

        {
            ApiPeekArgs args;
            args.begin = 0x8000;
            args.end = 0xc000;
            args.suffix = "e";

            ApiRequest request;
            request.type = API_REQUEST_TYPE_PEEK;
            request.args = args;

            requests.requests.push_back(std::move(request));
        }

        HTTPRequest http_request;
        http_request.url = url;
        http_request.method = "POST";
        http_request.content_type = HTTP_JSON_CONTENT_TYPE;
        http_request.body = SaveJSONData(requests);

        HTTPResponse http_response;
        int status = client->SendRequest(http_request, &http_response);
        TEST_EQ_II(status, 200);

        ApiMultipleResponses api_response;
        TEST_EQ_SS(http_response.content_type, HTTP_JSON_CONTENT_TYPE);
        TEST_TRUE(LoadJSONData(&api_response, http_response.content, &g_stdio_logs));

        TEST_TRUE(api_response.success);
        TEST_EQ_UU(api_response.responses.size(), 3);

        std::string exc_what;

        ApiPeekResult basic2_result;
        TEST_TRUE(LoadJSON(&basic2_result, api_response.responses[1].result, &exc_what));
        TEST_EQ_UU(basic2_result.data.bytes.size(), 16384);

        ApiPeekResult acorn_dfs_result;
        TEST_TRUE(LoadJSON(&acorn_dfs_result, api_response.responses[2].result, &exc_what));
        TEST_EQ_UU(acorn_dfs_result.data.bytes.size(), 16384);

        std::vector<uint8_t> basic2;
        TEST_TRUE(LoadFile(&basic2, BEEB_ROM_BASIC2.GetAssetPath(), &g_stdio_logs));
        TEST_EQ_UU(basic2.size(), 16384);
        TEST_EQ_AA(basic2_result.data.bytes.data(), basic2.data(), 16384);

        std::vector<uint8_t> acorn_dfs;
        TEST_TRUE(LoadFile(&acorn_dfs, BEEB_ROM_ACORN_DFS.GetAssetPath(), &g_stdio_logs));
        TEST_EQ_UU(acorn_dfs.size(), 16384);
        TEST_EQ_AA(acorn_dfs_result.data.bytes.data(), acorn_dfs.data(), 16384);

#else
        (void)thread_args;
#endif
    }

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// m_leds_popup_ticks
// m_messages_popup_ticks

class DocImageCreator : public DearImGuiTest {
  public:
    DocImageCreator(std::string output_path, const std::vector<std::string> &skip_sections, bool clean)
        : m_output_path(std::move(output_path))
        , m_clean(clean) {
        for (const std::string &skip_section : skip_sections) {
            m_skip_sections[skip_section] = false;
        }
    }

    std::string GetFullName() const override {
        return "b2.doc_image_creation";
    }

#if SYSTEM_OSX
    std::string GetFrameName() const override {
        return "";
    }
#endif

    bool IsHeadless() const override {
        return false;
    }

    // always interactive, so
    bool ShouldAutoRunTest() const override {
        return true;
    }

    void HandleBeebWindowPostInit(BeebWindow *beeb_window) override {
        beeb_window->m_imgui_test_engine_ui = false;
    }

    uint32_t GetUIFlags() const override {
        uint32_t ui_flags = m_ui_flags;

        ui_flags |= m_set_ui_flags;
        ui_flags &= ~m_clear_ui_flags;

        return ui_flags;
    }

    std::string GetDisplayedRecentPath(const std::string &path, const RecentPaths &paths) const override {
        (void)paths;

        // If the path is the cache path, strip it out, so the sub menus fit in the (not very wide) screen grabs.
        //
        // The cache path "API" was really not designed for this.

        std::string folder = PathGetFolder(path);
        std::string cache_folder = PathGetFolder(GetCachePath("dummy"));

        if (PathCompare(folder, cache_folder) == 0) {
            return PathGetName(path);
        } else {
            return path;
        }
    }

    bool IsFlashingCursorAlwaysVisible() const override {
        return true;
    }

    void DearImGuiTestFunc(ImGuiTestContext *ctx, BeebWindow *beeb_window) override {
        ASSERT(!m_beeb_window);
        m_beeb_window = beeb_window;

        ASSERT(!m_ctx);
        m_ctx = ctx;

        TEST_TRUE(PathCreateFolder(m_output_path));

        if (m_clean) {
            PathGlob(m_output_path, [](const std::string &path, bool is_folder) -> void {
                (void)is_folder;
                std::string ext = PathGetExtension(path);
                if (PathCompare(ext, ".png") == 0) {
                    PathDeleteFile(path);
                }
            });
        }
        //m_skip.insert("run_elite");

        ASSERT(!m_yielder);
        m_yielder = std::make_unique<Yielder>(ctx, m_beeb_window, this);

        this->HideMouse();
        m_set_ui_flags = UIFlag_HideAllPopups | UIFlag_HideDebuggerUI | UIFlag_HideExtrasUI;

        this->Capture("startup.png");

        //ctx->SetRef("##MainMenuBar");

        if (this->DoSection("elite")) {
            ctx->MenuAction(ImGuiTestAction_Hover, "//##MainMenuBar/###file/###run/###open_file");

            {
                ImRect rect = this->GetPopupStackEntryRect(0);
                this->UnionRect(&rect, this->GetPopupStackEntryRect(1));
                rect.Min.y = 0.f;

                this->CaptureRect("file_run_disk_image.png", rect);
            }

            //            ctx->SetRef("##MainMenuBar");
            //            ctx->MenuAction(ImGuiTestAction_Click, "###file/###run/###open_file");

            // https://github.com/mattgodbolt/jsbeeb/raw/refs/heads/8c46f43a7dcddb61ac2ae15504733c1d9b5633a0/public/discs/elite.ssd

            std::string elite_ssd_path = this->DownloadFileFromURL("Disc021-EliteD.ssd",
                                                                   "https://bbcmicro.co.uk/gameimg/discs/366/Disc021-EliteD.ssd",
                                                                   "application/vnd.acorn.disc-image.ssd");

            //            std::string elite_ssd_path = this->DownloadFileFromURL("elite.ssd",
            //                                                                   "https://raw.githubusercontent.com/mattgodbolt/jsbeeb/refs/heads/main/public/discs/elite.ssd",
            //                                                                   "application/octet-stream");

            this->SetNextSelectorDialogResult(OPEN_DISK_IMAGE_SELECTOR_GUID, elite_ssd_path);
            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###file/###run/###open_file");

            m_clear_ui_flags = UIFlag_HideAllPopups;

            // Extremely long-standing bug means the thing is briefly the wrong size.
            m_yielder->Yield();

            ImGuiTestItemInfo wi = ctx->WindowInfo("//###recent_messages");

            this->CaptureRect("message_popup.png",
                              ImRect(0, wi.Window->Pos.y - 50.f, FIXED_DISPLAY_SIZE.x, wi.Window->Pos.y + wi.Window->Size.y + 50.f),
                              CaptureRectFlag_DontInflateRect);

            m_clear_ui_flags = 0;

            // it's impossible to guarantee a good screen grab of the opening screen, because the emulatorl doesn't run determinastically relative to the UI. So this bit is commented out - but there's a known good screen grab (that happened to come out right on my Mac...) in the doc folder.

            {
                this->WaitForDiskAccess();

                this->HideMouse();

                this->Capture("running_elite_2.png");
            }
        }

        //std::string repton_ssd_path = this->DownloadFileFromURL("Disc015-ReptonP.ssd",
        //                                                        "https://bbcmicro.co.uk/gameimg/discs/266/Disc015-ReptonP.ssd",
        //                                                        "application/vnd.acorn.disc-image.ssd");

        //this->SetNextSelectorDialogResult(OPEN_DISK_IMAGE_SELECTOR_GUID, repton_ssd_path);
        //ctx->MenuAction(ImGuiTestAction_Click, "###file/###run/###open_file");

        //this->WaitForDiskAccess();

        ctx->MenuAction(ImGuiTestAction_Hover, "//##MainMenuBar/###file/###hard_reset/###confirm");

        {
            ImRect rect0 = this->GetPopupStackEntryRect(0);
            ImRect rect = this->GetPopupStackEntryRect(1);

            rect.Min.x = rect0.Min.x;
            rect.Min.y = 0.f; //bit cheesy

            this->CaptureRect("file.hard_reset.confirm.png", rect);
        }

        ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###file/###hard_reset/###confirm");

        this->Yield(20);

        {
            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###file");

            this->Yield(5);

            this->CaptureRect("menu.file.save_screenshot.png", this->GetItemRect("//##MainMenuBar/**/###save_screenshot"), CaptureRectFlag_MoveMouseToOrigin);

            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###edit");

            ImRect rect = this->GetPopupStackEntryRect(0);
            rect.Min.y = 0.f;
            rect.Max.y = this->GetItemRect("//##MainMenuBar/**/###paste_return").Max.y;

            this->CaptureRect("menu.edit.copy_text.png", rect, CaptureRectFlag_MoveMouseToOrigin);

            //ctx->MenuAction(ImGuiTestAction_Hover, "//##MainMenuBar/###edit/###copy_screenshot");

            this->CaptureRect("menu.edit.toggle_copy_oswrch_text.png", this->GetItemRect("//##MainMenuBar/**/###toggle_copy_oswrch_text"), CaptureRectFlag_MoveMouseToOrigin);
            this->CaptureRect("menu.edit.copy_basic.png", this->GetItemRect("//##MainMenuBar/**/###copy_basic"), CaptureRectFlag_MoveMouseToOrigin);
            this->CaptureRect("menu.edit.copy_screenshot.png", this->GetItemRect("//##MainMenuBar/**/###copy_screenshot"), CaptureRectFlag_MoveMouseToOrigin);
            this->CaptureRect("menu.edit.paste.png", this->GetItemRect("//##MainMenuBar/**/###paste"), CaptureRectFlag_MoveMouseToOrigin);
            this->CaptureRect("menu.edit.paste_return.png", this->GetItemRect("//##MainMenuBar/**/###paste_return"), CaptureRectFlag_MoveMouseToOrigin);

            ctx->MenuAction(ImGuiTestAction_Hover, "//##MainMenuBar/###edit/###copy_options");

            rect = this->GetPopupStackEntryRect(0);
            rect.Min.y = 0.f;
            this->UnionRect(&rect, this->GetPopupStackEntryRect(1));
            this->CaptureRect("menu.edit.copy_options.png", rect);
        }

        {
            std::string imgui_path = "//##MainMenuBar/###file/###drive0/###new_file/Blank DFS 80T SSD";
            std::string file_path = GetCachePath("80.ssd");

            ctx->MenuAction(ImGuiTestAction_Hover, imgui_path.c_str());

            {
                ImRect rect = this->GetPopupStackEntryRect(0);
                this->UnionRect(&rect, this->GetPopupStackEntryRect(1));
                this->UnionRect(&rect, this->GetPopupStackEntryRect(2));
                rect.Min.y = 0.f; //bit cheesy
                this->CaptureRect("file.drive0.new_file.80.ssd.png", rect);
            }

            this->SetNextSelectorDialogResult(NEW_DISK_IMAGE_SELECTOR_GUID, file_path);
            ctx->MenuAction(ImGuiTestAction_Click, imgui_path.c_str());

            ctx->MenuAction(ImGuiTestAction_Hover, "//##MainMenuBar/###file/###drive0");

            {
                ImRect rect = this->GetPopupStackEntryRect(0);
                this->UnionRect(&rect, this->GetPopupStackEntryRect(1));
                rect.Min.y = 0.f; //bit cheesy
                this->CaptureRect("file.drive0.info.png", rect);
            }

            std::shared_ptr<BeebThread> beeb_thread = beeb_window->GetBeebThread();

            Yielder yielder(ctx, beeb_window, this);

            PasteAndWait(&yielder, beeb_thread, "10MODE7\r20PRINTCHR$129\"Hello\"\r");

            this->Capture("example_basic_program.code.png");

            PasteAndWait(&yielder, beeb_thread, "RUN\r");

            this->Capture("example_basic_program.result.png");

            PasteAndWait(&yielder, beeb_thread, "SAVE\"TEST\"");

            // TODO: this should probably be productized
            {
                auto &&osword_0_counter = std::make_shared<CountOSWORD0s>();
                beeb_thread->Send(std::make_shared<BeebThread::AddOSWORD0CallbackMessage>(osword_0_counter));

                // (strictly speaking, no need to wait - polling the OSWORD 0 count would cover it)
                PasteAndWait(&yielder, beeb_thread, "\r");

                yielder.Reset();
                while (osword_0_counter->GetNumOSWORD0s() == 0) {
                    yielder.Yield();
                }

                beeb_thread->Send(std::make_shared<BeebThread::RemoveOSWORD0CallbackMessage>(osword_0_counter));
            }

            this->Capture("example_basic_program.save.png");

            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###file/###drive0/###eject/###confirm");
            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###file/###hard_reset/###confirm");
            this->Yield(20);
            //ctx->MenuAction(ImGuiTestAction_Hover, "//##MainMenuBar/###file/###drive0/###recent_file");
            ctx->MenuAction(ImGuiTestAction_Hover, "//##MainMenuBar/###file/###drive0/###recent_file/80.ssd");

            {
                ImRect rect = this->GetPopupStackEntryRect(0);
                this->UnionRect(&rect, this->GetPopupStackEntryRect(1));
                this->UnionRect(&rect, this->GetPopupStackEntryRect(2));
                rect.Min.y = 0.f; //bit cheesy
                this->CaptureRect("file.drive0.recent_file.80.ssd.png", rect);
            }
        }

        ctx->MenuAction(ImGuiTestAction_Hover, "//##MainMenuBar/###file/###hard_reset/###confirm");

        this->CaptureMouseRelativeRect("confirm.menu.png", -250, -25, 100, 25);

        ctx->MenuAction(ImGuiTestAction_Click, strprintf("//##MainMenuBar/###hardware/###%zu", this->MustFindConfigIndex("B/Acorn 1770")).c_str());

        ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###hardware");

        this->Capture("hardware_menu.png");

        ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###hardware/###toggle_configurations"); //on

        this->Capture("configs.png");

        this->ItemAction(ImGuiTestAction_Click, "//Configs/**/Host OS/...");

        this->CaptureRect("configs.rom_popup.b.host_os.png", this->GetPopupStackEntryRect(0), CaptureRectFlag_MoveMouseToOrigin);

        {
            // move entry 0 to entry 1, so both arrows are enabled
            ctx->ItemAction(ImGuiTestAction_Click, "//Configs/**/###down");

            ImRect rect;
            this->UnionRect(&rect, this->GetItemRect("//Configs/**/###up"));
            this->UnionRect(&rect, this->GetItemRect("//Configs/**/###delete"));

            this->CaptureRect("configs.buttons.png", rect, CaptureRectFlag_MoveMouseToOrigin);

            // reinstate original ordering
            ctx->ItemAction(ImGuiTestAction_Click, "//Configs/**/###up");
        }

        ctx->ItemAction(ImGuiTestAction_Click, "//Configs/**/D/...");

        this->CaptureRect("configs.rom_popup.b.sideways_rom.png", this->GetPopupStackEntryRect(0), CaptureRectFlag_MoveMouseToOrigin);

        ctx->ItemAction(ImGuiTestAction_Click, "//$FOCUSED/###type");

        this->CaptureRect("configs.rom_popup.b.sideways_rom_type.png", this->GetPopupStackEntryRect(1), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_DontInflateRect);

        this->ScrollToWindow("//Configs/###config/###tube_window");
        this->CaptureRect("config.tube.png", this->GetWindowRect("//Configs/###config/###tube_window"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);

        ctx->ScrollToItem("//Configs/**/###nula", ImGuiAxis_Y);
        this->CaptureRect("config.nula.png", this->GetItemRect("//Configs/**/###nula"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);

        ctx->ScrollToItem("//Configs/**/###mouse", ImGuiAxis_Y);
        ctx->ItemAction(ImGuiTestAction_Check, "//Configs/**/###mouse");
        this->CaptureRect("config.mouse.png", this->GetItemRect("//Configs/**/###mouse"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);
        ctx->ItemAction(ImGuiTestAction_Uncheck, "//Configs/**/###mouse");

        ctx->ScrollToItem("//Configs/**/###rom_board", ImGuiAxis_Y);
        this->CaptureRect("config.rom_board.png", this->GetItemRect("//Configs/**/###rom_board"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);

        ctx->ScrollToItem("//Configs/**/###beeblink", ImGuiAxis_Y);
        ctx->ItemAction(ImGuiTestAction_Check, "//Configs/**/###beeblink");
        this->CaptureRect("config.beeblink.png", this->GetItemRect("//Configs/**/###beeblink"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);
        ctx->ItemAction(ImGuiTestAction_Uncheck, "//Configs/**/###beeblink");

        ctx->ScrollToItem("//Configs/**/###ext_mem", ImGuiAxis_Y);
        ctx->ItemAction(ImGuiTestAction_Check, "//Configs/**/###ext_mem");
        this->CaptureRect("config.ext_mem.png", this->GetItemRect("//Configs/**/###ext_mem"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);
        ctx->ItemAction(ImGuiTestAction_Uncheck, "//Configs/**/###ext_mem");

        ctx->ScrollToItem("//Configs/**/###debug_hardware", ImGuiAxis_Y);
        ctx->ItemAction(ImGuiTestAction_Check, "//Configs/**/###debug_hardware");
        this->CaptureRect("config.debug_hardware.png", this->GetItemRect("//Configs/**/###debug_hardware"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);
        ctx->ItemAction(ImGuiTestAction_Uncheck, "//Configs/**/###debug_hardware");

        ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###hardware/###toggle_configurations"); //off

        ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###keyboard");

        this->Capture("keyboard_menu.png");

        ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###keyboard/###toggle_keyboard_layout");

        this->Capture("keyboard_layout_ui.png");

        ctx->ItemAction(ImGuiTestAction_Hover, "//Keyboard Layouts/**/###bbc/###LeftShift");

        {
            ImRect rect;
            this->UnionRect(&rect, this->GetItemRect("//Keyboard Layouts/**/###bbc/###LeftShift"));
            this->UnionRect(&rect, this->GetTooltipRect());
            this->CaptureRect("keyboard_layout_ui.key.hover.png", rect);
        }

        ctx->ItemAction(ImGuiTestAction_Click, "//Keyboard Layouts/**/###bbc/###LeftShift");

        this->CaptureRect("keyboard_layout_ui.key.click.png", this->GetPopupStackEntryRect(0));

        ctx->MouseClick(); // (a second click in the same point will cancel the popup)

        ctx->ItemAction(ImGuiTestAction_Click, "//Keyboard Layouts/**/###Default UK");

        ctx->ItemAction(ImGuiTestAction_Hover, "//Keyboard Layouts/**/###bbc/###ExclamationMark");

        {
            ImRect rect;
            this->UnionRect(&rect, this->GetItemRect("//Keyboard Layouts/**/###bbc/###ExclamationMark"));
            this->UnionRect(&rect, this->GetTooltipRect());
            this->CaptureRect("keyboard_layout_ui.char.hover.png", rect);
        }

        ctx->ItemAction(ImGuiTestAction_Click, "//Keyboard Layouts/**/###bbc/###ExclamationMark");

        this->CaptureRect("keyboard_layout_ui.char.click.png", this->GetPopupStackEntryRect(0));

        ctx->MouseClick(); // (a second click in the same point will cancel the popup)

        ctx->ItemAction(ImGuiTestAction_Click, "//Keyboard Layouts/**/###delete");
        ctx->ItemAction(ImGuiTestAction_Hover, "//Keyboard Layouts/**/###confirm");

        {
            ImRect rect;
            this->UnionRect(&rect, this->GetItemRect("//Keyboard Layouts/**/###delete"));
            this->UnionRect(&rect, this->GetItemRect("//Keyboard Layouts/**/###confirm"));
            this->CaptureRect("confirm.button.png", rect);
        }

        ctx->ItemAction(ImGuiTestAction_Click, "//Keyboard Layouts/**/###Default");

        ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###keyboard/###toggle_keyboard_layout");

        ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###tools/###toggle_messages");

        this->Capture("messages_popup.png");

        {
            ImVec2 pt = ctx->GetWindowTitlebarPoint("//Messages");
            ctx->MouseTeleportToPos(pt);
            ctx->MouseDown(ImGuiMouseButton_Left);

            // the dock handles seem to appear based on repeated mouse movement rather than distance. This value was determined by experiment.
            static constexpr int DOCK_HANDLE_MOVE_COUNT = 10;
            for (int i = 0; i < DOCK_HANDLE_MOVE_COUNT; ++i) {
                ctx->MouseMoveToPos({pt.x + i, pt.y});
            }
            this->Capture("dock_handles.png");

            // TODO: DockInto works fine for docking into dialogs, but not for
            // docking into the main display? Clearly something is wrong, but I
            // don't know what yet
            ctx->MouseTeleportToPos(ImVec2(FIXED_DISPLAY_SIZE.x * .5f, 30));
            ctx->Yield();

            ctx->MouseUp(ImGuiMouseButton_Left);
            this->Capture("1_dialog_docked.png");

            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###keyboard/###toggle_keyboard_layout");

            ctx->MouseTeleportToPos(ctx->GetWindowTitlebarPoint("//Keyboard Layouts"));
            ctx->MouseDown(ImGuiMouseButton_Left);

            for (int i = 0; i < DOCK_HANDLE_MOVE_COUNT; ++i) {
                ctx->MouseMoveToPos({pt.x + i, pt.y});
            }

            ctx->MouseTeleportToPos(ImVec2(20, FIXED_DISPLAY_SIZE.y * .5f));
            ctx->Yield();
            ctx->MouseUp(ImGuiMouseButton_Left);

            this->Capture("2_dialogs.docked.0.png");

            ctx->DockInto("//Keyboard Layouts", "//Messages", ImGuiDir_Left);

            this->Capture("2_dialogs.docked.1.png");

            ctx->DockInto("//Keyboard Layouts", "//Messages", ImGuiDir_None);

            this->Capture("2_dialogs.tabbed.png");

            {
                ImGuiWindow *window = ctx->GetWindowByRef("//Messages");
                TEST_NON_NULL(window);
                TEST_NON_NULL(window->DockNode);
                TEST_NON_NULL(window->DockNode->TabBar);

                // BarRect is the tabs only, excluding the disclosure arow.
                ImVec2 pos = window->DockNode->TabBar->BarRect.Min;
                pos.x -= 10;
                pos.y += 5;

                ctx->MouseTeleportToPos(pos);

                ctx->MouseDown(ImGuiMouseButton_Left);
                ctx->Yield();
                ctx->MouseUp(ImGuiMouseButton_Left);
                ctx->Yield();
                this->CaptureMouseRelativeRect("tab_bar_dropdown.png", -10.f, -5.f, 160.f, 75.f);
            }

            //this->Capture("test.png");

            //ctx->DockClear("//Keyboard Layouts",nullptr);

            // TODO: figure out how this works...
            //ctx->DockClear("//Messages", nullptr);
            //ctx->Yield();
            //ctx->DockInto("//Messages", "//DockSpace", ImGuiDir_Up);
            //ctx->Yield();
        }

        ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###keyboard/###toggle_keyboard_layout");
        ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###tools/###toggle_messages");

        {
            static const BeebROM *const MOS_PARTS[] = {
                &BEEB_ROM_MOS350_MOS_ROM,
                &BEEB_ROM_MOS350_SIDEWAYS_ROM_9,
                &BEEB_ROM_MOS350_SIDEWAYS_ROM_A,
                &BEEB_ROM_MOS350_SIDEWAYS_ROM_B,
                &BEEB_ROM_MOS350_SIDEWAYS_ROM_C,
                &BEEB_ROM_MOS350_SIDEWAYS_ROM_D,
                &BEEB_ROM_MOS350_SIDEWAYS_ROM_E,
                &BEEB_ROM_MOS350_SIDEWAYS_ROM_F,
            };

            std::vector<uint8_t> mos;
            for (const BeebROM *rom : MOS_PARTS) {
                std::vector<uint8_t> part;
                TEST_TRUE(LoadFile(&part, rom->GetAssetPath(), nullptr));
                TEST_EQ_UU(part.size(), 16384);
                mos.insert(mos.end(), part.begin(), part.end());
            }

            std::vector<uint8_t> multimos;
            for (size_t i = 0; i < 4; ++i) {
                multimos.insert(multimos.end(), mos.begin(), mos.end());
            }
            TEST_EQ_UU(multimos.size(), 524288);

            std::string multimos_path = PathJoined(TRANSIENT_DATA_FOLDER, "multimos.bin");
            TEST_TRUE(SaveFile(multimos, multimos_path, nullptr));

            static const char MASTER_128_MULTI_OS_CONFIG_NAME[] = "Master 128 (multi-OS)";
            {
                BeebConfig new_config = *MustGetDefaultConfigByName("Master 128 (MOS 3.50)");

                new_config.name = MASTER_128_MULTI_OS_CONFIG_NAME;
                new_config.os.standard_rom = nullptr;
                new_config.os.file_name = multimos_path;
                new_config.os_rom_type = OSROMType_MultiOSBank0;

                BeebWindows::AddConfig(new_config);
            }

            static const char MASTER_128_OS_TYPE_CONFIG_NAME[] = "Master 128 (MOS 3.50) (Test)";
            {
                BeebConfig new_config = *MustGetDefaultConfigByName("Master 128 (MOS 3.50)");

                new_config.name = MASTER_128_OS_TYPE_CONFIG_NAME;

                TEST_NON_NULL(new_config.os.standard_rom);
                new_config.os.file_name = new_config.os.standard_rom->GetAssetPath();
                new_config.os.standard_rom = nullptr;

                BeebWindows::AddConfig(new_config);
            }

            static const char BBC_B_OS_TYPE_CONFIG_NAME[] = "B/Acorn 1770 (Test)";
            {
                BeebConfig new_config = *MustGetDefaultConfigByName("B/Acorn 1770");

                new_config.name = BBC_B_OS_TYPE_CONFIG_NAME;

                TEST_NON_NULL(new_config.os.standard_rom);
                new_config.os.file_name = new_config.os.standard_rom->GetAssetPath();
                new_config.os.standard_rom = nullptr;

                BeebWindows::AddConfig(new_config);
            }

            ctx->MenuAction(ImGuiTestAction_Click, strprintf("//##MainMenuBar/###hardware/###%zu", this->MustFindConfigIndex("Master 128 (multi-OS)")).c_str());

            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###file/###hard_reset");

            {
                ImRect rect0 = this->GetPopupStackEntryRect(0);
                ImRect rect1 = this->GetPopupStackEntryRect(1);

                ImRect rect = rect1;
                rect.Min.x = rect0.Min.x;

                this->CaptureRect("reset.multi_os.png", rect);
            }

            ctx->MenuAction(ImGuiTestAction_Click, strprintf("//##MainMenuBar/###hardware/###%zu", this->MustFindConfigIndex("B/Acorn 1770 + 6502 second processor")).c_str());

            ctx->MenuAction(ImGuiTestAction_Hover, "//##MainMenuBar/###file/###hard_reset");

            {
                ImRect rect0 = this->GetPopupStackEntryRect(0);
                ImRect rect1 = this->GetPopupStackEntryRect(1);

                ImRect rect = rect1;
                rect.Min.x = rect0.Min.x;

                this->CaptureRect("reset.second_processor.png", rect);
            }

            {
                static std::pair<const char *, const char *> const TYPES[] = {
                    {BBC_B_OS_TYPE_CONFIG_NAME, "b"},
                    {MASTER_128_OS_TYPE_CONFIG_NAME, "master"},
                    {nullptr, nullptr},
                };

                for (size_t i = 0; TYPES[i].first; ++i) {
                    ctx->MenuAction(ImGuiTestAction_Click, strprintf("//##MainMenuBar/###hardware/###%zu", this->MustFindConfigIndex(TYPES[i].first)).c_str());
                    ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###hardware/###toggle_configurations"); //on

                    ImGuiTestItemInfo info = ctx->WindowInfo("//Configs/###config");
                    TEST_NE_UU(info.ID, 0);

                    ctx->ScrollToTop(info.ID);
                    ctx->Yield();

                    ctx->ItemAction(ImGuiTestAction_Click, "//Configs/**/Host OS/...");
                    ctx->ItemAction(ImGuiTestAction_Click, "//$FOCUSED/###type");

                    this->CaptureRect(strprintf("configs.rom_popup.%s.os_rom_type.png", TYPES[i].second), this->GetPopupStackEntryRect(1), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_DontInflateRect);

                    ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###hardware/###toggle_configurations"); //off
                }
            }
        }

        {
            ctx->MenuAction(ImGuiTestAction_Click, strprintf("//##MainMenuBar/###hardware/###%zu", this->MustFindConfigIndex("Electron/Plus 1/Plus 3")).c_str());
            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###hardware/###toggle_configurations"); //on
            ctx->ScrollToItem("//Configs/**/###plus3", ImGuiAxis_Y);
            this->CaptureRect("config.plus3.png", this->GetItemRect("//Configs/**/###plus3"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);
            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###hardware/###toggle_configurations"); //off
        }

        {
            ctx->MenuAction(ImGuiTestAction_Click, strprintf("//##MainMenuBar/###hardware/###%zu", this->MustFindConfigIndex("Master Compact (MOS 5.10)")).c_str());
            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###hardware/###toggle_configurations"); //on
            ctx->ScrollToItem("//Configs/**/###serial", ImGuiAxis_Y);
            ctx->ItemAction(ImGuiTestAction_Check, "//Configs/**/###serial");
            this->CaptureRect("config.serial.png", this->GetItemRect("//Configs/**/###serial"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);
            ctx->ItemAction(ImGuiTestAction_Uncheck, "//Configs/**/###serial");
            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###hardware/###toggle_configurations"); //off
        }

        {
            ctx->MenuAction(ImGuiTestAction_Click, strprintf("//##MainMenuBar/###hardware/###%zu", this->MustFindConfigIndex("Master 128 (MOS 3.20)")).c_str());

            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###hardware/###toggle_configurations"); //on

            // ADJI
            {
                ctx->ScrollToItem("//Configs/**/###adji", ImGuiAxis_Y);
                ctx->ItemAction(ImGuiTestAction_Check, "//Configs/**/###adji");
                this->ScrollToWindow("//Configs/###config/###adji_window");
                this->CaptureRect("config.adji.png", this->GetWindowRect("//Configs/###config/###adji_window"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);
                ctx->ItemAction(ImGuiTestAction_Uncheck, "//Configs/**/###adji");
            }

            // SCSI
            {
                ctx->ScrollToItem("//Configs/**/###scsi", ImGuiAxis_Y);
                this->ScrollToWindow("//Configs/###config/###scsi_window");
                ctx->ItemAction(ImGuiTestAction_Check, "//Configs/**/###scsi");
                this->ScrollToWindow("//Configs/###config/###scsi_window");
                this->CaptureRect("config.scsi.png", this->GetWindowRect("//Configs/###config/###scsi_window"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);

                {
                    ctx->ItemAction(ImGuiTestAction_Click, "//Configs/**/$$0/###scsi_file_menu");
                    ctx->MenuAction(ImGuiTestAction_Hover, "//$FOCUSED/File...");

                    ImRect rect = this->GetItemRect("//Configs/**/$$0/###scsi_file_menu");
                    this->UnionRect(&rect, this->GetPopupStackEntryRect(0));

                    this->CaptureRect("config.scsi.file.png", rect);
                }

                {
                    ctx->ItemAction(ImGuiTestAction_Click, "//Configs/**/$$0/###scsi_file_menu");
                    ctx->MenuAction(ImGuiTestAction_Hover, "//$FOCUSED/New/###10MB");

                    ImRect rect = this->GetItemRect("//Configs/**/$$0/###scsi_file_menu");
                    this->UnionRect(&rect, this->GetPopupStackEntryRect(0));
                    this->UnionRect(&rect, this->GetPopupStackEntryRect(1));

                    this->CaptureRect("config.scsi.new.png", rect);
                }

                ctx->ItemAction(ImGuiTestAction_Uncheck, "//Configs/**/###scsi");
            }

            // MMFS
            {
                ctx->ScrollToItem("//Configs/**/###mmfs", ImGuiAxis_Y);
                this->ScrollToWindow("//Configs/###config/###mmfs_window");
                ctx->ItemAction(ImGuiTestAction_Check, "//Configs/**/###mmfs");
                this->ScrollToWindow("//Configs/###config/###mmfs_window");
                this->CaptureRect("config.mmfs.png", this->GetWindowRect("//Configs/###config/###mmfs_window"), CaptureRectFlag_MoveMouseToOrigin | CaptureRectFlag_AddBorder);

                {
                    ctx->ItemAction(ImGuiTestAction_Click, "//Configs/**/###mmfs_file_menu");
                    ctx->ItemAction(ImGuiTestAction_Hover, "//$FOCUSED/File...");

                    ImRect rect = this->GetItemRect("//Configs/**/###mmfs_file_menu");
                    this->UnionRect(&rect, this->GetPopupStackEntryRect(0));

                    this->CaptureRect("config.mmfs.file.png", rect);
                }

                ctx->ItemAction(ImGuiTestAction_Uncheck, "//Configs/**/###mmfs");
            }

            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###hardware/###toggle_configurations"); //off
        }

        {
            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###keyboard/###toggle_command_keymaps"); //on
            this->CaptureRect("keyboard.command_keys.png", this->GetWindowRect("//Command Keys"), CaptureRectFlag_MoveMouseToOrigin);
            ctx->ItemAction(ImGuiTestAction_Click, "//Command Keys/**/###beeb_window");
            this->CaptureRect("keyboard.command_keys.beeb_window.png", this->GetWindowRect("//Command Keys"), CaptureRectFlag_MoveMouseToOrigin);
            ctx->ScrollToItem("//Command Keys/**/###hard_reset_multi_os_bank_0", ImGuiAxis_Y);
            ctx->ScrollToItem("//Command Keys/**/###copy_translation_none", ImGuiAxis_Y);
            this->CaptureRect("keyboard.command_keys.ambiguous.png", this->GetWindowRect("//Command Keys"), CaptureRectFlag_MoveMouseToOrigin);
            ctx->ItemAction(ImGuiTestAction_Click, "//Command Keys/**/###toggle_emulator_options");
            {
                ImRect rect = this->GetItemRect("//Command Keys/**/###toggle_emulator_options");
                this->UnionRect(&rect, this->GetPopupStackEntryRect(0));

                this->CaptureRect("keyboard.command_keys.choose_shortcut.png", rect);
            }

            static constexpr uint32_t SHORTCUT = PCKeyModifier_Shift | PCKeyModifier_Ctrl | SDLK_o;
            bool done = false;
            ForEachCommandTable2([&done](CommandTable2 *table) -> void {
                table->ForEachCommand([table, &done](Command2 *command) -> void {
                    if (!done) {
                        if (command->GetName() == "toggle_emulator_options") {
                            table->AddMapping(SHORTCUT, command);
                            done = true;
                        }
                    }
                });
            });
            ASSERT(done);

            std::string id = strprintf("//Command Keys/**/$$%" PRIu32 "/x", SHORTCUT);
            ctx->ItemAction(ImGuiTestAction_Hover, id.c_str());
            {
                ImRect wrect = this->GetWindowRect("//Command Keys");

                ImRect rect = this->GetItemRect("//Command Keys/**/###toggle_emulator_options");
                this->UnionRect(&rect, this->GetItemRect(id.c_str()));
                rect.Max.x = wrect.Max.x; //bit cheesy

                this->CaptureRect("keyboard.command_keys.remove_shortcut.png", rect);
            }

            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###keyboard/###toggle_command_keymaps"); //off
        }

        {
            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###keyboard/###toggle_prioritize_shortcuts");
            ctx->MenuAction(ImGuiTestAction_Hover, "//##MainMenuBar/###keyboard/###toggle_prioritize_shortcuts");
            {
                ImRect rect = this->GetPopupStackEntryRect(0);
                rect.Min.y = 0.f; //cheat
                this->CaptureRect("keyboard.toggle_prioritize_shortcuts.png", rect);
            }
            ctx->MenuAction(ImGuiTestAction_Click, "//##MainMenuBar/###keyboard/###toggle_prioritize_shortcuts");
        }

        //ImGuiTestItemInfo wi = ctx->WindowInfo("//Keyboard Layouts/###layouts");
        //ASSERT(wi.Window);
        //ctx->SetRef(wi.Window);
        ////wi=ctx->WindowInfo("###layouts_11749C39");
        ////ASSERT(wi.Window);
        ////ctx->SetRef(wi.Window);
        ////ctx->ItemAction(ImGuiTestAction_Hover, "");

        //###bbc/###3"); ///###stuff/###bbc/###3");

        // check that all sections specified were valid.
        for (auto &&name_and_tested : m_skip_sections) {
            (void)name_and_tested;
            ASSERT(name_and_tested.second);
        }

        // don't leave the UI state messed up! I find it useful to poke about
        // afterwards
        m_set_ui_flags = 0;
        m_beeb_window = nullptr;
        m_ctx = nullptr;
        m_yielder.reset();
    }

    void Run() override {
        TEST_EQ_II(this->Run2(), 0);
    }

  protected:
  private:
    std::string m_output_path;
    uint32_t m_set_ui_flags = 0;
    uint32_t m_clear_ui_flags = 0;
    uint32_t m_ui_flags = 0;
    BeebWindow *m_beeb_window = nullptr;
    ImGuiTestContext *m_ctx = nullptr;
    std::unique_ptr<Yielder> m_yielder;
    std::map<std::string, bool> m_skip_sections;
    bool m_clean = false;

    void Yield(unsigned n) {
        for (unsigned i = 0; i < n; ++i) {
            m_ctx->Yield();
        }
    }

    bool DoSection(const std::string &name) {
        auto &&it = m_skip_sections.find(name);
        if (it == m_skip_sections.end()) {
            return true;
        } else {
            it->second = true;
            return false;
        }
    }

    void HideMouse() {
        m_ctx->MouseMoveToPos({-100, -100});
    }

    size_t MustFindConfigIndex(const std::string &name) const {
        for (size_t i = 0; i < BeebWindows::GetNumConfigs(); ++i) {
            const BeebConfig *config = BeebWindows::GetConfigByIndex(i);
            if (config->name == name) {
                return i;
            }
        }

        TEST_FAIL("failed to find config: %s", name.c_str());
    }

    void WaitForDiskAccess() {
        // Wait for access to start.
        while (!(m_beeb_window->m_leds >> BBCMicroLEDFlag_FloppyDisksShift & BBCMicroLEDFlag_FloppyDisksMask)) {
            m_yielder->Yield();
        }

        // Wait for access to finish.
        while (m_beeb_window->m_leds >> BBCMicroLEDFlag_FloppyDisksShift & BBCMicroLEDFlag_FloppyDisksMask) {
            m_yielder->Yield();
        }
    }

    void Capture(const std::string &name) {
        SDLUniquePtr<SDL_Surface> surface = this->Capture();
        TEST_TRUE(SaveSDLSurface(surface.get(), PathJoined(m_output_path, name), g_stdio_logs));
    }

    void CaptureRect(const std::string &name, ImRect rect, uint32_t flags = 0) {
        ImVec2 mouse_pos = ImGui::GetMousePos();

        if (flags & CaptureRectFlag_MoveMouseToOrigin) {
            m_ctx->MouseTeleportToPos(ImVec2(0.f, 0.f));
        }

        bool border = false;

        if (!(flags & CaptureRectFlag_DontInflateRect)) {
            if (!(flags & CaptureRectFlag_MoveMouseToOrigin)) {
                // If not moving the mouse to the origin: inflate the rect a bit more so the whole cursor will end up visible inside the inflated area.
                ImVec2 offset, size, uv_border[2], uv_fill[2];
                if (ImFontAtlasGetMouseCursorTexData(m_ctx->UiContext->DrawListSharedData.FontAtlas, ImGui::GetMouseCursor(), &offset, &size, uv_border, uv_fill)) {
                    ImRect mrect;
                    mrect.Min = mouse_pos;
                    mrect.Max = mrect.Min + size;
                    this->UnionRect(&rect, mrect);
                }
            }

            border = true;
        }

        if (flags & CaptureRectFlag_AddBorder) {
            border = true;
        }

        static constexpr float BORDER_SIZE = 5.f;
        if (border) {
            this->InflateRect(&rect, BORDER_SIZE);
        }

        SDLUniquePtr<SDL_Surface> full_surface = this->Capture();

        int w = (int)(rect.Max.x - rect.Min.x);
        int h = (int)(rect.Max.y - rect.Min.y);

        SDLUniquePtr<SDL_Surface> subsurface(SDL_CreateRGBSurfaceWithFormat(0, w, h, -1, full_surface->format->format));

        SDL_Rect src_rect;
        src_rect.x = (int)rect.Min.x;
        src_rect.y = (int)rect.Min.y;
        src_rect.w = w;
        src_rect.h = h;

        SDL_Rect dest_rect = {};

        int blit_result = SDL_BlitSurface(full_surface.get(), &src_rect, subsurface.get(), &dest_rect);
        TEST_EQ_II(blit_result, 0);

        if (flags & CaptureRectFlag_AddBorder) {
            Uint32 colour = SDL_MapRGBA(subsurface->format, 0, 0, 0, 255);

            // these do overlap.
            SDL_Rect top = {0, 0, w, (int)BORDER_SIZE};
            SDL_Rect left = {0, 0, (int)BORDER_SIZE, h};
            SDL_Rect bottom = {0, h - (int)BORDER_SIZE, w, (int)BORDER_SIZE};
            SDL_Rect right = {w - (int)BORDER_SIZE, 0, (int)BORDER_SIZE, h};

            SDL_FillRect(subsurface.get(), &top, colour);
            SDL_FillRect(subsurface.get(), &left, colour);
            SDL_FillRect(subsurface.get(), &bottom, colour);
            SDL_FillRect(subsurface.get(), &right, colour);
        }

        TEST_TRUE(SaveSDLSurface(subsurface.get(), PathJoined(m_output_path, name), g_stdio_logs));

        if (flags & CaptureRectFlag_MoveMouseToOrigin) {
            m_ctx->MouseTeleportToPos(mouse_pos);
        }
    }

    void ItemAction(ImGuiTestAction action, ImGuiTestRef ref) {
        ASSERT(ref.Path);
        printf("ItemAction: action=%d; Path=\"%s\"\n", (int)action, ref.Path);

        ImGuiTestItemInfo info = m_ctx->ItemInfo(ref);
        ASSERT(info.ID != 0);

        printf("ItemAction: info.ID=%" PRIu32 " (0x%" PRIx32 ")\n", info.ID, info.ID);
        printf("ItemAction: info.Window=%p\n", (void *)info.Window);
        if (info.Window) {
            printf("ItemAction: info.Window->ParentWindow=%p\n", (void *)info.Window->ParentWindow);
        }

        m_ctx->ItemAction(action, ref);
    }

    void ScrollToWindow(ImGuiTestRef ref) {
        ImGuiTestItemInfo info = m_ctx->WindowInfo(ref);

        ASSERT(info.Window);
        ASSERT(info.Window->ParentWindow);

        float min_y = info.Window->Pos.y - info.Window->ParentWindow->Pos.y;
        float max_y = min_y + info.Window->Size.y;

        m_ctx->ScrollToPos(info.Window->ParentWindow->ID, min_y, ImGuiAxis_Y);
        m_ctx->ScrollToPos(info.Window->ParentWindow->ID, max_y, ImGuiAxis_Y);
    }

    ImRect GetMouseRelativeRect(float dx0, float dy0, float dx1, float dy1) {
        ImVec2 mouse_pos = ImGui::GetMousePos();

        float mx = mouse_pos.x;
        float my = mouse_pos.y;

        return ImRect(mx + dx0, my + dy0, mx + dx1, my + dy1);
    }

    ImRect GetItemRect(ImGuiTestRef ref) {
        ImGuiTestItemInfo info = m_ctx->ItemInfo(ref);
        ASSERT(info.ID != 0);
        return info.RectFull;
    }

    ImRect GetWindowRect(ImGuiTestRef ref) {
        ImGuiTestItemInfo info = m_ctx->WindowInfo(ref);
        ASSERT(info.ID != 0);
        return info.RectFull;
    }

    ImRect GetWindowRect(const ImGuiWindow *window) {
        ASSERT(window);
        ImRect rect(window->Pos, window->Pos + window->Size);
        return rect;
    }

    ImRect GetTooltipRect() {
        ASSERT(m_ctx->UiContext->TooltipPreviousWindow);
        return this->GetWindowRect(m_ctx->UiContext->TooltipPreviousWindow);
    }

    ImRect GetPopupStackEntryRect(int index) {
        ASSERT(index >= 0 && index < m_ctx->UiContext->OpenPopupStack.Size);
        const ImGuiPopupData *popup = &m_ctx->UiContext->OpenPopupStack[index];

        return this->GetWindowRect(popup->Window);
    }

    void UnionRect(ImRect *rect, const ImRect &other) {
        if (rect->Min.x == 0.f && rect->Min.y == 0.f && rect->Max.x == 0.f && rect->Max.y == 0.f) {
            *rect = other;
        } else {
            rect->Min.x = std::min(rect->Min.x, other.Min.x);
            rect->Min.y = std::min(rect->Min.y, other.Min.y);
            rect->Max.x = std::max(rect->Max.x, other.Max.x);
            rect->Max.y = std::max(rect->Max.y, other.Max.y);
        }
    }

    ImRect GetRectsUnion(const std::initializer_list<ImRect> &rects) {
        ImRect rect;

        if (rects.size() > 0) {
            std::initializer_list<ImRect>::const_iterator it = rects.begin();

            rect = *it++;

            while (it != rects.end()) {
                rect.Min.x = std::min(rect.Min.x, it->Min.x);
                rect.Min.y = std::min(rect.Min.y, it->Min.y);
                rect.Max.x = std::max(rect.Max.x, it->Max.x);
                rect.Max.y = std::max(rect.Max.y, it->Max.y);

                ++it;
            }
        }

        return rect;
    }

    void InflateRect(ImRect *rect, float amt) {
        rect->Min.x -= amt;
        rect->Min.y -= amt;
        rect->Max.x += amt;
        rect->Max.y += amt;
    }

    void CaptureMouseRelativeRect(const std::string &name, float dx0, float dy0, float dx1, float dy1) {
        ImRect rect = this->GetMouseRelativeRect(dx0, dy0, dx1, dy1);
        return this->CaptureRect(name, rect, CaptureRectFlag_DontInflateRect);
    }

    SDLUniquePtr<SDL_Surface> Capture() {
        TEST_TRUE(m_beeb_window->CaptureNextBackBuffer());

        SDLUniquePtr<SDL_Surface> surface;
        while (!m_beeb_window->TakeCapturedBackBuffer(&surface)) {
            m_yielder->Yield();
        }
        TEST_NON_NULL(surface);

        return surface;
    }

    std::string DownloadFileFromURL(const std::string &name, std::string url, const char *expected_mime_type = nullptr) {
        std::string local_path = GetCachePath(name);
        if (!PathIsFileOnDisk(local_path, nullptr, nullptr)) {
            HTTPRequest request;
            request.url = std::move(url);
            request.method = "GET";

            std::unique_ptr<HTTPClient> client = CreateHTTPClient();
            client->SetLogs(&g_stdio_logs);
            //client->SetVerbose(true);

            HTTPResponse response;
            int status = client->SendRequest(request, &response);
            TEST_EQ_II(status, 200);
            if (expected_mime_type) {
                TEST_EQ_SS(response.content_type, expected_mime_type);
            }

            TEST_TRUE(SaveFile(response.content, local_path, &g_stdio_logs, SaveFlag_CreateFolder));
        }

        return local_path;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BeebConfig *FindConfigByName(size_t *index, const std::string &name) {
    for (size_t i = 0; i < BeebWindows::GetNumConfigs(); ++i) {
        const BeebConfig *config = BeebWindows::GetConfigByIndex(i);
        if (config->name == name) {
            *index = i;
            return config;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Pick out the MODE value from the *STATUS output.
static uint8_t GetMODEFromSTATUSOutput(const std::string &status_output) {
    uint8_t mode;
    bool got_mode = false;
    std::string mode_prefix = "Mode     ";
    ForEachLine(status_output, [&mode, &got_mode, mode_prefix](const std::string_view &line) -> bool {
        if (line.size() > mode_prefix.size()) {
            if (line.substr(0, mode_prefix.size()) == mode_prefix) {
                TEST_FALSE(got_mode);
                std::string mode_str(line.substr(mode_prefix.size()));
                TEST_TRUE(GetUInt8FromString(&mode, mode_str, 10, nullptr));
                got_mode = true;
            }
        }

        return true;
    });
    TEST_TRUE(got_mode);
    return mode;
}

// Do a *STATUS and retrieve the output.
static std::string GetSTATUSOutput(ImGuiTestContext *ctx, BeebWindow *beeb_window, DearImGuiTest *test) {
    // the captures here are a little questionable. But nothing will be out of scope at the wrong point!
    std::shared_ptr<BeebThread> beeb_thread = beeb_window->GetBeebThread();

    Yielder yielder(ctx, beeb_window, test);

    PasteAndWait(&yielder, beeb_thread, "*STATUS");

    auto &&osword_0_counter = std::make_shared<CountOSWORD0s>();
    beeb_thread->Send(std::make_shared<BeebThread::AddOSWORD0CallbackMessage>(osword_0_counter));
    beeb_window->StartCaptureOSWRCH();

    // (strictly speaking, no need to wait - polling the OSWORD 0 count would cover it)
    PasteAndWait(&yielder, beeb_thread, "\r");

    yielder.Reset();
    while (osword_0_counter->GetNumOSWORD0s() == 0) {
        yielder.Yield();
    }

    beeb_thread->Send(std::make_shared<BeebThread::RemoveOSWORD0CallbackMessage>(osword_0_counter));

    std::vector<uint8_t> data;
    beeb_window->StopCaptureOSWRCH(&data);

    std::string text = GetUTF8FromBBCASCII(data, BBCUTF8ConvertMode_PassThrough, false);
    return text;
}

// https://github.com/tom-seddon/b2/issues/559
class TestNVRAMUpdate : public DearImGuiTest {
  public:
    TestNVRAMUpdate(std::string model_name, std::string config_name)
        : m_model_name(std::move(model_name))
        , m_config_name(std::move(config_name)) {
    }

    std::string GetFullName() const override {
        return "b2ui.nvram_update." + m_model_name;
    }

    void DearImGuiTestFunc(ImGuiTestContext *ctx, BeebWindow *beeb_window) override {
        Yielder yielder(ctx, beeb_window, this);

        size_t config_index = 0;
        const BeebConfig *config = FindConfigByName(&config_index, m_config_name);
        TEST_NON_NULL(config);

        uint8_t old_nvram_mode = config->nvram[10] & 7;
        TEST_FALSE(config->nvram.empty());

        const uint8_t new_mode = (old_nvram_mode + 1) & 7;

        ctx->SetRef("##MainMenuBar");
        std::string hardware_config_path = "###hardware/###" + std::to_string(config_index);
        ctx->MenuClick(hardware_config_path.c_str());

        // TODO: the stuff that's being done via the beeb thread might not be
        // feeding back in time to the UI for the test engine to deal with it?
        // This yield is not a good solution
        yielder.Yield();

        std::shared_ptr<BeebThread> beeb_thread = beeb_window->GetBeebThread();

        std::string old_status_output = GetSTATUSOutput(ctx, beeb_window, this);
        uint8_t old_status_mode = GetMODEFromSTATUSOutput(old_status_output);

        printf("original mode: %u\n", old_status_mode);
        TEST_EQ_UU(old_nvram_mode, old_status_mode);

        PasteAndWait(&yielder, beeb_thread, "*CONFIGURE MODE " + std::to_string(new_mode) + "\r");

        std::string new_status_output = GetSTATUSOutput(ctx, beeb_window, this);
        uint8_t new_status_mode = GetMODEFromSTATUSOutput(new_status_output);
        TEST_EQ_UU(new_mode, new_status_mode);

        // Ensure the BeebThread's copy of the BeebConfig got updated.
        ctx->MenuClick("###file/###hard_reset/###confirm");

        new_status_output = GetSTATUSOutput(ctx, beeb_window, this);
        new_status_mode = GetMODEFromSTATUSOutput(new_status_output);
        TEST_EQ_UU(new_mode, new_status_mode);

        // Ensure the original copy of the BeebConfig got updated.
        ctx->MenuClick(hardware_config_path.c_str());

        new_status_output = GetSTATUSOutput(ctx, beeb_window, this);
        new_status_mode = GetMODEFromSTATUSOutput(new_status_output);
        TEST_EQ_UU(new_mode, new_status_mode);

        // And make sure this wasn't just all a big coincidence.
        TEST_EQ_UU(config->nvram[10] & 7, new_mode);

        //        if(!this->IsHeadless()){
        //            // reset for the next interactive round.
        //            config->nvram[10]=(config->nvram[10]&0xf8)|old_mode;
        //        }
    }

    void Run() override {
        TEST_EQ_II(this->Run2(), 0);
    }

  protected:
  private:
    const std::string m_model_name;
    const std::string m_config_name;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static void PRINTF_LIKE(3, 4) PrintLibUVError(Log *log, int rc, const char *fmt, ...) {
//    va_list v;
//
//    va_start(v, fmt);
//    log->v(fmt, v);
//    va_end(v);
//
//    log->f(": %s (%s)\n", uv_strerror(rc), uv_err_name(rc));
//}

struct RerunState {
    int64_t exit_status = -1;
    int term_signal = -1;
};

static void HandleRerunExit(uv_process_t *process, int64_t exit_status, int term_signal) {
    auto rerun_state = (RerunState *)process->data;

    rerun_state->exit_status = exit_status;
    rerun_state->term_signal = term_signal;
}

static void Rerun(const std::string &name) {
    int rc;

    std::vector<std::string> args;
    args.push_back(PathGetEXEFileName());
    args.push_back("-t");
    args.push_back(name);

    std::vector<char *> args2;
    for (std::string &arg : args) {
        args2.push_back(arg.data());
    }
    args2.push_back(nullptr);

    std::vector<uv_stdio_container_t> stdios;
    for (int i = 0; i < 3; ++i) {
        uv_stdio_container_t stdio;
        stdio.flags = UV_INHERIT_FD;
        stdio.data.fd = i;
        stdios.push_back(stdio);
    }

    RerunState rerun_state;

    uv_loop_t loop{};
    rc = uv_loop_init(&loop);
    if (rc != 0) {
        TEST_FAIL("uv_loop_init failed: %s (%d; %s)", uv_strerror(rc), rc, uv_err_name(rc));
    }

    uv_process_options_t options = {};
    options.exit_cb = &HandleRerunExit;
    options.file = args[0].c_str();
    options.args = args2.data();
    ASSERT(stdios.size() <= INT_MAX);
    options.stdio_count = (int)stdios.size();
    options.stdio = stdios.data();

    uv_process_t subprocess = {};
    subprocess.data = &rerun_state;
    rc = uv_spawn(&loop, &subprocess, &options);
    if (rc != 0) {
        TEST_FAIL("uv_spawn failed: %s (%d; %s)", uv_strerror(rc), rc, uv_err_name(rc));
    }

    rc = uv_run(&loop, UV_RUN_DEFAULT);
    if (rc != 0) {
        TEST_FAIL("uv_run failed: %s (%d; %s)", uv_strerror(rc), rc, uv_err_name(rc));
    }

    uv_loop_close(&loop);

    TEST_EQ_II(rerun_state.term_signal, 0);
    TEST_EQ_II(rerun_state.exit_status, 0);
}

struct CompareJSONState {
    std::vector<std::variant<size_t, std::string>> path_parts;
};

static void CompareJSON2(const nlohmann::json &got,
                         const nlohmann::json &wanted,
                         CompareJSONState *state) {
    if (got.is_null()) {
        TEST_TRUE(wanted.is_null());
    } else if (got.is_number()) {
        if (got.is_number_float()) {
            TEST_TRUE(wanted.is_number_float());
            double got_number = got.template get<double>();
            double wanted_number = wanted.template get<double>();
            TEST_TRUE(fabs(wanted_number - got_number) < 1e-3f); //whatever
        } else if (got.is_number_integer()) {
            TEST_TRUE(wanted.is_number_integer());
            TEST_EQ_II(got.template get<int64_t>(), wanted.template get<int64_t>());
        } else if (got.is_number_unsigned()) {
            TEST_TRUE(wanted.is_number_unsigned());
            TEST_EQ_UU(got.template get<uint64_t>(), wanted.template get<uint64_t>());
        } else {
            TEST_FAIL("unknown number type");
        }
    } else if (got.is_string()) {
        TEST_TRUE(wanted.is_string());
        TEST_EQ_SS(got.template get<std::string>(), wanted.template get<std::string>());
    } else if (got.is_array()) {
        TEST_TRUE(wanted.is_array());
        TEST_EQ_UU(got.size(), wanted.size());
        state->path_parts.push_back({});
        for (size_t i = 0; i < got.size(); ++i) {
            state->path_parts.back() = i;
            CompareJSON2(got[i], wanted[i], state);
        }
        state->path_parts.pop_back();
    } else if (got.is_object()) {
        TEST_TRUE(wanted.is_object());
        state->path_parts.push_back({});
        for (const auto &kv : got.items()) {
            std::string key = kv.key();
            const nlohmann::json &avalue = kv.value();

            TEST_TRUE(wanted.contains(key));
            const nlohmann::json &wanted_value = wanted[key];

            state->path_parts.back() = kv.key();
            CompareJSON2(avalue, wanted_value, state);
        }

        for (const auto &kv : wanted.items()) {
            state->path_parts.back() = kv.key();
            TEST_TRUE(got.contains(kv.key()));
        }
        state->path_parts.pop_back();
    }
}

static void CompareJSON(const nlohmann::json &got, const nlohmann::json &wanted) {
    CompareJSONState state;
    state.path_parts.push_back("$");

    TestFailFnAdder adder([&state](const TestFailArgs *args) -> void {
        (void)args;
        std::string path;
        for (const std::variant<size_t, std::string> &part : state.path_parts) {
            if (const size_t *index = std::get_if<size_t>(&part)) {
                path += "[" + std::to_string(*index) + "]";
            } else if (const std::string *key = std::get_if<std::string>(&part)) {
                if (!path.empty()) {
                    path.push_back('.');
                }
                path.append(*key);
            } else {
                TEST_FAIL("...");
            }
        }

        LOGF(TESTING, "Path: %s\n", path.c_str());
    });

    CompareJSON2(got, wanted, &state);
}

// https://github.com/tom-seddon/b2/issues/627

static const std::string PRESERVE_CONFIG_JSON_TEST_NAME = "b2ui.preserve_config_json";

// the helper test doesn't need to do anything apart from start b2, then have it quit, saving the updated config.
class TestPreserveConfigJSONHelper : public DearImGuiTest {
  public:
    bool IsHidden() const override {
        return true;
    }

    bool GetConfigOverrideFolder(std::string *folder) const override {
        return HandleGetConfigOverrideFolder(folder, PRESERVE_CONFIG_JSON_TEST_NAME);
    }

    bool GetCacheOverrideFolder(std::string *folder) const override {
        return HandleGetCacheOverrideFolder(folder, PRESERVE_CONFIG_JSON_TEST_NAME);
    }

    void Run() override {
        TEST_EQ_II(this->Run2(), 0);
    }

  protected:
  private:
};

class TestPreserveConfigJSONHelper1 : public TestPreserveConfigJSONHelper {
  public:
    std::string GetFullName() const override {
        return "b2ui._preserve_config_json_1";
    }

  protected:
  private:
};

class TestPreserveConfigJSONHelper2 : public TestPreserveConfigJSONHelper {
  public:
    std::string GetFullName() const override {
        return "b2ui._preserve_config_json_2";
    }

    bool ShouldClearConfigFolder() const override {
        // reuse the b2.json from the previous helper's run.
        return false;
    }

  protected:
  private:
};

class TestPreserveConfigJSON : public Test {
  public:
    std::string GetFullName() const override {
        return "b2ui.preserve_config_json";
    }

    void Run() override {

        Rerun("b2ui._preserve_config_json_1");

        std::string config_folder;
        TEST_TRUE(HandleGetConfigOverrideFolder(&config_folder, PRESERVE_CONFIG_JSON_TEST_NAME));
        std::string b2_json_path = PathJoined(config_folder, "b2.json");

        // Take a backup of the original config.
        std::vector<uint8_t> b2_json_data;
        TEST_TRUE(LoadFile(&b2_json_data, b2_json_path, nullptr));
        TEST_TRUE(SaveFile(b2_json_data, PathJoined(config_folder, "b2.old.json"), nullptr));

        nlohmann::json config_j;
        TEST_TRUE(LoadJSONFile2(&config_j, b2_json_path, nullptr, 0));
        nlohmann::json old_config_j = config_j;

        // Muck up one of the configs.
        TEST_TRUE(config_j.is_object());
        TEST_TRUE(config_j.contains("new_configs"));
        TEST_TRUE(config_j["new_configs"].is_array());
        TEST_FALSE(config_j["new_configs"].empty());
        TEST_EQ_SS(config_j["new_configs"][0]["name"], "B/Acorn 1770");
        TEST_EQ_SS(config_j["new_configs"][0]["type"], "B");
        config_j["new_configs"][0]["type"] = "BadType";

        TEST_TRUE(SaveJSONFile(config_j, b2_json_path, nullptr));

        //CompareJSON(config_j,old_config_j);

        nlohmann::json wanted_config_j = config_j;

        Rerun("b2ui._preserve_config_json_2");

        nlohmann::json got_config_j;
        TEST_TRUE(LoadJSONFile2(&got_config_j, b2_json_path, nullptr, 0));

        CompareJSON(got_config_j, wanted_config_j);
    }

  protected:
  private:
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
    bool doc_images = false;
    std::string doc_images_path = PathJoined(b2_SOURCE_DIR, "doc/wip/generated");
    std::vector<std::string> doc_images_skip;

    // Or should this actually be default true?
    bool doc_images_clean = false;
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
    p.AddOption("wip").SetIfPresent(&options.wip).Help("include WIP tests that aren't finished or passing yet");
    p.AddOption('b', "b2").SetIfPresent(&options.b2).Help("pretend to be ordinary b2, with Dear ImGui Test Engine enabled. Tests will be available - run at own risk");
    p.AddOption('B', "b2-arg").AddArgToList(&options.b2_argv).Help("add a string, verbatim, to the b2 argv");
    p.AddOption("interactive").SetIfPresent(&options.interactive).Help("if running a single Dear ImGui Test Engine test, run UI in interactive mode");

    // This oddity exists because b2_test happens to have all the infrastructure
    // in place to make it work, and not because it makes any sense
    // conceptually.
    p.AddOption("doc-images").SetIfPresent(&options.doc_images).Help("run in doc image creation mode");

    p.AddOption("doc-images-path").Meta("PATH").Arg(&options.doc_images_path).ShowDefault().Help("output doc images to PATH");

    // A time-saving bodge. There's no specific list of possible sections.
    p.AddOption("doc-images-skip").Meta("SECTION").AddArgToList(&options.doc_images_skip).Help("skip doc image section SECTION when creating images");

    //
    p.AddOption("doc-images-clean").SetIfPresent(&options.doc_images_clean).Help("delete PNG files in doc images output folder before starting");

    // Intended for use when adding new tests, in conjunction with -T, on the
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
    void DearImGuiTestEngineDidBecomeReady(BeebWindow *beeb_window, ImGuiStuff *imgui_stuff) override {
        ImGuiTestEngine *test_engine = imgui_stuff->GetTestEngine();
        ASSERT(test_engine);

        for (const std::unique_ptr<Test> &test : *m_tests) {
            test->RegisterDearImGuiTest(test_engine, beeb_window);
        }
    }
#endif

    void MessageLoopWillStart() override {
        printf("Config path: %s\n", GetConfigPath("").c_str());
        printf("Cache path: %s\n", GetCachePath("").c_str());
        printf("Example asset path: %s\n", GetAssetPath(GAMECONTROLLER_DB_FILE_NAME).c_str());
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const MOSType MOS_TYPES[] = {
    {"os120", "B/Acorn 1770", "OS 1.20"},
    {"os200", "B+", "OS 2.00"},
    {"mos320", "Master 128 (MOS 3.20)", "OS 3.20"},
    {"mos350", "Master 128 (MOS 3.50)", "MOS 3.50"},
    {"mos500", "Master Compact (MOS 5.00)", "MOS 5.00"},
    {"mos510", "Master Compact (MOS 5.10)", "MOS 5.10"},
    {"mos511i", "Master Compact (MOS 5.11i+Arabic)", "MOS 5.11i"},
    {"mosI510C", "Olivetti PC 128 S", "MOS I5.10C"},
    {"electron", "Electron/Plus 1", "OS 1.00"},
};

int main(int argc, char *argv[]) {
    TestOptions options = GetOptions(argc, argv);

    std::vector<std::unique_ptr<Test>> all_tests;

    all_tests.push_back(std::make_unique<TestUTF8>());
    all_tests.push_back(std::make_unique<TestSymbolTable>());
    all_tests.push_back(std::make_unique<TestJobQueue>());
    all_tests.push_back(std::make_unique<TestFileExit>());
    all_tests.push_back(std::make_unique<TestStbImageUTF8>());
    all_tests.push_back(std::make_unique<TestLoadPossiblyGzippedFile>());
    all_tests.push_back(std::make_unique<TestLoadUEF>());
    all_tests.push_back(std::make_unique<TestBase64>());

    // the callback handling is model-dependent, so not much point checking the whole lineup.
    all_tests.push_back(std::make_unique<TestNVRAMUpdate>("master", "Master 128 (MOS 3.20)"));
    all_tests.push_back(std::make_unique<TestNVRAMUpdate>("compact", "Master Compact (MOS 5.10)"));

    AddCopyOfDiskTests(&all_tests, BLANK_DFS_DISCS, NUM_BLANK_DFS_DISCS);
    AddCopyOfDiskTests(&all_tests, BLANK_ADFS_DISCS, NUM_BLANK_ADFS_DISCS);
    AddCopyOfDiskTests(&all_tests, WELCOME_DISKS, NUM_WELCOME_DISKS);

    all_tests.push_back(std::make_unique<TestLoadZippedDisk>(PathJoined(b2_SOURCE_DIR, "etc/tests/disks/80.ssd.zip"),
                                                             PathJoined(b2_SOURCE_DIR, "etc/discs/80.ssd")));
    all_tests.push_back(std::make_unique<TestLoadZippedDisk>(PathJoined(b2_SOURCE_DIR, "etc/tests/disks/MasterWelcome.adl.zip"),
                                                             PathJoined(b2_SOURCE_DIR, "etc/discs/MasterWelcome.adl")));
    all_tests.push_back(std::make_unique<TestLoadZippedDisk>(PathJoined(b2_SOURCE_DIR, "etc/tests/disks/two_disks.zip"),
                                                             ""));

    for (const MOSType &mos_type : MOS_TYPES) {
        all_tests.push_back(std::make_unique<TestHTTPConfig>(mos_type));
        all_tests.push_back(std::make_unique<TestHTTPPasteOSWORD0Timeout>(mos_type));
        all_tests.push_back(std::make_unique<TestHTTPConfigOSWORD0Timeout>(mos_type));
    }

    for (bool do_brk : {false, true}) {
        all_tests.push_back(std::make_unique<TestHTTPBRKTracking>(do_brk));
    }

    all_tests.push_back(std::make_unique<TestHTTPPeek>());

    all_tests.push_back(std::make_unique<TestPreserveConfigJSON>());
    all_tests.push_back(std::make_unique<TestPreserveConfigJSONHelper1>());
    all_tests.push_back(std::make_unique<TestPreserveConfigJSONHelper2>());

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //
    // all tests to be added by this point.
    //
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

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
            const char *prefix;
            if (name_and_test.second->IsHidden()) {
                prefix = "1902bf7f-8607-4cd3-a6e1-abaf2ebe85c9:";
            } else {
                prefix = "2fcf9707-9498-4a03-9b27-ef501fa2fbb6:";
            }

            printf("%sb2_test: %s\n", prefix, name_and_test.first.c_str());
        }

        return 0;
    }

    if (options.doc_images) {
        DocImageCreator doc_images(options.doc_images_path, options.doc_images_skip, options.doc_images_clean);

        doc_images.Run();
    } else if (options.b2) {
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
            g_interactive = true;
        } else {
            size_t num_dear_imgui = 0;

            for (size_t test_index = 0; test_index < all_tests.size(); ++test_index) {
                if (run_test[test_index]) {
                    if (dynamic_cast<DearImGuiTest *>(all_tests[test_index].get())) {
                        ++num_dear_imgui;
                    }
                }
            }

            // TODO: the b2 code isn't designed to be re-initialised after it's
            // quit, but... maybe it'd actually be possible to make this work?
            // To be continued.
            TEST_LE_UU(num_dear_imgui, 1);
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
