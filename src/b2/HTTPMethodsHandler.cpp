#include <shared/system.h>
#include "json.h"
#include "HTTPMethodsHandler.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "b2.h"
#include <utility>
#include <regex>
#include <http/HTTPServer.h>
#include "misc.h"
#include "BeebWindows.h"
#include "BeebWindow.h"
#include "BeebThread.h"
#include <inttypes.h>
#include <beeb/MemoryDiscImage.h>
#include "Messages.h"
#include <shared/path.h>
#include <beeb/DiscGeometry.h>
#include <http/http.h>
#include "LoadMemoryDiscImage.h"
#include <beeb/DirectDiscImage.h>
#include "SymbolTable.h"
#include "http_api.h"
#include <shared/strings.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string HTTP_DISC_IMAGE_LOAD_METHOD = "http";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<std::string> GetPathParts(const std::string &path) {
    std::vector<std::string> parts;
    std::string part;

    for (char c : path) {
        if (c == '/') {
            if (!part.empty()) {
                parts.push_back(part);
                part.clear();
            }
        } else {
            part.append(1, c);
        }
    }

    if (!part.empty()) {
        parts.push_back(part);
    }

    return parts;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HTTPMethodsHandler : public HTTPHandler {
    struct HandleRequestData {
        HTTPServer *server;
        HTTPRequest request;
    };

  public:
    bool ThreadHandleRequest(HTTPResponse *response, HTTPServer *server, HTTPRequest &&request) override {
        (void)response;

        if (!CloseModalDialog()) {
            server->SendResponse(request, HTTPResponse::ServiceUnavailable("A modal dialog is open that can't be automatically closed"));
            return false;
        }

        auto data = new HandleRequestData{};

        data->server = server;
        data->request = std::move(request);

        PushMainThreadMessage(std::make_unique<FunctionMessage>(
            [this, data]() -> void {
                HTTPServer *server = data->server;
                HTTPRequest request = std::move(data->request);

                delete data;

                this->HandleRequest(server, std::move(request));
            }));

        return false;
    }

  protected:
  private:
    // Method handlers are called on the main thread.
    const std::map<std::string, void (HTTPMethodsHandler::*)(HTTPServer *, HTTPRequest &&, const std::vector<std::string> &, size_t)> m_request_handlers = {
#if BBCMICRO_DEBUGGER
        {"reset", &HTTPMethodsHandler::HandleResetRequest},
        {"paste", &HTTPMethodsHandler::HandlePasteRequest},
        {"poke", &HTTPMethodsHandler::HandlePokeRequest},
        {"peek", &HTTPMethodsHandler::HandlePeekRequest},
        {"mount", &HTTPMethodsHandler::HandleMountRequest},
        {"run", &HTTPMethodsHandler::HandleRunRequest},
        {"load-disc", &HTTPMethodsHandler::HandleLoadDiskRequest},
        {"load-disk", &HTTPMethodsHandler::HandleLoadDiskRequest},
        {"clear-symbols", &HTTPMethodsHandler::HandleClearSymbolsRequest},
        {"load-symbols", &HTTPMethodsHandler::HandleLoadSymbolsRequest},
        {"set-address-breakpoint", &HTTPMethodsHandler::HandleSetAddressBreakpointRequest},
        {"clear-address-breakpoint", &HTTPMethodsHandler::HandleClearAddressBreakpointRequest},
        {"set-byte-breakpoint", &HTTPMethodsHandler::HandleSetByteBreakpointRequest},
        {"clear-byte-breakpoint", &HTTPMethodsHandler::HandleClearByteBreakpointRequest},
        {"clear-breakpoints", &HTTPMethodsHandler::HandleClearBreakpointsRequest},
        {"request", &HTTPMethodsHandler::HandleGenericRequest},
        {"request-multiple", &HTTPMethodsHandler::HandleGenericMultipleRequest},
#endif
        {"launch", &HTTPMethodsHandler::HandleLaunchRequest},
    };

    struct ParseArgsState {
        BeebWindow *implicit_beeb_window = nullptr;
        HTTPServer *server = nullptr;
        const HTTPRequest *request = nullptr;
    };

    template <class T>
    static bool HandleArgOrSendResponse(const ParseArgsState &state,
                                        T *result,
                                        const std::string &value,
                                        bool (*f)(T *, const std::string &, int, const char **),
                                        int radix,
                                        const char *what) {
        if (!(*f)(result, value, radix, nullptr)) {
            state.server->SendResponse(*state.request, HTTPResponse::BadRequest(*state.request, "bad %s: %s", what, value.c_str()));
            return false;
        }

        return true;
    }

    typedef bool (*ParseArgsCallbackFn)(ParseArgsState *state, const std::string &str, void *context);

    static bool ParseU8(ParseArgsState *state, const std::string &str, void *context) {
        return HandleArgOrSendResponse(*state, (uint8_t *)context, str, &GetUInt8FromString, 0, "8-bit value");
    }

    static bool ParseU16(ParseArgsState *state, const std::string &str, void *context) {
        return HandleArgOrSendResponse(*state, (uint16_t *)context, str, &GetUInt16FromString, 0, "16-bit value");
    }

    static bool ParseU32(ParseArgsState *state, const std::string &str, void *context) {
        return HandleArgOrSendResponse(*state, (uint32_t *)context, str, &GetUInt32FromString, 0, "32-bit value");
    }

    static bool ParseU64(ParseArgsState *state, const std::string &str, void *context) {
        return HandleArgOrSendResponse(*state, (uint64_t *)context, str, &GetUInt64FromString, 0, "64-bit value");
    }

    static bool ParseX16(ParseArgsState *state, const std::string &str, void *context) {
        return HandleArgOrSendResponse(*state, (uint16_t *)context, str, &GetUInt16FromString, 16, "16-bit hex value");
    }

    static bool ParseX32(ParseArgsState *state, const std::string &str, void *context) {
        return HandleArgOrSendResponse(*state, (uint32_t *)context, str, &GetUInt32FromString, 16, "32-bit hex value");
    }

    static bool ParseX64(ParseArgsState *state, const std::string &str, void *context) {
        return HandleArgOrSendResponse(*state, (uint64_t *)context, str, &GetUInt64FromString, 16, "64-bit hex value");
    }

    struct EndOrLength {
        uint64_t value = 0;
        bool is_length = false;
    };

    static bool ParseEndAdressOrLength(ParseArgsState *state, const std::string &str, void *context) {
        auto result = (EndOrLength *)context;

        if (!str.empty() && str[0] == '+') {
            result->is_length = true;

            if (!GetUInt64FromString(&result->value, str.c_str() + 1)) {
                state->server->SendResponse(*state->request, HTTPResponse::BadRequest(*state->request, "bad length: %s", str.c_str()));
                return false;
            }
        } else {
            result->is_length = false;

            if (!GetUInt64FromString(&result->value, str.c_str(), 16)) {
                state->server->SendResponse(*state->request, HTTPResponse::BadRequest(*state->request, "bad address: %s", str.c_str()));
                return false;
            }
        }

        return true;
    }

    static bool ParseBool(ParseArgsState *state, const std::string &str, void *context) {
        if (!GetBoolFromString((bool *)context, str)) {
            state->server->SendResponse(*state->request, HTTPResponse::BadRequest(*state->request, "bad bool value: %s", str.c_str()));
            return false;
        }

        return true;
    }

    static bool ParseStdString(ParseArgsState *state, const std::string &str, void *context) {
        (void)state;

        *(std::string *)context = str;

        return true;
    }

    static bool ParseWindow(ParseArgsState *state, const std::string &str, void *context) {
        auto ptr = (BeebWindow **)context;

        if (str == "*") {
            *ptr = BeebWindows::FindMRUBeebWindow();
        } else {
            *ptr = BeebWindows::FindBeebWindowByName(str);
        }

        if (!*ptr) {
            state->server->SendResponse(*state->request, HTTPResponse::NotFound(*state->request));
            return false;
        }

        if (!state->implicit_beeb_window) {
            state->implicit_beeb_window = *ptr;
        }

        return true;
    }

#if BBCMICRO_DEBUGGER
    static bool ParseDSO(ParseArgsState *state, const std::string &str, void *context) {
        if (!state->implicit_beeb_window) {
            state->server->SendResponse(*state->request, HTTPResponse::InternalServerError("no BeebWindow for DSO"));
            return false;
        }

        std::string log_string;
        LogPrinterString log_printer_string(&log_string);
        Log log("", &log_printer_string);

        std::shared_ptr<const BBCMicroType> type = state->implicit_beeb_window->GetBeebThread()->GetBBCMicroType();
        if (!ParseAddressSuffix((uint32_t *)context, type, str.c_str(), &log)) {
            state->server->SendResponse(*state->request, HTTPResponse::BadRequest(*state->request, "%s", log_string.c_str()));
            return false;
        }

        return true;
    }
#endif

    struct PathParameter {
        ParseArgsCallbackFn fn = nullptr;
        void *fn_context = nullptr;
    };

    struct QueryParameter {
        const char *name = nullptr;
        ParseArgsCallbackFn fn = nullptr;
        void *fn_context = nullptr;
    };

    static bool ParseArgsOrSendResponse2(HTTPServer *server, const HTTPRequest &request, const std::vector<std::string> &parts, size_t command_index, const PathParameter *path_parameters, size_t num_path_parameters, const QueryParameter *query_parameters, size_t num_query_parameters) {
        ParseArgsState pas;
        pas.implicit_beeb_window = nullptr;
        pas.server = server;
        pas.request = &request;

        // Parse path args.
        size_t num_path_args = parts.size() - (command_index + 1);
        if (num_path_args != num_path_parameters) {
            server->SendResponse(request, HTTPResponse::BadRequest(request, "%zu arguments supplied; %zu required", num_path_args, num_path_parameters));
            return false;
        }

        for (size_t i = 0; i < num_path_parameters; ++i) {
            const PathParameter *p = &path_parameters[i];

            if (!(*p->fn)(&pas, parts[command_index + 1 + i], p->fn_context)) {
                return false;
            }
        }

        // Parse query args.
        //
        // (The naming isn't really great, as HTTP refers to them as query parameters.)
        for (const HTTPQueryParameter &query_arg : request.query) {
            const QueryParameter *p = nullptr;

            if (query_parameters) {
                for (size_t i = 0; i < num_query_parameters; ++i) {
                    if (query_arg.key == query_parameters[i].name) {
                        p = &query_parameters[i];
                        break;
                    }
                }
            }

            if (!p) {
                server->SendResponse(request, HTTPResponse::BadRequest(request, "unrecognised query parameter: %s", query_arg.key.c_str()));
                return false;
            }

            if (!(p->fn)(&pas, query_arg.value, p->fn_context)) {
                return false;
            }
        }

        return true;
    }

    template <size_t NUM_PATH_PARAMETERS, size_t NUM_QUERY_PARAMETERS>
    static bool ParseArgsOrSendResponse(HTTPServer *server, const HTTPRequest &request, const std::vector<std::string> &parts, size_t command_index, const PathParameter (&path_parameters)[NUM_PATH_PARAMETERS], const QueryParameter (&query_parameters)[NUM_QUERY_PARAMETERS]) {
        return ParseArgsOrSendResponse2(server, request, parts, command_index, path_parameters, NUM_PATH_PARAMETERS, query_parameters, NUM_QUERY_PARAMETERS);
    }

    template <size_t NUM_PATH_PARAMETERS>
    static bool ParseArgsOrSendResponse(HTTPServer *server, const HTTPRequest &request, const std::vector<std::string> &parts, size_t command_index, const PathParameter (&path_parameters)[NUM_PATH_PARAMETERS]) {
        return ParseArgsOrSendResponse2(server, request, parts, command_index, path_parameters, NUM_PATH_PARAMETERS, nullptr, 0);
    }

#if BBCMICRO_DEBUGGER
    void HandleResetRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        std::string config_name;
        bool boot = false;
        const PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
        };
        const QueryParameter qps[] = {
            {"config", &ParseStdString, &config_name},
            {"boot", &ParseBool, &boot},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps, qps)) {
            return;
        }

        if (!config_name.empty()) {
            auto message_list = std::make_shared<MessageList>("HTTP reset");
            Messages messages(message_list);

            BeebConfigArguments arguments;

            BeebLoadedConfig loaded_config;
            if (!BeebWindows::LoadConfigByName(&loaded_config, config_name, arguments, &messages)) {
                this->SendMessagesResponse(server, request, message_list);
                return;
            }

            auto config_message = std::make_shared<BeebThread::HardResetAndChangeConfigMessage>(std::move(loaded_config),
                                                                                                BeebThreadHardResetFlag_Run);
            beeb_window->GetBeebThread()->Send(std::move(config_message));
        }

        uint32_t flags = BeebThreadHardResetFlag_Run;
        if (boot) {
            flags |= BeebThreadHardResetFlag_Boot;
        }

        auto reset_message = std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(flags);

        //        message->reload_config=true;
        //        message->run=true;

        this->SendMessage(beeb_window, server, request, std::move(reset_message));
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandlePasteRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        const PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps)) {
            return;
        }

        std::vector<uint8_t> bbc_ascii;
        if (request.content_type == HTTP_TEXT_CONTENT_TYPE && (request.content_type_charset.empty() || request.content_type_charset == HTTP_ISO_8859_1_CHARSET)) {
            if (GetBBCASCIIFromISO8859_1(&bbc_ascii, request.body) != 0) {
                server->SendResponse(request, HTTPResponse::BadRequest(request));
                return;
            }
        } else if (request.content_type == HTTP_TEXT_CONTENT_TYPE && request.content_type_charset == HTTP_UTF8_CHARSET) {
            if (!GetBBCASCIIFromUTF8(&bbc_ascii, request.body, nullptr, nullptr, nullptr)) {
                server->SendResponse(request, HTTPResponse::BadRequest(request));
                return;
            }
        } else {
            // Maybe support octet-stream?? Like, if you've got
            // verbatim *SPOOL output from a real BBC or something?
            server->SendResponse(request, HTTPResponse::BadRequest(request, "Unsupported Content-Type \"%s\", charset \"%s\"\n", request.content_type.c_str(), request.content_type_charset.c_str()));
            return;
        }

        FixBBCASCIINewlines(&bbc_ascii);

        this->SendMessage(beeb_window, server, request, std::make_shared<BeebThread::StartPasteMessage>(std::move(bbc_ascii), 0));
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandlePokeRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        uint16_t addr;
        uint32_t dso = 0;
        bool mos = false;
        const PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
            {&ParseX16, &addr},
        };
        const QueryParameter qps[] = {
            {"s", &ParseDSO, &dso},
            {"mos", &ParseBool, &mos},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps, qps)) {
            return;
        }

        if (addr + request.body.size() > 0x10000) {
            server->SendResponse(request, HTTPResponse::BadRequest(request, "can't poke past 0xffff"));
            return;
        }

        this->SendMessage(beeb_window, server, request, std::make_shared<BeebThread::DebugSetBytesMessage>(addr, dso, mos, std::move(request.body)));
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandlePeekRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        uint16_t begin;
        EndOrLength end;
        uint32_t dso = 0;
        bool mos = false;
        const PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
            {&ParseX16, &begin},
            {&ParseEndAdressOrLength, &end},
        };
        const QueryParameter qps[] = {
            {"dso", &ParseDSO, &dso},
            {"mos", &ParseBool, &mos},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps, qps)) {
            return;
        }

        if (end.is_length) {
            end.value += begin;
        }

        if (end.value > 0x10000) {
            server->SendResponse(request, HTTPResponse::BadRequest(request, "can't peek past 0xffff"));
            return;
        }

        beeb_window->GetBeebThread()->Send(std::make_unique<BeebThread::CallbackMessage>([begin, end, dso, mos, server, response_data = request.response_data](BBCMicro *m) -> void {
            std::vector<uint8_t> data;
            data.resize(end.value - begin);

            m->DebugGetBytes(data.data(), data.size(), {begin}, dso, mos);

            HTTPResponse response(HTTP_OCTET_STREAM_CONTENT_TYPE, std::move(data));
            server->SendResponse(response_data, std::move(response));
        }));
    }

