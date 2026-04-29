#ifndef HEADER_BBD76FE1EE134F62B4A86BFA7901132C // -*- mode:c++ -*-
#define HEADER_BBD76FE1EE134F62B4A86BFA7901132C

#include "json.h"
#include <string>
#include "roms.h"
#include <beeb/type.h>
#include "BeebConfig.h"
#include <shared/enums.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Structured JSON-based HTTP API, for use by automated tools.
//
// In the long run, the more ad-hoc shell-friendlier stuff will defer to this,
// in some documented fashion.
//
// Unlike most names in b2, these names have prefixes. This stuff may end up
// getting pulled out into a separate library.
//
// Anything marked TODO: is for my benefit, and can be ignored.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(T,...) means struct T is part
// of the JSON API. Only structs tagged this way are part of the API.
//
// C++ types used, and how they map to JSON.
//
// - std::string - JSON string
// - bool - JSON bool
// - uint8_t - JSON number, integer 0-255
// - uint16_t - JSON number, integer 0-65535
// - std::vector<T> - JSON array of T
// - nlohmann::json - JSON of any kind (probably depends on some other
// - Enum<T> - JSON string, the name of one of the enum values of T, an enum
//   type from the b2 code. Use the list_values endpoint to list the valid JSON
//   values for the enum. Note that the valid JSON values exclude the prefix;
//   so, for example, for the StandardROM enum, StandardROM_None in C++ maps to
//   "None" in JSON.
// - std::variant<T0,T1...Tn> - JSON for either T0, or T1 - and so on
// - BBCString - JSON array of strings and numbers. See the BBCString struct

// If a field is std::optional<T>, its type is T (see above), and there is some
// specific handling when the field is absent.
//
// Otherwise, if the field is absent, it is treated as having its default value:
//
// - std::string - empty string
// - bool, uint8_t, uint16_t, Enum<T> - as noted
// - std::vector<T> - empty array
// - nlohmann::json - null

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// BBCString represents a BBC Micro string: a sequence of bytes in the BBC Micro
// character set.
//
// Such strings are represented in JSON as an array containing strings and
// numbers, representing the contents of the BBC Micro string, as follows:
//
// - number - value between 0-255, the byte value in question
// - string - BBC Micro chars, translated to/from PC character set as per the
//   translation tables below:
//
// Other values (number <0; number >255; char not mentioned) are invalid.
//
// BBC->JSON character translation table:
//
// - BBC bytes 10, 13, and 32-126 inclusive are passed through as the
//   corresponding Unicode codepoint
//
// Note that this means that BBC 96 (£) will end up in JSON as U++0060 ` GRAVE
// ACCENT.
//
// JSON->BBC character translation table:
//
// - Unicode codepoints 10, 13 and 32-126 inclusive are passed through as the
//   corresponding byte value
// - Unicode U+00A3 £ POUND SIGN is converted to BBC 96
//
// Note that this means that U+0060 ` GRAVE ACCENT will end up on the BBC as BBC
// 96 (£). Note that this means there are two ways specify BBC 96 (£). This is
// deliberate.
//
// Further notes:
//
// - Numbers and strings are considered equivalent. As an example: JSON ["ABC"],
//   JSON [65,"BC"] and JSON [65,"B",67] all represent the same BBC string: BBC
//   "ABC"
// - This encoding is designed to be vaguely human readable and writeable
//   assuming that the data is captured OSWRCH output or typeable text intended
//   for OSRDCH paste
struct BBCString {
    std::vector<uint8_t> bytes;
};
void from_json(const nlohmann::json &j, BBCString &s);
void to_json(nlohmann::json &j, const BBCString &s);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// BBCBinaryData represents binary data: a sequence of arbitrary bytes.
//
// The encoding is always base64. This is not really ideal, but everything supports it.
struct BBCBinaryData {
    std::vector<uint8_t> bytes;
};
void from_json(const nlohmann::json &j, BBCBinaryData &s);
void to_json(nlohmann::json &j, const BBCBinaryData &s);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// A few calls give you the option of waiting for the next OSWORD 0 (line input)
// call before continuing.
//
// The timeout is configurable, and this is the default value.
//
// Units are emulated seconds.
static constexpr double API_DEFAULT_OSWORD_0_TIMEOUT_SECONDS = 15.;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// A single API request.
struct ApiRequest {
    // The type of request. Use the value of the API_XXX_REQUEST_TYPE value, where XXX is the request type name in upper case snake_case format.
    std::string type;

