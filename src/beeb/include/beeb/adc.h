#ifndef HEADER_D25D4C67FBA245129DD1B24A37ADC208 // -*- mode:c++ -*-
#define HEADER_D25D4C67FBA245129DD1B24A37ADC208

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <6502/6502.h>
#include "conf.h"

#if BBCMICRO_TRACE
class Trace;
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ADCCommandRegisterBits {
    uint8_t channel : 2;
    uint8_t flag : 1;
    uint8_t prec_10_bit : 1;
    uint8_t _ : 4;
};

union ADCCommandRegister {
    uint8_t value;
    ADCCommandRegisterBits bits;
};

static_assert(sizeof(ADCCommandRegister) == 1, "");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ADCStatusRegisterBits {
    uint8_t channel : 2;
    uint8_t flag : 1;
    uint8_t prec_10_bit : 1;
    uint8_t msb : 2;
    uint8_t not_busy : 1;
    uint8_t not_eoc : 1;
};

union ADCStatusRegister {
    uint8_t value;
    ADCStatusRegisterBits bits;
};

static_assert(sizeof(ADCStatusRegister) == 1, "");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ADC {
  public:
    ADC();

    static void Write0(void *adc, M6502Word addr, uint8_t value);
    static void Write1(void *adc, M6502Word addr, uint8_t value);
    static void Write2(void *adc, M6502Word addr, uint8_t value);
    static void Write3(void *adc, M6502Word addr, uint8_t value);

    static uint8_t Read0(void *adc, M6502Word addr);
    static uint8_t Read1(void *adc, M6502Word addr);
    static uint8_t Read2(void *adc, M6502Word addr);
    static uint8_t Read3(void *adc, M6502Word addr);

    // Returns ~EOC.
    bool Update();

#if BBCMICRO_TRACE
    void SetTrace(Trace *t);
#endif

    typedef uint16_t (*HandlerFn)(uint8_t channel, void *context);
    void SetHandler(HandlerFn fn, void *context);

  protected:
  private:
    HandlerFn m_handler_fn = nullptr;
    void *m_handler_context = nullptr;
    ADCStatusRegister m_status = {};
    uint16_t m_avalue = 0;
    M6502Word m_dvalue = {}; //result of previous conversion
    uint16_t m_timer = 0;

#if BBCMICRO_TRACE
    Trace *m_trace = nullptr;
#endif

#if BBCMICRO_DEBUGGER
    friend class ADCDebugWindow;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