#endif

#if BBCMICRO_DEBUGGER
    void HandleMountRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        std::string name;
        uint32_t drive = 0;
        const PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
        };
        const QueryParameter qps[] = {
            {"name", &ParseStdString, &name},
            {"drive", &ParseU32, &drive},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps, qps)) {
            return;
        }

        if (drive >= NUM_DRIVES) {
            server->SendResponse(request, HTTPResponse::BadRequest(request, "bad drive: %" PRIu32, drive));
            return;
        }

        std::shared_ptr<DiscImage> disc_image = LoadDiscImageFromRequestOrSendResponse(server, request, name);
        if (!disc_image) {
            return;
        }

        this->SendMessage(beeb_window, server, request, std::make_shared<BeebThread::LoadDiscMessage>((int)drive, std::move(disc_image), true));
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandleRunRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        std::string name;
        const PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
        };
        const QueryParameter qps[] = {
            {"name", &ParseStdString, &name},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps, qps)) {
            return;
        }

        // Assume BBC disc image.
        std::shared_ptr<DiscImage> disc_image = this->LoadDiscImageFromRequestOrSendResponse(server, request, name);
        if (!disc_image) {
            return;
        }

        beeb_window->GetBeebThread()->Send(std::make_shared<BeebThread::LoadDiscMessage>(0, std::move(disc_image), true));

        auto message = std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(BeebThreadHardResetFlag_Run |
                                                                                     BeebThreadHardResetFlag_Boot);
        this->SendMessage(beeb_window, server, request, std::move(message));
        return;
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandleLoadDiskRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        std::string path;
        bool in_memory = false;
        uint32_t drive = 0;
        const PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
        };
        const QueryParameter qps[] = {
            {"path", &ParseStdString, &path},
            {"in_memory", &ParseBool, &in_memory},
            {"drive", &ParseU32, &drive},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps, qps)) {
            return;
        }

        auto message_list = std::make_shared<MessageList>("HTTP load-disk request");
        Messages messages(message_list);

        std::shared_ptr<DiscImage> image;
        if (in_memory) {
            image = LoadMemoryDiscImage(path, messages);
        } else {
            image = DirectDiscImage::CreateForFile(path, messages);
        }

        if (!image) {
            this->SendMessagesResponse(server, request, message_list);
            return;
        }

        beeb_window->GetBeebThread()->Send(std::make_shared<BeebThread::LoadDiscMessage>(drive, std::move(image), true));

        server->SendResponse(request, HTTPResponse::OK());
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandleClearSymbolsRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        const PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps)) {
            return;
        }

        SymbolTable *symbol_table = beeb_window->GetMutableSymbolTable();
        symbol_table->Clear();

        server->SendResponse(request, HTTPResponse::OK());
    }