    // The args for the request. Use the ApiXXXArgs struct, where XXX is the request type name in PascalCase format - or null if no such.
    nlohmann::json args;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiRequest, type, args);

// The response to a ApiRequest.
struct ApiResponse {
    // Success flag. True if the request succeeded; false if it didn't.
    bool success = false;

    // The result struct. If the request failed, this will be an ApiFailureResult; otherwise, this will be the ApiXXXResult struct, where XXX is the name of th request in PascalCase format.
    nlohmann::json result;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiResponse, success, result);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Batch of API requests.
//
// This is deliberately its own special thing, rather than a special type of ApiRequest.
struct ApiMultipleRequests {
    // The window to send the requests to. If not provided, pick the MRU window.
    std::string window;

    // The sequence of requests to make.
    std::vector<ApiRequest> requests;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiMultipleRequests, requests);

// The response to a ApiMultipleRequests.
struct ApiMultipleResponses {
    // Success flag for the multiple requests as a whole. True if all requests
    // succeeded.
    bool success = true;

    // The responses to the requests that were processed. There may be fewer
    // responses than requests; if a request fails, its response is included,
    // but the remaining requests are discarded.
    std::vector<ApiResponse> responses;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiMultipleResponses, responses);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Result struct for a request that failed.
struct ApiFailureResult {
    // Short string indicating reason for failure.
    std::string reason;

    // Any log messages that were printed during the execution, intended for human consumption.
    std::vector<std::string> messages;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiFailureResult, reason, messages);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Struct representing the contents of a ROM.
struct ApiROMContents {
    // The StandardROM to use, if any. If StandardROM_None, try the path.
    Enum<StandardROM> standard_rom{StandardROM_None};

    // Path to ROM on disk, somewhere the target b2 can find it, relative to the api path. If empty, assume the bank is empty.
    std::string path;

    // TODO: ROM contents? base64 encoded?
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiROMContents, standard_rom, path);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Struct representing the contents of a sideways ROM bank.
struct ApiSidewaysROM {
    // The bank to use. Must be 0-15 inclusive.
    uint8_t bank = 0;

    // The contents of the ROM.
    ApiROMContents contents;

    // If true, this bank is sideways RAM.
    bool writeable = false;

    // The ROM type. Only relevant if the OS is being loaded from disk; the standard ROMs are all 16 KB.
    Enum<ROMType> rom_type{ROMType_16KB};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiSidewaysROM, bank, contents, writeable, rom_type);

// Struct representing the contents of the OS ROM.
struct ApiOSROM {
    // The contents of the ROM.
    ApiROMContents contents;

    // The OS ROM type. Only relevant if the OS is being loaded from disk; the standard ROMs are all 16 KB.
    Enum<OSROMType> os_rom_type{OSROMType_16KB};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiOSROM, contents, os_rom_type);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// There's no BeebConfig/BeebLoadedConfig separation via the HTTP API. You
// specify the config, and a new running BBC with that config appears,
// corresponding to no entry on the hardware menu.

static const char API_CONFIG_REQUEST_TYPE[] = "config";

struct ApiConfigArgs {
    //
    std::string base_default_config;

    std::optional<ApiOSROM> os_rom;

    std::vector<ApiSidewaysROM> sideways_roms;

    std::optional<bool> video_nula;

