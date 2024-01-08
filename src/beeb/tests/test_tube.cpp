#include <shared/system.h>
#include <shared/testing.h>
#include "test_common.h"
#include "test_tube.h"

LOG_EXTERN(BBC_OUTPUT);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestTube(const std::string &test_name,
              TestBBCMicroType type,
              uint32_t flags,
              const std::string &beeblink_volume_path,
              const std::string &beeblink_drive,
              const std::string &name,
              uint32_t addr,
              const std::string &pre_paste_text,
              const std::string &post_paste_text) {
    TestBBCMicro bbc(type, flags);

    //bbc.StartTrace(0, 256 * 1024 * 1024);
    bbc.StartCaptureOSWRCH();
    bbc.RunUntilOSWORD0(10.0);
    bbc.LoadFile(GetTestFileName(beeblink_volume_path,
                                 beeblink_drive,
                                 "$." + name),
                 addr);

    //bbc.SaveTestTrace("test_tube");

    if (!pre_paste_text.empty()) {
        bbc.Paste(pre_paste_text);
    }

    bbc.Paste("RUN\r");

    bbc.RunUntilOSWORD0(200);

    if (!post_paste_text.empty()) {
        bbc.Paste(post_paste_text);
        bbc.RunUntilOSWORD0(10);
    }

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