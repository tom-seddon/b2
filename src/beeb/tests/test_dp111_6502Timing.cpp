#include <shared/system.h>
#include <shared/log.h>
#include "test_common.h"

LOG_EXTERN(BBC_OUTPUT);

static int g_num_failures = -1;

static void WriteFailureCount(void *context, M6502Word addr, uint8_t value) {
    (void)context, (void)addr;

    g_num_failures = value;
}

int main() {
    TestBBCMicro bbc(BBC_TYPE);

    bbc.StartCaptureOSWRCH();
    bbc.LoadSSD(0, SSD_PATH);
    bbc.RunUntilOSWORD0(10.0);
    bbc.Paste("*CAT\r");
    bbc.Paste("*RUN 6502tim\r");
    bbc.SetXFJIO(0xfcd0, nullptr, nullptr, &WriteFailureCount, nullptr);
    bbc.RunUntilOSWORD0(20.0);

    {
        LOGF(BBC_OUTPUT, "All Output: ");
        LOGI(BBC_OUTPUT);
        LOG_STR(BBC_OUTPUT, GetPrintable(bbc.oswrch_output).c_str());
        LOG(BBC_OUTPUT).EnsureBOL();
    }

    printf("Number of failures: %d\n", g_num_failures);
    if (g_num_failures != 0) {
        return 1;
    }

    return 0;
}
