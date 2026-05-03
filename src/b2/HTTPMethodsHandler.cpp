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
#include "load_save.h"
#include <shared/file_io.h>
#include "LoadMemoryDiscImage.h"

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

#if BBCMICRO_DEBUGGER

// Common arguments supplied to the ApiExecuteXXXRequest functions.
struct ApiExecuteArgs {
    // Do not pass to later stages of the execution.
    BeebWindow *beeb_window = nullptr;

    // If passing to later stages of the execution, pass a weak_ptr<BeebThread>.
    std::shared_ptr<BeebThread> beeb_thread;

    // OK to pass to later stages of the execution.
    std::shared_ptr<Messages> messages;
};

#endif

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

#if BBCMICRO_DEBUGGER
static bool GetFilePath(std::string *full_path, const std::string &path, const ApiSetGlobalsArgs &api_globals, const LogSet &logs) {
    if (PathIsFullySpecified(path)) {
        *full_path = path;
        return true;
    } else {
        if (!api_globals.read_path.has_value()) {
            logs.e.f("API global read_path not set for relative path: %s\n", path.c_str());
            return false;
        }

        *full_path = PathJoined(*api_globals.read_path, path);
        return true;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

static bool CopyROM(BeebConfig::ROM *dest, const ApiSetGlobalsArgs &api_globals, const ApiROMContents &src, const LogSet &logs) {
    if (src.standard_rom != StandardROM_None) {
        dest->standard_rom = FindBeebROM(src.standard_rom);
        if (!dest->standard_rom) {
            logs.e.f("StandardROM not available: %s", GetStandardROMEnumName(src.standard_rom));
            return false;
        }
    } else {
        dest->standard_rom = nullptr;
    }

    if (!GetFilePath(&dest->file_name, src.path, api_globals, logs)) {
        return false;
    }

    return true;
}

template <class T>
static void SetOptional(T *dest, const std::optional<T> &src) {
    if (src.has_value()) {
        *dest = *src;
    }
}

static bool Load(BeebLoadedConfig *loaded_config, const ApiSetGlobalsArgs &api_globals, const ApiConfigArgs &src, const LogSet &logs) {
    BeebConfig dest;

    if (!src.base_default_config.empty()) {
        const BeebConfig *base_config = nullptr;

        for (size_t i = 0; i < GetNumDefaultBeebConfigs(); ++i) {
            const BeebConfig *default_config = GetDefaultBeebConfigByIndex(i);
            if (default_config->name == src.base_default_config) {
                base_config = default_config;
                break;
            }
        }

        if (!base_config) {
            logs.e.f("base default config not found: %s", src.base_default_config.c_str());
            return false;
        }

        dest = *base_config;
    } else {
        logs.e.f("no base config supplied");
        return false;
    }

    dest.name = ""; //src.name;

    if (src.os_rom.has_value()) {
        if (!CopyROM(&dest.os, api_globals, src.os_rom->contents, logs)) {
            return false;
        }

        dest.os_rom_type = src.os_rom->os_rom_type;
    }

    bool got_rom[16] = {};
    for (const ApiSidewaysROM &src_rom : src.sideways_roms) {
        if (src_rom.bank < 0 || src_rom.bank >= 16) {
            logs.e.f("invalid ROM bank: %d", src_rom.bank);
            return false;
        }

        if (got_rom[src_rom.bank]) {
            logs.e.f("already set bank: %d", src_rom.bank);
            return false;
        }

        BeebConfig::SidewaysROM *dest_rom = &dest.roms[src_rom.bank];

        if (!CopyROM(dest_rom, api_globals, src_rom.contents, logs)) {
            return false;
        }

        dest_rom->writeable = src_rom.writeable;
        dest_rom->type = src_rom.rom_type;
    }

    SetOptional(&dest.video_nula, src.video_nula);
    SetOptional(&dest.beeblink, src.beeblink);

    for (const ApiConfigNVRAMByte &byte : src.nvram_bytes) {
        if (byte.index < 0 || (size_t)byte.index >= dest.nvram.size()) {
            logs.e.f("invalid NVRAM byte index: %d\n", byte.index);
            return false;
        }

        dest.nvram[(size_t)byte.index] &= byte.mask;
        dest.nvram[(size_t)byte.index] |= byte.value;
    }

    BeebConfigArguments arguments;

    if (dest.os_rom_type >= OSROMType_MultiOSBank0 && dest.os_rom_type <= OSROMType_MultiOSBank3) {
        arguments.multi_os_bank = dest.os_rom_type - OSROMType_MultiOSBank0;
    }

    if (!BeebLoadedConfig::Load(loaded_config, dest, arguments, logs)) {
        return false;
    }

    return true;
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
template <class RequestArgsType>
static void GetResetArguments(uint32_t *flags, double *osword_0_timeout_seconds, const RequestArgsType &request_args) {
    *flags = BeebThreadHardResetFlag_Run;
    *osword_0_timeout_seconds = BeebThread::HardResetMessage::DEFAULT_OSWORD_0_TIMEOUT_SECONDS;

    if (request_args.wait_for_osword_0) {
        *flags |= BeebThreadHardResetFlag_WaitForOSWORD0;
        *osword_0_timeout_seconds = request_args.wait_for_osword_0_timeout_seconds.value_or(API_DEFAULT_OSWORD_0_TIMEOUT_SECONDS);
    }

    if (request_args.boot) {
        *flags |= BeebThreadHardResetFlag_Boot;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApiExecuteConfig(const ApiExecuteArgs &execute_args,
                             ApiConfigArgs &&request_args,
                             std::function<void(const char *, std::nullptr_t &&)> completion_fun) {
    ASSERT(IsMainThread());

    BeebLoadedConfig loaded_config;
    if (!Load(&loaded_config, execute_args.beeb_window->api_globals, request_args, *execute_args.messages)) {
        completion_fun("load_failure", nullptr);
        return;
    }

    uint32_t flags;
    double osword_0_timeout_seconds;
    GetResetArguments(&flags, &osword_0_timeout_seconds, request_args);

    execute_args.beeb_thread->Send(std::make_shared<BeebThread::HardResetAndChangeConfigMessage>(std::move(loaded_config),
                                                                                                 flags,
                                                                                                 osword_0_timeout_seconds),
                                   [completion_fun,
                                    messages = execute_args.messages](const char *failure_reason, const char *failure_text) -> void {
                                       if (failure_text) {
                                           messages->e.f("%s failed: %s\n", API_REQUEST_TYPE_CONFIG, failure_text);
                                       }
                                       completion_fun(failure_reason, nullptr);
                                   });
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApiExecuteReset(const ApiExecuteArgs &execute_args,
                            ApiResetArgs &&request_args,
                            std::function<void(const char *, std::nullptr_t &&)> completion_fun) {
    uint32_t flags;
    double osword_0_timeout_seconds;
    GetResetArguments(&flags, &osword_0_timeout_seconds, request_args);

    execute_args.beeb_thread->Send(std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(flags,
                                                                                                 osword_0_timeout_seconds),
                                   [completion_fun,
                                    messages = execute_args.messages](const char *failure_reason, const char *failure_text) -> void {
                                       if (failure_text) {
                                           messages->e.f("%s failed: %s\n", API_REQUEST_TYPE_RESET, failure_text);
                                       }
                                       completion_fun(failure_reason, nullptr);
                                   });
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApiExecutePaste(const ApiExecuteArgs &execute_args,
                            ApiPasteArgs &&request_args,
                            std::function<void(const char *, std::nullptr_t &&)> completion_fun) {
    execute_args.beeb_thread->Send(std::make_shared<BeebThread::StopPasteMessage>());

    uint32_t flags = 0;
    double osword_0_timeout_seconds = BeebThread::StartPasteMessage::DEFAULT_OSWORD_0_TIMEOUT_SECONDS;

    if (request_args.wait_for_osword_0) {
        flags |= BeebThreadPasteFlag_WaitForOSWORD0;
        osword_0_timeout_seconds = request_args.wait_for_osword_0_timeout_seconds.value_or(API_DEFAULT_OSWORD_0_TIMEOUT_SECONDS);
    }

    execute_args.beeb_thread->Send(std::make_shared<BeebThread::StartPasteMessage>(std::move(request_args.input.bytes),
                                                                                   flags,
                                                                                   osword_0_timeout_seconds),
                                   [completion_fun,
                                    messages = execute_args.messages](const char *failure_reason, const char *failure_text) -> void {
                                       ASSERT(!!messages);
                                       if (failure_text) {
                                           messages->e.f("%s failed: %s\n", API_REQUEST_TYPE_PASTE, failure_text);
                                       }
                                       completion_fun(failure_reason, nullptr);
                                   });
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApiExecuteStartCaptureOSWRCH(const ApiExecuteArgs &execute_args,
                                         std::nullptr_t &&,
                                         std::function<void(const char *, std::nullptr_t &&)> completion_fun) {
    execute_args.beeb_window->StartCaptureOSWRCH();
    completion_fun(nullptr, nullptr);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApiExecuteStopCaptureOSWRCH(const ApiExecuteArgs &execute_args,
                                        std::nullptr_t &&,
                                        std::function<void(const char *, ApiStopCaptureOSWRCHResult &&)> completion_fun) {
    ApiStopCaptureOSWRCHResult result;
    if (!execute_args.beeb_window->StopCaptureOSWRCH(&result.output.bytes)) {
        execute_args.messages->e.f("Not capturing\n");
        completion_fun("not_capturing", {});
        return;
    }

    completion_fun(nullptr, std::move(result));
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

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
                                 std::function<void(const char *, ApiListValuesResult &&)> completion_fun) {
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
        execute_args.messages->e.f("unknown value: %s\n", request_args.name.c_str());
        completion_fun("unknown_value", {});
        return;
    }

    completion_fun(nullptr, std::move(result));
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApiExecuteSetGlobals(const ApiExecuteArgs &execute_args,
                                 ApiSetGlobalsArgs &&request_args,
                                 std::function<void(const char *, std::nullptr_t &&)> completion_fun) {
    if (request_args.read_path.has_value()) {
        if (!PathIsFullySpecified(*request_args.read_path)) {
            completion_fun("read_path not fully specified", {});
            return;
        }

        execute_args.beeb_window->api_globals.read_path = std::move(*request_args.read_path);
    }

    if (request_args.write_path.has_value()) {
        if (!PathIsFullySpecified(*request_args.write_path)) {
            completion_fun("write_path not fully specified", {});
            return;
        }

        execute_args.beeb_window->api_globals.write_path = std::move(*request_args.write_path);
    }

    completion_fun(nullptr, nullptr);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApiExecuteScreenGrabPNGData(const ApiExecuteArgs &execute_args,
                                        ApiScreenGrabPNGDataArgs &&request_args,
                                        std::function<void(const char *, ApiScreenGrabPNGDataResult &&)> completion_fun) {
    SDLUniquePtr<SDL_Surface> screenshot = execute_args.beeb_window->GetDisplayData(request_args.correct_aspect_ratio, *execute_args.messages);
    if (!screenshot) {
        completion_fun("screenshot_error", {});
        return;
    }

    ApiScreenGrabPNGDataResult result;
    if (!SaveSDLSurfaceToPNGData(&result.data.bytes, screenshot.get(), *execute_args.messages)) {
        completion_fun("screenshot_error", {});
        return;
    }

    completion_fun(nullptr, std::move(result));
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApiExecuteScreenGrabPNGFile(const ApiExecuteArgs &execute_args,
                                        ApiScreenGrabPNGFileArgs &&request_args,
                                        std::function<void(const char *, ApiScreenGrabPNGFileResult &&)> completion_fun) {
    SDLUniquePtr<SDL_Surface> screenshot = execute_args.beeb_window->GetDisplayData(request_args.correct_aspect_ratio, *execute_args.messages);
    if (!screenshot) {
        completion_fun("screenshot_error", {});
        return;
    }

    std::vector<uint8_t> png_data;
    if (!SaveSDLSurfaceToPNGData(&png_data, screenshot.get(), *execute_args.messages)) {
        completion_fun("screenshot_error", {});
        return;
    }

    std::string path;
    if (PathIsFullySpecified(request_args.path)) {
        path = request_args.path;
    } else {
        if (std::find_if(path.begin(), path.end(), &PathIsSeparatorChar) != path.end()) {
            execute_args.messages->e.f("Name includes path separator: %s\n", request_args.path.c_str());
            completion_fun("screenshot_error", {});
            return;
        }

        if (!execute_args.beeb_window->api_globals.write_path.has_value()) {
            execute_args.messages->e.f("API global write_path not set for name: %s\n", request_args.path.c_str());
            completion_fun("screenshot_error", {});
            return;
        }

        path = PathJoined(*execute_args.beeb_window->api_globals.write_path, request_args.path);
    }

    if (!SaveFile(png_data, path, execute_args.messages.get())) {
        completion_fun("screenshot_error", {});
        return;
    }

    ApiScreenGrabPNGFileResult result;
    result.path = std::move(path);
    completion_fun(nullptr, std::move(result));
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApiExecuteStartCountingBRKs(const ApiExecuteArgs &execute_args,
                                        std::nullptr_t &&,
                                        std::function<void(const char *, std::nullptr_t &&)> completion_fun) {
    execute_args.beeb_window->StartCountingBRKs();

    completion_fun(nullptr, {});
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApiExecuteStopCountingBRKs(const ApiExecuteArgs &execute_args,
                                       ApiStopCountingBRKsArgs &&request_args,
                                       std::function<void(const char *, std::nullptr_t &&)> completion_fun) {
    uint64_t num_brks;
    if (!execute_args.beeb_window->StopCountingBRKs(&num_brks)) {
        execute_args.messages->e.f("not currently counting BRKs");
        completion_fun("request_error", {});
        return;
    }

    if (request_args.expected_brk_count.has_value()) {
        if (*request_args.expected_brk_count != num_brks) {
            execute_args.messages->e.f("expected BRK count mismatch: expected %" PRIu64 ", got %" PRIu64, *request_args.expected_brk_count, num_brks);
            completion_fun("test_failed", {});
            return;
        }
    }

    completion_fun(nullptr, {});
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApiExecuteLoadDiskImage(const ApiExecuteArgs &execute_args,
                                    ApiLoadDiskImageArgs &&request_args,
                                    std::function<void(const char *, std::nullptr_t &&)> completion_fun) {
    if (request_args.drive < 0 || request_args.drive >= NUM_DRIVES) {
        execute_args.messages->e.f("Invalid drive: %d\n", request_args.drive);
        completion_fun("request_error", {});
        return;
    }

    std::string path;
    if (!GetFilePath(&path, request_args.path, execute_args.beeb_window->api_globals, *execute_args.messages)) {
        completion_fun("load_failed", {});
        return;
    }

    std::shared_ptr<MemoryDiscImage> disc_image = LoadMemoryDiscImage(path, *execute_args.messages);
    if (!disc_image) {
        completion_fun("load_failed", {});
        return;
    }

    execute_args.beeb_thread->Send(std::make_shared<BeebThread::LoadDiscMessage>(request_args.drive, std::move(disc_image), true),
                                   [messages = execute_args.messages,
                                    completion_fun](const char *failure_reason, const char *failure_text) -> void {
                                       if (failure_text) {
                                           messages->e.f("%s failed: %s\n", API_REQUEST_TYPE_LOAD_DISK_IMAGE, failure_text);
                                       }

                                       completion_fun(failure_reason, {});
                                   });
}
#endif

#if BBCMICRO_DEBUGGER
static void ApiExecutePeek(const ApiExecuteArgs &execute_args,
                           ApiPeekArgs &&request_args,
                           std::function<void(const char *, ApiPeekResult &&)> completion_fun) {
    if (request_args.end.has_value() == request_args.size.has_value()) {
        execute_args.messages->e.f("Must specify exactly one of size or value\n");
        completion_fun("request_error", {});
        return;
    }

    uint32_t end;
    if (request_args.end.has_value()) {
        end = *request_args.end;
        if (end < request_args.begin) {
            execute_args.messages->e.f("end must be >= begin\n");
            completion_fun("request_error", {});
            return;
        }
    } else {
        end = request_args.begin + *request_args.size;
    }

    uint32_t dso;
    std::shared_ptr<const BBCMicroType> type = execute_args.beeb_thread->GetBBCMicroType();
    if (!ParseAddressSuffix(&dso, type, request_args.suffix.c_str(), &execute_args.messages->e)) {
        completion_fun("request_error", {});
        return;
    }

    execute_args.beeb_thread->Send(std::make_unique<BeebThread::CallbackMessage>([begin = request_args.begin,
                                                                                  end,
                                                                                  dso,
                                                                                  mos = request_args.mos,
                                                                                  completion_fun](BBCMicro *m) -> void {
        ApiPeekResult result;
        result.data.bytes.resize(end - begin);
        m->DebugGetBytes(result.data.bytes.data(), result.data.bytes.size(), {(uint16_t)begin}, dso, mos);

        completion_fun(nullptr, std::move(result));
    }));
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
template <class ArgsType, class ResultType>
static void HandleApiExecute(const ApiExecuteArgs &execute_args,
                             const ApiRequest &request,
                             std::function<void(const char *, nlohmann::json)> completion_fun,
                             void (*execute_fn)(const ApiExecuteArgs &,
                                                ArgsType &&,
                                                std::function<void(const char *, ResultType &&)>),
                             bool requires_beeb_window) {
    if (requires_beeb_window) {
        if (!execute_args.beeb_window) {
            execute_args.messages->e.f("Must specify window\n");
            completion_fun("request_error", nullptr);
            return;
        }
    }

    std::string exc_what;
    ArgsType request_args;

    if (!LoadJSON(&request_args, request.args, &exc_what)) {
        execute_args.messages->e.f("Args parse failed: %s\n", exc_what.c_str());
        completion_fun("request_error", nullptr);
        return;
    }

    (*execute_fn)(execute_args,
                  std::move(request_args),
                  [completion_fun](const char *failure_reason, ResultType &&result) -> void {
                      completion_fun(failure_reason, std::move(result));
                  });
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

static void ExecuteSingleRequest(ApiExecuteArgs execute_args,
                                 ApiRequest request,
                                 std::function<void(const char *, nlohmann::json)> completion_fun) {
    if (request.type == API_REQUEST_TYPE_CONFIG) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteConfig, true);
    } else if (request.type == API_REQUEST_TYPE_PASTE) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecutePaste, true);
    } else if (request.type == API_REQUEST_TYPE_START_CAPTURE_OSWRCH) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteStartCaptureOSWRCH, true);
    } else if (request.type == API_REQUEST_TYPE_STOP_CAPTURE_OSWRCH) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteStopCaptureOSWRCH, true);
    } else if (request.type == API_REQUEST_TYPE_LIST_VALUES) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteListValues, false);
    } else if (request.type == API_REQUEST_TYPE_SET_GLOBALS) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteSetGlobals, true);
    } else if (request.type == API_REQUEST_TYPE_SCREEN_GRAB_PNG_DATA) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteScreenGrabPNGData, true);
    } else if (request.type == API_REQUEST_TYPE_SCREEN_GRAB_PNG_FILE) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteScreenGrabPNGFile, true);
    } else if (request.type == API_REQUEST_TYPE_START_COUNTING_BRKS) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteStartCountingBRKs, true);
    } else if (request.type == API_REQUEST_TYPE_STOP_COUNTING_BRKS) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteStopCountingBRKs, true);
    } else if (request.type == API_REQUEST_TYPE_LOAD_DISK_IMAGE) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteLoadDiskImage, true);
    } else if (request.type == API_REQUEST_TYPE_RESET) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecuteReset, true);
    } else if (request.type == API_REQUEST_TYPE_PEEK) {
        HandleApiExecute(execute_args, request, completion_fun, &ApiExecutePeek, true);
    } else {
        execute_args.messages->e.f("Unsupported request type: %s\n", request.type.c_str());
        completion_fun("request_error", nullptr);
    }
}

