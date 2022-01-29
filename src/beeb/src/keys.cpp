#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/keys.h>

#include <shared/enum_def.h>
#include <beeb/keys.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int IsNumericKeypadKey(BeebKey beeb_key) {
    ASSERT((int)beeb_key >= 0 && (int)beeb_key < 128);

    if (beeb_key < 0) {
        return 0;
    }

    int8_t column = beeb_key & 0xf;

    if (column >= 10 && column <= 14) {
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
