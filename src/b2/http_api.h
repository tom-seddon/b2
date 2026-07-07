#ifndef HEADER_BBD76FE1EE134F62B4A86BFA7901132C // -*- mode:c++ -*-
#define HEADER_BBD76FE1EE134F62B4A86BFA7901132C

#if BBCMICRO_DEBUGGER

#include "json.h"
#include <string>
#include "roms.h"
#include <beeb/type.h>
#include "BeebConfig.h"
#include <shared/enums.h>
#include "SymbolTable.h"

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
// - nlohmann::json - JSON of any kind (probably depending on some other value)
// - Enum<T> - JSON string, the name of one of the enum values of T, an enum
//   type from the b2 code. Use the list_values endpoint to list the valid JSON
//   values for the enum. (If looking at the C++ code to find names: note that the valid JSON values exclude the prefix;
//   so, for example, for the StandardROM enum, StandardROM_None in C++ maps to
//   "None" in JSON.
// - std::variant<T0,T1...Tn> - JSON for either T0, or T1 - and so on
// - BBCString - JSON array of strings and numbers. See the BBCString struct

// If the field is absent, it is treated as having its default value:
//
// - std::string - empty string
// - bool, uint8_t, uint16_t, Enum<T> - as noted
// - std::vector<T> - empty array
// - nlohmann::json - null

// If a field is std::optional<T>, its type is T (see above), but it doesn't
// have a default value, and there is some specific handling when the field is
// absent. (Which could be that its absence is an error! Don't read too much into the specific term "optional" - that's just what C++ calls this concept. An "optional" value
// could actually be mandatory.)

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
// - Unicode U+00A3 £ POUND SIGN is converted to BBC 96 (£)
//
// Note that this means that U+0060 ` GRAVE ACCENT will end up on the BBC as BBC
// 96 (£). Note that this means there are two ways to specify BBC 96 (£). This is
// deliberate.
//
// Aside from £, numbers and strings are considered equivalent. As an example: JSON ["ABC"],
// JSON [65,"BC"] and JSON [65,"B",67] all represent the same BBC string: BBC
// "ABC". (Regarding £: JSON ["£"] and JSON ["`"] both represent the same BBC string: BBC "£". But the £ translation only applies in strings, so: JSON [96] represents BBC "£", but JSON [163] represents BBC CHR$163.)
//
// Other notes:
//
// - This encoding is designed to be vaguely human readable and writeable
//   assuming that the data is captured OSWRCH output or typeable text intended
//   for OSRDCH paste
struct ApiBBCString {
    std::vector<uint8_t> bytes;
};
void from_json(const nlohmann::json &j, ApiBBCString &s);
void to_json(nlohmann::json &j, const ApiBBCString &s);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// ApiBinaryData binary data: a sequence of arbitrary bytes.
//
// The encoding is always base64. This is not really ideal, but everything supports it.
struct ApiBinaryData {
    std::vector<uint8_t> bytes;
};
void from_json(const nlohmann::json &j, ApiBinaryData &s);
void to_json(nlohmann::json &j, const ApiBinaryData &s);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// ApiKeyAndValue: a key/value pair.
struct ApiKeyAndValue {
    std::string key;
    nlohmann::json value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiKeyAndValue, key, value);

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
    // The type of request. Use the value of the API_REQUEST_TYPE_XXX value, where XXX is the request type name in upper case snake_case format.
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

// Updated NVRAM byte.
struct ApiConfigNVRAMByte {
    // Offset in NVRAM.
    int index = -1;

    // AND value for byte.
    uint8_t mask = 0;

    // OR value for byte.
    uint8_t value = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiConfigNVRAMByte,
                                                index,
                                                mask,
                                                value);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Specify a new BBC config.

static const char API_REQUEST_TYPE_CONFIG[] = "config";

struct ApiConfigArgs {
    std::string base_default_config;

    std::optional<ApiOSROM> os_rom;

    std::vector<ApiSidewaysROM> sideways_roms;

    std::optional<bool> video_nula;

    std::optional<bool> beeblink;

    std::optional<bool> extra_debugging_hardware;

    std::vector<ApiConfigNVRAMByte> nvram_bytes;

    bool boot = false;

    // If true, wait for the next OSWORD 0 call before the request completes.
    bool wait_for_osword_0 = false;

    // If wait_for_osword_0 is true: the number of (emulated) seconds to wait for the OSWORD 0
    // call to be made. If the timeout is exceeded, the request
    // fails. If not provided, a default will be used; if the value is <=0, no timeout, and the emulator will wait indefinitely.
    //
    // If wait_for_osword_0 is false: ignored.
    std::optional<double> wait_for_osword_0_timeout_seconds;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiConfigArgs,
                                                base_default_config,
                                                os_rom,
                                                sideways_roms,
                                                video_nula,
                                                beeblink,
                                                extra_debugging_hardware,
                                                nvram_bytes,
                                                boot,
                                                wait_for_osword_0,
                                                wait_for_osword_0_timeout_seconds);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_REQUEST_TYPE_RESET[] = "reset";

struct ApiResetArgs {
    bool boot = false;
    bool wait_for_osword_0 = false;
    std::optional<double> wait_for_osword_0_timeout_seconds;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiResetArgs,
                                                boot,
                                                wait_for_osword_0,
                                                wait_for_osword_0_timeout_seconds);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_REQUEST_TYPE_PASTE[] = "paste";

