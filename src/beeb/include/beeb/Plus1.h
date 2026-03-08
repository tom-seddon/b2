#ifndef HEADER_F645675C20C54B4B9051DACEAA2AFEF3 // -*- mode:c++ -*-
#define HEADER_F645675C20C54B4B9051DACEAA2AFEF3

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if ENABLE_ELECTRON

#include <shared/enum_decl.h>
#include "Plus1.inl"
#include <shared/enum_end.h>

union M6502Word;
class PrinterBuffer;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The Plus 1 has a 0844 ADC, simple enough that it doesn't get its own struct.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Plus1StatusBits {
    uint8_t _ : 4;
    uint8_t fire2 : 1;
    uint8_t fire1 : 1;
    uint8_t adc_eoc : 1;
    uint8_t printer_busy : 1;
};

union Plus1Status {
    uint8_t value;
    Plus1StatusBits bits;
};
CHECK_SIZEOF(Plus1Status, 1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Plus1 {
  public:
    bool fire1 = false;
    bool fire2 = false;

    // The handler function is deliberately compatible with ADC::HandlerFn. The
    // ADC0844 is 8-bit though so the bottom 8 bits of the result are ignored.
    typedef uint16_t (*ADCHandlerFn)(uint8_t channel, void *context);

    void SetADCHandlerFn(ADCHandlerFn fn, void *context);

    static void Write0(void *plus1, M6502Word addr, uint8_t value);
    static void Write1(void *plus1, M6502Word addr, uint8_t value);
    static void Write3(void *plus1, M6502Word addr, uint8_t value);

    static uint8_t Read0(void *plus1, M6502Word addr);
    static uint8_t Read2(void *plus1, M6502Word addr);

#if BBCMICRO_DEBUGGER
    static uint8_t DebugRead0(const void *plus1, M6502Word addr);
    static uint8_t DebugRead2(const void *plus1, M6502Word addr);
#endif

    void Update(PrinterBuffer *printer_buffer);

  protected:
  private:
    struct ADCChannel {
        static constexpr uint8_t DEFAULT_VALUE = 0x7c;

        uint8_t value = DEFAULT_VALUE;
        CycleCount start_time = {};
    };

    ADCHandlerFn m_adc_handler_fn = nullptr;
    void *m_adc_handler_context = nullptr;

    uint8_t m_adc_channels[4] = {};
    uint8_t m_adc_channel = 0;
    uint8_t m_adc_conversion_timer = 0;

    uint8_t m_printer_busy_timer = 0;
    bool m_printer_byte_written = false;
    uint8_t m_printer_byte = 0;

    Plus1Status GetStatus() const;

#if BBCMICRO_DEBUGGER
    friend class Plus1DebugWindow;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
