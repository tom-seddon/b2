#include <shared/system.h>
#include <beeb/scsi.h>

#if ENABLE_SCSI

#include <beeb/Trace.h>
#include <beeb/6502.h>
#include <shared/log.h>
#include <shared/debug.h>
#include <beeb/HardDiskImage.h>
#include <shared/load_store.h>
#include <inttypes.h>

#include <shared/enum_def.h>
#include <beeb/scsi.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Based on code by Jon Welch and Y. Tanaka

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr uint8_t CODE_BAD_DISK_ADDRESS = 0x21;
static constexpr uint8_t CODE_VOLUME_ERROR = 0x23;
static constexpr uint8_t CODE_BAD_DRIVE_NUMBER = 0x25;
static constexpr uint8_t CODE_UNSUPPORTED_CONTROLLER_COMMAND = 0x27;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE

#define TRACE(SELF, ...)                                                       \
    BEGIN_MACRO {                                                              \
        if ((SELF)->m_trace) {                                                 \
            (SELF)->m_trace->AllocStringf(TraceEventSource_Host, __VA_ARGS__); \
        }                                                                      \
    }                                                                          \
    END_MACRO

#else

#define TRACE(...) ((void)0)

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if ASSERT_ENABLED

#define CHECK_SR_BIT(NAME, VALUE)   \
    BEGIN_MACRO {                   \
        SCSIStatusRegister s;       \
        s.value = 0;                \
        s.bits.NAME = 1;            \
        ASSERT(s.value == (VALUE)); \
    }                               \
    END_MACRO

#else

#define CHECK_SR_BIT(...) ((void)0)

#endif

