#ifndef HEADER_D00B4638BE6448AE9200E1E2A470ED2F // -*- mode:c++ -*-
#define HEADER_D00B4638BE6448AE9200E1E2A470ED2F

#include "conf.h"
#include <memory>

class HardDiskImage;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// May move elsewhere when/if I get round to doing IDE.
struct HardDiskImageSet {
    std::shared_ptr<HardDiskImage> images[NUM_HARD_DISKS];
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if ENABLE_SCSI

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// X3.131 page references refer to X3.131-1986 doc - see
// https://github.com/bitshifters/bbc-documents/blob/master/Hard%20Disk/ANSI%20SCSI-1%20Standard%20X3.131-1986.pdf

// SM page references refer to the Acorn Winchester service manual - see
// https://github.com/bitshifters/bbc-documents/blob/master/Hard%20Disk/Acorn_WinchesterSM.pdf

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
    // SM p16
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

    // The disk images are assumed to be set on startup, and then not changed
    // thereafter. These are fixed disks after all.
    const HardDiskImageSet hds;

    // bit i set if HD i's LED is active.
    uint8_t leds : NUM_HARD_DISKS;

    // The SCSI emulation sets the CPU interrupt flag itself on read/write -
    // judging by BeebEm and B-em, there's no need for a per cycle check. Which
    // is lucky because this avoids adding another BBCMicro update flag.
    explicit SCSI(HardDiskImageSet hds, M6502 *cpu, uint8_t cpu_irq_flag);
    explicit SCSI(const SCSI &src) = default;
    explicit SCSI(SCSI &&) = delete;
    SCSI &operator=(const SCSI &) = delete;
    SCSI &operator=(SCSI &&) = delete;

    static uint8_t Read0(void *scsi, M6502Word a);
    static uint8_t Read1(void *scsi, M6502Word a);

    static void Write0(void *scsi, M6502Word a, uint8_t value);
    static void Write1(void *scsi, M6502Word a, uint8_t value);
    static void Write2(void *scsi, M6502Word a, uint8_t value);
    static void Write3(void *scsi, M6502Word a, uint8_t value);

    void SetTrace(Trace *t);

  protected:
  private:
    static constexpr uint8_t MAX_COMMAND_LENGTH = 10;

    //Value copied from B-em - but is this needlessly large?
    static constexpr uint32_t BUFFER_SIZE = 0x800;

    SCSIPhase m_phase = SCSIPhase_BusFree;
    uint8_t m_code = 0;
    uint32_t m_sector = 0;
    SCSIStatusRegister m_status_register = {};
    uint8_t m_status = 0;
    bool m_sel = false;
    uint32_t m_blocks = 0;
    uint32_t m_length = 0;
    uint8_t m_message = 0;
    uint8_t m_lun = 0;
    uint32_t m_next = 0;
    uint8_t m_last_write = 0;
    uint8_t m_cmd[MAX_COMMAND_LENGTH] = {};

    M6502 *const m_cpu = nullptr;
    const uint8_t m_cpu_irq_flag = 0;

    uint8_t m_buffer[BUFFER_SIZE] = {};
    size_t m_offset = 0;

    Trace *m_trace = nullptr;

    uint8_t ReadData();
    void WriteData(uint8_t value);

    void EnterBusFreePhase();
    void EnterMessagePhase();
    void EnterStatusPhase();
    void EnterSelectionPhase();
    void EnterCommandPhase();
    void EnterExecutePhase();

    // Sets status to Good (with 2-bit LUN in bits 5 and 6), and message to 0,
    // then EnterStatusPhase.
    void EnterGoodStatusPhase();

    // Sets status to Check Condition (with 2-bit LUN in bits 5 and 6), and
    // message to 0, then EnterStatusPhase.
    void EnterCheckConditionStatusPhase();

    HardDiskImage *GetHardDisk(uint8_t lun) const;

#ifdef BBCMICRO_DEBUGGER
    friend class SCSIDebugWindow;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
