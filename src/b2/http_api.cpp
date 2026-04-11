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

bool Load(BeebLoadedConfig *loaded_config, const ApiConfigArgs &src, const LogSet *logs) {
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

// All the execute functions have this sort of signature.
//
// RUNTIME_ARGS is the runtime arguments, put together by b2 to make the call.
//
// REQUEST_ARGS is the request arguments, probably deserialized from the HTTP request's body.
//
// COMPLETION_FUN is the completion function, to be called on success/failure. The first argument is the success flag, and the second, which is ignored on failure, is the request result (or std::nullptr_t if the request returns no specific result).
void ApiExecuteConfigRequest(const ApiRuntimeArgs &runtime_args, ApiConfigArgs &&request_args, std::function<void(bool, const std::nullptr_t &)> completion_fun) {
    BeebLoadedConfig loaded_config;
    if (!Load(&loaded_config, request_args, runtime_args.messages.get())) {
        completion_fun(false, nullptr);
        return;
    }

    std::function<void(bool, std::string)> message_completion_fun = [completion_fun, messages = runtime_args.messages](bool success, std::string message) {
        if (!success) {
            messages->e.f("%s failed: %s\n", API_CONFIG_REQUEST_TYPE, message.c_str());
        }
        completion_fun(success, nullptr);
    };

    runtime_args.beeb_thread->Send(std::make_shared<BeebThread::HardResetAndChangeConfigMessage>(BeebThreadHardResetFlag_Run, loaded_config),
                                   std::move(message_completion_fun));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Helper for ApiPasteArgs.
namespace nlohmann {
    template <>
    struct adl_serializer<std::variant<uint8_t, std::string>> {
        static void from_json(const json &j, std::variant<uint8_t, std::string> &v) {
            if (j.is_string()) {
                v = j.get<std::string>();
            } else if (j.is_number_unsigned()) {
                uint64_t value = j.get<uint64_t>();
                if (value >= 256) {
                    throw nlohmann::json::type_error::create(302, strprintf("invalid uint8_t value: %" PRIu64, value), nullptr);
                }

                v = (uint8_t)value;
            } else {
                throw nlohmann::json::type_error::create(302, "value not uint8_t or string", nullptr);
            }
        }
    };
} // namespace nlohmann

void ApiExecutePasteRequest(const ApiRuntimeArgs &runtime_args, ApiPasteArgs &&request_args, std::function<void(bool, const std::nullptr_t &)> completion_fun) {
    std::string text;
    for (size_t i = 0; i < request_args.parts.size(); ++i) {
        if (const uint8_t *byte = std::get_if<uint8_t>(&request_args.parts[i])) {
            text.push_back((char)*byte);
        } else if (const std::string *string = std::get_if<std::string>(&request_args.parts[i])) {
            text += *string;
        } else {
            ASSERT(false);
        }
    }

    runtime_args.beeb_thread->Send(std::make_shared<BeebThread::StopPasteMessage>());

    runtime_args.beeb_thread->Send(std::make_shared<BeebThread::StartPasteMessage>(std::move(text)),
                                   [completion_fun, messages = runtime_args.messages](bool success, std::string message) {
                                       if (!success) {
                                           messages->e.f("%s failed: %s\n", API_PASTE_REQUEST_TYPE, message.c_str());
                                       }
                                       completion_fun(success, nullptr);
                                   });
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class ArgsType, class ResultType>
static void HandleApiExecute(const ApiRuntimeArgs &runtime_args,
                             const ApiRequest &request,
                             std::function<void(bool, nlohmann::json)> completion_fun,
                             void (*execute_fn)(const ApiRuntimeArgs &,
                                                ArgsType &&,
                                                std::function<void(bool, const ResultType &)>)) {
    std::string exc_what;
    ArgsType request_args;

    if (!LoadJSON(&request_args, request.args, &exc_what)) {
        runtime_args.messages->e.f("Args parse failed: %s\n", exc_what.c_str());
        completion_fun(false, nullptr);
        return;
    }

    (*execute_fn)(runtime_args,
                  std::move(request_args),
                  [completion_fun](bool success, ResultType result) -> void {
                      completion_fun(success, result);
                  });
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ApiExecute(const ApiRuntimeArgs &runtime_args, const ApiRequest &request, std::function<void(bool, nlohmann::json)> completion_fun) {
    if (request.type == API_CONFIG_REQUEST_TYPE) {
        HandleApiExecute(runtime_args, request, completion_fun, &ApiExecuteConfigRequest);
    } else {
        runtime_args.messages->e.f("Unsupported request type: %s\n", request.type.c_str());
        completion_fun(false, nullptr);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MultipleRequestsState {
    ApiRuntimeArgs runtime_args;
    ApiMultipleRequests request;
    ApiMultipleResponses responses;
    size_t index = 0;
    std::function<void(bool, nlohmann::json)> request_completion_fun;
    std::function<void(bool, nlohmann::json)> overall_completion_fun;
};

static void ExecuteNext(const std::shared_ptr<MultipleRequestsState> &state) {
    ASSERT(state->index <= state->request.requests.size());

    if (state->index == state->request.requests.size()) {
        state->overall_completion_fun(true, state->responses);
    } else {
        ApiExecute(state->runtime_args,
                   state->request.requests[state->index],
                   state->request_completion_fun);
    }
}

void ApiExecute(const ApiRuntimeArgs &runtime_args, ApiMultipleRequests request, std::function<void(bool, nlohmann::json)> completion_fun) {
    auto state = std::make_shared<MultipleRequestsState>();

    state->runtime_args = runtime_args;
    state->request = std::move(request);
    state->overall_completion_fun = std::move(completion_fun);
    state->request_completion_fun = [state](bool success, nlohmann::json j) -> void {
        if (success) {
            state->responses.responses.push_back({true, std::move(j)});
            ++state->index;
            ExecuteNext(state);
        } else {
            state->responses.responses.push_back({false, {}});
            ASSERT(state->index < state->request.requests.size());
            state->runtime_args.messages->e.f("request %zu (%s) failed\n", state->index, state->request.requests[state->index].type.c_str());
            state->overall_completion_fun(false, nlohmann::json());
        }
    };
}
