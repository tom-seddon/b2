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
#include <variant>

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

// Unicode to BBC string conversion rules:
//
// The API treats strings as sequences of Unicode codepoints. These are converted to BBC ASCII bytes as follows:
//
// - Unicode codepoints 32-126 inclusive are passed through as bytes, as-is. (The BBC Micro character set in this range is the same as Unicode, with one exception: BBC Micro byte 96 represents £. This means U+0060 ` GRAVE ACCENT will come through as £ at the BBC end.)
//
// - U+00A3 £ POUND SIGN is converted to 96
//
// - U+000A LINE FEED (LF) is passed through as 10
//
// - U+000D CARRIAGE RETURN (CR) is passed through as 13
//
// Other Unicode codepoints are rejected and will cause an error. Where it would be useful to be able to supply arbitrary BBC ASCII values, additional encoding schemes will be provided.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// BBC to Unicode string conversion rules:
//
// BBC output is considered to be a sequence of BBC ASCII bytes, converted to Unicode strings as follows:
//
// - BBC ASCII values 32-126 inclusive are passed through as-is. (The BBC Micro character set in this range is the same as Unicode, with one exception: BBC Micro byte 96 represents £. This means £ will come though as U+0060 ` GRAVE ACCENT.)
//
// - BBC ASCII 10 is passed through as U+000A LINE FEED (LF)
//
// - BBC ASCII 13 is passed through as U+000D CARRIAGE RETURN (CR)
//
// Other BBC ASCII values are rejected: which means they may get stripped out at source, or they may be encoded in some other fashion, depending on endpoint.

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
    // All the parts of the paste data are converted to BBC ASCII, and concatenated, to form the string that ultimately gets pasted.
    //
    // JSON strings are converted to a sequence of BBC ASCII bytes, as per the Unicode to BBC string conversion rules above.
    //
    // JSON numbers from 0-255 are converted to byte values.
    //
    // Other values are rejected.
    std::vector<std::variant<uint8_t, std::string>> parts;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiPasteArgs, parts);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_START_CAPTURE_OSWRCH_REQUEST_TYPE[] = "start_capture_oswrch";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_STOP_CAPTURE_OSWRCH_REQUEST_TYPE[] = "stop_capture_oswrch";

struct ApiStopCaptureOSWRCHResult {
    std::vector<std::variant<uint8_t, std::string>> parts;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiStopCaptureOSWRCHResult, parts);

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
