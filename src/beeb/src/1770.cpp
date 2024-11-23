#include <shared/system.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <beeb/1770.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <beeb/DiscInterface.h>
#include <beeb/Trace.h>

#include <shared/enum_def.h>
#include <beeb/1770.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(1770, "1770", "1770", &log_printer_stdout_and_debugger, false);
LOG_TAGGED_DEFINE(1770nd, "1770", "1770", &log_printer_stdout, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE

#define HEX_DUMP_TRACE_LEN (16384u)

#define TRACE(...)                                                     \
    BEGIN_MACRO {                                                      \
        if (m_trace) {                                                 \
            m_trace->AllocStringf(TraceEventSource_Host, __VA_ARGS__); \
        }                                                              \
    }                                                                  \
    END_MACRO

#define BEGIN_LOG_TRACE(OBJ, N)                                                                                   \
    if (Trace *t = (OBJ)->m_trace) {                                                                              \
        Log log_value("", (OBJ)->m_trace->GetLogPrinter(TraceEventSource_Host, (N)), 1), *const log = &log_value; \
        {
#define END_LOG_TRACE() \
    }                   \
    t->FinishLog(log);  \
    }                   \
    else BEGIN_MACRO {  \
    }                   \
    END_MACRO

#define TRACE_STATE(OBJ, PREFIX)        \
    BEGIN_LOG_TRACE(OBJ, 1000) {        \
        log->f("1770 - %s", (PREFIX));  \
        (OBJ)->Print1770Registers(log); \
    }                                   \
    END_LOG_TRACE()

#else

#define BEGIN_LOG_TRACE(OBJ, N) \
    if constexpr (false) {      \
        Log *log = nullptr;     \
        {
#define END_LOG_TRACE() \
    }                   \
    }

#define TRACE(...) ((void)0)
#define TRACE_STATE(X, Y) ((void)0)

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The difference between the 1772 and the 1770 is the timings.

const int WD1770::STEP_RATES_MS_1770[] = {6, 12, 20, 30};
static const int SETTLE_uS_1770 = 30000;

// The data sheet has these as (2,3,5,6), but just about every other
// reference has 2,3,6,12.
const int WD1770::STEP_RATES_MS_1772[] = {2, 3, 6, 12};
//static const int SETTLE_uS_1772=30000;

// Time between each byte when doing a read or write.
//
// This is what model-b used, and I don't remember where the value
// came from!
static const int uS_PER_BYTE = 64;

// Assuming 300rpm.
#define INDEX_PULSES_uS(N) ((N)*200000)

static const uint8_t STEP_IN = 0;
static const uint8_t STEP_OUT = 1;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

WD1770Handler::WD1770Handler() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

WD1770Handler::~WD1770Handler() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::SpinUp() {
    m_handler->SpinUp();
    m_status.bits.motor_on = 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::SpinDown() {
    m_handler->SpinDown();
    m_status.bits.motor_on = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int WD1770::GetStepRateMS(uint8_t index) const {
    ASSERT(index < 4);

    if (m_is1772) {
        return STEP_RATES_MS_1772[index];
    } else {
        return STEP_RATES_MS_1770[index];
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::SetState(WD1770State state) {
    m_state = state;
    m_state_time = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

WD1770::WD1770() {
    this->Reset();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::ResetStatusRegister() {
    uint8_t motor_was_on = m_status.bits.motor_on;

    m_status.value = 0;
    m_status.bits.motor_on = motor_was_on;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::Reset() {
    this->ResetStatusRegister();

    m_command.value = 0;
    //m_track=0;
    //m_sector=0;
    m_data = 0;

    m_pins.value = 0;

    m_state = WD1770State_BeginIdle;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::SetHandler(WD1770Handler *handler) {
    m_handler = handler;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::Print1770Registers(Log *log) {
    log->f("%s T%02u S%02u +%03zu (+0x%02zX); Cmd: 0x%02X; Status: 0x%02X",
           GetWD1770StateEnumName(m_state),
           m_track,
           m_sector,
           m_offset, m_offset,
           m_command.value,
           m_status.value);

    if (m_status.value != 0) {
        log->f(" (");

        const char *bits[] = {"Bu", "D/I", "L/0", "CRC", "RNF", "R/S", "WP", "Mo"};
        int any = 0;

        for (size_t i = 0; i < 8; ++i) {
            if (m_status.value & 1 << i) {
                if (any) {
                    log->f(" ");
                }

                log->f("%s", bits[i]);
                any = 1;
            }
        }

        log->f(")");
    }

    log->f("; Data: %d (0x%02X", m_data, m_data);

    if (isprint(m_data)) {
        log->f("; '%c'", m_data);
    }

    log->f("); DRQ=%d INTRQ=%d", m_pins.bits.drq, m_pins.bits.intrq);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static void PrintValueHeader(WD1770 *fdc,Log *log,const char *action,uint8_t value) {
//    log->f("1770 - %s: ",action);
//    this->Print1770Registers(log);
//    log->f("\n");
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::SetDRQ(bool value) {
    m_pins.bits.drq = value;
    m_status.bits.drq_or_idx = !!value;

    if (value) {
        TRACE_STATE(this, "SetDRQ: ");
    }
}

void WD1770::SetINTRQ(bool value) {
    if (m_no_intrq) {
        m_pins.bits.intrq = 0;
    } else {
        m_pins.bits.intrq = value;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::DoSpinUp(int h, WD1770State state) {
    if (h == 0 && !m_status.bits.motor_on) {
        this->SpinUp();
        m_wait_us = INDEX_PULSES_uS(6);
        this->SetState(WD1770State_WaitForSpinUp);
        m_next_state = state;
        m_status.bits.deleted_or_spinup = 0;
    } else {
        this->SetState(state);
        m_status.bits.deleted_or_spinup = 1;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::DoTypeI(WD1770State state) {
    LOGF(1770, "%s: state=%s: u=%d h=%d v=%d r=%d (%d ms) (track=%u)\n",
         __func__,
         GetWD1770StateEnumName(state),
         m_command.bits_step.u, //bogus when restore/seek... but what can you do?
         m_command.bits_i.h,
         m_command.bits_i.v,
         m_command.bits_i.r,
         this->GetStepRateMS(m_command.bits_i.r),
         m_track);

    m_status.bits.busy = 1;
    m_status.bits.crc_error = 0;
    m_status.bits.rnf = 0;

    this->DoSpinUp(m_command.bits_i.h, state);
}

void WD1770::DoTypeII(WD1770State state) {
    LOGF(1770, "%s: state=%s: m=%d h=%d E=%d p=%d a0=%d (track=%u; sector=%u)\n",
         __func__,
         GetWD1770StateEnumName(state),
         m_command.bits_ii.m,
         m_command.bits_ii.h,
         m_command.bits_ii.e,
         m_command.bits_ii.p,
         m_command.bits_ii.a0,
         m_track,
         m_sector);

    m_status.bits.busy = 1;              //set busy
    this->SetDRQ(0);                     //reset DRQ
    m_status.bits.lost_or_track0 = 0;    //reset lost data
    m_status.bits.rnf = 0;               //reset record not found
    m_status.bits.deleted_or_spinup = 0; //reset status bit 5
    m_status.bits.write_protect = 0;     //reset status bit 6
    this->SetINTRQ(0);                   //reset intrq
    m_status.bits.crc_error = 0;
    m_offset = 0;

    this->DoSpinUp(m_command.bits_ii.h, state);
}

void WD1770::DoTypeIII(WD1770State state) {
    LOGF(1770, "%s: state=%s: h=%d E=%d p=%d _=%d (track=%u)\n",
         __func__,
         GetWD1770StateEnumName(state),
         m_command.bits_iii.h,
         m_command.bits_iii.e,
         m_command.bits_iii.p,
         m_command.bits_iii._,
         m_track);

    this->ResetStatusRegister();
    m_status.bits.busy = 1;
    this->SetDRQ(0);

    this->DoSpinUp(m_command.bits_iii.h, state);
}

void WD1770::DoTypeIV() {
    LOGF(1770, "%s: state=%s: i3=%d i2=%d i10=%d\n",
         __func__,
         GetWD1770StateEnumName(m_state),
         m_command.bits_iv.immediate,
         m_command.bits_iv.index,
         m_command.bits_iv._);

    if (m_status.bits.busy) {
        m_status.bits.busy = 0;
        this->SetState(WD1770State_BeginIdle);
    } else {
        if (m_command.bits_iv.immediate) {
            this->SetState(WD1770State_ForceInterrupt);
        } else if (m_command.bits_iv.index) {
            // "Used by Superior Collection *INIT command", says my
            // comments from model-b...
            m_wait_us = INDEX_PULSES_uS(1);
            this->SetState(WD1770State_Wait);
            m_next_state = WD1770State_ForceInterrupt;
        } else if ((m_command.value & 0x0f) == 0) {
            this->SetState(WD1770State_ForceInterrupt);
        }
    }
}

void WD1770::Write0(void *fdc_, M6502Word addr, uint8_t value) {
    auto fdc = (WD1770 *)fdc_;
    (void)addr;

    LOGF(1770, "Write command: 0x%02X\n", value);

    if (fdc->m_status.bits.busy) {
        // Busy. Accept $d0 only.
        if ((value & 0xf0) != 0xd0) {
            LOGF(1770, "Busy - refusing new command\n");
            return;
        }
    }

    fdc->m_command.value = value;
    fdc->SetINTRQ(0); //reset intrq

    switch (fdc->m_command.value & 0xf0) {
    case 0x00:
        fdc->m_restore_count = 0;
        fdc->DoTypeI(WD1770State_Restore);
        break;

    case 0x10:
        fdc->DoTypeI(WD1770State_Seek);
        break;

    case 0x20:
    case 0x30:
        fdc->m_next_state = WD1770State_FinishStep;
        fdc->DoTypeI(WD1770State_Step);
        break;

    case 0x40:
    case 0x50:
        fdc->DoTypeI(WD1770State_StepIn);
        break;

    case 0x60:
    case 0x70:
        fdc->DoTypeI(WD1770State_StepOut);
        break;

    case 0x80:
    case 0x90:
        fdc->DoTypeII(WD1770State_ReadSector);
        break;

    case 0xa0:
    case 0xb0:
        // Type II - Write Sector
        fdc->DoTypeII(WD1770State_WriteSector);
        break;

    case 0xc0:
        // Type III - Read Address
        fdc->DoTypeIII(WD1770State_ReadAddressFindSector);
        break;

    case 0xd0:
        // Type IV - Force Interrupt
        fdc->DoTypeIV();
        break;

    case 0xe0:
        // Type III - Read Track
        fdc->DoTypeIII(WD1770State_ReadTrack);
        break;

    case 0xf0:
        // Type III - Write Track
        fdc->DoTypeIII(WD1770State_WriteTrack);
        break;
    }

    TRACE_STATE(fdc, "Write Command Register: ");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::Write1(void *fdc_, M6502Word addr, uint8_t value) {
    auto fdc = (WD1770 *)fdc_;
    (void)addr;

#if WD1770_VERBOSE_WRITES
    LOGF(1770nd, "Write track register: was %u, now %u\n", fdc->m_track, value);
#endif
    fdc->m_track = value;

    TRACE_STATE(fdc, "Write Track Register: ");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::Write2(void *fdc_, M6502Word addr, uint8_t value) {
    auto fdc = (WD1770 *)fdc_;
    (void)addr;

#if WD1770_VERBOSE_WRITES
    LOGF(1770nd, "Write sector register: was %u, now %u\n", fdc->m_track, value);
#endif
    fdc->m_sector = value;

    TRACE_STATE(fdc, "Write Sector Register: ");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::Write3(void *fdc_, M6502Word addr, uint8_t value) {
    auto fdc = (WD1770 *)fdc_;
    (void)addr;

    fdc->SetDRQ(0);

    fdc->m_data = value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t WD1770::Read0(void *fdc_, M6502Word addr) {
    auto fdc = (WD1770 *)fdc_;
    (void)addr;

    fdc->SetINTRQ(0); //reset intrq

    return fdc->m_status.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t WD1770::Read1(void *fdc_, M6502Word addr) {
    auto fdc = (WD1770 *)fdc_;
    (void)addr;

    return fdc->m_track;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t WD1770::Read2(void *fdc_, M6502Word addr) {
    auto fdc = (WD1770 *)fdc_;
    (void)addr;

    return fdc->m_sector;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t WD1770::Read3(void *fdc_, M6502Word addr) {
    auto fdc = (WD1770 *)fdc_;
    (void)addr;

    fdc->SetDRQ(0);

    return fdc->m_data;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::Wait(int us, WD1770State next_state) {
    m_wait_us = us;
    m_state = WD1770State_Wait;
    m_next_state = next_state;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// If the command's E bit is set, delay 30ms and go to next_state.
// Otherwise, go to next_state immediately.
void WD1770::DoTypeIIOrTypeIIIDelay(WD1770State next_state) {
#if WD1770_SAVE_SECTOR_DATA
    memset(m_sector_data, 0, sizeof m_sector_data);
#endif

    if (m_command.bits_ii.e) {
        this->Wait(30000, next_state);
    } else {
        m_state = next_state;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int WD1770::DoTypeIIFindSector() {
    //if(!m_drive) {
    //    goto rnf;
    //}

    uint8_t side;
    uint8_t track;
    if (!m_handler->GetSectorDetails(&track, &side, &m_sector_size, m_sector, m_dden)) {
        goto rnf;
    }

    if (track != m_track) {
        goto rnf;
    }

#if WD1770_SAVE_SECTOR_DATA
    if (m_sector_size > sizeof m_sector_data) {
        goto rnf;
    }
#endif

    m_status.bits.deleted_or_spinup = 0; //not deleted data

    return 1;

rnf:
    this->Wait(INDEX_PULSES_uS(6), WD1770State_RecordNotFound);
    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Advance to the next byte in a Type II command.
//
// If there are still more bytes to go in this sector, go to
// next_byte_state.
//
// If the sector is done, and it's a multi-sector command, advance to
// the next sector and go to next_sector_state.
//
// Otherwise, FinishCommand.
void WD1770::DoTypeIINextByte(WD1770State next_byte_state, WD1770State next_sector_state) {
    ++m_offset;
    if (m_offset >= m_sector_size) {
        m_offset = 0;

#if WD1770_DUMP_WRITTEN_SECTOR || WD1770_DUMP_READ_SECTOR
        LOGF(1770nd, "T%02u S%02u: state=%s\n", m_track, m_sector, GetWD1770StateEnumName(m_state));
        LOGF(1770nd, "    ");
        LOGI(1770nd);
        LogDumpBytes(&LOG(1770nd), m_sector_data, m_sector_size);
#endif

#if BBCMICRO_TRACE
        BEGIN_LOG_TRACE(this, HEX_DUMP_TRACE_LEN) {
            ASSERT(m_offset < sizeof m_sector_data);
            log->f("1770 - got sector: ");
            this->Print1770Registers(log);
            log->f("\n");
            LogDumpBytes(log, m_sector_data, m_sector_size);
        }
        END_LOG_TRACE();
#endif

        if (m_command.bits_ii.m) {
            ++m_sector;

            this->SetState(next_sector_state);
        } else {
            this->SetState(WD1770State_FinishCommand);
        }
    } else {
        this->SetState(next_byte_state);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::UpdateTrack0Status() {
    m_status.bits.lost_or_track0 = m_handler->IsTrack0();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static void FinishTypeI() {
//    if(m_command.bits_i.v) {
//        this->SetState(WD1770State_Settle);
//    } else {
//        this->FinishCommand();
//    }
//}

void WD1770::SetDDEN(bool dden) {
    m_dden = dden;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void WD1770::SetDrive(DiscDrive *drive) {
//    m_drive=drive;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::SetTrace(Trace *trace) {
#if BBCMICRO_TRACE

    m_trace = trace;

#else

    (void)trace;

#endif
    //TRACE("1770 - %s ($%04X = Control; $%04X = Status/Cmd;Track;Sector;Data)",m_disc_interface->name,m_disc_interface->control_address,m_disc_interface->fdc_address);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

WD1770::Pins WD1770::Update() {
    WD1770State old_state = m_state;

    switch (m_state) {
    case WD1770State_BeginIdle:
        {
            if (m_status.bits.motor_on) {
                this->Wait(INDEX_PULSES_uS(10), WD1770State_SpinDown);
            } else {
                m_state = WD1770State_IdleWithMotorOff;
            }
        }
        break;

    default:
        ASSERT(0);
        // fall through
    case WD1770State_IdleWithMotorOff:
        {
            // Nothing...
        }
        break;

    case WD1770State_SpinDown:
        {
            this->SpinDown();

            m_status.bits.motor_on = 0;

            m_state = WD1770State_IdleWithMotorOff;
        }
        break;

    case WD1770State_Restore:
        {
            if (m_restore_count++ == 255 && m_command.bits_i.v) {
                this->SetState(WD1770State_RecordNotFound);
            } else if (m_handler->IsTrack0()) {
                m_track = 0;
                this->SetState(WD1770State_FinishTypeI);
            } else {
                m_direction = STEP_OUT;
                m_next_state = WD1770State_Restore;
                this->SetState(WD1770State_Step);
            }
        }
        break;

    case WD1770State_Seek2:
        {
            ASSERT(m_direction == STEP_IN || m_direction == STEP_OUT);
            if (m_direction == STEP_IN) {
                ++m_track;
            } else {
                --m_track;
            }
        }
        // fall through
    case WD1770State_Seek:
        {
            if (m_track < m_data) {
                m_direction = STEP_IN;
                this->SetState(WD1770State_Step);
                m_next_state = WD1770State_Seek2;
            } else if (m_track > m_data) {
                m_direction = STEP_OUT;
                this->SetState(WD1770State_Step);
                m_next_state = WD1770State_Seek2;
            } else {
                this->SetState(WD1770State_FinishTypeI);
            }
        }
        break;

    case WD1770State_StepIn:
        m_direction = STEP_IN;
        this->SetState(WD1770State_Step);
        m_next_state = WD1770State_FinishStep;
        break;

    case WD1770State_StepOut:
        m_direction = STEP_OUT;
        this->SetState(WD1770State_Step);
        m_next_state = WD1770State_FinishStep;
        break;

    case WD1770State_FinishStep:
        {
            if (m_command.bits_step.u) {
                ASSERT(m_direction == STEP_IN || m_direction == STEP_OUT);
                if (m_direction == STEP_IN) {
                    if (m_track < 255) {
                        ++m_track;
                    }
                } else {
                    if (m_track > 0) {
                        --m_track;
                    }
                }
            }

            this->SetState(WD1770State_FinishTypeI);
        }
        break;

    case WD1770State_Step:
        {
            ASSERT(m_next_state != WD1770State_BeginIdle);
            int step_rate_ms = this->GetStepRateMS(m_command.bits_i.r);

            ASSERT(m_direction == STEP_IN || m_direction == STEP_OUT);
            if (m_direction == STEP_IN) {
                m_handler->StepIn(step_rate_ms);
            } else {
                m_handler->StepOut(step_rate_ms);
            }

            this->Wait(step_rate_ms * 1000, m_next_state);
        }
        break;

    case WD1770State_RecordNotFound:
        {
            m_status.bits.rnf = 1;

            // indicate error was in the ID field.
            m_status.bits.crc_error = 1;

            this->SetState(WD1770State_FinishCommand);
        }
        break;

    case WD1770State_WaitForSpinUp:
        // fall through
    case WD1770State_Wait:
        {
            ASSERT(m_wait_us >= 0);
            ++m_state_time;
            if (m_state_time >= m_wait_us) {
                m_wait_us = -1;
                this->SetState(m_next_state);
                m_next_state = WD1770State_BeginIdle;

                if (old_state == WD1770State_WaitForSpinUp) {
                    m_status.bits.deleted_or_spinup = 1;
                }
            }
        }
        break;

    case WD1770State_FinishTypeI:
        {
            this->UpdateTrack0Status();

            if (m_command.bits_i.v) {
                this->Wait(SETTLE_uS_1770, WD1770State_FinishCommand);
            } else {
                m_state = WD1770State_FinishCommand;
            }
        }
        break;

    case WD1770State_ForceInterrupt:
        {
            this->ResetStatusRegister();

            //m_status.bits.deleted_or_spinup=1;
            this->UpdateTrack0Status();

            this->SetINTRQ(m_command.bits_iv.immediate || m_command.bits_iv.index);
            this->SetState(WD1770State_BeginIdle);
        }
        break;

    case WD1770State_FinishCommand:
        {
            ASSERT(m_status.bits.busy);
            m_status.bits.busy = 0;
            this->SetINTRQ(1);

            TRACE_STATE(this, "");

            this->SetState(WD1770State_BeginIdle);
        }
        break;

    case WD1770State_ReadSector:
        {
            this->DoTypeIIOrTypeIIIDelay(WD1770State_ReadSectorFindSector);
        }
        break;

    case WD1770State_ReadSectorFindSector:
        {
            if (!this->DoTypeIIFindSector()) {
                break;
            }

            // Watford DDFS reads by initiating a multi-sector read,
            // counting sectors read, then doing this when it's got
            // the data it wants:
            //
            // <pre>
            // 0D43: ldy #$60                A=00 X=FF Y=60 S=DC P=nvdIzC (D=60)
            // 0D45: dey                     A=00 X=FF Y=5F S=DC P=nvdIzC (D=D0)
            // 0D46: bne $0D45               A=00 X=FF Y=5F S=DC P=nvdIzC (D=01)
            // 0D48: lda #$D0                A=D0 X=FF Y=00 S=DC P=NvdIzC (D=D0)
            // 0D4A: sta $FE84               A=D0 X=FF Y=00 S=DC P=NvdIzC (D=D0)
            // </pre>
            //
            // The loop at D43 is $5f*(2+3)+(2+2)=$1df=479 cycles, or
            // 240 usec.
            //
            // So delay a bit more than that between sectors.

            this->Wait(300, WD1770State_ReadSectorReadByte);
        }
        break;

    case WD1770State_ReadSectorReadByte:
        {
            //if(!m_drive) {
            //    this->SetState(WD1770State_RecordNotFound);
            //    break;
            //}

            if (!m_handler->GetByte(&m_data, m_sector, m_offset)) {
                this->SetState(WD1770State_RecordNotFound);
                break;
            }

#if WD1770_VERBOSE_READ_SECTOR
            LOGF(1770nd, "T%02u S%02u +%03zu (0x%02zx): %d 0x%x", m_track, m_sector, m_offset, m_offset, m_data, m_data);
            if (isprint(m_data)) {
                LOGF(1770nd, " '%c'", (char)m_data);
            } else {
                LOGF(1770nd, "    ");
            }

            {
                LOGF(1770nd, "[");
                int frac = (int)(m_offset / (double)m_sector_size * 50);
                for (int i = 0; i < 50; ++i) {
                    LOGF(1770nd, "%c", i < frac ? '*' : '.');
                }
                LOGF(1770nd, "]");
            }

            LOGF(1770nd, "\n");
#endif

            if (m_pins.bits.drq == 1) {
#if WD1770_VERBOSE_READ_SECTOR
                LOGF(1770nd, "(DRQ still set; data was lost.)\n");
#endif
                m_status.bits.lost_or_track0 = 1;
            }

#if WD1770_SAVE_SECTOR_DATA
            ASSERT(m_offset < sizeof m_sector_data);
            m_sector_data[m_offset] = m_data;
#endif

            this->SetDRQ(1);

            this->Wait(uS_PER_BYTE, WD1770State_ReadSectorNextByte);
        }
        break;

    case WD1770State_ReadSectorNextByte:
        {
            this->DoTypeIINextByte(WD1770State_ReadSectorReadByte, WD1770State_ReadSectorFindSector);
        }
        break;

    case WD1770State_WriteSector:
        {
            this->DoTypeIIOrTypeIIIDelay(WD1770State_WriteSectorFindSector);
        }
        break;

    case WD1770State_WriteSectorFindSector:
        {
            if (m_handler->IsWriteProtected()) {
                m_state = WD1770State_WriteProtectError;
                break;
            }

            if (!this->DoTypeIIFindSector()) {
                break;
            }

            this->Wait(300, WD1770State_WriteSectorSetFirstDRQ);
        }
        break;

    case WD1770State_WriteSectorSetFirstDRQ:
        {
            this->SetDRQ(1);
            this->Wait(9 * uS_PER_BYTE, WD1770State_WriteSectorReceiveFirstDataByte);
        }
        break;

    case WD1770State_WriteSectorReceiveFirstDataByte:
        {
            if (m_pins.bits.drq == 1) {
                // Cancel command due to underrun.
                m_status.bits.lost_or_track0 = 1;
                this->SetState(WD1770State_FinishCommand);
                goto done;
            }

            // Need to handle double density properly here.
            this->Wait(1 * uS_PER_BYTE, WD1770State_WriteSectorWriteByte);
        }
        break;

    case WD1770State_WriteSectorWriteByte:
        {
            uint8_t value;
            if (m_pins.bits.drq == 1) {
                // Write zero byte and set data lost. (This will never
                // happen for the first byte, since this case is
                // checked for in WriteSectorReceiveFirstDataByte.)
                value = 0;
                m_status.bits.lost_or_track0 = 1;
            } else {
                value = m_data;
            }

#if WD1770_VERBOSE_WRITE_SECTOR
            LOGF(1770nd, "T%02u S%02u +%03zu (0x%02zx): %d 0x%x", m_track, m_sector, m_offset, m_offset, value, value);
            if (isprint(value)) {
                LOGF(1770nd, " '%c'", (char)value);
            }
            LOGF(1770nd, "\n");

            if (m_status.bits.lost_or_track0) {
                LOGF(1770nd, "(DRQ still set; data was lost.)\n");
            }
#endif

#if WD1770_SAVE_SECTOR_DATA
            ASSERT(m_offset < sizeof m_sector_data);
            m_sector_data[m_offset] = value;
#endif

            if (!m_handler->SetByte(m_sector, m_offset, value)) {
                value = 0;
                m_status.bits.lost_or_track0 = 1;

#if WD1770_VERBOSE_WRITE_SECTOR
                LOGF(1770nd, "(DiscDrive_SetByte failed.)\n");
#endif
            }

            this->DoTypeIINextByte(WD1770State_WriteSectorNextByte, WD1770State_WriteSectorFindSector);
        }
        break;

    case WD1770State_WriteSectorNextByte:
        {
            this->SetDRQ(1);

            this->Wait(uS_PER_BYTE, WD1770State_WriteSectorWriteByte);
        }
        break;

    case WD1770State_ReadAddress:
        {
            this->DoTypeIIOrTypeIIIDelay(WD1770State_ReadAddressFindSector);
        }
        break;

    case WD1770State_ReadAddressFindSector:
        {
            // This just picks up whichever sector comes round next - here,
            // assumed to be always sector 0.

            uint8_t track;
            uint8_t side;
            size_t size;
            if (!m_handler->GetSectorDetails(&track, &side, &size, 0, m_dden)) {
                this->SetState(WD1770State_RecordNotFound);
                break;
            }

            switch (size) {
            default:
                // ???
                m_state = WD1770State_RecordNotFound;
                goto done;

            case 128:
                m_address[3] = 0x00;
                break;

            case 256:
                m_address[3] = 0x01;
                break;

            case 512:
                m_address[3] = 0x02;
                break;

            case 1024:
                m_address[3] = 0x03;
                break;
            }

            m_address[0] = track;
            m_address[1] = side;
            m_address[2] = 0;
            // 3 set above
            m_address[4] = 0; //bogus CRC
            m_address[5] = 0; //bogus CRC

            m_sector = m_address[0];

            m_offset = 0;

            m_status.bits.deleted_or_spinup = 0;

            TRACE("1770 - Read Address: Track=%u, Side=%u, Sector=%u, Size=%u (%zu bytes), CRC1=%u, CRC2=%u\n",
                  m_address[0], m_address[1], m_address[2], m_address[3], size, m_address[4], m_address[5]);

            m_state = WD1770State_ReadAddressNextByte;
        }
        break;

    case WD1770State_ReadAddressNextByte:
        {
            if (m_pins.bits.drq == 1) {
                m_status.bits.lost_or_track0 = 1;
            }

            if (m_offset == sizeof m_address) {
                m_state = WD1770State_FinishCommand;
                goto done;
            }

            m_data = m_address[m_offset];

            this->SetDRQ(1);

            ++m_offset;

            this->Wait(uS_PER_BYTE, WD1770State_ReadAddressNextByte);
        }
        break;

    case WD1770State_ReadTrack:
    case WD1770State_WriteTrack:
        {
            this->DoTypeIIOrTypeIIIDelay(WD1770State_UnsupportedCommand);
        }
        break;

    case WD1770State_UnsupportedCommand:
        {
            m_status.bits.crc_error = 1;
            m_status.bits.rnf = 1;

            m_state = WD1770State_FinishCommand;
        }
        break;

    case WD1770State_WriteProtectError:
        {
            m_status.bits.write_protect = 1;

            m_state = WD1770State_FinishCommand;
        }
        break;
    }
done:;

    if (m_state != old_state) {
#if WD1770_VERBOSE_STATE_CHANGES
        LOGF(1770nd, "State change: %s -> %s\n", GetWD1770StateEnumName(old_state), GetWD1770StateEnumName(m_state));
#endif
    }

    return m_pins;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::Set1772(bool is1772) {
    m_is1772 = is1772;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void WD1770::SetNoINTRQ(bool no_intrq) {
    m_no_intrq = no_intrq;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
