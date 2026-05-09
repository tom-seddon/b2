#include <shared/system.h>
#include <shared/CommandLineParser.h>
#include "conf.h"
#include <set>
#include <shared/log.h>
#include "b2.h"
#include <shared/guid.h>
#include "BeebWindow.h"
#include "http_api.h"
#include "HTTPMethodsHandler.h"
#include <shared/file_io.h>

#if BBCMICRO_DEBUGGER

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char PRODUCT_NAME[] = "b2 headless - " STRINGIZE(RELEASE_NAME);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// don't try to call these "stdout" or "stderr" - on VC++, the name expands to
// something that isn't an identifier.
LOG_DEFINE(std_out, "", &log_printer_stdout);
LOG_DEFINE(std_err, "", &log_printer_stderr);

static const LogSet g_stdio_logs(LOG(std_out), LOG(std_err), LOG(std_err));

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct HeadlessOptions {
    std::string argv0;
    bool verbose = false;
    bool headless = true;
    bool help = false;
    std::string config_folder;
    bool config_folder_specified = false;
    std::vector<std::string> enable_logs, disable_logs;
    int http_port = -1;
    std::string api_read_path;
    bool api_read_path_specified = false;
    std::string api_write_path;
    bool api_write_path_specified = false;
    std::string api_input_path;
    std::string api_output_path;
    bool enable_sound = false;
    bool echo_oswrch = false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool ParseCommandLineOptions(HeadlessOptions *options, int argc, char *argv[]) {
    CommandLineParser p(PRODUCT_NAME);

    p.AddOption('v', "verbose").SetIfPresent(&options->verbose).Help("be more verbose");
    p.AddOption("gui").ResetIfPresent(&options->headless).Help("show ordinary b2 UI while running");
    p.AddOption("enable-sound").SetIfPresent(&options->enable_sound).Help("enable sound when showing ordinary b2 UI");
    p.AddHelpOption(&options->help);
    p.AddOption("config-folder").Arg(&options->config_folder).Help("specify folder for config and cache files (will be created if required)").SetIfPresent(&options->config_folder_specified);
    p.AddOption("http-port").Arg(&options->http_port).Meta("PORT").Help("specify TCP port for HTTP server to listen on (0 means system will choose)");

    std::set<std::string> tags;
    for (const LogWithTag *tagged_log = LogWithTag::GetFirst(); tagged_log; tagged_log = tagged_log->GetNext()) {
        tags.insert(tagged_log->tag);
    }

    std::string list;
    for (const std::string &tag : tags) {
        if (!list.empty()) {
            list += " ";
        }

        list += tag;
    }

    if (!list.empty()) {
        p.AddOption('e', "enable-log").AddArgToList(&options->enable_logs).Meta("LOG").Help("enable additional log LOG. One of: " + list);
        p.AddOption('d', "disable-log").AddArgToList(&options->disable_logs).Meta("LOG").Help("disable additional log LOG. One of: " + list);
    }

    p.AddOption("api-read").Meta("PATH").Arg(&options->api_read_path).Help("set initial JSON API read path to PATH").SetIfPresent(&options->api_read_path_specified);
    p.AddOption("api-write").Meta("PATH").Arg(&options->api_write_path).Help("set initial JSON API write path to PATH").SetIfPresent(&options->api_write_path_specified);
    p.AddOption("api-input").Meta("PATH").Arg(&options->api_input_path).Help("pass contents of PATH to JSON API on startup");
    p.AddOption("api-output").Meta("PATH").Arg(&options->api_output_path).Help("write JSON API output to PATH");

    p.AddOption("echo-oswrch").SetIfPresent(&options->echo_oswrch).Help("echo printable OSWRCH chars to stdout");

    if (!p.Parse(argc, argv, nullptr)) {
        return false;
    }

    if (argv[0]) {
        options->argv0 = argv[0];
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HeadlessAppHandler : public AppHandler {
  public:
    explicit HeadlessAppHandler(const HeadlessOptions &options, std::unique_ptr<ApiMultipleRequests> api_request)
        : m_options(options)
        , m_api_request(std::move(api_request)) {
    }

    std::string GetProductName() const override {
        return PRODUCT_NAME;
    }

#if SYSTEM_OSX
    std::string GetFrameName() const override {
        return "";
    }
#endif

    bool IsHeadless() const override {
        return m_options.headless;
    }

    bool IsHighDPIEnabled() const override {
        // As per b2_test: safest just to run in DPI-unaware mode.
        //
        // This intentionally also affects --gui.

        return false;
    }

    bool IsSoundEnabled() const override {
        return m_options.enable_sound;
    }

    std::vector<std::string> GetCommandLineArgs() const override {
        // TODO: anything more needed here? Like -d/-e?
        return {m_options.argv0};
    }

    bool GetConfigAndCacheOverrideFolder(std::string *folder) const override {
        if (folder) {
            *folder = m_options.config_folder;
        }

        return true;
    }

    bool GetFixedDisplaySize(ImVec2 *display_size) const override {
        // TODO: might be useful to be able to configure this?
        (void)display_size;

        return true;
    }

    int GetRequestedHttpServerListenPort() const override {
        return m_options.http_port;
    }

    int GetActualHttpServerListenPort() const {
        return m_http_port;
    }

    void SetActualHttpServerListenPort(int port) override {
        m_http_port = port;
    }

    int GetLaunchRequestHttpServerPort() const override {
        return 0;
    }

    void HandleBeebWindowPostInit(BeebWindow *beeb_window) override {
        if (m_options.api_read_path_specified) {
            beeb_window->api_globals.read_path = m_options.api_read_path;
        }

        if (m_options.api_write_path_specified) {
            beeb_window->api_globals.write_path = m_options.api_write_path;
        }

        if (m_options.echo_oswrch) {
            beeb_window->StartEchoOSWRCH();
        }

        if (!!m_api_request) {
            std::unique_ptr<ApiMultipleRequests> api_request = std::move(m_api_request);
            ApiExecuteMultipleRequests(std::move(*api_request),
                                       [this](ApiMultipleResponses &&response) -> void {
                                           this->HandleApiRequestComplete(std::move(response));
                                       });
        }
    }

    uint32_t GetUIFlags() const override {
        return UIFlag_HideAllPopups;
    }

#if IMGUI_ENABLE_TEST_ENGINE
    bool IsDearImGuiTestEngineEnabled() const override {
        return false;
    }

    bool ShouldQuitWhenTestQueueEmpty() const override {
        // actually irrelevant in this case.
        return false;
    }
#endif

    bool HandleSelectorDialogOpen(std::string *result, const Guid &guid) override {
        char guid_str[GUID_STR_SIZE];
        GetStringFromGuid(guid_str, guid);

        fprintf(stderr, "WARNING: file dialog opened (will be automatically cancelled): %s\n", guid_str);

        result->clear();
        return true;
    }

  protected:
  private:
    int m_http_port = -1;
    HeadlessOptions m_options;
    std::unique_ptr<ApiMultipleRequests> m_api_request;

    void QuitIfHeadless(int exit_code) {
        if (this->IsHeadless()) {
            PushQuitMessage(exit_code);
        }
    }

    void HandleApiRequestComplete(ApiMultipleResponses &&response) {
        if (!m_options.api_output_path.empty()) {
            if (!SaveJSONFile(response, m_options.api_output_path, &g_stdio_logs, SaveFlag_CreateFolder)) {
                this->QuitIfHeadless(1);
                return;
            }
        }

        // TODO: this output can get mixed in with any buffered-up echo-oswrch output that hasn't been flushed yet. The official b2_headless stdout policy is that it's for eyeballing purposes only, but it'd be nice to fix this if it would be easy.
        if (!this->IsHeadless() || m_options.api_output_path.empty()) {
            printf("JSON result (success=%s):\n", BOOL_STR(response.success));
            printf("---8<---\n");
            puts(nlohmann::json(response).dump(4).c_str());
            printf("---8<---\n");
        }

        this->QuitIfHeadless(response.success ? 0 : 1);
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {

    HeadlessOptions options;
    if (!ParseCommandLineOptions(&options, argc, argv)) {
        if (options.help) {
            return 0;
        } else {
            return 1;
        }
    }

    if (!options.config_folder_specified) {
        fprintf(stderr, "FATAL: config folder not specified\n");
        return 1;
    }

    std::unique_ptr<ApiMultipleRequests> api_request;
    if (!options.api_input_path.empty()) {
        api_request = std::make_unique<ApiMultipleRequests>();
        if (!LoadJSONFile(api_request.get(), options.api_input_path, &g_stdio_logs)) {
            fprintf(stderr, "FATAL: failed to load API input file\n");
            return 1;
        }

        // TODO: bit janky, this.
        if (!api_request->window.empty()) {
            fprintf(stderr, "FATAL: API request window name must be empty\n");
            return 1;
        }
    }

    HeadlessAppHandler app_handler(options, std::move(api_request));

    int result = b2_main(&app_handler);
    return result;
}

#else

int main(int, char *[]) {
    fprintf(stderr, "FATAL: this is a non-functional placeholder build.\n");
    return 1;
}

#endif
