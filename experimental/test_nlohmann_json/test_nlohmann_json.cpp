#include <shared/system.h>
#include <shared/strings.h>
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
#include <nlohmann/json.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#include <shared/testing.h>
#include <shared/path.h>
#include <string>
#include <variant>
#include <map>
#include <vector>
#include <memory>
#include <optional>
#include <type_traits>
#include <SDL.h>
#include <shared/guid.h>
#include <shared/debug.h>

#include <shared/enum_decl.h>
#include "test_nlohmann_json.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "test_nlohmann_json.inl"
#include <shared/enum_end.h>

// https://github.com/nlohmann/json/issues/975 - unique_ptr/shared_ptr

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// void to_json(nlohmann::json &j, const Guid &guid){
//     char guid_str[GUID_STR_SIZE];
//     GetStringFromGuid(guid_str,guid);

//     j=guid_str;
// }

// void from_json(const nlohmann::json&j,Guid&guid){
//     std::string str=j.template get<std::string>();
//     if(!GetGuidFromString(&guid,str.c_str())){
//         throw nlohmann::json::type_error::create(302, std::string("bad GUID"), nullptr);
//     }
// }

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//template <class EnumType, class EnumBaseType>
//static void HandleEnumToJSON(nlohmann::json &j, const EnumType *value, const char *enum_type_name, const char *(*get_name_fn)(EnumBaseType)) {
//    (void)enum_type_name;
//
//    const char *name = (*get_name_fn)(*value);
//    j = name;
//}
//
//template <class EnumType, class EnumBaseType>
//static void HandleEnumFromJSON(const nlohmann::json &j, EnumType *value, const char *enum_type_name, const char *(*get_name_fn)(EnumBaseType)) {
//    (void)enum_type_name;
//
//    std::string str = j.get<std::string>();
//
//    EnumBaseType i = 0;
//    for (;;) {
//        const char *name = (*get_name_fn)(i);
//        if (name[0] == '?') {
//            // Reached end of list.
//            break;
//        }
//
//        if (str == name) {
//            *value = (EnumType)i;
//            return;
//        }
//
//        ++i;
//    }
//
//    //throw nlohmann::json::type_error::create(302, std::string("unrecognised ") + enum_type_name, nullptr);
//}
//
//template <class T>
//void to_json(nlohmann::json &j, const Enum<T> &value) {
//    HandleEnumToJSON(j, &value.value, EnumTraits<T>::NAME, EnumTraits<T>::GET_NAME_FN);
//}
//
//template <class T>
//void from_json(const nlohmann::json &j, Enum<T> &value) {
//    HandleEnumFromJSON(j, &value.value, EnumTraits<T>::NAME, EnumTraits<T>::GET_NAME_FN);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//template <class EnumType, class EnumBaseType>
//static void HandleFlagsEnumToJSON(nlohmann::json &j, const EnumType *value, const char *enum_type_name, const char *(*get_name_fn)(EnumBaseType)) {
//    (void)enum_type_name;
//
//    j = nlohmann::json::array_t{};
//    for (typename std::make_unsigned<EnumBaseType>::type mask = 1; mask != 0; mask <<= 1) {
//        const char *name = (*get_name_fn)((EnumBaseType)mask);
//        if (name[0] == '?') {
//            continue;
//        }
//
//        if (!(*value & (EnumBaseType)mask)) {
//            continue;
//        }
//
//        j.push_back(name);
//    }
//}
//
//template <class EnumType, class EnumBaseType>
//static void HandleFlagsEnumFromJSON(const nlohmann::json &j, EnumType *value, const char *enum_type_name, const char *(*get_name_fn)(EnumBaseType)) {
//    (void)enum_type_name;
//
//    if (!j.is_array()) {
//        // Should really warn here. But the options for passing logging stuff
//        // into the deserialization functions seem a bit limited.
//        *value = (EnumType)0;
//        //throw nlohmann::json::type_error::create(302, enum_type_name + std::string(" value must be an array"), nullptr);
//        return;
//    }
//
//    for (size_t i = 0; i < j.size(); ++i) {
//        if (!j[i].is_string()) {
//            const std::string &j_name = j[i].get<std::string>();
//            bool found = false;
//
//            for (typename std::make_unsigned<EnumBaseType>::type mask = 1; mask != 0; mask <<= 1) {
//                const char *name = (*get_name_fn)((EnumBaseType)mask);
//                if (name[0] == '?') {
//                    continue;
//                }
//
//                if (j_name == name) {
//                    found = true;
//                    *value = (EnumType)((EnumBaseType)*value | mask);
//                    break;
//                }
//            }
//
//            //if (!found) {
//            //    throw nlohmann::json::type_error::create(302, std::string("unrecognised ") + enum_type_name, nullptr);
//            //}
//        }
//    }
//}
//
//template <class T>
//void to_json(nlohmann::json &j, const EnumFlags<T> &value) {
//    HandleFlagsEnumToJSON(j, &value.value, EnumTraits<T>::NAME, EnumTraits<T>::GET_NAME_FN);
//}
//
//template <class T>
//void from_json(const nlohmann::json &j, EnumFlags<T> &value) {
//    HandleFlagsEnumFromJSON(j, &value.value, EnumTraits<T>::NAME, EnumTraits<T>::GET_NAME_FN);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Poorly-designed serialization format, easy to deal with using rapidjson but
// rather inconvenient with nlohmann::json...
struct KeymapKey {
    uint32_t keycode = 0;
    uint32_t scancode = 0;
};