static std::shared_ptr<BeebThread> GetBeebThread(BeebWindow *beeb_window) {
    if (beeb_window) {
        return beeb_window->GetBeebThread();
    } else {
        return nullptr;
    }
}

static const char *GetMessagePrefix(const MessageList::Message *message) {
    switch (message->type) {
    default:
        ASSERT(false);
        return "?";

    case MessageType_Info:
        return "I";

    case MessageType_Warning:
        return "W";

    case MessageType_Error:
        return "E";
    }
}

static std::shared_ptr<Messages> CreateMessages() {
    return std::make_shared<Messages>(std::make_shared<MessageList>("API request"));
}

static ApiResponse GetApiResponse(const char *failure_reason, nlohmann::json j, const std::shared_ptr<Messages> &messages) {
    ApiResponse response;

    if (!failure_reason) {
        response.success = true;
        response.result = std::move(j);
    } else {
        response.success = false;

        ApiFailureResult result;

        result.reason = failure_reason;

        messages->i.Flush();
        messages->w.Flush();
        messages->e.Flush();

        std::shared_ptr<MessageList> message_list = messages->GetMessageList();
        message_list->ForEachMessage([&result](MessageList::Message *message) -> void {
            std::string str = GetMessagePrefix(message);
            str += ": ";
            str += message->text;

            result.messages.push_back(std::move(str));
        });

        response.result = std::move(result);
    }

    return response;
}

