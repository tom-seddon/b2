#include <shared/system.h>
#include "SymbolTable.h"

#if BBCMICRO_DEBUGGER

#include <shared/testing.h>
#include <beeb/type.h>

static const char TEST_DATA_1[] =
    "label1_1=$8000\n"
    "label2_1=$8001\n"
    "label3_1=$8000\n";

static const char TEST_DATA_2[] =
    "label1_2=$8000\n"
    "label2_2=$8001\n"
    "label3_2=$8000\n";

#define TEST_EQ_SYMBOL_S(GOT_NAME_PTR, WANTED_NAME) \
    BEGIN_MACRO {                                   \
        TEST_NON_NULL((GOT_NAME_PTR));              \
        TEST_EQ_SS(*(GOT_NAME_PTR), WANTED_NAME);   \
    }                                               \
    END_MACRO

static size_t MustFindGroupIndex(const SymbolTable &st, const std::string &file_path) {
    for (size_t i = 0; i < st.GetNumGroups(); ++i) {
        const SymbolGroup *group = st.GetGroupByIndex(i);
        if (group->file_path == file_path) {
            return i;
        }
    }

    TEST_FAIL("couldn't find expected symbol group: %s", file_path.c_str());
}

int main() {
    SymbolTable::SymbolParserRegistry::InitializeBuiltinParsers();

    {
        const SymbolTable::SymbolParser *acme_parser = SymbolTable::SymbolParserRegistry::FindParserByFormatName("ACME");
        TEST_NON_NULL(acme_parser);

        SymbolTable st;

        TEST_TRUE(st.LoadFromString(TEST_DATA_1, "1", acme_parser));

        size_t group1_index = MustFindGroupIndex(st, "1");
        TEST_GE_II(group1_index, 0);

        TEST_TRUE(st.LoadFromString(TEST_DATA_2, "2", acme_parser));

        size_t group2_index = MustFindGroupIndex(st, "2");
        TEST_GE_II(group2_index, 0);

        ROMType rom_types[16];
        for (int i = 0; i < 16; ++i) {
            rom_types[i] = ROMType_16KB;
        }

        std::shared_ptr<const BBCMicroType> type = CreateBBCMicroType(BBCMicroTypeID_B, rom_types);

        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_1");

        st.EnableGroup(group1_index, false);

        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_2");

        st.EnableGroup(group1_index, true);

        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_1");

        st.MoveGroup(group2_index, group1_index);

        TEST_EQ_SYMBOL_S(st.GetSymbolNameForAddress(0x8000, 0, type), "label3_2");
    }
}

#else

// BBCMICRO_DEBUGGER is controlled entirely at the C++ level, so the test will
// be built (and run)R in all configurations.
//
// So, if no debugger, compile a stub, that'll then always succeed.

int main() {
}

#endif