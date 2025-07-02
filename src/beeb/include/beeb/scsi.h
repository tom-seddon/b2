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
    //
    // leds reflects current state and is always updated. Bits in leds_ever_on
    // are only ever set - caller should reset if it uses them. (leds_ever_on
    // exists to allow even brief periods of activity to be reflected in the
    // UI.)
    uint8_t leds : NUM_HARD_DISKS;
    uint8_t leds_ever_on : NUM_HARD_DISKS;

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
    bool FlushBufferForCurrentCommand();
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
    // message to 0, and code to the provided code, then EnterStatusPhase.
    void EnterCheckConditionStatusPhase(uint8_t code);

    // Fetches the LBA from bytes 1/2/3 of current command data.
    uint32_t GetLBAForCurrentCommand() const;

    HardDiskImage *GetHardDisk(uint8_t lun) const;

    // Retrieves image for corresponding to m_lun. Enters check condition status
    // and returns false if no corresponding image.
    bool GetHardDiskImageForCurrentCommand(HardDiskImage **hd_ptr);

#ifdef BBCMICRO_DEBUGGER
    friend class SCSIDebugWindow;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
