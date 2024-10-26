#include <shared/system.h>
#include <shared/testing.h>
#include <6502/6502.h>
#include <stdio.h>
#include <shared/log.h>
#include <stdlib.h>
#include <shared/path.h>
#include <string.h>
#include <shared/file_io.h>

/* Test driver for Klaus Dormann's test suite,
 * https://github.com/Klaus2m5/6502_65C02_functional_tests */

LOG_DEFINE(DEBUG, "", &log_printer_stdout_and_debugger);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint8_t g_mem[65536];
static int g_disassemble = 0;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int DoTest(const std::string &fname,
                  uint16_t load_address,
                  uint16_t init_pc,
                  const M6502Config *config) {
    {
        std::string path = PathJoined(KLAUS_FOLDER_NAME, fname);

        std::vector<uint8_t> data;
        TEST_TRUE(LoadFile(&data, path, nullptr));
        TEST_TRUE(load_address + data.size() <= sizeof g_mem);

        memset(g_mem, 0, sizeof g_mem);
        memcpy(g_mem + load_address, data.data(), data.size());

        LOGF(DEBUG, "Test file: %s (%zu bytes)\n", path.c_str(), data.size());
        LOGF(DEBUG, "CPU type: %s\n", config->name);
    }

    auto s = new M6502;
    M6502_Init(s, config);
    s->tfn = &M6502_NextInstruction;

    s->pc.w = init_pc;
    int last_pc = -1;
    int success = 0;

    for (;;) {
        if (M6502_IsAboutToExecute(s)) {
            if (last_pc >= 0) {
                if (g_disassemble) {
                    int ia, ad;
                    char tmp[100];
                    M6502_DisassembleLastInstruction(s, tmp, sizeof tmp, &ia, &ad);
                    printf("%04X %s\n", last_pc, tmp);
                }
            }

            if (s->abus.w == last_pc) {
                goto error;
            }

            last_pc = s->abus.w;
        }

        (*s->tfn)(s);

        if (s->read) {
            s->dbus = g_mem[s->abus.w];
        } else {
            if (s->abus.w == 0xff00) {
                if (s->dbus) {
                    // finished with error
                    goto error;
                } else {
                    // finished with success
                    goto success;
                }
            } else {
                g_mem[s->abus.w] = s->dbus;
            }
        }
    }
success:;
    success = 1;
error:;

    if (success) {
        LOGF(DEBUG, "Success.\n");
    } else {
        char pbuf[9];
        M6502P p = M6502_GetP(s);
        LOGF(DEBUG, "Failure.\n");
        LOGF(DEBUG, "PC=$%04X A=$%02X X=$%02X Y=$%02X S=$%02X P=%s ($%02X) (DATA=$%02X)\n",
             last_pc, s->a, s->x, s->y, s->s.b.l, M6502P_GetString(pbuf, p), p.value, s->data);

        LOGF(DEBUG, "Low memory: ");
        LOGI(DEBUG);
        LogDumpBytes(&LOG(DEBUG), g_mem, 0x300);
    }

    M6502_Destroy(s);
    delete s;
    s = NULL;

    return success;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main() {
    int good = 1;

    good &= DoTest("6502.bin", 0, 0x400, &M6502_nmos6502_config);
    good &= DoTest("6502.bin", 0, 0x400, &M6502_cmos6502_config);

    good &= DoTest("65c02.bin", 0, 0x400, &M6502_cmos6502_config);
    good &= DoTest("65c02_rockwell.bin", 0, 0x400, &M6502_rockwell65c02_config);

    good &= DoTest("d0.bin", 0x200, 0x200, &M6502_nmos6502_config);
    good &= DoTest("d1.bin", 0x200, 0x200, &M6502_cmos6502_config);

    return good ? 0 : 1;
}
