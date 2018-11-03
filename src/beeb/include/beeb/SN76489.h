#ifndef HEADER_39735FBEB51F4C25BF03DD5B20608254
#define HEADER_39735FBEB51F4C25BF03DD5B20608254

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SN76489 {
public:
    struct Output {
        int8_t ch[4];
    };

    SN76489();

    void Reset();
    void Fixup(const SN76489 *src);

    Output Update();
    void Write(uint8_t value);
protected:
private:
    static const uint16_t NOISE0;
    static const uint16_t NOISE1;
    static const uint16_t NOISE2;

    struct ToneChannelOutput {
        int8_t mul;
    };

#include <shared/pushwarn_bitfields.h>
    struct NoiseChannelOutput {
        uint8_t toggle:1;
        uint8_t value:1;
    };

#include <shared/popwarn.h>
    union ChannelOutput {
        uint8_t value;
        ToneChannelOutput tone;
        NoiseChannelOutput noise;
    };

    struct Channel {
        // Frequency value, as set by the %1xx0xxxx and %0xxxxxxx
        // commands.
        uint16_t freq=0;

        // Output counter. Runs from 0-freq.
        uint16_t counter=0;

        // Volume, as set by the %1xx1xxxx command. 15=max.
        uint8_t vol=0;

        // Current output value.
        ChannelOutput output={};
    };

    // The part that is suitable for copying with a default
    // constructor or operator.
    struct State {
        Channel channels[4]={};
        uint8_t reg=0;
        uint8_t noise=0;
        uint16_t noise_seed=1<<14;
        int8_t write_delay=0;
    };

    State m_state;
    const uint16_t *const m_noise_pointers[4]={&NOISE0,&NOISE1,&NOISE2,&m_state.channels[2].freq};

    uint8_t NextWhiteNoiseBit();
    uint8_t NextPeriodicNoiseBit();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
