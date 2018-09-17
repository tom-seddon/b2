#ifndef HEADER_ExtMem
#define HEADER_ExtMem

#include <shared/system.h>
#include <shared/debug.h>
#include <vector>
#include "conf.h"
#include <6502/6502.h>

class ExtMem {
public:
    ExtMem();

    static uint8_t ReadAddressL(void *c_, M6502Word a);

    static void WriteAddressL(void *c_, M6502Word a, uint8_t value);

    static uint8_t ReadAddressH(void *c_, M6502Word a);

    static void WriteAddressH(void *c_, M6502Word a, uint8_t value);

    static uint8_t ReadData(void *c_, M6502Word a);

    static void WriteData(void *c_, M6502Word a, uint8_t value);

    static uint8_t ReadMemory(const void *c_, uint32_t a);

    static void WriteMemory(void *c_, uint32_t a, uint8_t value);

    uint8_t m_address_l = 0;
    uint8_t m_address_h = 0;

    std::vector<uint8_t> ram_buffer;
};

#endif