#include <shared/system.h>
#include <shared/testing.h>
#include <shared/path.h>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>
#include <map>
#include <vector>
#include <memory>
#include <optional>
#include <type_traits>

#include <shared/enum_decl.h>
#include "test_nlohmann_json.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "test_nlohmann_json.inl"
#include <shared/enum_end.h>

// https://github.com/nlohmann/json/issues/975 - unique_ptr/shared_ptr

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
struct Enum {
    T value{};
};

template <class EnumType, class EnumBaseType>
static void HandleEnumToJSON(nlohmann::json &j, const EnumType *value, const char *enum_type_name, const char *(*get_name_fn)(EnumBaseType)) {
    (void)enum_type_name;

    const char *name = (*get_name_fn)(*value);
    j = name;
}

template <class EnumType, class EnumBaseType>
static void HandleEnumFromJSON(nlohmann::json &j, EnumType *value, const char *enum_type_name, const char *(*get_name_fn)(EnumBaseType)) {
    std::string str = j.get<std::string>();

    EnumBaseType i = 0;
    for (;;) {
        const char *name = (*get_name_fn)(i);
        if (name[0] == '?') {
            // Reached end of list.
            break;
        }

        if (str == name) {
            *value = (EnumType)i;
            return;
        }

        ++i;
    }

    throw nlohmann::json::type_error::create(302, std::string("unrecognised ") + enum_type_name, nullptr);
}

template <class T>
struct nlohmann::adl_serializer<Enum<T>> {
    static void to_json(nlohmann::json &j, const Enum<T> &value) {
        HandleEnumToJSON(j, &value.value, EnumTraits<T>::NAME, EnumTraits<T>::GET_NAME_FN);
    }

    static void from_json(nlohmann::json &j, Enum<T> &value) {
        HandleEnumFromJSON(j, &value.value, EnumTraits<T>::NAME, EnumTraits<T>::GET_NAME_FN);
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
struct FlagsEnum {
    T value{};
};

template <class EnumType, class EnumBaseType>
static void HandleFlagsEnumToJSON(nlohmann::json &j, const EnumType *value, const char *enum_type_name, const char *(*get_name_fn)(EnumBaseType)) {
    (void)enum_type_name;

    j = nlohmann::json::array_t{};
    for (typename std::make_unsigned<EnumBaseType>::type mask = 1; mask != 0; mask <<= 1) {
        const char *name = (*get_name_fn)((EnumBaseType)mask);
        if (name[0] == '?') {
            continue;
        }

        if (!(*value & (EnumBaseType)mask)) {
            continue;
        }

        j.push_back(name);
    }
}

template <class EnumType, class EnumBaseType>
static void HandleFlagsEnumFromJSON(nlohmann::json &j, EnumType *value, const char *enum_type_name, const char *(*get_name_fn)(EnumBaseType)) {
    if (!j.is_array()) {
        throw nlohmann::json::type_error::create(302, "not an array", nullptr);
    }

    for (size_t i = 0; i < j.size(); ++j) {
        if (!j[i].is_string()) {
            const std::string &j_name = j[i].get<std::string>();
            bool found = false;

            for (typename std::make_unsigned<EnumBaseType>::type mask = 1; mask != 0; mask <<= 1) {
                const char *name = (*get_name_fn)((EnumBaseType)mask);
                if (name[0] == '?') {
                    continue;
                }

                if (j_name == name) {
                    found = true;
                    *value |= (EnumBaseType)mask;
                    break;
                }
            }

            if (!found) {
                throw nlohmann::json::type_error::create(302, std::string("unrecognised ") + enum_type_name, nullptr);
            }
        }
    }
}

template <class T>
struct nlohmann::adl_serializer<FlagsEnum<T>> {
    static void to_json(nlohmann::json &j, const FlagsEnum<T> &value) {
        HandleFlagsEnumToJSON(j, &value.value, EnumTraits<T>::NAME, EnumTraits<T>::GET_NAME_FN);
    }

    static void from_json(nlohmann::json &j, FlagsEnum<T> &value) {
        HandleFlagsEnumFromJSON(j, &value.value, EnumTraits<T>::NAME, EnumTraits<T>::GET_NAME_FN);
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Trace {
    FlagsEnum<TraceOutputFlags> output_flags;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Trace, output_flags);

struct ROM {
    bool writeable = false;
    Enum<StandardROM> standard_rom{StandardROM_None};
    std::optional<std::string> file_name;
    Enum<ROMType> rom_type{ROMType_None};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ROM, writeable, standard_rom, file_name, rom_type);

struct Config {
    std::string name;
    std::optional<ROM> os;
    Enum<ROMType> os_rom_type{ROMType_None};
    std::optional<ROM> roms[16];
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Config, name, os, os_rom_type, roms);

struct Shortcut {
    bool Shift = false, Ctrl = false, Alt = false;
    std::string keycode;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Shortcut, Shift, Ctrl, Alt, keycode);

struct Root {
    // map window to map of command name to shortcuts for that command
    std::map<std::string, std::map<std::string, std::vector<Shortcut>>> shortcuts;
    std::vector<Config> configs;
    Trace trace;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Root, shortcuts, configs, trace);

static std::string PRINTF_LIKE(1, 2) strprintf(const char *fmt, ...) {
    char *tmp;

    va_list v;
    va_start(v, fmt);
    vasprintf(&tmp, fmt, v);

    std::string result = tmp;
    free(tmp), tmp = nullptr;

    return result;
}

int main() {
    printf("SRC_FOLDER: %s\n", SRC_FOLDER);
    printf("DEST_FOLDER: %s\n", DEST_FOLDER);

    PathCreateFolder(DEST_FOLDER);

    Root test;
    for (int w = 0; w < 2; ++w) {
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < 8; ++k) {
                Shortcut s;
                s.Shift = !!(k & 1);
                s.Ctrl = !!(k & 2);
                s.Alt = !!(k & 4);
                s.keycode = strprintf("key_%d", k);
                test.shortcuts[strprintf("win_%d", w)][strprintf("cmd_%d", c)].push_back(s);
            }
        }
    }

    for (int i = 0; i < 4; ++i) {
        Config c;
        c.name = strprintf("conf_%d", i);

        ROM romf;
        romf.standard_rom.value = StandardROM_BASIC2;

        ROM rome;
        rome.file_name = "other.rom";
        rome.rom_type.value = ROMType_Type1;

        ROM romd;
        romd.writeable = true;

        ROM os;
        os.standard_rom.value = StandardROM_OS12;

        c.os = os;
        c.roms[15] = romf;
        c.roms[14] = rome;
        c.roms[13] = romd;

        test.configs.push_back(std::move(c));
    }

    test.trace.output_flags.value = (TraceOutputFlags)(TraceOutputFlags_RegisterNames | TraceOutputFlags_ROMMapper);

    nlohmann::json test_j = test;
    std::string test_str = test_j.dump(4);
    fputs(test_str.c_str(), stdout);
}