static const char KEYCODE[] = "keycode";

static const uint32_t PCKeyModifier_All = PCKeyModifier_Shift | PCKeyModifier_Ctrl | PCKeyModifier_Alt | PCKeyModifier_Gui | PCKeyModifier_AltGr | PCKeyModifier_NumLock;

void to_json(nlohmann::json &j, const KeymapKey &value) {
    if (value.keycode != 0) {
        j = nlohmann::json{};
        for (uint32_t mask = PCKeyModifier_Begin;
             mask != PCKeyModifier_End;
             mask <<= 1) {
            if (value.keycode & mask) {
                j[GetPCKeyModifierEnumName((int)mask)] = true;
            }

            SDL_Keycode keycode = (SDL_Keycode)(value.keycode & ~PCKeyModifier_All);
            const char *key_name = SDL_GetKeyName(keycode);
            if (key_name) {
                j[KEYCODE] = key_name;
            } else {
                j[KEYCODE] = keycode;
            }
        }
    } else {
        const char *scancode_name = SDL_GetScancodeName((SDL_Scancode)value.scancode);
        if (scancode_name) {
            j = scancode_name;
        } else {
            j = value.scancode;
        }
    }
}

void from_json(const nlohmann::json &j, KeymapKey &value) {
    if (j.is_object()) {
        // Assume the worst... it'll be ignored if the effective keycode is
        // 0.
        value.keycode = 0;

        if (j.count(KEYCODE) > 0) {
            const nlohmann::json &j_keycode = j.at(KEYCODE);
            if (j_keycode.is_string()) {
                std::string keycode_name = j_keycode.template get<std::string>();
                value.keycode = (uint32_t)SDL_GetKeyFromName(keycode_name.c_str());
            } else if (j_keycode.is_number()) {
                value.keycode = (uint32_t)j_keycode.template get<std::uint32_t>();
            }
        }

        for (uint32_t mask = PCKeyModifier_Begin;
             mask != PCKeyModifier_End;
             mask <<= 1) {
            const char *mask_name = GetPCKeyModifierEnumName((int)mask);
            if (j.count(mask_name) > 0) {
                bool set = j.at(mask_name).template get<bool>();
                if (set) {
                    value.keycode |= mask;
                }
            }
        }
    } else if (j.is_number()) {
        value.scancode = j.template get<uint32_t>();
    } else if (j.is_string()) {
        std::string str = j.template get<std::string>();
        value.scancode = SDL_GetScancodeFromName(str.c_str());
    } else {
        value = {};
        //throw nlohmann::json::type_error::create(302, std::string("NewKeymapKey must be object, number or  string"), nullptr);
    }
}

struct NewKeymap {
    std::string name;
    bool keysyms = false;
    bool prefer_shortcuts = false;
    std::map<std::string, std::vector<KeymapKey>> keys;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NewKeymap, name, keysyms, prefer_shortcuts, keys);

