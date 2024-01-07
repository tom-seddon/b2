#include <shared/system.h>
#include <shared/testing.h>
#include "test_common.h"
#include "test_kevin_edwards.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestKevinEdwards(const std::string &beeblink_volume_path,
                      const std::string &beeblink_drive,
                      const std::string &name,
                      const std::string &paste_text,
                      bool save_trace) {
    TestBBCMicro bbc(TestBBCMicroType_BTape);

    bbc.SetTestTraceFlags(0);
    bbc.StartCaptureOSWRCH();
    bbc.RunUntilOSWORD0(10.0);
    bbc.LoadFile(GetTestFileName(beeblink_volume_path,
                                 beeblink_drive,
                                 "$." + name),
                 0x3000);

    TEST_LT_UU(paste_text.size(), 250);
    bbc.Paste(paste_text);

    const M6502 *cpu = bbc.GetM6502();

    bool good = false;

    uint64_t num_cycles = 0;
    uint64_t max_num_cycles = 100 * 1000 * 1000;
    while (num_cycles < max_num_cycles) {
        if (M6502_IsAboutToExecute(cpu)) {
            if (cpu->abus.w == 0xfff4 &&
                cpu->a == 200 &&
                cpu->x == 3) {
                break;
            } else if (cpu->abus.w == 0xe00) {
                good = true;
                break;
            }
        }

        bbc.Update1();
        ++num_cycles;
    }

    if (save_trace) {
        // Not super useful... trace is rather large and utterly impenetrable.
        bbc.SaveTestTrace(name);
    }

    TEST_LT_UU(num_cycles, max_num_cycles);
    TEST_TRUE(good);
}
