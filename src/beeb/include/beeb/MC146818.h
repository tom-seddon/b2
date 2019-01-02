#ifndef HEADER_955877FE3EF348158B5DE057F21184C7
#define HEADER_955877FE3EF348158B5DE057F21184C7

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Trace;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"
#include <time.h>
#include <vector>

#include <shared/enum_decl.h>
#include "MC146818.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MC146818 {
public:
    static const size_t RAM_SIZE=50;

#include <shared/pushwarn_bitfields.h>
    struct ABits {
        uint8_t rs:4;
        uint8_t dv:3;
        uint8_t uip:1;
    };
#include <shared/popwarn.h>

    union A {
        uint8_t value;
        ABits bits;
    };

#include <shared/pushwarn_bitfields.h>
    struct BBits {
        uint8_t dse:1;
        uint8_t mil:1;//24H
        uint8_t dm:1;
        uint8_t sqwe:1;
        uint8_t uie:1;
        uint8_t aie:1;
        uint8_t pie:1;
        uint8_t set:1;
    };
#include <shared/popwarn.h>

    union B {
        uint8_t value;
        BBits bits;
    };


#include <shared/pushwarn_bitfields.h>
    struct CBits {
        uint8_t _:4;
        uint8_t uf:1;
        uint8_t af:1;
        uint8_t pf:1;
        uint8_t irqf:1;
    };
#include <shared/popwarn.h>

    union C {
        uint8_t value;
        CBits bits;
    };

#include <shared/pushwarn_bitfields.h>
    struct DBits {
        uint8_t _:7;
        uint8_t vrt:1;
    };
#include <shared/popwarn.h>

    union D {
        uint8_t value;
        DBits bits;
    };

    struct RegisterBits {
        uint8_t seconds;
        uint8_t seconds_alarm;
        uint8_t minutes;
        uint8_t minutes_alarm;
        uint8_t hours;
        uint8_t hours_alarm;
        uint8_t day_of_week;
        uint8_t day_of_month;
        uint8_t month;
        uint8_t year;
        A a;
        B b;
        C c;
        D d;
        uint8_t ram[RAM_SIZE];
    };

    union Registers {
        uint8_t values[64];
        RegisterBits bits;
    };

    MC146818();

    MC146818(const MC146818 &)=default;
    MC146818(MC146818 &&)=default;

#if BBCMICRO_TRACE
    void SetTrace(Trace *t);
#endif

    // Sets time from the given tm in the current format.
    void SetTime(const tm *t);

    // Sets time in the current format.
    //
    // - year: 0-99 (0=1900)
    // - month: 1-12
    // - day_of_month: 1-31
    // - day_of_week: 1-7 (1=Sunday)
    // - hours_24h: 0-23
    // - minutes: 0-59
    // - seconds: 0-59
    void SetTime(int year,int month,int day_of_month,int day_of_week,int hours_24h,int minutes,int seconds);

    uint8_t Read();

    // Set the address register to VALUE.
    void SetAddress(uint8_t value);

    // Set one of the data registers to VALUE. The register address
    // comes from the address register.
    void SetData(uint8_t value);

    // Get pointer to first byte of the RAM_SIZE bytes of RAM.
    const uint8_t *GetRAM() const;

    // Set RAM contents. If DATA is the wrong size, the RTC RAM will
    // be zero-padded, or the excess ignored.
    void SetRAMContents(const std::vector<uint8_t> &data);

    // The clock runs at emulated speed, rather than being tied to the
    // host clock.
    void Update();
protected:
private:
    Registers m_regs={};
    uint8_t m_reg=0;
    uint32_t m_counter=0;
#if BBCMICRO_TRACE
    Trace *m_trace=nullptr;
#endif

    int GetTimeValue(uint8_t value);
    void SetTimeValue(uint8_t *dest,int value);
    void SetClampedTimeValue(uint8_t *dest,int value,int min_value,int max_value);
    int IncTimeValue(uint8_t *value,int max_value,int reset_value);
    int GetHours();
    void SetHours(int h);
    int IncHours();
    int IncDay();
};

CHECK_SIZEOF(MC146818::Registers,64);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
