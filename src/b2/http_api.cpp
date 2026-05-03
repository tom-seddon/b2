#include <shared/system.h>
#include "http_api.h"
#include "misc.h"
#include <shared/strings.h>
#include <inttypes.h>

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
