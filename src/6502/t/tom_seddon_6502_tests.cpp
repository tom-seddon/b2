#include <shared/system.h>
#include <shared/log.h>
#include <shared/testing.h>
#include <6502/6502.h>
#include <string.h>
#include <vector>
#include <string>

LOG_DEFINE(TEST, "TEST", &log_printer_stdout);

struct State {
    uint8_t a, x, y, s, p, operand;
};
static_assert(sizeof(State) == 6, "");

// One of the HLTs - not tested by these tests.
static const uint8_t HACK_OPCODE = 0x12;

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

static const uint16_t LOAD_ADDR = 0x2000;

// The tests don't use IRQs, but the IRQ vector points somewhere so that
// unexpected IRQs can be detected. (e.g., due to a wayward BRK...)
static const uint16_t IRQ_ADDR = 0xf000;

struct TestState {
    uint32_t num_tests_run = 0;
    bool any_fails = false;
    std::vector<std::string> tests;
    bool run_tests = false;
    bool done = false;
    std::string line;
    char ram[65536] = {};
};

static uint16_t GetYXAddr(const M6502 *s) {
    M6502Word addr;
    addr.b.h = s->y;
    addr.b.l = s->x;
    return addr.w;
}

static void SetC(M6502 *s, bool c) {
    M6502P p = M6502_GetP(s);
    p.bits.c = c;
    M6502_SetP(s, p.value);
}

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

                SetC(s, state->run_tests);

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

static void RunTests(const M6502Config *cpu_config) {
    M6502 s;
    M6502_Init(&s, cpu_config);

    M6502Fns test_opcodes[256];
    memcpy(test_opcodes, s.fns, sizeof test_opcodes);
    test_opcodes[HACK_OPCODE].t0fn = &HackOpcode;
    s.fns = test_opcodes;

    TestState test_state;
    s.context = &test_state;
    test_state.run_tests = true;

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

int main() {
    RunTests(&M6502_nmos6502_config);
    RunTests(&M6502_cmos6502_config);

    return 0;
}
