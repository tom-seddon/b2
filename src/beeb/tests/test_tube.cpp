#include <shared/system.h>
#include <shared/testing.h>
#include "test_common.h"
#include "test_tube.h"

LOG_EXTERN(BBC_OUTPUT);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestTube(const std::string &test_name,
              TestBBCMicroType type,
              const std::string &beeblink_volume_path,
              const std::string &beeblink_drive,
              const std::string &name,
              const std::string &paste_text) {
    TestBBCMicro bbc(type);

    bbc.StartTrace(0, 256 * 1024 * 1024);
    bbc.StartCaptureOSWRCH();
    bbc.RunUntilOSWORD0(10.0);
    bbc.LoadFile(GetTestFileName(beeblink_volume_path,
                                 beeblink_drive,
                                 "$." + name),
                 0x1900);

    bbc.SaveTestTrace("test_tube");

    bbc.Paste("PAGE=&1900\rOLD\r");

    TEST_LT_UU(paste_text.size(), 250);
    bbc.Paste(paste_text);

    bbc.Paste("RUN\r");

    bbc.RunUntilOSWORD0(200);

    {
        LOGF(BBC_OUTPUT, "All Output: ");
        LOGI(BBC_OUTPUT);
        LOG_STR(BBC_OUTPUT, GetPrintable(bbc.oswrch_output).c_str());
        LOG(BBC_OUTPUT).EnsureBOL();
    }

    TestSpooledOutput(bbc,
                      beeblink_volume_path,
                      beeblink_drive,
                      test_name);
}