struct Trace {
    EnumFlags<TraceOutputFlags> output_flags;
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
    std::vector<NewKeymap> new_keymaps;
    std::vector<NewKeymap> keymaps;
    std::string test_string;
    //char test_string[50] = {};
    std::vector<char> test_char_array;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Root, shortcuts, configs, trace, new_keymaps, keymaps, test_string, test_char_array);

struct OptionalTest2 {
    int x = 0, y = 0, z = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(OptionalTest2, x, y, z);

struct OptionalTest {
    std::optional<OptionalTest2> test;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(OptionalTest, test);

struct OptionalTest3 {
    int x = 0, y = 0, z = 0;
    std::string str = "fred";
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(OptionalTest3, x, y, z, str);

struct NonOptionalTest {
    OptionalTest3 test;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NonOptionalTest, test);

static std::string TestOptionalStuff2(const char *msg, const OptionalTest &value) {
    nlohmann::json j_value = value;
    std::string str = j_value.dump(4);
    printf("%s:\n---8<---\n%s\n---8<---\n", msg, str.c_str());
    return str;
}

static void TestOptionalStuff() {
    OptionalTest test1;
    std::string test1_str = TestOptionalStuff2("test1", test1);
    OptionalTest test2;
    test2.test = {1, 2, 3};
    std::string test2_str = TestOptionalStuff2("test2", test2);
    NonOptionalTest test3;
    try {
        nlohmann::json j = nlohmann::json::parse(test1_str);
        test3 = j.template get<NonOptionalTest>();
    } catch (nlohmann::json::exception &ex) {
        fprintf(stderr, "FATAL: error: %s\n", ex.what());
    }
    printf("%d %d %d %s\n", test3.test.x, test3.test.y, test3.test.z, test3.test.str.c_str());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void TestGuid() {
    nlohmann::json j_value = Guid{0x90, 0x08, 0x17, 0x67, 0xcc, 0xdb, 0x4b, 0xa7, 0xb3, 0x26, 0x64, 0x73, 0xf2, 0x19, 0x6b, 0x37};
    std::string str = j_value.dump(4);
    printf("GUID:\n---8<---\n%s\n---8<---\n", str.c_str());

    try {
        nlohmann::json j = nlohmann::json::parse(str);
        Guid guid = j.template get<Guid>();
        char guid_str[GUID_STR_SIZE];
        GetStringFromGuid(guid_str, guid);
        printf("Got Guid back: %s\n", guid_str);
    } catch (nlohmann::json::exception &ex) {
        fprintf(stderr, "FATAL: error: %s\n", ex.what());
    }

    std::map<Guid, std::string> test_map;

    Guid g0{0xb4, 0xe0, 0x9b, 0xd4, 0x13, 0xf7, 0x4e, 0x06, 0xad, 0x7f, 0x85, 0xc1, 0x2e, 0x95, 0xcf, 0x59};
    Guid g1{0x23, 0x3a, 0xa5, 0xbd, 0x33, 0xd8, 0x49, 0x44, 0x81, 0x12, 0x9b, 0x81, 0x92, 0xa1, 0xd0, 0x8a};
    Guid g2{0x2e, 0x6a, 0xf0, 0x0e, 0xb9, 0x04, 0x46, 0x14, 0xa9, 0xc5, 0x68, 0x8f, 0xc2, 0xdb, 0x05, 0x4e};

    test_map[g0] = "g0";
    test_map[g1] = "g1";
    test_map[g2] = "g2";

    nlohmann::json j_map = test_map;
    std::string j_map_str = j_map.dump(4);
    printf("GUID map:\n--8<--\n%s\n--8<--\n", j_map_str.c_str());

    try {
        nlohmann::json j = nlohmann::json::parse(j_map_str);
        std::map<Guid, std::string> test_map_2 = j.template get<std::map<Guid, std::string>>();
        printf("it worked...\n");
        TEST_EQ_UU(test_map.size(), test_map_2.size());
        auto &&ait = test_map.begin();
        auto &&bit = test_map_2.begin();
        while (ait != test_map.end()) {
            TEST_TRUE(ait->first == bit->first);
            TEST_EQ_SS(ait->second, bit->second);
            ++ait;
            ++bit;
        }
    } catch (nlohmann::json::exception &ex) {
        fprintf(stderr, "FATAL: error: %s\n", ex.what());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
static T LoadJSONString(const std::string &str, const char *func) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(str);
    } catch (const nlohmann::json::exception &exc) {
        TEST_FAIL("%s: json parse exception: %s", func, exc.what());
    }

    try {
        return j.get<T>();
    } catch (const nlohmann::json::exception &exc) {
        TEST_FAIL("%s: json get exception: %s", func, exc.what());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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

struct VariantTest2 {
    std::variant<uint8_t, std::string> value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(VariantTest2, value);

struct VariantTest1 {
    std::vector<std::variant<uint8_t, std::string>> bytes;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(VariantTest1, bytes);

static void TestVariant() {
    VariantTest1 test1;
    printf("%s: size=%zu\n", __func__, sizeof test1.bytes[0]);

    std::string test1_str = "{\"bytes\":[\"str\",5,6,\"str2\"]}";

    test1 = LoadJSONString<VariantTest1>(test1_str, __func__);

    TEST_EQ_UU(test1.bytes.size(), 4);

    const std::string *s;
    const uint8_t *u;

    s = std::get_if<std::string>(&test1.bytes[0]);
    TEST_NON_NULL(s);
    TEST_EQ_SS(*s, "str");

    u = std::get_if<uint8_t>(&test1.bytes[1]);
    TEST_NON_NULL(u);
    TEST_EQ_UU(*u, 5);

    u = std::get_if<uint8_t>(&test1.bytes[2]);
    TEST_NON_NULL(u);
    TEST_EQ_UU(*u, 6);

    s = std::get_if<std::string>(&test1.bytes[3]);
    TEST_NON_NULL(s);
    TEST_EQ_SS(*s, "str2");

    //    std::string test2_str = "{\"value\":5}";
    //    try {
    //        j = nlohmann::json::parse(test2_str);
    //    } catch (const nlohmann::json::exception &exc) {
    //        TEST_FAIL("json parse exception: %s\n", exc.what());
    //    }
    //
    //    VariantTest2 test2;
    //    try {
    //        test2 = j.get<VariantTest2>();
    //    } catch (const nlohmann::json::exception &exc) {
    //        TEST_FAIL("json parse exception: %s\n", exc.what());
    //    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct StringTest1 {
    std::string s;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(StringTest1, s);

static void TestString() {
    std::string test1_str = "{\"s\":\"\\u0000\"}";

    StringTest1 test1 = LoadJSONString<StringTest1>(test1_str, __func__);

    TEST_EQ_UU(test1.s.size(), 1);
    TEST_EQ_UU((unsigned char)test1.s[0], 0u);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    (void)argc, (void)argv;

    if (SDL_Init(0) != 0) {
        fprintf(stderr, "FATAL: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

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

    {
        NewKeymap k;

        k.keysyms = false;
        k.name = "test_keymap";
        k.prefer_shortcuts = false;
        k.keys["f0"].push_back({SDLK_UNKNOWN, SDL_SCANCODE_F10});
        k.keys["f1"].push_back({SDLK_F10 | (uint32_t)PCKeyModifier_Shift});

        test.keymaps.push_back(k);
    }

    //strcpy(test.test_string, "test_string");
    test.test_string = "test_string";

    for (char c : "test_char_array") {
        test.test_char_array.push_back(c);
    }

    nlohmann::json j_test = test;
    std::string serialize1 = j_test.dump(4);
    //fputs(serialize1.c_str(), stdout);

    Root test2;
    try {
        nlohmann::json j_deserialize1 = nlohmann::json::parse(serialize1);
        test2 = j_deserialize1.template get<Root>();
    } catch (nlohmann::json::exception &ex) {
        fprintf(stderr, "FATAL: exception: %s\n", ex.what());
        return 1;
    }

    nlohmann::json j_test2 = test2;
    std::string serialize2 = j_test.dump(4);
    TEST_EQ_SS(serialize1, serialize2);

    TestOptionalStuff();

    fputs(serialize2.c_str(), stdout);

    TestGuid();

    TestVariant();

    TestString();

    return 0;
}
