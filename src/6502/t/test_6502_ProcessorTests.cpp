#include <shared/system.h>
#include <shared/debug.h>
#include <shared/path.h>
#include <shared/testing.h>
#include <algorithm>
#include <stdio.h>
#include <string>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <6502/6502.h>

#define PRIsizetype "u"
CHECK_SIZEOF(rapidjson::SizeType, sizeof(unsigned));

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<std::string> FindJSONFiles(const std::string &folder_path) {
    std::vector<std::string> file_paths;
    PathGlob(folder_path,
             [&file_paths](const std::string &path, bool is_folder) {
                 if (!is_folder) {
                     file_paths.push_back(path);
                 }
             });

    std::sort(file_paths.begin(), file_paths.end());
    return file_paths;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void NORETURN Fatal(const char *fmt, ...) {
    fprintf(stderr, "FATAL: ");

    va_list v;
    va_start(v, fmt);
    vfprintf(stderr, fmt, v);
    va_end(v);

    fprintf(stderr, "\n");

    exit(1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct RAMByte {
    uint16_t addr = 0;
    uint8_t value = 0;
};

struct Cycle {
    uint16_t addr = 0;
    uint8_t value = 0;
    bool read = false;
};

struct ProcessorTestState {
    uint16_t pc = 0;
    uint8_t s = 0, a = 0, x = 0, y = 0, p = 0;
    std::vector<RAMByte> ram;
};

static uint64_t LoadUn(const rapidjson::Value &test_object, const char *key, uint64_t mini, uint64_t maxi) {
    rapidjson::Value::ConstMemberIterator it = test_object.FindMember(key);
    TEST_TRUE(it != test_object.MemberEnd());
    TEST_TRUE(it->value.IsNumber());

    uint64_t value = it->value.GetUint64();
    TEST_GE_UU(value, mini);
    TEST_LE_UU(value, maxi);

    return value;
}

static uint8_t LoadU8(const rapidjson::Value &test_object, const char *key) {
    return (uint8_t)LoadUn(test_object, key, 0, 255);
}

static uint16_t LoadU16(const rapidjson::Value &test_object, const char *key) {
    return (uint16_t)LoadUn(test_object, key, 0, 65535);
}

static ProcessorTestState LoadProcessorTestState(const rapidjson::Value &test_object, const char *key) {
    rapidjson::Value::ConstMemberIterator it = test_object.FindMember(key);
    TEST_TRUE(it != test_object.MemberEnd());
    TEST_TRUE(it->value.IsObject());

    ProcessorTestState pts;
    pts.pc = LoadU16(it->value, "pc");
    pts.s = LoadU8(it->value, "s");
    pts.a = LoadU8(it->value, "a");
    pts.x = LoadU8(it->value, "x");
    pts.y = LoadU8(it->value, "y");
    pts.p = LoadU8(it->value, "p");

    rapidjson::Value::ConstMemberIterator ram_it = it->value.FindMember("ram");
    TEST_TRUE(ram_it != it->value.MemberEnd());
    TEST_TRUE(ram_it->value.IsArray());

    rapidjson::Value::ConstArray ram = ram_it->value.GetArray();
    for (rapidjson::SizeType i = 0; i < ram.Size(); ++i) {
        TEST_TRUE(ram[i].IsArray());
        rapidjson::Value::ConstArray byte = ram[i].GetArray();
        TEST_EQ_UU(byte.Size(), 2);

        TEST_TRUE(byte[0].IsNumber());
        uint64_t addr = byte[0].GetUint64();
        TEST_GE_UU(addr, 0);
        TEST_LE_UU(addr, 65535);

        TEST_TRUE(byte[1].IsNumber());
        uint64_t value = byte[1].GetUint64();
        TEST_GE_UU(value, 0);
        TEST_LE_UU(value, 255);

        pts.ram.push_back({(uint16_t)addr, (uint8_t)value});
    }

    return pts;
}

//static std::string GetVisual6502URL(const ProcessorTestState &pts, const uint64_t *num_cycles) {
//    std::string url = "file:///" VISUAL6502_PATH "/expert.html/graphics=f&loglevel=2&logmore=clk0";
//
//    if (num_cycles) {
//        url += "&steps=" + std::to_string(*num_cycles);
//    }
//
//    for (const RAMByte &byte : pts.ram) {
//        url += strprintf("&a=%04x&d=%02x", byte.addr, byte.value);
//    }
//
//
//} 

static void Test6502(const std::string &processor_tests_path) {
    std::vector<std::string> jsons = FindJSONFiles(PathJoined(PROCESSOR_TESTS_FOLDER, processor_tests_path));
    if (jsons.empty()) {
        Fatal("no JSON files found in path: %s", processor_tests_path.c_str());
    }

    for (size_t json_idx = 0; json_idx < jsons.size(); ++json_idx) {
        printf("%zu. %s...\n", json_idx, jsons[json_idx].c_str());

        std::vector<char> contents;
        if (!PathLoadTextFile(&contents, jsons[json_idx])) {
            Fatal("failed to load JSON file");
        }

        contents.push_back(0);

        rapidjson::Document doc;
        doc.ParseInsitu(contents.data());

        if (doc.HasParseError()) {
            Fatal("JSON error: +%zu: %s", doc.GetErrorOffset(), rapidjson::GetParseError_En(doc.GetParseError()));
        }

        TEST_TRUE(doc.IsArray());

        rapidjson::Value::Array jarray = doc.GetArray();
        for (rapidjson::SizeType test_idx = 0; test_idx < jarray.Size(); ++test_idx) {
            const rapidjson::Value &jtest_object = jarray[test_idx];
            TEST_TRUE(jtest_object.IsObject());

            rapidjson::Value::ConstMemberIterator jname_it = jtest_object.FindMember("name");
            TEST_TRUE(jname_it != jtest_object.MemberEnd());
            TEST_TRUE(jname_it->value.IsString());

            std::string name = jname_it->value.GetString();

            ProcessorTestState initial = LoadProcessorTestState(jtest_object, "initial");
            ProcessorTestState final = LoadProcessorTestState(jtest_object, "final");

            std::vector<Cycle> cycles;
            rapidjson::Value::ConstMemberIterator jcycles_it = jtest_object.FindMember("cycles");
            TEST_TRUE(jcycles_it != jtest_object.MemberEnd());
            rapidjson::Value::ConstArray jcycles = jcycles_it->value.GetArray();
            for (rapidjson::SizeType i = 0; i < jcycles.Size(); ++i) {
                TEST_TRUE(jcycles[i].IsArray());
                rapidjson::Value::ConstArray jcycle = jcycles[i].GetArray();
                TEST_EQ_UU(jcycle.Size(), 3);

                TEST_TRUE(jcycle[0].IsNumber());
                uint64_t addr = jcycle[0].GetUint64();
                TEST_GE_UU(addr, 0);
                TEST_LE_UU(addr, 65535);

                TEST_TRUE(jcycle[1].IsNumber());
                uint64_t value = jcycle[1].GetUint64();
                TEST_GE_UU(value, 0);
                TEST_LE_UU(value, 255);

                TEST_TRUE(jcycle[2].IsString());
                const char *type = jcycle[2].GetString();
                TEST_TRUE(strcmp(type, "read") == 0 || strcmp(type, "write") == 0);

                Cycle cycle = {(uint16_t)addr, (uint8_t)value, strcmp(type, "read") == 0};
                cycles.push_back(cycle);
            }

            uint8_t ram[65536] = {};
            for (const RAMByte &byte : initial.ram) {
                ram[byte.addr] = byte.value;
            }

            M6502 cpu;
            M6502_Init(&cpu, &M6502_nmos6502_config);

            cpu.a = initial.a;
            cpu.x = initial.x;
            cpu.y = initial.y;
            cpu.s.b.l = initial.s;
            cpu.pc.w = initial.pc;
            M6502_SetP(&cpu, initial.p);

            cpu.tfn = &M6502_NextInstruction;

            for (size_t i = 0; i < cycles.size(); ++i) {
                const Cycle &cycle = cycles[i];

                (*cpu.tfn)(&cpu);

                TEST_EQ_II(!!cpu.read, cycle.read);
                TEST_EQ_UU(cpu.abus.w, cycle.addr);

                if (cpu.read) {
                    cpu.dbus = ram[cpu.abus.w];
                } else {
                    ram[cpu.abus.w] = cpu.dbus;
                }

                TEST_EQ_UU(cpu.dbus, cycle.value);
            }

            (*cpu.tfn)(&cpu);

            TEST_EQ_UU(cpu.a, final.a);
            TEST_EQ_UU(cpu.x, final.x);
            TEST_EQ_UU(cpu.y, final.y);
            TEST_EQ_UU(cpu.s.b.l, final.s);
            TEST_EQ_UU(cpu.pc.w - 1, final.pc);

            M6502P p = M6502_GetP(&cpu);
            p.bits._ = 1;
            p.bits.b = 0;

            TEST_EQ_UU(p.value, final.p);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main() {
    Test6502("6502/v1");
}
