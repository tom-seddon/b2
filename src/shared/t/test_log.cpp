#include <shared/system.h>
#include <shared/log.h>
#include <shared/testing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <thread>
#include <atomic>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char EXPECTED[]={
#include "test_log.expected.txt"
};

static std::string g_str;
static LogPrinterString g_str_printer(&g_str);

LOG_DEFINE(TEST,"TEST",&g_str_printer)
LOG_DEFINE(OUT,"",&log_printer_stdout)

static int TestHighlightFn(size_t offset,void *data) {
    return offset==(uintptr_t)data;
}

static void TestDumpBytes(void) {
    LOG(TEST).SetPrefix("");

    const uint8_t data[]={
        0x5B,0x5E,0x54,0x43,0x94,0xFB,0xB4,0x1D,0x58,0x7F,0xF9,0xD3,0x95,0xAB,0x26,0x64,
        0x66,0x80,0x2C,0x4A,0x95,0x9D,0x45,0x66,0xE7,0x89,0xB4,0x0B,0x09,0xBE,0x41,0x0D,
        0xD6,0xF0,0x0B,0xDF,0xBA,0x38,0xBE,0x2B,0xBE,0x94,0x5E,0xF8,0x8F,0x75,0x9E,0xF5,
        0x2C,0xE2,0xFD,0x4A,0x21,0x8F,0x86,0xF7,0x68,0x97,0xC5,0xEB,0x1B,0x7D,0x24,0x1A,
        0x55,0x58,0x17,0x0E,0xB3,0x3B,0x47,0x13,0x95,0x19,0xE5,0x1C,0xC7,0x7D,0x2E,0x8B,
        0x20,0xC4,0x86,0x83,0xCA,0xCD,0x29,0xE4,0x06,0xA7,0x4D,0xE3,0x1A,
    };

    const char data_expected[]=
        "0x00000000: 5B 5E 54 43 94 FB B4 1D 58 7F F9 D3 95 AB 26 64  [^TC....X.....&d\n"
        "0x00000010: 66 80 2C 4A 95 9D 45 66 E7 89 B4 0B 09 BE 41 0D  f.,J..Ef......A.\n"
        "0x00000020: D6 F0 0B DF BA 38 BE 2B BE 94 5E F8 8F 75 9E F5  .....8.+..^..u..\n"
        "0x00000030: 2C E2 FD 4A 21 8F 86 F7 68 97 C5 EB 1B 7D 24 1A  ,..J!...h....}$.\n"
        "0x00000040: 55 58 17 0E B3 3B 47 13 95 19 E5 1C C7 7D 2E 8B  UX...;G......}..\n"
        "0x00000050: 20 C4 86 83 CA CD 29 E4 06 A7 4D E3 1A            .....)...M..   \n";

    const char data_17_columns_expected[]=
        "0x00000000: 5B 5E 54 43 94 FB B4 1D 58 7F F9 D3 95 AB 26 64 66  [^TC....X.....&df\n"
        "0x00000011: 80 2C 4A 95 9D 45 66 E7 89 B4 0B 09 BE 41 0D D6 F0  .,J..Ef......A...\n"
        "0x00000022: 0B DF BA 38 BE 2B BE 94 5E F8 8F 75 9E F5 2C E2 FD  ...8.+..^..u..,..\n"
        "0x00000033: 4A 21 8F 86 F7 68 97 C5 EB 1B 7D 24 1A 55 58 17 0E  J!...h....}$.UX..\n"
        "0x00000044: B3 3B 47 13 95 19 E5 1C C7 7D 2E 8B 20 C4 86 83 CA  .;G......}.. ....\n"
        "0x00000055: CD 29 E4 06 A7 4D E3 1A                             .)...M..         \n";

    const char data_highlighted_expected[]=
        "0x00000000: 5B 5E 54 43 94 FB*B4 1D 58 7F F9 D3 95 AB 26 64  [^TC....X.....&d\n"
        "0x00000010: 66 80 2C 4A 95 9D 45 66 E7 89 B4 0B 09 BE 41 0D  f.,J..Ef......A.\n"
        "0x00000020: D6 F0 0B DF BA 38 BE 2B BE 94 5E F8 8F 75 9E F5  .....8.+..^..u..\n"
        "0x00000030: 2C E2 FD 4A 21 8F 86 F7 68 97 C5 EB 1B 7D 24 1A  ,..J!...h....}$.\n"
        "0x00000040: 55 58 17 0E B3 3B 47 13 95 19 E5 1C C7 7D 2E 8B  UX...;G......}..\n"
        "0x00000050: 20 C4 86 83 CA CD 29 E4 06 A7 4D E3 1A            .....)...M..   \n";

    const char data_first_address_expected[]=
        "0x1122334455667788: 5B 5E 54 43 94 FB B4 1D 58 7F F9 D3 95 AB 26 64  [^TC....X.....&d\n"
        "0x1122334455667798: 66 80 2C 4A 95 9D 45 66 E7 89 B4 0B 09 BE 41 0D  f.,J..Ef......A.\n"
        "0x11223344556677A8: D6 F0 0B DF BA 38 BE 2B BE 94 5E F8 8F 75 9E F5  .....8.+..^..u..\n"
        "0x11223344556677B8: 2C E2 FD 4A 21 8F 86 F7 68 97 C5 EB 1B 7D 24 1A  ,..J!...h....}$.\n"
        "0x11223344556677C8: 55 58 17 0E B3 3B 47 13 95 19 E5 1C C7 7D 2E 8B  UX...;G......}..\n"
        "0x11223344556677D8: 20 C4 86 83 CA CD 29 E4 06 A7 4D E3 1A            .....)...M..   \n";

    g_str.clear();
    LogDumpBytes(&LOG(TEST),data,sizeof data);
    TEST_EQ_SS(g_str,data_expected);

    g_str.clear();
    {
        LogDumpBytesExData d={};

        d.num_dump_columns=17;

        LogDumpBytesEx(&LOG(TEST),data,sizeof data,&d);
    }
    TEST_EQ_SS(g_str,data_17_columns_expected);

    g_str.clear();
    {
        LogDumpBytesExData d={};

        d.highlight_fn=&TestHighlightFn;
        d.highlight_data=(void *)(uintptr_t)5;

        LogDumpBytesEx(&LOG(TEST),data,sizeof data,&d);
    }
    TEST_EQ_SS(g_str,data_highlighted_expected);

    g_str.clear();
    {
        LogDumpBytesExData d={};

        d.first_address=0x1122334455667788;

        LogDumpBytesEx(&LOG(TEST),data,sizeof data,&d);
    }
    TEST_EQ_SS(g_str,data_first_address_expected);
}