SCSI::SCSI(HardDiskImageSet hds_, M6502 *cpu, uint8_t cpu_irq_flag)
    : hds(std::move(hds_))
    , leds(0)
    , leds_ever_on(0)
    , m_cpu(cpu)
    , m_cpu_irq_flag(cpu_irq_flag) {
    ASSERT(cpu_irq_flag != 0);

    CHECK_SR_BIT(msg, 1);
    CHECK_SR_BIT(bsy, 2);
    CHECK_SR_BIT(irq, 0x10);
    CHECK_SR_BIT(req, 0x20);
    CHECK_SR_BIT(io, 0x40);
    CHECK_SR_BIT(cd, 0x80);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t SCSI::Read0(void *scsi_, M6502Word) {
    auto const scsi = (SCSI *)scsi_;

    uint8_t value = scsi->ReadData();
    return value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t SCSI::Read1(void *scsi_, M6502Word) {
    auto const scsi = (SCSI *)scsi_;

    SCSIStatusRegister status_register = scsi->m_status_register;

    // https://github.com/stardot/beebem-windows/blob/624a0806b4809e980289bfe9ed851ebdf7be42dd/Src/Scsi.cpp#L225
    //
    // ...???
    status_register.bits.req = 1;

    TRACE(scsi, "SCSI - Read Status: $%02x: msg=%d bsy=%d irq=%d req=%d io=%d cd=%d\n",
          status_register.value,
          status_register.bits.msg,
          status_register.bits.bsy,
          status_register.bits.irq,
          status_register.bits.req,
          status_register.bits.io,
          status_register.bits.cd);

    return status_register.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::Write0(void *scsi_, M6502Word, uint8_t value) {
    auto const scsi = (SCSI *)scsi_;

    scsi->m_sel = true;

    scsi->WriteData(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::Write1(void *scsi_, M6502Word, uint8_t value) {
    auto const scsi = (SCSI *)scsi_;
    (void)value;

    // Not sure what this is for? Looks like ADFS doesn't write to it and the
    // service manual doesn't explain it.

    TRACE(scsi, "SCSI - Write Status\n");

    scsi->m_sel = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::Write2(void *scsi_, M6502Word, uint8_t value) {
    auto const scsi = (SCSI *)scsi_;

    TRACE(scsi, "SCSI - Write Select\n");

    scsi->m_sel = false;

    scsi->WriteData(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::Write3(void *scsi_, M6502Word, uint8_t value) {
    auto const scsi = (SCSI *)scsi_;

    scsi->m_sel = true;

    if (value == 0xff) {
        scsi->m_status_register.bits.irq = 1;
        M6502_SetDeviceIRQ(scsi->m_cpu, scsi->m_cpu_irq_flag, true);
        scsi->m_status = 0;

        TRACE(scsi, "SCSI - Set IRQ\n");
    } else {
        scsi->m_status_register.bits.irq = 0;
        M6502_SetDeviceIRQ(scsi->m_cpu, scsi->m_cpu_irq_flag, false);

        TRACE(scsi, "SCSI - Clear IRQ\n");
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::SetTrace(Trace *t) {
    m_trace = t;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t SCSI::ReadData() {
    TRACE(this, "SCSI - ReadData: Phase=%s\n", GetSCSIPhaseEnumName(m_phase));

    switch (m_phase) {
    default:
        this->EnterBusFreePhase();
        [[fallthrough]];
    case SCSIPhase_BusFree:
        return m_last_write;

    case SCSIPhase_Status:
        {
            uint8_t data = m_status;
            m_status_register.bits.req = 0;
            this->EnterMessagePhase();
            return data;
        }
        break;

    case SCSIPhase_Message:
        {
            uint8_t data = m_message;
            m_status_register.bits.req = 0;
            this->EnterBusFreePhase();
            return data;
        }
        break;

    case SCSIPhase_Read:
        {
            ASSERT(m_blocks > 0);
            ASSERT(m_length > 0);

            TRACE(this, "SCSI - ReadData: m_offset=%zu (0x%zx); m_length=%" PRIu32 " (0x%" PRIx32 ")", m_offset, m_offset, m_length, m_length);
            uint8_t data = m_buffer[m_offset];
            ++m_offset;
            --m_length;
            m_status_register.bits.req = 0;

            if (m_length == 0) {
                --m_blocks;
                if (m_blocks == 0) {
                    this->EnterStatusPhase();
                    return data;
                }

                ASSERT(!!this->hds.images[m_lun]);
                if (!this->hds.images[m_lun]->ReadSector(m_buffer, m_next)) {
                    this->EnterCheckConditionStatusPhase(CODE_VOLUME_ERROR);
                    return data;
                }

                m_length = 256;
                m_offset = 0;
                ++m_next;
            }

            return data;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::WriteData(uint8_t value) {
    TRACE(this, "SCSI - WriteData: Phase=%s; value=$%02x\n", GetSCSIPhaseEnumName(m_phase), value);
    m_last_write = value;

    switch (m_phase) {
    case SCSIPhase_BusFree:
        if (m_sel) {
            this->EnterSelectionPhase();
        }
        break;

    case SCSIPhase_Selection:
        if (!m_sel) {
            this->EnterCommandPhase();
        }
        break;

    case SCSIPhase_Command:
        m_cmd[m_offset] = value;
        if (m_offset == 0) {
            if (value >= 0x20 && value <= 0x3f) {
                m_length = 10;
            }
        }
        ASSERT(m_length <= MAX_COMMAND_LENGTH);

        --m_length;

        TRACE(this, "SCSI - Command +%u (%u left): $%02x\n", m_offset, m_length, value);

        ++m_offset;

        m_status_register.bits.req = 0;

        if (m_length == 0) {
            this->EnterExecutePhase();
        }
        break;

    case SCSIPhase_Write:
        ASSERT(m_blocks > 0);
        ASSERT(m_length > 0);

        m_buffer[m_offset] = value;
        ++m_offset;
        --m_length;
        m_status_register.bits.req = 0;

        if (m_length == 0) {
            if (!this->FlushBufferForCurrentCommand()) {
                this->EnterCheckConditionStatusPhase(CODE_VOLUME_ERROR);
                break;
            }

            --m_blocks;
            if (m_blocks == 0) {
                this->EnterStatusPhase();
                break;
            }

            m_length = 256;
            ++m_next;
            m_offset = 0;
        }
        break;

    default:
        this->EnterBusFreePhase();
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SCSI::FlushBufferForCurrentCommand() {
    switch (m_cmd[0]) {
    case SCSICommand_WriteExtended:
        // TODO: BeebEm's code has a case for this, but no actual handling for
        // it. Does anything 8-bit actually use it?!
        //
        // (It looks like it's a fancy version of Write (0x0a). 32-bit LBA,
        // 16-bit block count. The wider parameters mean the parameter block
        // layout is different from Write, so it does need special handling.)
    default:
        break;

    case SCSICommand_Write:
    case SCSICommand_WriteAndVerify:
        {
            HardDiskImage *hd = this->GetHardDisk(m_lun);
            if (!hd) {
                break;
            }

            if (!hd->WriteSector(m_buffer, m_next - 1)) {
                break;
            }
        }
        return true;

    case SCSICommand_ModeSelect6:
        {
            HardDiskImage *hd = this->GetHardDisk(m_lun);
            if (!hd) {
                break;
            }

            if (!hd->SetSCSIDeviceParameters(m_buffer, m_offset)) {
                break;
            }
        }
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::EnterBusFreePhase() {
    m_phase = SCSIPhase_BusFree;

    m_status_register.value = 0;

    this->leds = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::EnterMessagePhase() {
    m_phase = SCSIPhase_Message;

    m_status_register.bits.msg = 1;
    m_status_register.bits.req = 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::EnterStatusPhase() {
    m_phase = SCSIPhase_Status;

    m_status_register.bits.io = 1;
    m_status_register.bits.cd = 1;
    m_status_register.bits.req = 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::EnterSelectionPhase() {
    m_phase = SCSIPhase_Selection;

    m_status_register.bits.bsy = 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::EnterCommandPhase() {
    m_phase = SCSIPhase_Command;

    m_status_register.bits.io = 0;
    m_status_register.bits.cd = 1;
    m_status_register.bits.msg = 0;

    m_offset = 0;
    m_length = 6;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::EnterExecutePhase() {
    m_phase = SCSIPhase_Execute;

    m_lun = m_cmd[1] >> 5;

    if (m_lun < NUM_HARD_DISKS) {
        uint8_t mask = 1 << m_lun;
        this->leds |= mask;
        this->leds_ever_on |= mask;
    }

    TRACE(this, "SCSI - Execute: LUN=%u CMD=%s (%u; 0x%x)\n", m_lun, GetSCSICommandEnumName(m_cmd[0]), m_cmd[0], m_cmd[0]);

    switch (m_cmd[0]) {
    default:
        this->EnterCheckConditionStatusPhase(CODE_UNSUPPORTED_CONTROLLER_COMMAND);
        break;

    case SCSICommand_TestUnitReady:
        {
            if (!this->GetHardDiskImageForCurrentCommand(nullptr)) {
                break;
            }

            this->EnterGoodStatusPhase();
        }
        break;

    case SCSICommand_RequestSense:
        {
            m_length = m_cmd[4];
            if (m_length == 0) {
                m_length = 4;
            }

            memset(m_buffer, 0, m_length);
            m_buffer[0] = m_code;
            if (m_code == CODE_BAD_DISK_ADDRESS) {
                m_buffer[1] = (uint8_t)(m_sector >> 16);
                m_buffer[2] = (uint8_t)(m_sector >> 8);
                m_buffer[3] = (uint8_t)(m_sector >> 0);
            }

            m_code = 0;
            m_sector = 0;

            m_offset = 0;
            m_blocks = 1;
            m_phase = SCSIPhase_Read;
            m_status_register.bits.io = 1;
            m_status_register.bits.cd = 0;
            m_status = m_lun << 5;
            m_message = 0;
            m_status_register.bits.req = 1;
        }
        break;

    case SCSICommand_FormatUnit:
        {
            HardDiskImage *hd;
            if (!this->GetHardDiskImageForCurrentCommand(&hd)) {
                break;
            }

            hd->Format();

            this->EnterGoodStatusPhase();
        }
        break;

    case SCSICommand_Read:
        {
            HardDiskImage *hd;
            if (!this->GetHardDiskImageForCurrentCommand(&hd)) {
                break;
            }

            uint32_t lba = (m_cmd[1] & 0x1f) << 16 | m_cmd[2] << 8 | m_cmd[3];
            m_blocks = m_cmd[4];
            if (m_blocks == 0) {
                m_blocks = 256;
            }

            TRACE(this, "SCSI - Read Sector: LBA=%u (0x%x)\n", m_lun, lba, lba);
            if (!hd->ReadSector(m_buffer, lba)) {
                this->EnterCheckConditionStatusPhase(CODE_VOLUME_ERROR);
                break;
            }

            m_status = m_lun << 5;
            m_message = 0;

            m_length = 256;
            m_offset = 0;
            m_next = lba + 1;

            m_phase = SCSIPhase_Read;

            m_status_register.bits.io = 1;
            m_status_register.bits.cd = 0;
            m_status_register.bits.req = 1;

            // TODO: as per B-em - reset auto boot flag at this point.
        }
        break;

    case SCSICommand_Write:
        {
            HardDiskImage *hd;
            if (!this->GetHardDiskImageForCurrentCommand(&hd)) {
                break;
            }

            uint32_t lba = (m_cmd[1] & 0x1f) << 16 | m_cmd[2] << 8 | m_cmd[3];
            m_blocks = m_cmd[4];
            if (m_blocks == 0) {
                m_blocks = 256;
            }

            m_status = m_lun << 5;
            m_message = 0;

            m_length = 256;

            // TODO: why is there a +1 here? There's a -1 in WriteSector... and
            // that presumably cancels this one out?
            m_next = lba + 1;
            m_offset = 0;

            m_phase = SCSIPhase_Write;

            m_status_register.bits.cd = 0;
            m_status_register.bits.req = 1;
        }
        break;

    case SCSICommand_TranslateV:
        {
            uint32_t lba = (m_cmd[1] & 0x1f) << 16 | m_cmd[2] << 8 | m_cmd[3];

            Store32LE(&m_buffer[0], lba);

            m_length = 4;
            m_offset = 0;
            m_blocks = 1;
            m_phase = SCSIPhase_Read;
            m_status_register.bits.io = 1;
            m_status_register.bits.cd = 0;

            m_status = m_lun << 5;
            m_message = 0;

            m_status_register.bits.req = 0;
        }
        break;

    case SCSICommand_ModeSelect6:
        {
            m_length = m_cmd[4];
            m_blocks = 1;
            m_status = m_lun << 5;
            m_message = 0;

            m_next = 0;
            m_offset = 0;
            m_blocks = 1; //???
            m_phase = SCSIPhase_Write;
            m_status_register.bits.cd = 0;
            m_status_register.bits.req = 1;
        }
        break;

    case SCSICommand_ModeSense6:
        {
            HardDiskImage *hd;
            if (!this->GetHardDiskImageForCurrentCommand(&hd)) {
                break;
            }

            memset(m_buffer, 0, sizeof m_buffer);
            if (!hd->GetSCSIDeviceParameters(m_buffer)) {
                this->EnterCheckConditionStatusPhase(CODE_VOLUME_ERROR);
                break;
            }

            TRACE(this, "SCSI - Sense: Buffer: [00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17]");
            TRACE(this, "SCSI - Sense: Buffer: [%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x]",
                  m_buffer[0], m_buffer[1], m_buffer[2], m_buffer[3], m_buffer[4],
                  m_buffer[5], m_buffer[6], m_buffer[7], m_buffer[8], m_buffer[9],
                  m_buffer[10], m_buffer[11], m_buffer[12], m_buffer[13], m_buffer[14],
                  m_buffer[15], m_buffer[16], m_buffer[17], m_buffer[18], m_buffer[19],
                  m_buffer[20], m_buffer[21]);

            uint16_t num_cylinders = Load16BE(m_buffer + 13);
            uint8_t num_heads = m_buffer[15];

            TRACE(this, "SCSI - Sense: Heads=%u ($%x) Cylinders=%u ($%x) ", num_cylinders, num_cylinders, num_heads, num_heads);

            uint32_t size = m_cmd[4];
            if (size == 0) {
                size = 256;
            }

            m_offset = 0;
            m_blocks = 1;
            m_length = 22;
            m_phase = SCSIPhase_Read;
            m_status_register.bits.io = 1;
            m_status_register.bits.cd = 0;
            m_status = m_lun << 5;
            m_message = 0;
            m_status_register.bits.req = 1;
        }
        break;

    case SCSICommand_StartOrStopUnit:
        {
            HardDiskImage *hd;
            if (!this->GetHardDiskImageForCurrentCommand(&hd)) {
                break;
            }

            this->EnterGoodStatusPhase();
        }
        break;

    case SCSICommand_Verify:
        {
            HardDiskImage *hd;
            if (!this->GetHardDiskImageForCurrentCommand(&hd)) {
                break;
            }

            uint32_t lba = (m_cmd[1] & 0x1f) << 16 | m_cmd[2] << 8 | m_cmd[3];
            if (!hd->IsValidLBA(lba)) {
                m_sector = lba;
                this->EnterCheckConditionStatusPhase(CODE_BAD_DISK_ADDRESS);
                break;
            }

            this->EnterGoodStatusPhase();
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::EnterGoodStatusPhase() {
    // X3.131 p185

    TRACE(this, "SCSI - Status=Good: LUN=%u\n", m_lun);
    m_status = m_lun << 5 | 0;

    m_message = 0;

    this->EnterStatusPhase();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::EnterCheckConditionStatusPhase(uint8_t code) {
    // X3.131 p185

    TRACE(this, "SCSI - Status=CheckCondition: LUN=%u; code=0x%x\n", m_lun, code);
    m_status = m_lun << 5 | 2;

    m_message = 0;

    m_code = code;

    this->EnterStatusPhase();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HardDiskImage *SCSI::GetHardDisk(uint8_t lun) const {
    ASSERT(lun < 8); //3-bit field
    if (lun > NUM_HARD_DISKS) {
        return nullptr;
    } else {
        return this->hds.images[lun].get();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SCSI::GetHardDiskImageForCurrentCommand(HardDiskImage **hd_ptr) {
    HardDiskImage *hd = this->GetHardDisk(m_lun);
    if (!hd) {
        this->EnterCheckConditionStatusPhase(CODE_BAD_DRIVE_NUMBER);
        return false;
    }

    if (hd_ptr) {
        *hd_ptr = hd;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