#endif

#if BBCMICRO_DEBUGGER
    static bool ParseLoadSymbolsSuffix(ParseArgsState *state, const std::string &str, void *context) {
        // copy of code in debugger.cpp - should really unify.
        for (char c : str) {
            if (!isalnum(c)) {
                // cheeky way of avoiding running into any UTF-8...
                state->server->SendResponse(*state->request, HTTPResponse::BadRequest(*state->request, "suffixes must be alphanumeric only"));
                return false;
            } else if (!IsValidAddressSuffixChar(c)) {
                state->server->SendResponse(*state->request, HTTPResponse::BadRequest(*state->request, "invalid address suffix char: %c", c));
                return false;
            }
        }

        auto suffixes = (std::vector<std::string> *)context;

        suffixes->push_back(str);

        return true;
    }
#endif

#if BBCMICRO_DEBUGGER
    static bool ParseSymbolFileAddressSuffixMode(ParseArgsState *state, const std::string &str, void *context) {
        if (str == "e") {
            *(SymbolFileAddressSuffixMode *)context = SymbolFileAddressSuffixMode_Exclusive;
            return true;
        } else if (str == "i") {
            *(SymbolFileAddressSuffixMode *)context = SymbolFileAddressSuffixMode_Inclusive;
            return true;
        } else {
            state->server->SendResponse(*state->request, HTTPResponse::BadRequest(*state->request, "invalid address suffix mode: %s", str.c_str()));
            return false;
        }
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandleLoadSymbolsRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        std::string format;
        std::string path;
        uint16_t group = 0xffff; //cheesy way of detecting 8-bit value not provided...
        std::vector<std::string> suffixes;
        SymbolFileAddressSuffixMode mode = SymbolFileAddressSuffixMode_Exclusive;
        const PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
            {&ParseStdString, &format},
        };
        const QueryParameter qps[] = {
            {"path", &ParseStdString, &path},
            {"group", &ParseU16, &group},
            {"s", &ParseLoadSymbolsSuffix, &suffixes},
            {"mode", &ParseSymbolFileAddressSuffixMode, &mode},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps, qps)) {
            return;
        }

        if (format.empty() || path.empty()) {
            server->SendResponse(request, HTTPResponse::BadRequest(request, "must supply path and format"));
            return;
        }

        const SymbolTable::SymbolParser *parser = SymbolTable::SymbolParserRegistry::FindParserByFormatName(format);
        if (!parser) {
            std::string formats;
            for (const std::unique_ptr<const SymbolTable::SymbolParser> &registered_parser : SymbolTable::SymbolParserRegistry::GetParsers()) {
                if (!formats.empty()) {
                    formats += "; ";
                }
                formats += registered_parser->GetFormatName();
            }
            server->SendResponse(request, HTTPResponse::BadRequest(request, "unknown format: %s (must be one of: %s)", format.c_str(), formats.c_str()));
            return;
        }

        SymbolTable *symbol_table = beeb_window->GetMutableSymbolTable();

        auto message_list = std::make_shared<MessageList>("HTTP load-symbols request");
        Messages messages(message_list);

        size_t file_index;
        if (!symbol_table->LoadFromFile(path, parser, &messages, &file_index)) {
            this->SendMessagesResponse(server, request, message_list);
            return;
        }

        if (group < 256) {
            symbol_table->SetFileGroupIndex(file_index, (uint8_t)group);
        }

        symbol_table->SetFileAddressSuffixes(file_index, std::move(suffixes));

        symbol_table->SetFileAddressSuffixMode(file_index, mode);

        server->SendResponse(request, HTTPResponse::OK());
    }
