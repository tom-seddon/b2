#include <shared/system.h>
#include "http_api.h"
#include <shared/log.h>
#include <shared/debug.h>
#include <beeb/DiscInterface.h>
#include "BeebWindow.h"
#include "BeebThread.h"
#include "BeebWindows.h"
#include "misc.h"
#include <shared/strings.h>
#include "b2.h"
#include <inttypes.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Common arguments supplied to the ApiExecuteXXXRequest functions.
struct ApiExecuteArgs {
    // Do not pass to later stages of the execution.
    BeebWindow *beeb_window = nullptr;

    // If passing to later stages of the execution, pass a weak_ptr<BeebThread>.
    std::shared_ptr<BeebThread> beeb_thread;

    // OK to pass to later stages of the execution.
    std::shared_ptr<Messages> messages;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//namespace nlohmann {
//    template <>
//    struct adl_serializer<std::variant<uint8_t, std::string>> {
//        static void from_json(const json &j, std::variant<uint8_t, std::string> &v) {
//            if (j.is_string()) {
//                v = j.get<std::string>();
//            } else if (j.is_number_unsigned()) {
//                uint64_t value = j.get<uint64_t>();
//                if (value >= 256) {
//                    throw nlohmann::json::type_error::create(302, strprintf("invalid uint8_t value: %" PRIu64, value), nullptr);
//                }
//
//                v = (uint8_t)value;
//            } else {
//                throw nlohmann::json::type_error::create(302, "value not uint8_t or string", nullptr);
//            }
//        }
//
//        static void to_json(json &j, const std::variant<uint8_t, std::string> &v) {
//            if (const uint8_t *u = std::get_if<uint8_t>(&v)) {
//                j = *u;
//            } else if (const std::string *s = std::get_if<std::string>(&v)) {
//                j = *s;
//            } else {
//                ASSERT(false);
//                j = {};
//            }
//        }
//    };
//} // namespace nlohmann

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void from_json(const nlohmann::json &j, BBCString &s) {
    if (!j.is_array()) {
        throw nlohmann::json::type_error::create(302, strprintf("invalid BBCString value: must be an array"), nullptr);
    }

    size_t index = 0;
    for (const nlohmann::json &value_j : j) {
        if (value_j.is_string()) {
            const std::string &value = value_j.get<std::string>();

            std::vector<uint8_t> bbc;
            int32_t bad_codepoint;
            size_t bad_char_start;
            int bad_char_len;
            if (!GetBBCASCIIFromUTF8(&bbc, value, &bad_codepoint, &bad_char_start, &bad_char_len)) {
                if (bad_codepoint < 0) {
                    // Shouldn't see this? nlohmann::json should have sorted this out!
                    throw nlohmann::json::type_error::create(302, strprintf("invalid BBCString value: element %zu not valid UTF-8", index), nullptr);
                } else {
                    throw nlohmann::json::type_error::create(302, strprintf("invalid BBCString value: element %zu contains unsupported codepoint: %" PRId32 " (0x%" PRIx32 ")", index, bad_codepoint, bad_codepoint), nullptr);
                }
            }

            s.bytes.insert(s.bytes.end(), bbc.begin(), bbc.end());
        } else if (value_j.is_number_unsigned()) {
            uint64_t value = value_j.get<uint64_t>();
            if (value >= 256) {
                throw nlohmann::json::type_error::create(302, strprintf("invalid BBCString value: element %zu not valid byte value", index), nullptr);
            }

            s.bytes.push_back((uint8_t)value);
        } else {
            throw nlohmann::json::type_error::create(302, strprintf("invalid BBCString value: element %zu not string or byte", index), nullptr);
        }

        ++index;
    }
}

void to_json(nlohmann::json &j, const BBCString &s) {
    j = nlohmann::json::value_t::array;

    std::string str;
    bool in_str = false;
    for (const uint8_t byte : s.bytes) {
        if ((byte >= 32 && byte < 127) || byte == 10 || byte == 13) {
            if (!in_str) {
                str.clear();
                in_str = true;
            }
            str.push_back((char)byte);
        } else {
            if (in_str) {
                j.push_back(std::move(str));
                in_str = false;
            }
            j.push_back(byte);
        }
    }

    if (in_str) {
        j.push_back(std::move(str));
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool CopyROM(BeebConfig::ROM *dest, const ApiROMContents &src, const LogSet *logs) {
    if (src.standard_rom != StandardROM_None) {
        dest->standard_rom = FindBeebROM(src.standard_rom);
        if (!dest->standard_rom) {
            logs->e.f("StandardROM not available: %s", GetStandardROMEnumName(src.standard_rom));
            return false;
        }
    } else {
        dest->standard_rom = nullptr;
    }

    dest->file_name = src.path;

    return true;
}

template <class T>
static void SetOptional(T *dest, const std::optional<T> &src) {
    if (src.has_value()) {
        *dest = *src;
    }
}

static bool Load(BeebLoadedConfig *loaded_config, const ApiConfigArgs &src, const LogSet *logs) {
    BeebConfig dest;

    if (!src.base_stock_config.empty()) {
        const BeebConfig *base_config = nullptr;

        for (size_t i = 0; i < GetNumDefaultBeebConfigs(); ++i) {
            const BeebConfig *default_config = GetDefaultBeebConfigByIndex(i);
            if (default_config->name == src.base_stock_config) {
                base_config = default_config;
                break;
            }
        }

        if (!base_config) {
            logs->e.f("base stock config not found: %s", src.base_stock_config.c_str());
            return false;
        }

        dest = *base_config;
    } else if (!src.base_config.empty()) {
        const BeebConfig *base_config = nullptr;

        for (size_t i = 0; i < BeebWindows::GetNumConfigs(); ++i) {
            const BeebConfig *config = BeebWindows::GetConfigByIndex(i);
            if (config->name == src.base_config) {
                base_config = config;
                break;
            }
        }

        if (!base_config) {
            logs->e.f("base config not found: %s", src.base_config.c_str());
            return false;
        }

        dest = *base_config;
    } else {
        logs->e.f("no base config supplied");
        return false;
    }

    dest.name = src.name;

    if (src.os_rom.has_value()) {
        if (!CopyROM(&dest.os, src.os_rom->contents, logs)) {
            return false;
        }

        dest.os_rom_type = src.os_rom->os_rom_type;
    }

    bool got_rom[16] = {};
    for (const ApiSidewaysROM &src_rom : src.sideways_roms) {
        if (src_rom.bank < 0 || src_rom.bank >= 16) {
            logs->e.f("invalid ROM bank: %d", src_rom.bank);
            return false;
        }

        if (got_rom[src_rom.bank]) {
            logs->e.f("already set bank: %d", src_rom.bank);
            return false;
        }

        BeebConfig::SidewaysROM *dest_rom = &dest.roms[src_rom.bank];

        if (!CopyROM(dest_rom, src_rom.contents, logs)) {
            return false;
        }

        dest_rom->writeable = src_rom.writeable;
        dest_rom->type = src_rom.rom_type;
    }

    SetOptional(&dest.video_nula, src.video_nula);
    SetOptional(&dest.beeblink, src.beeblink);

    for (size_t i = 0; i < dest.nvram.size() && i < src.nvram.size(); ++i) {
        dest.nvram[i] = src.nvram[i];
    }

    SetOptional(&dest.mouse, src.mouse);

    BeebConfigArguments arguments;

    if (dest.os_rom_type >= OSROMType_MultiOSBank0 && dest.os_rom_type <= OSROMType_MultiOSBank3) {
        arguments.multi_os_bank = dest.os_rom_type - OSROMType_MultiOSBank0;
    }

    if (!BeebLoadedConfig::Load(loaded_config, dest, arguments, logs)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ConfigWaitForOSWORD0Callback : OSWORD0Callback {
  public:
    ConfigWaitForOSWORD0Callback(std::function<void(bool, std::nullptr_t &&)> completion_fun)
        : m_completion_fun(std::move(completion_fun)) {
    }

    bool ThreadOnOSWORD0(BeebThread *beeb_thread) override {
        (void)beeb_thread;

        m_completion_fun(true, nullptr);

        return false;
    }

  protected:
  private:
    std::function<void(bool, std::nullptr_t &&)> m_completion_fun;
};

static void ApiExecuteConfigRequest(const ApiExecuteArgs &execute_args,
                                    ApiConfigArgs &&request_args,
                                    std::function<void(bool, std::nullptr_t &&)> completion_fun) {
    ASSERT(IsMainThread());

    BeebLoadedConfig loaded_config;
    if (!Load(&loaded_config, request_args, execute_args.messages.get())) {
        completion_fun(false, nullptr);
        return;
    }

    std::function<void(bool, std::string)> message_completion_fun = [completion_fun,
                                                                     messages = execute_args.messages](bool success,
                                                                                                       std::string message) {
        if (!success) {
            messages->e.f("%s failed: %s\n", API_CONFIG_REQUEST_TYPE, message.c_str());
        }
        completion_fun(success, nullptr);
    };

    uint32_t flags = BeebThreadHardResetFlag_Run;
    double osword_0_timeout_seconds = BeebThread::HardResetMessage::DEFAULT_OSWORD_0_TIMEOUT_SECONDS;

    if (request_args.wait_for_osword_0) {
        flags |= BeebThreadHardResetFlag_WaitForOSWORD0;
        osword_0_timeout_seconds = 15.; //TODO: should probably be configurable?
    }

    execute_args.beeb_thread->Send(std::make_shared<BeebThread::HardResetAndChangeConfigMessage>(std::move(loaded_config),
                                                                                                 flags,
                                                                                                 osword_0_timeout_seconds),
                                   std::move(message_completion_fun));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ApiExecutePasteRequest(const ApiExecuteArgs &execute_args,
                                   ApiPasteArgs &&request_args,
                                   std::function<void(bool, std::nullptr_t &&)> completion_fun) {
    execute_args.beeb_thread->Send(std::make_shared<BeebThread::StopPasteMessage>());

    execute_args.beeb_thread->Send(std::make_shared<BeebThread::StartPasteMessage>(std::move(request_args.input.bytes)),
                                   [completion_fun, messages = execute_args.messages](bool success, std::string message) {
                                       if (!success) {
                                           messages->e.f("%s failed: %s\n", API_PASTE_REQUEST_TYPE, message.c_str());
                                       }
                                       completion_fun(success, nullptr);
                                   });
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ApiExecuteStartCaptureOSWRCHRequest(const ApiExecuteArgs &execute_args,
                                                std::nullptr_t &&,
                                                std::function<void(bool, std::nullptr_t &&)> completion_fun) {
    execute_args.beeb_window->StartCopyOSWRCH();
    completion_fun(true, nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ApiExecuteStopCaptureOSWRCHRequest(const ApiExecuteArgs &execute_args,
                                               std::nullptr_t &&,
                                               std::function<void(bool, ApiStopCaptureOSWRCHResult &&)> completion_fun) {
    ApiStopCaptureOSWRCHResult result;
    if (!execute_args.beeb_window->StopCopyOSWRCH(&result.output.bytes)) {
        execute_args.messages->e.f("Not copying\n");
        completion_fun(false, {});
        return;
    }

    completion_fun(true, std::move(result));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
static std::vector<std::string> ListOrdinaryEnumValues(const char *(*get_enum_name_fn)(T)) {
    std::vector<std::string> names;

    T value = 0;
    for (;;) {
        const char *name = (*get_enum_name_fn)(value);
        if (name[0] == '?') {
            break;
        }

        names.push_back(name);

        ++value;
    }

    return names;
}

static std::vector<std::string> GetBeebConfigNames(size_t (*get_num_configs_fn)(),
                                                   const BeebConfig *(*get_config_by_index_fn)(size_t)) {
    std::vector<std::string> names;

    size_t n = (*get_num_configs_fn)();
    for (size_t i = 0; i < n; ++i) {
        const BeebConfig *config = (*get_config_by_index_fn)(i);
        names.push_back(config->name);
    }

    return names;
}

static void ApiExecuteListValues(const ApiExecuteArgs &execute_args,
                                 ApiListValuesArgs &&request_args,
                                 std::function<void(bool, ApiListValuesResult &&)> completion_fun) {
    ApiListValuesResult result;
    if (request_args.name == "StandardROM") {
        result.values = ListOrdinaryEnumValues(&GetStandardROMEnumName);
    } else if (request_args.name == "ROMType") {
        result.values = ListOrdinaryEnumValues(&GetROMTypeEnumName);
    } else if (request_args.name == "OSROMType") {
        result.values = ListOrdinaryEnumValues(&GetOSROMTypeEnumName);
    } else if (request_args.name == "stock_configs") {
        result.values = GetBeebConfigNames(&GetNumDefaultBeebConfigs, &GetDefaultBeebConfigByIndex);
    } else if (request_args.name == "configs") {
        result.values = GetBeebConfigNames(&BeebWindows::GetNumConfigs, &BeebWindows::GetConfigByIndex);
    } else {
        execute_args.messages->e.f("unknown enum: %s\n", request_args.name.c_str());
        completion_fun(false, {});
        return;
    }

    completion_fun(true, std::move(result));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class ArgsType, class ResultType>
static void HandleApiExecute(const ApiExecuteArgs &execute_args,
                             const ApiRequest &request,
                             std::function<void(bool, nlohmann::json)> completion_fun,
                             void (*execute_fn)(const ApiExecuteArgs &,
                                                ArgsType &&,
                                                std::function<void(bool, ResultType &&)>),
                             bool requires_beeb_window) {
    if (requires_beeb_window) {
        if (!execute_args.beeb_window) {
            execute_args.messages->e.f("Must specify window\n");
            completion_fun(false, nullptr);
            return;
        }
    }

    std::string exc_what;
    ArgsType request_args;

    if (!LoadJSON(&request_args, request.args, &exc_what)) {
        execute_args.messages->e.f("Args parse failed: %s\n", exc_what.c_str());
        completion_fun(false, nullptr);
        return;
    }

    (*execute_fn)(execute_args,
                  std::move(request_args),
                  [completion_fun](bool success, ResultType &&result) -> void {
                      completion_fun(success, std::move(result));
                  });
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ExecuteSingleRequest(ApiExecuteArgs execute_args,
                                 ApiRequest request,
                                 std::function<void(bool, nlohmann::json)> completion_fun) {
    if (request.type == API_CONFIG_REQUEST_TYPE) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteConfigRequest, true);
    } else if (request.type == API_PASTE_REQUEST_TYPE) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecutePasteRequest, true);
    } else if (request.type == API_START_CAPTURE_OSWRCH_REQUEST_TYPE) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteStartCaptureOSWRCHRequest, true);
    } else if (request.type == API_STOP_CAPTURE_OSWRCH_REQUEST_TYPE) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteStopCaptureOSWRCHRequest, true);
    } else if (request.type == API_LIST_VALUES_REQUEST_TYPE) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteListValues, false);
    } else {
        execute_args.messages->e.f("Unsupported request type: %s\n", request.type.c_str());
        completion_fun(false, nullptr);
    }
}

static std::shared_ptr<BeebThread> GetBeebThread(BeebWindow *beeb_window) {
    if (beeb_window) {
        return beeb_window->GetBeebThread();
    } else {
        return nullptr;
    }
}

void ApiExecuteSingleRequest(const ApiRuntimeArgs &runtime_args,
                             ApiRequest request,
                             std::function<void(bool, nlohmann::json)> completion_fun) {
    // since this is on the main thread, the BeebWindow is not going away (even if only not just quite yet).
    ASSERT(IsMainThread());

    ApiExecuteArgs execute_args;

    execute_args.beeb_window = runtime_args.beeb_window;
    execute_args.beeb_thread = GetBeebThread(execute_args.beeb_window);
    execute_args.messages = runtime_args.messages;

    ExecuteSingleRequest(std::move(execute_args),
                         std::move(request),
                         std::move(completion_fun));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MultipleRequestsState {
    BeebWindow *beeb_window = nullptr;
    std::weak_ptr<BeebThread> beeb_thread;
    std::shared_ptr<Messages> messages;
    ApiMultipleRequests request;
    ApiMultipleResponses responses;
    size_t index = 0;
    std::function<void(bool, nlohmann::json)> request_completion_fun;
    std::function<void(bool, nlohmann::json)> overall_completion_fun;
};

class ExecuteNextMainThreadMessage : public MainThreadMessage {
  public:
    explicit ExecuteNextMainThreadMessage(std::shared_ptr<MultipleRequestsState> state)
        : m_state(std::move(state)) {
    }

    void HandleMessage() override {
        ASSERT(m_state->index <= m_state->request.requests.size());

        if (m_state->index == m_state->request.requests.size()) {
            m_state->overall_completion_fun(true, m_state->responses);
        } else {
            ApiExecuteArgs execute_args;

            execute_args.beeb_thread = m_state->beeb_thread.lock();
            if (!execute_args.beeb_thread || !execute_args.beeb_thread->IsStarted()) {
                m_state->messages->e.f("BeebThread gone\n");
                m_state->overall_completion_fun(false, nullptr);
                return;
            }

            execute_args.beeb_window = m_state->beeb_window;
            execute_args.messages = m_state->messages;

            ExecuteSingleRequest(execute_args,
                                 m_state->request.requests[m_state->index],
                                 m_state->request_completion_fun);
        }
    }

  protected:
  private:
    std::shared_ptr<MultipleRequestsState> m_state;
};

void ApiExecuteMultipleRequests(const ApiRuntimeArgs &runtime_args,
                                ApiMultipleRequests request,
                                std::function<void(bool, nlohmann::json)> completion_fun) {
    ASSERT(IsMainThread());

    if (request.requests.empty()) {
        completion_fun(true, nullptr);
        return;
    }

    auto state = std::make_shared<MultipleRequestsState>();

    state->beeb_window = runtime_args.beeb_window;
    state->beeb_thread = GetBeebThread(state->beeb_window);
    state->messages = runtime_args.messages;
    state->request = std::move(request);
    state->overall_completion_fun = std::move(completion_fun);
    state->request_completion_fun = [state_weak = std::weak_ptr<MultipleRequestsState>(state)](bool success, nlohmann::json j) -> void {
        std::shared_ptr<MultipleRequestsState> state = state_weak.lock();
        ASSERT(!!state);

        if (success) {
            state->responses.responses.push_back({true, std::move(j)});
            ++state->index;
            PushMainThreadMessage(std::make_unique<ExecuteNextMainThreadMessage>(state));
        } else {
            state->responses.responses.push_back({false, {}});
            ASSERT(state->index < state->request.requests.size());
            state->messages->e.f("request %zu (%s) failed\n", state->index, state->request.requests[state->index].type.c_str());
            state->overall_completion_fun(false, nlohmann::json());
        }
    };

    PushMainThreadMessage(std::make_unique<ExecuteNextMainThreadMessage>(state));
}