int main(void) {
    LOGF(TEST,"Line 1\nLine 2\n");
    LOGF(TEST,"Ordinary line\n\tTabbed line\n");

    LOGF(TEST,"Indent prefix: ");
    {
        LOGI(TEST);
        LOGF(TEST,"Indented line 1\n");
        LOGF(TEST,"Indented line 2\n");
        LOGF(TEST,"Indented line 3\n");

        LOGF(TEST,"Extra indent: ");

        {
            LOGI(TEST);
            LOGF(TEST,"Indented line 1\n");
            LOGF(TEST,"Indented line 2\n");
            LOGF(TEST,"Indented line 3\n");
        }
    }

    {
        std::string big_string;
        int num_lines=0;

        while(big_string.size()<9900) {
            big_string+="Line "+std::to_string(1+num_lines)+": ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz\n";
            ++num_lines;
        }

        big_string+="***\n";

        LOGF(TEST,"%s",big_string.c_str());

        if(big_string.size()<Log::PRINTF_BUFFER_SIZE) {
            LOGF(TEST,"NOTE: string was too small to exercise code for printing large strings.\n");
        }
    }

    TEST_EQ_SS(g_str,EXPECTED);

    LOG(TEST).EnsureBOL();
    g_str.clear();

    LOG(TEST).SetPrefix("");
    LogStringPrintable(&LOG(TEST),"Hello\n\r\t\bHello");
    LOG(TEST).Flush();

    /* LogDumpBytes(&LOG(OUT),g_buffer.text,g_buffer.size); */

    TEST_EQ_SS(g_str,"Hello\\n\\r\\t\\bHello");

    TestDumpBytes();

    return 0;
}
