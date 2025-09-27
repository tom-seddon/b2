#include <shared/system.h>
#include <shared/log.h>
#include <shared/testing.h>
#include <shared/debug.h>
#include <shared/CommandLineParser.h>
#include <6502/6502.h>
#include <string.h>
#include <vector>
#include <string>
#include <regex>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(TEST, "TEST", &log_printer_stdout);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    bool help = false;
    bool list_for_check_ctest_log = false;
    std::vector<std::regex> test_include_regexes;
    std::vector<std::regex> test_exclude_regexes;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct State {
    uint8_t a, x, y, s, p, operand;
};
static_assert(sizeof(State) == 6, "");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// One of the HLTs - not tested by these tests.
static const uint8_t HACK_OPCODE = 0x12;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

enum Callback {
    Callback_Start,
    Callback_TestBegin,
    Callback_TestFail,
    Callback_TestPass,
    Callback_TestEnd,
    Callback_Finish,
    Callback_PrintChar,

    Callback_Count
};

static const size_t CALLBACK_SIZE = 8;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint16_t LOAD_ADDR = 0x2000;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The tests don't use IRQs, but the IRQ vector points somewhere so that
// unexpected IRQs can be detected. (e.g., due to a wayward BRK...)
static const uint16_t IRQ_ADDR = 0xf000;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct TestState {
    uint32_t num_tests_run = 0;
    bool any_fails = false;
    std::vector<std::string> tests;
    bool run_tests = false;
    bool done = false;
    std::string line;
    char ram[65536] = {};
    const Options *options = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint16_t GetYXAddr(const M6502 *s) {
    M6502Word addr;
    addr.b.h = s->y;
    addr.b.l = s->x;
    return addr.w;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SetC(M6502 *s, bool c) {
    M6502P p = M6502_GetP(s);
    p.bits.c = c;
    M6502_SetP(s, p.value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void Replace(std::string *str, const std::string &from, const std::string &to) {
    std::string::size_type i = 0;

    for (;;) {
        i = str->find(from, i);
        if (i == std::string::npos) {
            return;
        }

        *str = str->substr(0, i) + to + str->substr(i + from.size());
        i += to.size();
        ASSERT(i <= str->size());
    }
}

// I should really fix up the 6502 tests so they have actual identifier-style
// names. The current test "name" is more like a human-readable description. I
// didn't anticipate this particular use case.
static std::string GetCTestName(const std::string &name) {
    std::string ctest_name = name;

    Replace(&ctest_name, " ", ".");
    Replace(&ctest_name, "#$nn", "imm");
    Replace(&ctest_name, "$nnnn,x", "abx");
    Replace(&ctest_name, "$nnnn,y", "aby");
    Replace(&ctest_name, "$nnnn", "abs");
    Replace(&ctest_name, "($nn,x)", "inx");
    Replace(&ctest_name, "($nn),y", "iny");
    Replace(&ctest_name, "$nn,x", "zpx");
    Replace(&ctest_name, "$nn,y", "zpy");
    Replace(&ctest_name, "$nn", "zpg");
    Replace(&ctest_name, "(BCD)", "BCD");
    Replace(&ctest_name, "(CMOS)", "CMOS");
    Replace(&ctest_name, "(NMOS)", "NMOS");

    return ctest_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool MatchesAny(const std::string &str, const std::vector<std::regex> &regexes) {

    for (const std::regex &regex : regexes) {
        if (std::regex_match(str, regex)) {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void HackOpcode(M6502 *s) {
    auto state = (TestState *)s->context;
    uint16_t pc = s->pc.w - 1;

    if (pc == IRQ_ADDR) {
        TEST_FAIL("got an IRQ");
    } else {
        TEST_GE_UU(pc, LOAD_ADDR + 3);
        TEST_LT_UU(pc, LOAD_ADDR + 3 + Callback_Count * CALLBACK_SIZE);
        TEST_EQ_UU((pc - (LOAD_ADDR + 3u)) % CALLBACK_SIZE, 0);
        auto callback = (Callback)((pc - (LOAD_ADDR + 3u)) / CALLBACK_SIZE);
        switch (callback) {
        case Callback_Start:
            break;

        case Callback_TestBegin:
            {
                char *name = state->ram + GetYXAddr(s);
                state->tests.push_back(name);

                std::string ctest_name = GetCTestName(name);

                if (state->options->list_for_check_ctest_log) {
                    printf("2fcf9707-9498-4a03-9b27-ef501fa2fbb6:tom_seddon_6502_tests.%s\n", ctest_name.c_str());
                }

                bool run_test = false;
                if (state->run_tests) {
                    if (state->options->test_include_regexes.empty() && state->options->test_exclude_regexes.empty()) {
                        run_test = true;
                    } else {
                        bool include = state->options->test_include_regexes.empty() || MatchesAny(ctest_name, state->options->test_include_regexes);
                        bool exclude = MatchesAny(ctest_name, state->options->test_exclude_regexes);

                        run_test = include && !exclude;
                    }
                }

                if (run_test) {
                    printf("ea73a8dc-2d1a-43bc-ae41-078e441e53c5:tom_seddon_6502_tests.%s\n", ctest_name.c_str());
                }

                SetC(s, run_test);

                //printf("%s\n",name);
            }
            break;

        case Callback_TestFail:
            {
                // TODO: print results? Store them off somewhere?
                //
                // For now, just let the 6502 code do the work.

                //            uint16_t addr=GetYXAddr(s);
                //
                //            auto input=(State*)&state->ram[addr+0];
                //            auto output=(State*)&state->ram[addr+6];
                //            auto simulated=(State*)&state->ram[addr+12];

                state->any_fails = true;
                SetC(s, false);
            }
            break;

        case Callback_TestPass:
            break;

        case Callback_TestEnd:
            break;

        case Callback_Finish:
            state->done = true;
            break;

        case Callback_PrintChar:
            switch (s->a) {
            case 8:
                state->line.pop_back();
                break;

            case 10:
                LOGF(TEST, "%s\n", state->line.c_str());
                state->line.clear();
                break;

            case 13:
                // Ignore...
                break;

            default:
                state->line.append(1, (char)s->a);
                break;
            }
            break;

        default:
            TEST_FAIL("unexpected callback type");
            break;
        }
    }

    M6502_NextInstruction(s);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void RunTests(const M6502Config *cpu_config, const Options &options) {
    M6502 s;
    M6502_Init(&s, cpu_config);

    M6502Fns test_opcodes[256];
    memcpy(test_opcodes, s.fns, sizeof test_opcodes);
    test_opcodes[HACK_OPCODE].t0fn = &HackOpcode;
    s.fns = test_opcodes;

    TestState test_state;
    s.context = &test_state;
    test_state.options = &options;

    if (options.list_for_check_ctest_log) {
        test_state.run_tests = false;
    } else {
        test_state.run_tests = true;
    }

    {
        FILE *f = fopen(SRC_PATH, "rb");
        TEST_NON_NULL(f);

        uint16_t addr = LOAD_ADDR;
        int c;
        while ((c = fgetc(f)) != EOF) {
            test_state.ram[addr++] = (char)c;
        }

        fclose(f);
        f = nullptr;
    }

    M6502Word irq_addr = {IRQ_ADDR};
    test_state.ram[0xfffe] = (char)irq_addr.b.l;
    test_state.ram[0xffff] = (char)irq_addr.b.h;
    test_state.ram[IRQ_ADDR] = HACK_OPCODE;

    M6502Word load_addr = {LOAD_ADDR};
    test_state.ram[0xfffc] = (char)load_addr.b.l;
    test_state.ram[0xfffd] = (char)load_addr.b.h;

    for (size_t i = 0; i < Callback_Count; ++i) {
        uint16_t addr = (uint16_t)(LOAD_ADDR + 3 + i * CALLBACK_SIZE);
        test_state.ram[addr + 0] = HACK_OPCODE;
        test_state.ram[addr + 1] = 0x60;
    }

    M6502_Reset(&s);

    while (!test_state.done) {
        //printf("%04x\n",s.pc.w);
        (*s.tfn)(&s);

        if (s.read) {
            s.dbus = (uint8_t)test_state.ram[s.abus.w];
        } else {
            test_state.ram[s.abus.w] = (char)s.dbus;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool GetRegexesFromPatterns(std::vector<std::regex> *regexes, const std::vector<std::string> &patterns) {
    for (const std::string &pattern : patterns) {
        std::string regex_str;
        for (char c : pattern) {
            if (isalnum(c) || c == '_') {
                regex_str.push_back(c);
            } else if (c == '.') {
                regex_str += "\\.";
            } else if (c == '*') {
                regex_str += ".*";
            } else {
                fprintf(stderr, "FATAL: unsupported pattern: %s\n", pattern.c_str());
                return false;
            }
        }

        std::regex regex;
        try {
            regex = std::regex(regex_str, std::regex_constants::icase | std::regex_constants::extended);
        } catch (const std::regex_error &e) {
            fprintf(stderr, "FATAL: error in regex: %s\nFATAL: %s\n", regex_str.c_str(), e.what());
            return false;
        }

        regexes->push_back(regex);
    }

    return true;
}

static bool GetOptions(Options *options, int argc, char *argv[]) {
    CommandLineParser p;

    std::vector<std::string> include_name_patterns;
    std::vector<std::string> exclude_name_patterns;

    p.AddOption("list-for-check_ctest_log").SetIfPresent(&options->list_for_check_ctest_log).Help("list all test names, formatted for the benefit of check_ctest_log");
    p.AddOption('T').AddArgToList(&include_name_patterns).Meta("PATTERN").Help("run test(s) matching PATTERN");
    p.AddOption('X').AddArgToList(&exclude_name_patterns).Meta("PATTERN").Help("don't run test(s) matching PATTERN");

    if (!p.Parse(argc, argv)) {
        return false;
    }

    if (!GetRegexesFromPatterns(&options->test_include_regexes, include_name_patterns)) {
        return false;
    }

    if (!GetRegexesFromPatterns(&options->test_exclude_regexes, exclude_name_patterns)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    Options options;
    if (!GetOptions(&options, argc, argv)) {
        return 1;
    }

    if (options.help) {
        return 0;
    }

    RunTests(&M6502_nmos6502_config, options);
    RunTests(&M6502_cmos6502_config, options);

    return 0;
}
