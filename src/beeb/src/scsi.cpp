#include <shared/system.h>
#include <beeb/scsi.h>

#if ENABLE_SCSI

#include <beeb/Trace.h>
#include <beeb/6502.h>
#include <shared/log.h>
#include <shared/debug.h>

#include <shared/enum_def.h>
#include <beeb/scsi.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Based on code by Jon Welch and Y. Tanaka

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

void SCSI::SetCPU(M6502 *cpu, uint32_t irq_flag) {
    ASSERT(m_cpu);
    ASSERT(irq_flag != 0);

    m_cpu = cpu;
    m_cpu_irq_flag = irq_flag;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::SetTrace(Trace *t) {
    m_trace = t;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t SCSI::ReadData() {
    switch (m_phase) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case SCSIPhase_Status:
        {
            uint8_t data = m_status;
            m_status_register.bits.req = 0;
            this->Message();
            return data;
        }
        break;

    case SCSIPhase_Message:
        {
            uint8_t data = m_message;
            m_status_register.bits.req = 0;
            this->BusFree();
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
                    this->Status();
                    return data;
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SCSI::WriteData(uint8_t value) {
}

#endif