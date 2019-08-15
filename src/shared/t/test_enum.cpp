#include <shared/system.h>
#include <shared/testing.h>

enum OtherEnum {
    OE_1=1,
    OE_2=2,
    OE_5=5,
};

#include <shared/enum_decl.h>
#include "test_enum.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "test_enum.inl"
#include <shared/enum_end.h>

int main(void) {
    TEST_EQ_II(A,0);
    TEST_EQ_II(B,50);
    TEST_EQ_II(TestEnum_C,51);
    TEST_EQ_II(TestEnum_D,100);
    TEST_EQ_II(E,101);
    TEST_EQ_II(F,150);
    TEST_EQ_II(TestEnum_G,151);
    TEST_EQ_II(TestEnum_H,200);

    TEST_EQ_SS(GetTestEnumEnumName(0),"A");
    TEST_EQ_SS(GetTestEnumEnumName(50),"B");
    TEST_EQ_SS(GetTestEnumEnumName(51),"C");
    TEST_EQ_II(GetTestEnumEnumName(52)[0],'?');
    TEST_EQ_SS(GetTestEnumEnumName(100),"D");
    TEST_EQ_II(GetTestEnumEnumName(101)[0],'?');
    TEST_EQ_II(GetTestEnumEnumName(150)[0],'?');
    TEST_EQ_II(GetTestEnumEnumName(151)[0],'?');
    TEST_EQ_II(GetTestEnumEnumName(200)[0],'?');

    TEST_EQ_SS(GetOtherEnumEnumName((OtherEnum)1),"OE_1");
    TEST_EQ_SS(GetOtherEnumEnumName((OtherEnum)2),"OE_2");
    TEST_EQ_SS(GetOtherEnumEnumName((OtherEnum)5),"OE_5");
    TEST_EQ_II(GetOtherEnumEnumName((OtherEnum)0)[0],'?');
}
