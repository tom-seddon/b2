#include <shared/system.h>
#include <shared/testing.h>
#include <shared/path.h>
#include <6502/6502.h>
#include <stdlib.h>
#include <shared/log.h>
#include <ctype.h>
#include <string>
#include <shared/file_io.h>

LOG_DEFINE(OUTPUT, "", &log_printer_stdout_and_debugger);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<uint8_t> LoadFile(const std::string &dname, const std::string &prefix, uint8_t op, int c, int d) {
    std::string fname = prefix + M6502_cmos6502_config.disassembly_info[op].mnemonic + "C" + std::to_string(c) + "D" + std::to_string(d);
    for (char &x : fname) {
        x = (char)toupper(x);
    }

    std::string path = PathJoined(dname, fname);

    std::vector<uint8_t> data;
    TEST_TRUE(LoadFile(&data, path, nullptr));
    TEST_EQ_UU(data.size(), 65536);

    LOGF(OUTPUT, "%s: %zu\n", path.c_str(), data.size());

    return data;
}

static void TestOp2(const std::string &dname, uint8_t op, int c, int d) {
    std::vector<uint8_t> aresults = LoadFile(dname, "A", op, c, d);
    std::vector<uint8_t> presults = LoadFile(dname, "P", op, c, d);

    M6502Fn fn = M6502_cmos6502_config.fns[op].ifn;

    M6502 tmp;

    for (size_t a = 0; a < 256; ++a) {
        for (size_t b = 0; b < 256; ++b) {
            size_t i = a + b * 256;

            tmp.p.bits.c = !!c;
            tmp.p.bits.d = !!d;
            tmp.a = (uint8_t)a;
            tmp.data = (uint8_t)b;

            (*fn)(&tmp);

            M6502P presult;
            presult.value = presults[i];

            TEST_EQ_UU(tmp.a, aresults[i]);
            TEST_EQ_UU(tmp.p.bits.c, presult.bits.c);
            TEST_EQ_UU(tmp.p.bits.z, presult.bits.z);
            TEST_EQ_UU(tmp.p.bits.v, presult.bits.v);
            TEST_EQ_UU(tmp.p.bits.n, presult.bits.n);
        }
    }
}

static void TestOp(const char *dname, uint8_t op) {
    TestOp2(dname, op, 0, 0);
    TestOp2(dname, op, 0, 1);
    TestOp2(dname, op, 1, 0);
    TestOp2(dname, op, 1, 1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    M6502P p;
    p.value = 1;
    TEST_TRUE(p.bits.c);

    M6502Word w;
    w.w = 0x1234;
    TEST_EQ_UU(w.b.l, 0x34);
    TEST_EQ_UU(w.b.h, 0x12);

    if (argc > 1) {
        TestOp(argv[1], 0x69);
        TestOp(argv[1], 0xe9);
    }
}
