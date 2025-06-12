#ifndef HEADER_D00B4638BE6448AE9200E1E2A470ED2F // -*- mode:c++ -*-
#define HEADER_D00B4638BE6448AE9200E1E2A470ED2F

#include "conf.h"

#if ENABLE_SCSI

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "scsi.inl"
#include <shared/enum_end.h>

#include <vector>

struct LogSet;
class Trace;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// See:
//
// http://www.cowsarenotpurple.co.uk/bbccomputer/native/adfs.html
//
// https://github.com/stardot/beebem-windows/blob/master/Src/Scsi.cpp

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

union M6502Word;
struct M6502;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SCSIDiskSpec {
    uint16_t num_cylinders = 0;
    uint8_t num_heads = 0;
};

bool LoadSCSIDiskSpec(SCSIDiskSpec *spec, const std::vector<uint8_t> &dsc_contents, const LogSet &logs);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SCSI {
  public:
    struct SCSIStatusRegisterBits {
        uint8_t msg : 1;
        uint8_t bsy : 1;
        uint8_t _ : 2;
        uint8_t irq : 1;
        uint8_t req : 1;
        uint8_t io : 1;
        uint8_t cd : 1;
    };

    union SCSIStatusRegister {
        SCSIStatusRegisterBits bits;
        uint8_t value;
    };
    CHECK_SIZEOF(SCSIStatusRegister, 1);

    static uint8_t Read0(void *scsi, M6502Word a);
    static uint8_t Read1(void *scsi, M6502Word a);

    static void Write0(void *scsi, M6502Word a, uint8_t value);
    static void Write1(void *scsi, M6502Word a, uint8_t value);
    static void Write2(void *scsi, M6502Word a, uint8_t value);
    static void Write3(void *scsi, M6502Word a, uint8_t value);

    // The SCSI emulation sets the CPU interrupt flag itself on read/write -
    // judging by BeebEm and B-em, there's no need for a per cycle check. Which
    // is lucky because this avoids adding another BBCMicro update flag.
    void SetCPU(M6502 *cpu, uint32_t irq_flag);

    void SetTrace(Trace *t);

  protected:
  private:
    SCSIPhase m_phase = SCSIPhase_BusFree;
    uint8_t m_code = 0;
    uint8_t m_sector = 0;
    SCSIStatusRegister m_status_register = {};
    uint8_t m_status;
    bool m_sel = false;
    uint32_t m_blocks = 0;
    uint32_t m_length = 0;

    M6502 *m_cpu = nullptr;
    uint32_t m_cpu_irq_flag = 0;

    std::vector<uint8_t> m_buffer;
    size_t m_offset = 0;

    Trace *m_trace = nullptr;

    uint8_t ReadData();
    void WriteData(uint8_t value);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
