#include <shared/system.h>
#include <beeb/Plus1.h>

#if ENABLE_ELECTRON

#include <6502/6502.h>
#include <shared/debug.h>
#include <beeb/BBCMicro.h>

#include <shared/enum_def.h>
#include <beeb/Plus1.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Plus1::SetADCHandler(ADCHandlerFn fn, void *context) {
    m_adc_handler_fn = fn;
    m_adc_handler_context = context;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Plus1::Write0(void *plus1_, M6502Word addr, uint8_t value) {
    (void)addr;
    auto plus1 = (Plus1 *)plus1_;

    plus1->m_adc_channel = value & 3;

    uint8_t adc_value;
    if (plus1->m_adc_handler_fn) {
        uint16_t full_adc_value = (*plus1->m_adc_handler_fn)(plus1->m_adc_channel, plus1->m_adc_handler_context);
        adc_value = (uint8_t)(full_adc_value >> 2); //ADC values are 10 bits
    } else {
        adc_value = ADCChannel::DEFAULT_VALUE;
    }

    plus1->m_adc_channels[plus1->m_adc_channel] = adc_value;
    plus1->m_adc_conversion_timer = 100; // 50 microseconds
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Plus1::Write1(void *plus1_, M6502Word addr, uint8_t value) {
    (void)addr;
    auto plus1 = (Plus1 *)plus1_;

    plus1->m_printer_byte = value;
    plus1->m_printer_byte_written = true;
    plus1->m_printer_busy_timer = 200; //100 microseconds
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Plus1::Write3(void *plus1_, M6502Word addr, uint8_t value) {
    (void)addr, (void)value;
    auto plus1 = (Plus1 *)plus1_;

    (void)plus1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t Plus1::Read0(void *plus1_, M6502Word addr) {
    (void)addr;
    auto plus1 = (Plus1 *)plus1_;

    return plus1->m_adc_channels[plus1->m_adc_channel];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t Plus1::Read2(void *plus1_, M6502Word addr) {
    (void)addr;
    auto plus1 = (Plus1 *)plus1_;

    Plus1Status status = plus1->GetStatus();
    return status.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint8_t Plus1::DebugRead0(const void *plus1_, M6502Word addr) {
    (void)addr;
    auto plus1 = (const Plus1 *)plus1_;

    return plus1->m_adc_channels[plus1->m_adc_channel];
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint8_t Plus1::DebugRead2(const void *plus1_, M6502Word addr) {
    (void)addr;
    auto plus1 = (const Plus1 *)plus1_;

    Plus1Status status = plus1->GetStatus();
    return status.value;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void Plus1::SetTrace(Trace *t) {
    m_trace = t;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Plus1::Update(PrinterBuffer *printer_buffer) {
    if (m_printer_byte_written) {
        if (printer_buffer) {
            printer_buffer->AddByte(m_printer_byte);
        }
        m_printer_byte_written = false;

        m_printer_busy_timer = 10;
    }

    if (m_printer_busy_timer > 0) {
        --m_printer_busy_timer;
    }

    if (m_adc_conversion_timer > 0) {
        --m_adc_conversion_timer;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Plus1Status Plus1::GetStatus() const {
    Plus1Status status = {};

    // Printer busy time is (a very unrealistic) 100 microsec.
    status.bits.printer_busy = m_printer_busy_timer > 0;

    // ADC conversion time is 50 microsec.
    status.bits.adc_busy = m_adc_conversion_timer > 0;

    // Joystick fire buttons.
    status.bits.fire2 = this->fire2;
    status.bits.fire1 = this->fire1;

    return status;
}

#endif