#endif

#if BBCMICRO_DEBUGGER
    bool ParseBreakpointFlags(uint8_t *flags, const std::string &str) const {
        *flags = 0;

        for (char c : str) {
            if (c == 'r') {
                *flags |= BBCMicroByteDebugFlag_BreakRead;
            } else if (c == 'w') {
                *flags |= BBCMicroByteDebugFlag_BreakWrite;
            } else if (c == 'x') {
                *flags |= BBCMicroByteDebugFlag_BreakExecute;
            } else {
                return false;
            }
        }

        return true;
    }
#endif

#if BBCMICRO_DEBUGGER
    bool ParseBreakpointArgumentsOrSendResponse(M6502Word *addr,
                                                uint32_t *dso,
                                                uint8_t *flags,
                                                const std::string &addr_str,
                                                const std::string *dso_str,
                                                const std::string *flags_str,
                                                BeebWindow *beeb_window,
                                                HTTPServer *server,
                                                const HTTPRequest &request) {
        std::shared_ptr<BeebThread> beeb_thread = beeb_window->GetBeebThread();
        std::shared_ptr<const BBCMicroType> type = beeb_thread->GetBBCMicroType();

        if (flags) {
            if (flags_str) {
                if (!this->ParseBreakpointFlags(flags, *flags_str)) {
                    server->SendResponse(request, HTTPResponse::BadRequest("invalid breakpoint flags: %s", flags_str->c_str()));
                    return false;
                }
            } else {
                *flags = 0;
            }
        }

        bool got_explicit_dso = false;
        *dso = 0;
        if (dso_str) {
            if (!dso_str->empty()) {
                if (!ParseAddressSuffix(dso, type, dso_str->c_str(), nullptr)) {
                    server->SendResponse(request, HTTPResponse::BadRequest("invalid DSO: %s", dso_str->c_str()));
                    return false;
                }

                got_explicit_dso = true;
            }
        }

        const char *ep = nullptr;
        if (!GetUInt16FromString(&addr->w, addr_str, 0, &ep) || *ep != 0) {
            const SymbolTable *symbol_table = beeb_window->GetSymbolTable();

            uint32_t symbol_table_dso;
            if (!symbol_table->GetAddressForSymbol(&addr->w, &symbol_table_dso, type, addr_str)) {
                server->SendResponse(request, HTTPResponse::BadRequest("unknown symbol: %s", addr_str.c_str()));
                return false;
            }

            if (!got_explicit_dso) {
                *dso = symbol_table_dso;
            }
        }

        return true;
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandleAddressBreakpointRequest(HTTPServer *server, const HTTPRequest &request, const std::vector<std::string> &path_parts, size_t command_index, bool set) {
        BeebWindow *beeb_window;
        std::string addr_str;
        std::string dso_str;
        std::string flags_str;
        PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
            {&ParseStdString, &addr_str},
            {&ParseStdString, &flags_str},
        };
        QueryParameter qps[] = {
            {"s", &ParseStdString, &dso_str},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps, qps)) {
            return;
        }

        M6502Word addr;
        uint32_t dso;
        uint8_t flags;
        if (!this->ParseBreakpointArgumentsOrSendResponse(&addr, &dso, &flags, addr_str, &dso_str, &flags_str, beeb_window, server, request)) {
            return;
        }

        std::shared_ptr<BeebThread> beeb_thread = beeb_window->GetBeebThread();
        beeb_thread->Send(std::make_shared<BeebThread::DebugModifyAddressDebugFlags>(addr, dso, set ? (uint8_t)0 : flags, set ? flags : (uint8_t)0));

        server->SendResponse(request, HTTPResponse::OK());
    }

    void HandleSetAddressBreakpointRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        this->HandleAddressBreakpointRequest(server, request, path_parts, command_index, true);
    }

    void HandleClearAddressBreakpointRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        this->HandleAddressBreakpointRequest(server, request, path_parts, command_index, false);
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandleByteBreakpointRequest(HTTPServer *server, const HTTPRequest &request, const std::vector<std::string> &path_parts, size_t command_index, bool set) {
        BeebWindow *beeb_window;
        std::string addr_str;
        std::string dso_str;
        std::string flags_str;
        PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
            {&ParseStdString, &addr_str},
            {&ParseStdString, &flags_str},
        };
        QueryParameter qps[] = {
            {"s", &ParseStdString, &dso_str},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps, qps)) {
            return;
        }

        M6502Word addr;
        uint32_t dso;
        uint8_t flags;
        if (!this->ParseBreakpointArgumentsOrSendResponse(&addr, &dso, &flags, addr_str, &dso_str, &flags_str, beeb_window, server, request)) {
            return;
        }

        std::shared_ptr<BeebThread> beeb_thread = beeb_window->GetBeebThread();
        std::shared_ptr<const BBCMicroReadOnlyState> state;
        std::shared_ptr<const BBCMicroDebugState> debug_state;
        beeb_thread->DebugGetState(&state, &debug_state);

        BBCMicro::ReadOnlyBigPage bp;
        BBCMicro::DebugGetBigPageForAddress(&bp, state.get(), debug_state.get(), addr, false, dso);

        beeb_thread->Send(std::make_shared<BeebThread::DebugModifyByteDebugFlags>(bp.index, (uint16_t)addr.p.o, set ? (uint8_t)0 : flags, set ? flags : (uint8_t)0));

        server->SendResponse(request, HTTPResponse::OK());
    }

    void HandleSetByteBreakpointRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        this->HandleByteBreakpointRequest(server, request, path_parts, command_index, true);
    }

    void HandleClearByteBreakpointRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        this->HandleByteBreakpointRequest(server, request, path_parts, command_index, false);
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandleClearBreakpointsRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps)) {
            return;
        }

        std::shared_ptr<BeebThread> beeb_thread = beeb_window->GetBeebThread();
        beeb_thread->Send(std::make_shared<BeebThread::DebugClearBreakpoints>());

        server->SendResponse(request, HTTPResponse::OK());
    }
