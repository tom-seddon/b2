#include <shared/system.h>
#include "SymbolTable.h"

#if BBCMICRO_DEBUGGER

#include <shared/testing.h>
#include <shared/log.h>
#include <beeb/type.h>

static const char TEST_DATA_1[] =
    "label1_1=$8000\n"
    "label2_1=$8001\n"
    "label3_1=$8000\n"
    "ambiguous=$1000\n"
    "file1_only=$2001\n";

static const char TEST_DATA_2[] =
    "label1_2=$8000\n"
    "label2_2=$8001\n"
    "label3_2=$8000\n"
    "ambiguous=$1001\n"
    "file2_only=$2002\n";

static const char BEEBASM_TEST_DATA[] = "[{'.SPARE1':0L,'.SPARE2':2L,'.irqTmp':3L,'.runLenCnt':4L,'.joystickEnabledFlag':5L,'.snowWindow':6L,'.packedTileTable':10L,'.itemExtra':16L,'.itemTile':17L,'.itemID':18L,'.itemX':19L,'.itemY':20L}]";

#define TEST_EQ_SYMBOL_S(GOT_NAME_PTR, WANTED_NAME) \
    BEGIN_MACRO {                                   \
        TEST_NON_NULL((GOT_NAME_PTR));              \
        TEST_EQ_SS(*(GOT_NAME_PTR), WANTED_NAME);   \
    }                                               \
    END_MACRO

static size_t MustFindFileIndex(const SymbolTable &st, const std::string &file_path) {
    for (size_t i = 0; i < st.GetNumFiles(); ++i) {
        const SymbolFile *file = st.GetFileByIndex(i);
        if (file->file_path == file_path) {
            return i;
        }
    }

    TEST_FAIL("couldn't find expected symbol file: %s", file_path.c_str());
}

static std::shared_ptr<const BBCMicroType> CreateTestBBCMicroType() {
    ROMType rom_types[16];
    for (int i = 0; i < 16; ++i) {
        rom_types[i] = ROMType_16KB;
    }

    std::shared_ptr<const BBCMicroType> type = CreateBBCMicroType(BBCMicroTypeID_B, rom_types);
    return type;
}

static void TestBeebAsmStuff() {
    const SymbolTable::SymbolParser *beebasm_parser = SymbolTable::SymbolParserRegistry::FindParserByFormatName("BeebAsm");
    TEST_NON_NULL(beebasm_parser);

    SymbolTable st;

    TEST_TRUE(st.LoadFromString(BEEBASM_TEST_DATA, "1", beebasm_parser));

    std::shared_ptr<const BBCMicroType> type = CreateTestBBCMicroType();

    TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0, 0, type), "SPARE1");
    TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(20, 0, type), "itemY");

    uint16_t addr;
    uint32_t dso;

    TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "snowWindow"));
    TEST_EQ_UU(addr, 6);
}

static void TestMultiSymbolTableStuff() {
    const SymbolTable::SymbolParser *acme_parser = SymbolTable::SymbolParserRegistry::FindParserByFormatName("ACME");
    TEST_NON_NULL(acme_parser);

    SymbolTable st;

    TEST_TRUE(st.LoadFromString(TEST_DATA_1, "1", acme_parser));

    size_t file1_index = MustFindFileIndex(st, "1");

    TEST_TRUE(st.LoadFromString(TEST_DATA_2, "2", acme_parser));

    size_t file2_index = MustFindFileIndex(st, "2");

    std::shared_ptr<const BBCMicroType> type = CreateTestBBCMicroType();

    uint16_t addr;
    uint32_t dso;

    // 1=enabled, 2=enabled

    TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_1");

    TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "file1_only"));
    TEST_EQ_UU(addr, 0x2001);

    TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "file2_only"));
    TEST_EQ_UU(addr, 0x2002);

    TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "ambiguous"));
    TEST_EQ_UU(addr, 0x1000);

    st.EnableFile(file1_index, false);

    // 1=disabled, 2=enabled

    TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_2");

    TEST_FALSE(st.GetAddressForSymbol(&addr, &dso, type, "file1_only"));

    TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "file2_only"));
    TEST_EQ_UU(addr, 0x2002);

    TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "ambiguous"));
    TEST_EQ_UU(addr, 0x1001);

    st.EnableFile(file1_index, true);

    // 1=enabled, 2=enabled

    TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_1");

    TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "file1_only"));
    TEST_EQ_UU(addr, 0x2001);

    TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "file2_only"));
    TEST_EQ_UU(addr, 0x2002);

    st.MoveFile(file2_index, file1_index);

    // 2=enabled, 1=enabled

    TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_2");

    TEST_TRUE(st.GetAddressForSymbol(&addr, &dso, type, "ambiguous"));
    TEST_EQ_UU(addr, 0x1001);
}

LOG_EXTERN(SYMBOLS);

int main() {
    SymbolTable::SymbolParserRegistry::InitializeBuiltinParsers();

    LOG(SYMBOLS).Enable();

    TestMultiSymbolTableStuff();
    TestBeebAsmStuff();
}

#else

// BBCMICRO_DEBUGGER is controlled entirely at the C++ level, so the test will
// be built (and run)R in all configurations.
//
// So, if no debugger, compile a stub, that'll then always succeed.

int main() {
}

#endif
