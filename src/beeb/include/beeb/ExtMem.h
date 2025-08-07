#ifndef HEADER_ExtMem
#define HEADER_ExtMem

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <memory>
#include <vector>
#include "conf.h"

union M6502Word;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ExtMem {
  public:
    ExtMem();

    // Actually allocate the ExtRam buffer. It's 16MBytes, so no harm
    // in doing this only when necessary.
    void AllocateBuffer();

    uint8_t GetAddressL() const;
    uint8_t GetAddressH() const;

    static uint8_t ReadAddressL(void *c_, M6502Word a);
    static void WriteAddressL(void *c_, M6502Word a, uint8_t value);

    static uint8_t ReadAddressH(void *c_, M6502Word a);
    static void WriteAddressH(void *c_, M6502Word a, uint8_t value);

    static uint8_t ReadData(void *c_, M6502Word a);
    static void WriteData(void *c_, M6502Word a, uint8_t value);

    static uint8_t ReadMemory(const void *c_, uint32_t a);
    static void WriteMemory(void *c_, uint32_t a, uint8_t value);

  protected:
  private:
    uint8_t m_address_l = 0;
    uint8_t m_address_h = 0;

    std::shared_ptr<std::vector<uint8_t>> m_ram_buffer;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
