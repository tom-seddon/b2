#include <shared/system.h>
#include <beeb/scsi.h>

#if ENABLE_SCSI

#include <beeb/Trace.h>
#include <beeb/6502.h>
#include <shared/log.h>
#include <shared/debug.h>
#include <beeb/HardDiskImage.h>
#include <shared/load_store.h>

#include <shared/enum_def.h>
#include <beeb/scsi.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Based on code by Jon Welch and Y. Tanaka

// X.131 page references refer to X.131-1986 doc - see
// https://github.com/bitshifters/bbc-documents/blob/master/Hard%20Disk/ANSI%20SCSI-1%20Standard%20X3.131-1986.pdf

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE

#define TRACE(...)                                                     \
    BEGIN_MACRO {                                                      \
        if (m_trace) {                                                 \
            m_trace->AllocStringf(TraceEventSource_Host, __VA_ARGS__); \
        }                                                              \
    }                                                                  \
    END_MACRO

#else

#define TRACE(...) ((void)0)

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const size_t DSC_SIZE = 22;

bool LoadSCSIDiskSpec(SCSIDiskSpec *spec, const std::vector<uint8_t> &dsc_contents, const LogSet &logs) {
    if (dsc_contents.size() != DSC_SIZE) {
        logs.e.f("DSC data is wrong size: %zu bytes (should be %zu bytes)\n", dsc_contents.size(), DSC_SIZE);
        return false;
    }

    spec->num_cylinders = dsc_contents[13] << 8 | dsc_contents[14];
    spec->num_heads = dsc_contents[15];

    logs.i.f("DSC data: %u cylinders, %u heads\n", spec->num_cylinders, spec->num_heads);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SCSI::SCSI(HardDiskImageSet hds_, M6502 *cpu, uint8_t cpu_irq_flag)
    : hds(std::move(hds_))
    , m_cpu(cpu)
    , m_cpu_irq_flag(cpu_irq_flag) {
    ASSERT(cpu_irq_flag != 0);
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

    return status_register.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::Write0(void *scsi_, M6502Word, uint8_t value) {
    auto const scsi = (SCSI *)scsi_;

    scsi->WriteData(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::Write1(void *scsi_, M6502Word, uint8_t value) {
    auto const scsi = (SCSI *)scsi_;
    (void)value;

    scsi->m_sel = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::Write2(void *scsi_, M6502Word, uint8_t value) {
    auto const scsi = (SCSI *)scsi_;

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
    } else {
        scsi->m_status_register.bits.irq = 0;
        M6502_SetDeviceIRQ(scsi->m_cpu, scsi->m_cpu_irq_flag, false);
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
    TRACE("SCSI - ReadData: Phase=%s\n", GetSCSIPhaseEnumName(m_phase));

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
            uint8_t data = m_buffer[m_offset];
            ++m_offset;
            ASSERT(m_length > 0);
            --m_length;
            m_status_register.bits.req = 0;

            if (m_length == 0) {
                ASSERT(m_blocks > 0);
                --m_blocks;
                if (m_blocks == 0) {
                    this->EnterStatusPhase();
                    return data;
                }

                ASSERT(!!this->hds.images[m_lun]);
                if (!this->hds.images[m_lun]->ReadSector(m_buffer, m_next)) {
                    this->EnterCheckConditionStatusPhase();
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

        ++m_offset;
        --m_length;
        m_status_register.bits.req = 0;

        if (m_length == 0) {
            this->EnterExecutePhase();
        }
        break;

    case SCSIPhase_Write:
        m_buffer[m_offset] = value;
        ++m_offset;
        ASSERT(m_length > 0);
        --m_length;
        m_status_register.bits.req = 0;

        if (m_length == 0) {
            switch (m_cmd[0]) {
            default:
                this->EnterStatusPhase();
                break;

            case SCSICommand_Write:
                {
                    bool success = false;
                    if (HardDiskImage *hd = this->GetHardDisk(m_lun)) {
                        if (hd->WriteSector(m_buffer, m_next - 1)) {
                            success = true;
                        }
                    }

                    if (!success) {
                        m_status = m_lun << 5 | 2; //???
                        this->EnterStatusPhase();
                    }
                }
                break;

            case SCSICommand_ModeSelect6:
                // Is this actually useful?
                break;

            case SCSICommand_WriteExtended:
            case SCSICommand_WriteAndVerify:
                break;
            }

            ASSERT(m_blocks > 0);
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

void SCSI::EnterBusFreePhase() {
    m_phase = SCSIPhase_BusFree;

    m_status_register.value = 0;

    memset(this->leds, 0, sizeof this->leds);
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
    m_status_register.bits.cd = 0;
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
        this->leds[m_lun] = true;
    }

    TRACE("SCSI - Execute: LUN=%u CMD=%s (%u; 0x%x)\n", m_lun, GetSCSICommandEnumName(m_cmd[0]), m_cmd[0], m_cmd[0]);

    switch (m_cmd[0]) {
    default:
        m_status = m_lun << 5 | 2;
        m_message = 0;
        this->EnterStatusPhase();
        break;

    case SCSICommand_TestUnitReady: // 0x00 X.131 p62
        {
            if (this->GetHardDisk(m_lun)) {
                this->EnterCheckConditionStatusPhase();
                break;
            }

            this->EnterGoodStatusPhase();
        }
        break;

    case SCSICommand_RequestSense: // 0x03 X.131 p63
        {
            uint8_t size = m_cmd[4];
            if (size == 0) {
                size = 4;
            }

            memset(m_buffer, 0, size);
            if (m_code == 0x21) {
                m_buffer[0] = 0x21;
                m_buffer[1] = (uint8_t)(m_sector >> 16);
                m_buffer[2] = (uint8_t)(m_sector >> 8);
                m_buffer[3] = (uint8_t)(m_sector >> 0);
            }

            m_code = 0;
            m_sector = 0;

            if (size > 0) {
                m_offset = 0;
                m_blocks = 1;
                m_phase = SCSIPhase_Read;
                m_status_register.bits.io = 1;
                m_status_register.bits.cd = 0;
                m_status = m_lun << 5;
                m_message = 0;
                m_status_register.bits.req = 1;
            } else {
                m_status = m_lun << 5 | 2;
                m_message = 0;
                this->EnterStatusPhase();
            }
        }
        break;

        //case SCSICommand_FormatUnit: // 0x04 p87
        //    this->ExecuteFormatUnit();
        //    break;

    case SCSICommand_Read: // 0x08 X.131 p95
        {
            HardDiskImage *hd = this->GetHardDisk(m_lun);
            if (!hd) {
                this->EnterCheckConditionStatusPhase();
                break;
            }

            uint32_t lba = (m_cmd[1] & 0x1f) << 16 | m_cmd[2] << 8 | m_cmd[3];
            m_blocks = m_cmd[4];
            if (m_blocks == 0) {
                m_blocks = 256;
            }

            TRACE("SCSI - Read Sector: LBA=%u (0x%x)\n", m_lun, lba, lba);
            if (!hd->ReadSector(m_buffer, lba)) {
                this->EnterCheckConditionStatusPhase();
                break;
            }

            m_status = m_lun << 5;
            m_message = 0;

            m_offset = 0;
            m_next = lba + 1;
            m_phase = SCSIPhase_Read;
            m_status_register.bits.io = 1;
            m_status_register.bits.cd = 1;
            m_status_register.bits.req = 1;
        }
        break;

    case SCSICommand_Write: // 0x0a X.131 p96
        {
            HardDiskImage *hd = this->GetHardDisk(m_lun);
            if (!hd) {
                this->EnterCheckConditionStatusPhase();
                break;
            }

            uint32_t lba = (m_cmd[1] & 0x1f) << 16 | m_cmd[2] << 8 | m_cmd[3];
            m_blocks = m_cmd[4];
            if (m_blocks == 0) {
                m_blocks = 256;
            }

            m_status = m_lun << 5;
            m_message = 0;

            m_next = lba + 1;
            m_offset = 0;

            m_phase = SCSIPhase_Write;
            m_status_register.bits.cd = 0;
            m_status_register.bits.req = 1;
        }
        break;

    case SCSICommand_TranslateV: // 0x0f X.131 p86 - vender specific
        {
            uint32_t lba = (m_cmd[1] & 0x1f) << 16 | m_cmd[2] << 8 | m_cmd[3];

            Store32LE(&m_buffer[0], lba);

            m_length = 4;
            m_offset = 0;
            m_phase = SCSIPhase_Read;
            m_status_register.bits.io = 1;
            m_status_register.bits.cd = 0;

            m_status = m_lun << 5;
            m_message = 0;

            m_status_register.bits.req = 0;
        }
        break;

    case SCSICommand_ModeSelect6: // 0x15 X.131 p98
        {
            m_length = m_cmd[4];
            m_blocks = 1;
            m_status = m_lun << 5;
            m_message = 0;

            m_next = 0;
            m_offset = 0;
            m_phase = SCSIPhase_Write;
            m_status_register.bits.cd = 0;
            m_status_register.bits.req = 1;
        }
        break;

    case SCSICommand_ModeSense6: // 0x1a X.131 p108
        {
            HardDiskImage *hd = this->GetHardDisk(m_lun);
            if (!hd) {
                this->EnterCheckConditionStatusPhase();
                break;
            }

            HardDiskGeometry g = hd->GetGeometry();

            memset(m_buffer, 0, sizeof m_buffer);
            m_buffer[3] = 0x80;                             // Block length descriptor
            m_buffer[10] = 1;                               //Sector size bits 8-15
            m_buffer[12] = 1;                               //1 = soft sectors
            m_buffer[13] = (uint8_t)(g.num_cylinders >> 8); //Num cylinders bits 8-15
            m_buffer[14] = (uint8_t)(g.num_cylinders);      //Num cylinders bits 0-7
            m_buffer[15] = g.num_heads;                     //Num heads
            m_buffer[17] = 0x80;                            //RWCC bits 0-7
            m_buffer[19] = 0x80;                            //WPCC bits 0-7
            m_buffer[21] = 0;                               //Seek time = 3ms

            uint32_t size = m_cmd[4];
            if (size == 0) {
                size = 256;
            }

            m_offset = 0;
            m_blocks = 1;
            m_phase = SCSIPhase_Read;
            m_status_register.bits.io = 1;
            m_status_register.bits.cd = 1;
            m_status = m_lun << 5;
            m_message = 0;
            m_status_register.bits.req = 1;
        }
        break;

    case SCSICommand_StartOrStopUnit: // 0x1b X.131 p111
        {
            HardDiskImage *hd = this->GetHardDisk(m_lun);
            if (!hd) {
                this->EnterCheckConditionStatusPhase();
                break;
            }

            this->EnterGoodStatusPhase();
        }
        break;

    case SCSICommand_Verify: // 0x2f X.131 p121
        {
            HardDiskImage *hd = this->GetHardDisk(m_lun);
            if (!hd) {
                this->EnterCheckConditionStatusPhase();
                break;
            }

            uint32_t lba = (m_cmd[1] & 0x1f) << 16 | m_cmd[2] << 8 | m_cmd[3];
            if (!hd->IsValidLBA(lba)) {
                m_code = 0x21; //???
                m_sector = lba;
                this->EnterCheckConditionStatusPhase();
            }

            this->EnterGoodStatusPhase();
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::EnterGoodStatusPhase() {
    // X.131 p185

    TRACE("SCSI - Status=Good: LUN=%u\n", m_lun);
    m_status = m_lun << 5 | 0;

    m_message = 0;

    this->EnterStatusPhase();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::EnterCheckConditionStatusPhase() {
    // X.131 p185

    TRACE("SCSI - Status=CheckCondition: LUN=%u\n", m_lun);
    m_status = m_lun << 5 | 2;

    m_message = 0;

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

#endif