    std::optional<bool> beeblink;

    std::vector<std::optional<uint8_t>> nvram;

    std::optional<bool> mouse;

    // If true, wait for the next OSWORD 0 call before reporting success or
    // failure.
    bool wait_for_osword_0 = false;

    // If provided, the number of (emulated) seconds to wait for the OSWORD 0
    // call when wait_for_osword_0. If the timeout is exceeded, the request
    // fails.
    //
    // If not provided, a default will be used.
    std::optional<double> wait_for_osword_0_timeout_seconds;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiConfigArgs,
                                                base_default_config,
                                                os_rom,
                                                sideways_roms,
                                                video_nula,
                                                beeblink,
                                                nvram,
                                                mouse,
                                                wait_for_osword_0,
                                                wait_for_osword_0_timeout_seconds);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_PASTE_REQUEST_TYPE[] = "paste";

struct ApiPasteArgs {
    BBCString input;

    // If true, after the last character is pasted, wait for the next OSWORD 0
    // call before reporting success or failure.
    bool wait_for_osword_0 = false;

    // If provided, the number of (emulated) seconds to wait for the OSWORD 0
    // call when wait_for_osword_0. If the timeout is exceeded, the request
    // fails.
    //
    // If not provided, a default will be used.
    std::optional<double> wait_for_osword_0_timeout_seconds;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiPasteArgs, input, wait_for_osword_0, wait_for_osword_0_timeout_seconds);

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
    // - "stock_configs" - list stock config names, for possible use as
    //   base_stock_config for the config request type
    // - "configs" (configurable) - list config names, for possible use as
    //   base_config for the config request type
    std::string name;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiListValuesArgs, name);

struct ApiListValuesResult {
    std::vector<std::string> values;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiListValuesResult, values);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_SET_GLOBALS_REQUEST_TYPE[] = "set_globals";

struct ApiSetGlobalsArgs {
    //
    std::optional<std::string> read_path;

    std::optional<std::string> write_path;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiSetGlobalsArgs,
                                                read_path,
                                                write_path);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_SCREEN_GRAB_PNG_DATA_REQUEST_TYPE[] = "screen_grab_png_data";

struct ApiScreenGrabPNGDataArgs {
    // Correct aspect ratio looks right, but bitmap mode pixels will contain artefacts due to being resized.
    bool correct_aspect_ratio = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiScreenGrabPNGDataArgs,
                                                correct_aspect_ratio);

struct ApiScreenGrabPNGDataResult {
    BBCBinaryData data;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiScreenGrabPNGDataResult,
                                                data);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_SCREEN_GRAB_PNG_FILE_REQUEST_TYPE[] = "screen_grab_png_file";

struct ApiScreenGrabPNGFileArgs {
    // Correct aspect ratio looks right, but bitmap mode pixels will contain artefacts due to being resized.
    bool correct_aspect_ratio = false;

    // Name of file to save to. If fully-specified, will be saved to the named
    // file. Otherwise, must be a file name (no path components) and will be
    // saved relative to the API write path.
    std::string path;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiScreenGrabPNGFileArgs,
                                                correct_aspect_ratio,
                                                path);

struct ApiScreenGrabPNGFileResult {
    // Path of file actually saved.
    std::string path;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiScreenGrabPNGFileResult,
                                                path);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Start tracking BRK instructions. There is some overhead, so this isn't the
// default - and, in any event, you might want to ignore them sometimes.

static const char API_START_TRACKING_BRKS[] = "start_tracking_brks";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Stop tracking BRK instructions.

static const char API_STOP_TRACKING_BRKS[] = "stop_tracking_brks";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Fail if any BRK instructions were executed between a start_tracking_brks and
// stop_tracking_brks.
//
// (May be called before start_tracking_brks or after stop_tracking_brks: in
// this case, it succeeds.)

static const char API_FAIL_IF_BRK_TRACKED[] = "fail_if_brk_tracked";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
