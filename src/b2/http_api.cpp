#include <shared/system.h>
#include "conf.h"
#include "http_api.h"

#if BBCMICRO_DEBUGGER

#include "misc.h"
#include <shared/strings.h>
#include <inttypes.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const ApiFailureReason API_FAILURE_REASON_LOAD_FAILED{ApiFailureReasonCode_LoadFailed, "file load failed"};
const ApiFailureReason API_FAILURE_REASON_NOT_VALID_WHEN_HALTED{ApiFailureReasonCode_NotValidWhenHalted, "not valid when halted"};
const ApiFailureReason API_FAILURE_REASON_NOT_VALID_WHEN_REPLAYING{ApiFailureReasonCode_NotValidWhenReplaying, "not valid when replaying"};
const ApiFailureReason API_FAILURE_REASON_DISCARDED{ApiFailureReasonCode_Discarded, "discarded"};
const ApiFailureReason API_FAILURE_REASON_PREPARE_FAILED{ApiFailureReasonCode_PrepareFailed, "message prepare failed"};
const ApiFailureReason API_FAILURE_REASON_NOT_CAPTURING{ApiFailureReasonCode_NotCapturing, "not currently capturing"};
const ApiFailureReason API_FAILURE_REASON_REQUEST_ERROR{ApiFailureReasonCode_RequestError, "request error"};
const ApiFailureReason API_FAILURE_REASON_SAVE_FAILED{ApiFailureReasonCode_SaveFailed, "file save failed"};
const ApiFailureReason API_FAILURE_REASON_CONDITION_FALSE{ApiFailureReasonCode_ConditionFalse, "the specified condition was false"};
const ApiFailureReason API_FAILURE_REASON_SCREEN_GRAB_FAILED{ApiFailureReasonCode_ScreenGrabFailed, "failed to get screen grab"};
const ApiFailureReason API_FAILURE_REASON_WINDOW_NOT_FOUND{ApiFailureReasonCode_WindowNotFound, "window not found"};
const ApiFailureReason API_FAILURE_REASON_TIMED_OUT{ApiFailureReasonCode_TimedOut, "timed out"};
const ApiFailureReason API_FAILURE_REASON_CANCELLED{ApiFailureReasonCode_Cancelled, "cancelled"};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void from_json(const nlohmann::json &j, ApiBBCString &s) {
    if (!j.is_array()) {
        throw nlohmann::json::type_error::create(302, strprintf("invalid ApiBBCString value: must be an array"), nullptr);
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
                    throw nlohmann::json::type_error::create(302, strprintf("invalid ApiBBCString value: element %zu not valid UTF-8", index), nullptr);
                } else {
                    throw nlohmann::json::type_error::create(302, strprintf("invalid ApiBBCString value: element %zu contains unsupported codepoint: %" PRId32 " (0x%" PRIx32 ")", index, bad_codepoint, bad_codepoint), nullptr);
                }
            }

            s.bytes.insert(s.bytes.end(), bbc.begin(), bbc.end());
        } else if (value_j.is_number_unsigned()) {
            uint64_t value = value_j.get<uint64_t>();
            if (value >= 256) {
                throw nlohmann::json::type_error::create(302, strprintf("invalid ApiBBCString value: element %zu not valid byte value", index), nullptr);
            }

            s.bytes.push_back((uint8_t)value);
        } else {
            throw nlohmann::json::type_error::create(302, strprintf("invalid ApiBBCString value: element %zu not string or byte", index), nullptr);
        }

        ++index;
    }
}

void to_json(nlohmann::json &j, const ApiBBCString &s) {
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

void from_json(const nlohmann::json &j, ApiBinaryData &s) {
    if (!j.is_string()) {
        throw nlohmann::json::type_error::create(302, strprintf("invalid ApiBinaryData value: must be a string"), nullptr);
    }

    const std::string &base64_str = j.template get_ref<const std::string &>();

    s.bytes.clear();
    if (!Base64Decode(&s.bytes, base64_str, nullptr)) {
        throw nlohmann::json::type_error::create(302, strprintf("invalid ApiBinaryData value: invalid base64 data"), nullptr);
    }
}

void to_json(nlohmann::json &j, const ApiBinaryData &s) {
    j = Base64Encode(s.bytes);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string ID_KEY{"id"};
static const std::string METHOD_KEY{"method"};
static const std::string PARAMS_KEY{"params"};

void from_json(const nlohmann::json &j, ApiRequest &r) {
    if (j.contains(ID_KEY)) {
        r.id = j[ID_KEY];
    }

    r.method = j[METHOD_KEY].template get<std::string>();

    if (j.contains(PARAMS_KEY)) {
        r.params = j[PARAMS_KEY];
    }
}

void to_json(nlohmann::json &j, const ApiRequest &r) {
    if (r.id.has_value()) {
        j[ID_KEY] = r.id;
    }

    j[METHOD_KEY] = r.method;

    if (r.params.has_value()) {
        j[PARAMS_KEY] = r.params;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string RESULT_KEY{"result"};
static const std::string ERROR_KEY{"error"};

void from_json(const nlohmann::json &j, ApiResponse &r) {
    if (j.contains(RESULT_KEY)) {
        r.result = j[RESULT_KEY];
    }

    if (j.contains(ERROR_KEY)) {
        if (r.result.has_value()) {
            throw nlohmann::json::type_error::create(302, strprintf("invalid ApiResponse value: has both %s and %s", RESULT_KEY.c_str(), ERROR_KEY.c_str()), nullptr);
        }

        r.error = j[ERROR_KEY];
    }
}

void to_json(nlohmann::json &j, const ApiResponse &r) {
    if (r.result.has_value()) {
        j[RESULT_KEY] = *r.result;
    }

    if (r.error.has_value()) {
        j[ERROR_KEY] = *r.error;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool WasSuccessful(const ApiResponse &r) {
    if (r.error.has_value()) {
        return false;
    } else {
        return true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool WasSuccessful(const ApiMultipleResponses &response) {
    for (const ApiResponse &r : response.responses) {
        if (!WasSuccessful(r)) {
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
