#ifndef HEADER_BBD76FE1EE134F62B4A86BFA7901132C // -*- mode:c++ -*-
#define HEADER_BBD76FE1EE134F62B4A86BFA7901132C

#include "json.h"
#include <string>
#include "roms.h"
#include <beeb/type.h>
#include "BeebConfig.h"
#include <shared/enums.h>
#include <functional>
#include "Messages.h"
#include "b2.h"

class BeebWindow;
class BeebThread;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Structured JSON-based HTTP API, for use by automated tools.
//
// In the long run, the more ad-hoc shell-friendlier stuff will defer to this, in some documented fashion.
//
// Unlike most names in b2, these names have prefixes. This stuff may end up getting pulled out into a separate library.
//
// Ignore anything marked TODO:. These comments are for my benefit.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(T,...) means struct T is part of the JSON API. Only structs tagged this way are part of the API.
//
// C++ types used, and how they map to JSON.
//
// - std::string - JSON string
// - bool - JSON bool
// - uint8_t - JSON number, integer 0-255
// - uint16_t - JSON number, integer 0-65535
// - std::vector<T> - JSON array of T
// - nlohmann::json - JSON of any kind (probably depends on some other
// - Enum<T> - JSON string, the name of one of the enum values of T
// - std::variant<T0,T1...Tn> - JSON for either T0, or T1 - and so on
// - BBCString - JSON array of strings and numbers. See the BBCString struct

// If a field is std::optional<T>, its type is T (see above), and there is some specific handling when the field is absent.
//
// Otherwise, if the field is absent, it is treated as having its default value:
//
// - std::string - empty string
// - bool, uint8_t, uint16_t, Enum<T> - as noted
// - std::vector<T> - empty array
// - nlohmann::json - null

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// A BBC Micro string: a sequence of bytes in the BBC Micro character set.
//
// Such strings are represented in JSON as an array containing strings and numbers, representing the contents of the BBC Micro string, as follows:
//
// - number - value between 0-255, the byte value in question
// - string - BBC Micro chars, translated to/from PC character set as per the translation tables below:
//
// Other values (number <0; number >255; char not mentioned) are invalid.
//
// BBC->JSON character translation table:
//
// - BBC bytes 10, 13, and 32-126 inclusive are passed through as the corresponding Unicode codepoint
//
// Note that this means that BBC 96 (£) will end up in JSON as U++0060 ` GRAVE ACCENT.
//
// JSON->BBC character translation table:
//
// - Unicode codepoints 10, 13 and 32-126 inclusive are passed through as the corresponding byte value
// - Unicode U+00A3 £ POUND SIGN is converted to BBC 96
//
// Note that this means that U+0060 ` GRAVE ACCENT will end up on the BBC as BBC 96 (£).
// Note that this means there are two ways specify BBC 96 (£). This is deliberate.
//
// Further notes:
//
// - Numbers and strings are considered equivalent. As an example: JSON ["ABC"], JSON [65,"BC"] and JSON [65,"B",67] all represent the same BBC string: BBC "ABC"
// - This encoding is designed to be vaguely human readable and writeable assuming that the data is captured OSWRCH output or typeable text intended for OSRDCH paste
struct BBCString {
    std::vector<uint8_t> bytes;
};
void from_json(const nlohmann::json &j, BBCString &s);
void to_json(nlohmann::json &j, const BBCString &s);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ApiRequest {
    std::string type;

    nlohmann::json args;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiRequest, type, args);

struct ApiResponse {
    bool success = false;

    nlohmann::json result;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiResponse, success, result);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ApiMultipleRequests {
    std::vector<ApiRequest> requests;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiMultipleRequests, requests);

struct ApiMultipleResponses {
    std::vector<ApiResponse> responses;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiMultipleResponses, responses);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ApiROMContents {
    // The StandardROM to use, if any.
    Enum<StandardROM> standard_rom{StandardROM_None};

    // Path to ROM on disk, somewhere the target b2 can find it.
    std::string path;

    // TODO: ROM contents? base64 encoded?
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiROMContents, standard_rom, path);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ApiSidewaysROM {
    uint8_t bank = 0;

    ApiROMContents contents;

    bool writeable = false;

    Enum<ROMType> rom_type{ROMType_16KB};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiSidewaysROM, bank, contents, writeable, rom_type);

struct ApiOSROM {
    ApiROMContents contents;
    Enum<OSROMType> os_rom_type{OSROMType_16KB};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiOSROM, contents, os_rom_type);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// There's no BeebConfig/BeebLoadedConfig separation via the HTTP API. You specify the config, and a new running BBC with that config appears.

static const char API_CONFIG_REQUEST_TYPE[] = "config";

struct ApiConfigArgs {
    //
    std::string base_stock_config;
    std::string base_config;

    std::string name;

    std::optional<ApiOSROM> os_rom;

    std::vector<ApiSidewaysROM> sideways_roms;

    std::optional<std::string> disk_interface;

    std::optional<bool> video_nula;

    std::optional<bool> beeblink;

    std::vector<uint8_t> nvram;

    std::optional<bool> mouse;

    bool wait_for_osword_0 = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiConfigArgs,
                                                base_stock_config,
                                                base_config,
                                                name,
                                                os_rom,
                                                sideways_roms,
                                                disk_interface,
                                                video_nula,
                                                beeblink,
                                                nvram,
                                                mouse);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_PASTE_REQUEST_TYPE[] = "paste";

struct ApiPasteArgs {
    BBCString input;
    //bool wait_for_osword_0 = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiPasteArgs, input);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_START_CAPTURE_OSWRCH_REQUEST_TYPE[] = "start_capture_oswrch";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_STOP_CAPTURE_OSWRCH_REQUEST_TYPE[] = "stop_capture_oswrch";

struct ApiStopCaptureOSWRCHResult {
    BBCString output;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiStopCaptureOSWRCHResult, output);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Use the list_values command to list values of whatever sort.
//
// Values marked "(configurable)" are configurable. The set of names returned can vary.
// (TODO: is this even a good idea?)
//
// Other values are fixed, and will not change.

static const char API_LIST_VALUES_REQUEST_TYPE[] = "list_values";

struct ApiListValuesArgs {
    // One of:
    //
    // - "StandardROM" - list StandardROM enum values
    // - "OSROMType" - list OSROMType enum values
    // - "ROMType" - list ROMType enum values
    // - "stock_configs" - list stock config names, for possible use as base_stock_config for the config request type
    // - "configs" (configurable) - list config names, for possible use as base_config for the config request type
    std::string name;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiListValuesArgs, name);

struct ApiListValuesResult {
    std::vector<std::string> values;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiListValuesResult, values);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Common arguments supplied to ApiExecuteSingleRequest and ApiExecuteMultipleRequests.
//
// TODO: the naming here is not great
struct ApiRuntimeArgs {
    BeebWindow *beeb_window = nullptr;

    // Specially created for the handler of this message, along with its MessageList.
    std::shared_ptr<Messages> messages;
};

// Execute the given request. Must be called on the main thread.
//
// RUNTIME_ARGS is the runtime args.
//
// REQUEST is the request.
//
// COMPLETION_FUN is the function to call on success/failure. The first argument is the success flag, and the second, ignored on failure, is the JSON-serialized request result.
void ApiExecuteSingleRequest(const ApiRuntimeArgs &runtime_args,
                             ApiRequest request,
                             std::function<void(bool, nlohmann::json)> completion_fun);

void ApiExecuteMultipleRequests(const ApiRuntimeArgs &runtime_args,
                                ApiMultipleRequests request,
                                std::function<void(bool, nlohmann::json)> completion_fun);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