static void InitExecuteArgs(ApiExecuteArgs *execute_args, BeebWindow *beeb_window, std::shared_ptr<Messages> messages) {
    execute_args->beeb_window = beeb_window;
    execute_args->beeb_thread = GetBeebThread(execute_args->beeb_window);
    execute_args->messages = std::move(messages);
}

static void ExecuteSingleRequest(BeebWindow *beeb_window,
                                 std::shared_ptr<Messages> messages,
                                 ApiRequest request,
                                 std::function<void(ApiResponse &&)> completion_fun) {
    // since this is on the main thread, the BeebWindow is not going away (even if only not just quite yet).
    ASSERT(IsMainThread());

    ApiExecuteArgs execute_args;
    InitExecuteArgs(&execute_args, beeb_window, messages);

    ASSERT(!!execute_args.messages);

    ExecuteSingleRequest(std::move(execute_args),
                         std::move(request),
                         [messages, completion_fun](const char *failure_reason, nlohmann::json j) -> void {
                             ASSERT(!!messages);
                             completion_fun(GetApiResponse(failure_reason, std::move(j), messages));
                         });
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

struct MultipleRequestsState {
    BeebWindow *beeb_window = nullptr;
    std::weak_ptr<BeebThread> beeb_thread;
    std::shared_ptr<Messages> messages;
    ApiMultipleRequests request;
    ApiMultipleResponses response;
    size_t index = 0;
    std::function<void(ApiResponse)> request_completion_fun;
    std::function<void(ApiMultipleResponses)> overall_completion_fun;

    MultipleRequestsState() = default;
    ~MultipleRequestsState();
};

MultipleRequestsState::~MultipleRequestsState() {
    //printf("*** ~MultipleRequestsState\n");
}

static void CallOverallCompletionFun(const std::shared_ptr<MultipleRequestsState> &state) {
    // break the refcount cycle.
    state->request_completion_fun = nullptr;

    state->response.success = true;
    for (const ApiResponse &response : state->response.responses) {
        if (!response.success) {
            state->response.success = false;
            break;
        }
    }

    state->overall_completion_fun(std::move(state->response));
}

static void ExecuteNextRequest(const std::shared_ptr<MultipleRequestsState> &state) {
    ASSERT(IsMainThread());
    ASSERT(state->index <= state->request.requests.size());

    if (state->index == state->request.requests.size()) {
        CallOverallCompletionFun(state);
    } else {
        if (state->index == 0) {
            if (state->request.window.empty()) {
                state->beeb_window = BeebWindows::FindMRUBeebWindow();
                if (!state->beeb_window) {
                    state->messages->e.f("No recently used window\n");
                }
            } else {
                state->beeb_window = BeebWindows::FindBeebWindowByName(state->request.window);
                if (!state->beeb_window) {
                    state->messages->e.f("Window not found: %s\n", state->request.window.c_str());
                }
            }

            if (!state->beeb_window) {
                state->response.responses.push_back(GetApiResponse("window_not_found", nullptr, state->messages));
                CallOverallCompletionFun(state);
                return;
            }

            state->beeb_thread = state->beeb_window->GetBeebThread();
        } else {
            std::shared_ptr<BeebThread> beeb_thread = state->beeb_thread.lock();
            if (!beeb_thread || !beeb_thread->IsStarted()) {
                // Ugh. Have to abandon the whole thing.
                state->messages->e.f("Window has gone\n");
                state->response.responses.push_back(GetApiResponse("discarded", nullptr, state->messages));
                CallOverallCompletionFun(state);
                return;
            }
        }

        // TODO: could move the request? But that might end up a pain for debugging purposes.
        ExecuteSingleRequest(state->beeb_window,
                             state->messages,
                             state->request.requests[state->index],
                             state->request_completion_fun);
    }
}

static void HandleRequestCompletion(const std::shared_ptr<MultipleRequestsState> &state, ApiResponse &&response) {
    ASSERT(state->index < state->request.requests.size());

    state->response.responses.push_back(std::move(response));

    if (!state->response.responses.back().success) {
        // break out of the loop.
        state->index = state->request.requests.size();
    } else {
        // next request.
        ++state->index;
    }

    PushMainThreadMessage(std::make_unique<FunctionMessage>([state]() -> void {
        ExecuteNextRequest(state);
    }));
}

void ApiExecuteMultipleRequests(ApiMultipleRequests &&request,
                                std::function<void(ApiMultipleResponses &&)> completion_fun) {
    ASSERT(IsMainThread());

    if (request.requests.empty()) {
        completion_fun({});
        return;
    }

    auto state = std::make_shared<MultipleRequestsState>();

    //    state->beeb_window = runtime_args.beeb_window;
    //    state->beeb_thread = GetBeebThread(state->beeb_window);
    state->messages = CreateMessages();
    state->request = std::move(request);
    //state->response.responses.resize(state->request.requests.size());
    state->overall_completion_fun = std::move(completion_fun);

    // the capture of state introduces a refcount cycle, broken as part of CallOverallCompletionFun.
    state->request_completion_fun = [state](ApiResponse &&response) -> void {
        HandleRequestCompletion(state, std::move(response));
    };

    ExecuteNextRequest(state);
}

#endif

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
        {"screenshot", &HTTPMethodsHandler::HandleScreenshotRequest},
        {"api-set-globals", &HTTPMethodsHandler::HandleSetGlobalsRequest},
        {"api", &HTTPMethodsHandler::HandleGenericMultipleRequest},
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

    static bool ParseOptionalStdString(ParseArgsState *state, const std::string &str, void *context) {
        (void)state;

        *(std::optional<std::string> *)context = str;

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

    static bool ParseArgsOrSendResponse(HTTPServer *server, const HTTPRequest &request, const std::vector<std::string> &parts, size_t command_index) {
        return ParseArgsOrSendResponse2(server, request, parts, command_index, nullptr, 0, nullptr, 0);
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
            if (!BeebWindows::LoadConfigByName(&loaded_config, config_name, arguments, messages)) {
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
    void HandleScreenshotRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        const PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
        };
        ApiScreenGrabPNGDataArgs request_args;
        const QueryParameter qps[] = {
            {"correct_aspect_ratio", &ParseBool, &request_args.correct_aspect_ratio},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps, qps)) {
            return;
        }

        ApiExecuteArgs execute_args;
        InitExecuteArgs(&execute_args, beeb_window, CreateMessages());

        ApiExecuteScreenGrabPNGData(execute_args,
                                    std::move(request_args),
                                    [server,
                                     response_data = request.response_data,
                                     messages = execute_args.messages](const char *failure_reason, ApiScreenGrabPNGDataResult &&result) -> void {
                                        HTTPResponse response;

                                        if (failure_reason) {
                                            response = HTTPResponse::InternalServerError();

                                            response.content_type = HTTP_TEXT_CONTENT_TYPE;
                                            response.content_type_charset = HTTP_UTF8_CHARSET;

                                            std::string content_str = failure_reason;
                                            content_str += "\n";

                                            std::shared_ptr<MessageList> message_list = messages->GetMessageList();
                                            message_list->ForEachMessage([&content_str](MessageList::Message *message) -> void {
                                                content_str += GetMessagePrefix(message);
                                                content_str += ": ";
                                                content_str += message->text;
                                                content_str += "\n";
                                            });

                                            response.content.assign(content_str.begin(), content_str.end());
                                        } else {
                                            response = HTTPResponse::OK();

                                            response.content_type = "image/png";
                                            response.content = std::move(result.data.bytes);
                                        }
                                        server->SendResponse(response_data, std::move(response));
                                    });
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandleSetGlobalsRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        ApiSetGlobalsArgs request_args;
        const PathParameter pps[] = {
            {&ParseWindow, &beeb_window},
        };
        const QueryParameter qps[] = {
            {"read_path", &ParseOptionalStdString, &request_args.read_path},
            {"write_path", &ParseOptionalStdString, &request_args.write_path},
        };
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index, pps, qps)) {
            return;
        }

        ApiExecuteArgs execute_args;
        InitExecuteArgs(&execute_args, beeb_window, CreateMessages());

        bool completed = false;
        ApiExecuteSetGlobals(execute_args,
                             std::move(request_args),
                             [&completed](const char *failure_reason, std::nullptr_t &&) -> void {
                                 // early warning stuff, in case I change something later and forget to fix this bit.
                                 (void)failure_reason;
                                 ASSERT(!failure_reason);
                                 completed = true;
                             });
        ASSERT(completed);

        server->SendResponse(request, HTTPResponse::OK());
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
    void HandleGenericMultipleRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        // this doesn't take any URL or query args - but still do this, so that any supplied become an error.
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index)) {
            return;
        }

        nlohmann::json body_j;
        if (!this->GetJSONBodyOrSendResponse(&body_j, server, request)) {
            return;
        }

        ApiMultipleRequests api_request;
        std::string exc_what;
        if (!LoadJSON(&api_request, body_j, &exc_what)) {
            server->SendResponse(request, HTTPResponse::BadRequest("JSON parse error: %s", exc_what.c_str()));
            return;
        }

        ApiExecuteMultipleRequests(std::move(api_request),
                                   [response_data = request.response_data,
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
