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

class BeebWindow;
class BeebThread;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Structured JSON-based HTTP API, for use by automated tools.
//
// In the long run, the more ad-hoc shell-friendlier stuff will defer to this, in some documented fashion.
//
// Unlike most names in b2, these names have prefixes. This stuff may end up getting pulled out into a separate library.

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

struct ApiRuntimeArgs {
    // The BeebWindow of interest.
    BeebWindow *beeb_window = nullptr;

    // Result of beeb_window->GetBeebThread().
    std::shared_ptr<BeebThread> beeb_thread;

    // Specially created for the handler of this message, along with its MessageList.
    std::shared_ptr<Messages> messages;
};

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
    int bank = 0;

    ApiROMContents contents;

    bool writeable = false;

    Enum<ROMType> rom_type{ROMType_16KB};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApiSidewaysROM, bank, contents, writeable, rom_type);

struct ApiOSROM {
    ApiROMContents contents;
    OSROMType os_rom_type{OSROMType_16KB};
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

// Execute the given request.
//
// RUNTIME_ARGS is the runtime args.
//
// REQUEST is the request.
//
// COMPLETION_FUN is the function to call on success/failure. The first argument is the success flag, and the second, ignored on failure, is the JSON-serialized request result.
void ApiExecute(const ApiRuntimeArgs &runtime_args, const ApiRequest &request, std::function<void(bool, nlohmann::json)> completion_fun);

void ApiExecute(const ApiRuntimeArgs &runtime_args, ApiMultipleRequests requests, std::function<void(bool, nlohmann::json)> completion_fun);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
