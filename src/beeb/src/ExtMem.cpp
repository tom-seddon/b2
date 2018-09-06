#include <beeb/ExtMem.h>

ExtMem::ExtMem() {
    this->ram_buffer.resize(16777215);
}

uint8_t ExtMem::ReadAddressL(void *c_, M6502Word a) {
    auto c=(ExtMem *)c_;

    return c->m_address_l;
}

void ExtMem::WriteAddressL(void *c_, M6502Word a, uint8_t value) {
    auto c=(ExtMem *)c_;

    c->m_address_l = value;
}

uint8_t ExtMem::ReadAddressH(void *c_, M6502Word a) {
    auto c=(ExtMem *)c_;

    return c->m_address_h;
}

void ExtMem::WriteAddressH(void *c_, M6502Word a, uint8_t value) {
    auto c=(ExtMem *)c_;

    c->m_address_h = value;
}

uint8_t ExtMem::ReadData(void *c_, M6502Word a) {
    auto c=(ExtMem *)c_;

    uint32_t address = 0;

    address += (uint32_t)c->m_address_h << 16;
    address += (uint32_t)c->m_address_l << 8;
    address += (uint32_t)a.b.l;

    return c->ram_buffer[address];
}

void ExtMem::WriteData(void *c_, M6502Word a, uint8_t value){
    auto c=(ExtMem *)c_;

    uint32_t address = 0;

    address += (uint32_t)c->m_address_h << 16;
    address += (uint32_t)c->m_address_l << 8;
    address += (uint32_t)a.b.l;

    c->ram_buffer[address] = value;
}

uint8_t ExtMem::ReadMemory(const void *c_, uint32_t a){
    auto c=(ExtMem *)c_;

    return c->ram_buffer[a];
}

void ExtMem::WriteMemory(void *c_, uint32_t a, uint8_t value){
    auto c=(ExtMem *)c_;

    c->ram_buffer[a] = value;
}