#endif

#if BBCMICRO_DEBUGGER
    template <class RequestArgsType>
    bool PrepareForGenericRequestOrSendResponse(BeebWindow **beeb_window_ptr,
                                                RequestArgsType *request_args_ptr,
                                                HTTPServer *server,
                                                const HTTPRequest &request,
                                                const std::vector<std::string> &path_parts,
                                                size_t command_index) {
        if (command_index + 1 == path_parts.size()) {
            if (BeebWindows::GetNumWindows() == 1) {
                *beeb_window_ptr = BeebWindows::GetWindowByIndex(0);
            } else {
                *beeb_window_ptr = nullptr;
            }
        } else {
            PathParameter pps[] = {
                {&ParseWindow, beeb_window_ptr},
            };
            if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps)) {
                return false;
            }
        }

        nlohmann::json j;
        if (!this->GetJSONBodyOrSendResponse(&j, server, request)) {
            return false;
        }

        std::string exc_what;
        if (!LoadJSON(request_args_ptr, j, &exc_what)) {
            server->SendResponse(request, HTTPResponse::BadRequest("Request object parse error: %s", exc_what.c_str()));
            return false;
        }

        return true;
    }
#endif

#if BBCMICRO_DEBUGGER
    static void HandleGenericRequestCompletion(bool success,
                                               nlohmann::json j,
                                               HTTPServer *server,
                                               const HTTPResponseData &response_data) {
        HTTPResponse response;
        if (success) {
            response = HTTPResponse::OK();
        } else {
            response = HTTPResponse::InternalServerError();
        }

        response.content_type = HTTP_JSON_CONTENT_TYPE;
        response.content = SaveJSONData(std::move(j));

        server->SendResponse(response_data, std::move(response));
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandleGenericRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        ApiRuntimeArgs runtime_args;
        ApiRequest request_args;
        if (!this->PrepareForGenericRequestOrSendResponse(&runtime_args.beeb_window, &request_args, server, request, path_parts, command_index)) {
            return;
        }

        runtime_args.messages = std::make_shared<Messages>(std::make_shared<MessageList>("API request"));

        ApiExecuteSingleRequest(std::move(runtime_args),
                                std::move(request_args),
                                [messages = runtime_args.messages,
                                 response_data = request.response_data,
                                 server](ApiResponse response) -> void {
                                    HandleGenericRequestCompletion(response.success, std::move(response), server, response_data);
                                });
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandleGenericMultipleRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        ApiRuntimeArgs runtime_args;
        ApiMultipleRequests request_args;
        if (!this->PrepareForGenericRequestOrSendResponse(&runtime_args.beeb_window, &request_args, server, request, path_parts, command_index)) {
            return;
        }

        runtime_args.messages = std::make_shared<Messages>(std::make_shared<MessageList>("API request"));

        ApiExecuteMultipleRequests(std::move(runtime_args),
                                   std::move(request_args),
                                   [messages = runtime_args.messages,
                                    response_data = request.response_data,
                                    server](ApiMultipleResponses response) -> void {
                                       HandleGenericRequestCompletion(response.success, std::move(response), server, response_data);
                                   });
    }