struct ApiPasteArgs {
    ApiBBCString input;

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

static const char API_REQUEST_TYPE_START_CAPTURE_OSWRCH[] = "start_capture_oswrch";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_REQUEST_TYPE_STOP_CAPTURE_OSWRCH[] = "stop_capture_oswrch";

struct ApiStopCaptureOSWRCHResult {
    ApiBBCString output;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiStopCaptureOSWRCHResult, output);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Use the list_values command to list values of whatever sort.
//
// The set of values of may grow in future versions of b2, but the intent is that existing values will remain valid.

static const char API_REQUEST_TYPE_LIST_VALUES[] = "list_values";

struct ApiListValuesArgs {
    // One of the b2 enum types:
    //
    // - "StandardROM" - list StandardROM enum values
    // - "OSROMType" - list OSROMType enum values
    // - "ROMType" - list ROMType enum values
    //
    // Or, one of b2's internal lists of things:
    //
    // - "default_configs" - list stock config names, for possible use as
    //   base_default_config for the config request type
    // - "symbol_format_name" - list types of symbol parser
    std::string name;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiListValuesArgs, name);

struct ApiListValuesResult {
    std::vector<std::string> values;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiListValuesResult, values);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Use the list_keys_and_values command to list key/value pairs of whatever sort.
//
// The set may grow in future versions of b2, but the intent is that existing key/value pairs will remain valid.

static const char API_REQUEST_TYPE_LIST_KEYS_AND_VALUES[] = "list_keys_and_values";

struct ApiListKeysAndValuesArgs {
    // One of the b2 enum types:
    //
    // - "DebugCommand" - list names, and corresponding values that can be written to the debug command ports
    std::string name;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiListKeysAndValuesArgs, name);

struct ApiListKeysAndValuesResult {
    std::vector<ApiKeyAndValue> keys_and_values;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiListKeysAndValuesResult, keys_and_values);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_REQUEST_TYPE_SET_GLOBALS[] = "set_globals";

struct ApiSetGlobalsArgs {
    // Specify the read path. Relative names of files to read from are assumed to be relative to the read path. There is no attempt to provide any kind of sandboxing, and you can easily use .. to access paths above the specified read path.
    std::optional<std::string> read_path;

    // Specify the write path. Names of files to write to are assumed to be relative to the write path. Since writes are destructive, unlike the read path, there is some very basic attempt at sandboxing: names with path separators are not permitted. Any files written to are written directly into the specified folder.
    std::optional<std::string> write_path;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiSetGlobalsArgs,
                                                read_path,
                                                write_path);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_REQUEST_TYPE_SCREEN_GRAB_PNG_DATA[] = "screen_grab_png_data";

struct ApiScreenGrabPNGDataArgs {
    // If true, output will be resized (possibly with filtering) to match the aspect ratio of output from a real BBC Micro.
    bool correct_aspect_ratio = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiScreenGrabPNGDataArgs,
                                                correct_aspect_ratio);

struct ApiScreenGrabPNGDataResult {
    ApiBinaryData data;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiScreenGrabPNGDataResult,
                                                data);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_REQUEST_TYPE_SCREEN_GRAB_PNG_FILE[] = "screen_grab_png_file";

struct ApiScreenGrabPNGFileArgs {
    // If true, output will be resized (possibly with filtering) to match the aspect ratio of output from a real BBC Micro.
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

// Start counting BRK instructions. There is some overhead, so this isn't the
// default - and, in any event, you might want to ignore them sometimes.
//
// The counter starts at 0.

static const char API_REQUEST_TYPE_START_COUNTING_BRKS[] = "start_counting_brks";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Stop counting BRK instructions.

static const char API_REQUEST_TYPE_STOP_COUNTING_BRKS[] = "stop_counting_brks";

struct ApiStopCountingBRKsArgs {
    // If not supplied, the request always succeeds.
    //
    // Otherwise, specifies expected number of BRKs counted. The request will fail
    // if the actual number counted is different.
    std::optional<uint64_t> expected_brk_count;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiStopCountingBRKsArgs,
                                                expected_brk_count);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_REQUEST_TYPE_LOAD_DISK_IMAGE[] = "load_disk_image";

struct ApiLoadDiskImageArgs {
    std::string path;
    int drive = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiLoadDiskImageArgs,
                                                path,
                                                drive);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_REQUEST_TYPE_PEEK[] = "peek";

struct ApiPeekArgs {
    uint32_t begin = 0;
    std::optional<uint32_t> end;
    std::optional<uint32_t> size;
    std::string suffix;
    bool mos = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiPeekArgs,
                                                begin,
                                                end,
                                                size,
                                                suffix,
                                                mos);

struct ApiPeekResult {
    ApiBinaryData data;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiPeekResult,
                                                data);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static const char API_REQUEST_TYPE_WAIT_FOR_EVENT[] = "wait_for_event";
//
//struct ApiWaitForEventArgs {
//    std::optional<uint8_t> event;
//    std::optional<double> timeout_seconds;
//};
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiWaitForEventArgs,
//                                                event,
//                                                timeout_seconds);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_REQUEST_CLEAR_SYMBOLS[] = "clear_symbols";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char API_REQUEST_LOAD_SYMBOLS[] = "load_symbols";

struct ApiLoadSymbolsArgs {
    std::string path;
    std::string format_name;
    std::vector<std::string> suffixes;
    Enum<SymbolFileAddressSuffixMode> suffix_mode = SymbolFileAddressSuffixMode_Exclusive;
    uint8_t group = 0;

    // If no group name supplied, the group's existing name is retained.
    //
    // If the group name is "", the existing name is updated to include the
    // supplied path.
    //
    // Otherwise, the new group name replaces the old one.
    //
    // (There's not yet a separate way to set the group name.)
    std::optional<std::string> group_name;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiLoadSymbolsArgs,
                                                path,
                                                format_name,
                                                suffixes,
                                                suffix_mode,
                                                group,
                                                group_name);

struct ApiLoadSymbolsResult {
    size_t file_index = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiLoadSymbolsResult,
                                                file_index);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
