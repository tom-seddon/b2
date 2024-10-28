#ifndef HEADER_39735FBEB51F4C25BF03DD5B20608254
#define HEADER_39735FBEB51F4C25BF03DD5B20608254

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

class Trace;
class TraceEventType;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SN76489 {
  public:
#if BBCMICRO_TRACE
#include <shared/pshpack1.h>
    struct WriteEvent {
        // Value written.
        uint8_t write_value;

        // Internal register number.
        uint16_t reg : 3;

        // New value for register.
        uint16_t reg_value : 10;
    };
#include <shared/poppack.h>

    static const TraceEventType WRITE_EVENT;
    static const TraceEventType UPDATE_EVENT;
#endif

    struct ChannelValues {
        // Frequency value, as set by the %1xx0xxxx and %0xxxxxxx
        // commands.
        uint16_t freq = 1023;

        // Volume, as set by the %1xx1xxxx command. 15=max.
        uint8_t vol = 15;
    };

    struct Output {
        uint8_t ch[4];
    };

    SN76489();

    void Reset(bool tone);
    void Fixup(const SN76489 *src);

    Output Update(bool write, uint8_t value);

#if BBCMICRO_TRACE
    void SetTrace(Trace *t);
#endif

  protected:
  private:
    struct Channel {
        ChannelValues values;

        // Output counter. Runs from 0-freq.
        uint16_t counter = 0;

        // Current output mask.
        uint8_t mask = 0xff;
    };

    // The part that is suitable for copying with a default
    // constructor or operator.
    struct State {
        Channel channels[4] = {};
        uint8_t reg = 0;
        uint8_t noise = 0;
        uint16_t noise_seed = 1 << 14;
        uint8_t noise_toggle = 1;
    };

    State m_state;
#if BBCMICRO_TRACE
    Trace *m_trace;
#endif

    uint8_t NextWhiteNoiseBit();
    uint8_t NextPeriodicNoiseBit();

    // Inconsistent layout - but this needs a bunch of other stuff from above...
  public:
#if BBCMICRO_TRACE
#include <shared/pshpack1.h>
    struct UpdateEvent {
        State state;
        Output output;
    };
#include <shared/poppack.h>
#endif

#if BBCMICRO_DEBUGGER
    friend class SN76489DebugWindow;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