#endif

    void HandleLaunchRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        std::string path;
        PathParameter pps[] = {
            {&ParseStdString, &path},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps)) {
            return;
        }

        BeebWindowLaunchArguments arguments;
        arguments.file_path = path;
        arguments.type = BeebWindowLaunchType_UseExistingProcess;

        BeebWindow *beeb_window = BeebWindows::FindMRUBeebWindow();
        beeb_window->Launch(arguments);

        server->SendResponse(request, HTTPResponse::OK());
    }

    std::shared_ptr<DiscImage> LoadDiscImageFromRequestOrSendResponse(HTTPServer *server, const HTTPRequest &request, const std::string &name) {
        auto message_list = std::make_shared<MessageList>("HTTP disc image request");
        Messages messages(message_list);

        DiscGeometry geometry;

        if (FindDiscGeometryFromMIMEType(&geometry,
                                         request.content_type.c_str(),
                                         request.body.size(),
                                         messages)) {
            // ok...
        } else if (!name.empty() &&
                   FindDiscGeometryFromFileDetails(&geometry,
                                                   name.c_str(),
                                                   request.body.size(),
                                                   &messages)) {
            // ok...
        } else {
            this->SendMessagesResponse(server, request, message_list);
            return nullptr;
        }

        message_list->ClearMessages();

        std::shared_ptr<DiscImage> disc_image = MemoryDiscImage::LoadFromBuffer(name, HTTP_DISC_IMAGE_LOAD_METHOD, request.body.data(), request.body.size(), geometry, &messages);
        if (!disc_image) {
            this->SendMessagesResponse(server, request, message_list);
            return nullptr;
        }

        return disc_image;
    }

    void HandleRequest(HTTPServer *server, HTTPRequest &&request) {
        std::vector<std::string> path_parts = GetPathParts(request.url_path);

        if (path_parts.empty()) {
            server->SendResponse(request, HTTPResponse::NotFound(request));
            return;
        }

        if (!path_parts.empty()) {
            auto it = m_request_handlers.find(path_parts[0]);
            if (it == m_request_handlers.end()) {
                server->SendResponse(request, HTTPResponse::BadRequest(request, "Couldn't find endpoint for path: %s", request.url_path.c_str()));
                return;
            }

            (this->*it->second)(server, std::move(request), path_parts, 0);
            return;
        }

        server->SendResponse(request, HTTPResponse::BadRequest(request));
    }

    void SendMessagesResponse(HTTPServer *server, const HTTPRequest &request, const std::shared_ptr<MessageList> &message_list) {
        std::string text;

        message_list->ForEachMessage([&text](MessageList::Message *message) {
            if (!text.empty()) {
                text += "\r\n";
            }
            text += message->text;
        });

        server->SendResponse(request, HTTPResponse::BadRequest(request, "%s", text.c_str()));
    }

    void SendMessage(BeebWindow *beeb_window,
                     HTTPServer *server,
                     const HTTPRequest &request,
                     std::shared_ptr<BeebThread::Message> message) {
        auto completion_fun = [server, response_data = request.response_data](const char *failure_reason, const char *failure_text) -> void {
            LOGF(OUTPUT, "SendMessage completion_fun: connected ID=%" PRIu64 "\n", response_data.connection_id);

            HTTPResponse response;
            if (failure_reason) {
                response = HTTPResponse::InternalServerError("The request did not succeed");

                if (failure_text) {
                    response.content_type = HTTP_TEXT_CONTENT_TYPE;
                    response.content_type_charset = HTTP_UTF8_CHARSET;
                    response.SetContentString(failure_text);
                }
            } else {
                response = HTTPResponse::OK();
            }

            server->SendResponse(response_data, response);
        };

        std::shared_ptr<BeebThread> beeb_thread = beeb_window->GetBeebThread();
        beeb_thread->Send(std::move(message), std::move(completion_fun));
    }

    bool GetJSONBodyOrSendResponse(nlohmann::json *j, HTTPServer *server, const HTTPRequest &request) {
        // JSON only.
        if (request.content_type != HTTP_JSON_CONTENT_TYPE) {
            server->SendResponse(request, HTTPResponse::UnsupportedMediaType(request));
            return false;
        }

        // Parse the JSON.
        std::string exc_what;
        try {
            *j = nlohmann::json::parse(request.body.begin(), request.body.end());
        } catch (const nlohmann::json::exception &exc) {
            server->SendResponse(request, HTTPResponse::BadRequest("JSON parse error: %s", exc.what()));
            return false;
        }

        return true;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<HTTPHandler> CreateHTTPMethodsHandler() {
    return std::make_shared<HTTPMethodsHandler>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
