#include <shared/system.h>
#include <beeb/paging.h>

#include <shared/enum_def.h>
#include <beeb/paging.inl>
#include <shared/enum_end.h>

//#define ANDY_OFFSET (0x8000u+0u)
//#define NUM_ANDY_PAGES (0x10u)
//#define HAZEL_OFFSET (0x8000+0x1000u)
//#define NUM_HAZEL_PAGES (0x20u)
//#define SHADOW_OFFSET (0x8000+0x3000u)
//#define NUM_SHADOW_PAGES (0x30u)

const BigPageType ROM_BIG_PAGE_TYPES[16]={
    {'0',"ROM 0"},
    {'1',"ROM 1"},
    {'2',"ROM 2"},
    {'3',"ROM 3"},
    {'4',"ROM 4"},
    {'5',"ROM 5"},
    {'6',"ROM 6"},
    {'7',"ROM 7"},
    {'8',"ROM 8"},
    {'9',"ROM 9"},
    {'a',"ROM a"},
    {'b',"ROM b"},
    {'c',"ROM c"},
    {'d',"ROM d"},
    {'e',"ROM e"},
    {'f',"ROM f"},
};

const BigPageType MAIN_RAM_BIG_PAGE_TYPE={'m',"Main RAM"};
const BigPageType SHADOW_RAM_BIG_PAGE_TYPE={'s',"Shadow RAM"};
const BigPageType ANDY_BIG_PAGE_TYPE={'n',"ANDY"};
const BigPageType HAZEL_BIG_PAGE_TYPE={'h',"HAZEL"};
const BigPageType MOS_BIG_PAGE_TYPE={'o',"MOS ROM"};
const BigPageType IO_BIG_PAGE_TYPE={'i',"I/O area"};
