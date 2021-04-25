#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/keys.h>

#include <shared/enum_def.h>
#include <beeb/keys.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool KeyStates::GetKeyState(BeebKey beeb_key) const {
    ASSERT(beeb_key>=0&&(int)beeb_key<128);
    
    return !!(this->columns[beeb_key&0xf]&1<<(beeb_key>>4));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void KeyStates::SetKeyState(BeebKey beeb_key,bool down) {
    ASSERT(beeb_key>=0&&(int)beeb_key<128);

    uint8_t column=beeb_key&0xf;
    uint8_t mask=1<<(beeb_key>>4);
    if(down) {
        this->columns[column]|=mask;
    } else {
        this->columns[column]&=~mask;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int IsNumericKeypadKey(BeebKey beeb_key) {
    ASSERT((int)beeb_key>=0&&(int)beeb_key<128);

    if(beeb_key<0) {
        return 0;
    }

    int8_t column=beeb_key&0xf;

    if(column>=10&&column<=14) {
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
