#include <shared/system.h>
#include <beeb/MC146818.h>
#include <shared/log.h>
#include <string.h>
#include <beeb/Trace.h>
#include <shared/debug.h>

#include <shared/enum_def.h>
#include <beeb/MC146818.inl>
#include <shared/enum_end.h>

#define LOGGING 1

#if LOGGING
LOG_TAGGED_DEFINE(RTC, "nvram", "RTC...", &log_printer_stdout_and_debugger, false);
#endif

#define COUNTER (1000 * 1000)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint8_t g_ro_masks[64];

struct InitROMasks {
    InitROMasks() {
        g_ro_masks[MC146818Register_A] = 0x80;
        g_ro_masks[MC146818Register_C] = 0xff;
        g_ro_masks[MC146818Register_D] = 0xff;
    }
};

static InitROMasks g_init_ro_masks;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
static void GetRegisterDescription(char *d, size_t d_size, uint8_t reg) {
    const char *name = GetMC146818RegisterEnumName(reg);
    if (name[0] != '?') {
        snprintf(d, d_size, "%u ($%02X; %s)", reg, reg, name);
    } else {
        snprintf(d, d_size, "%u ($%02X; RAM+%u)", reg, reg, reg - MC146818Register_FirstRAMByte);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MC146818::MC146818() {
    // This is the format the MOS uses. This is a bit of a cheat and
    // just ensures that calling SetTime before the MOS has started up
    // sets the time in the right format.
    m_regs.bits.b.bits.mil = true;

    // 5/12/78 is Acorn's birthday.
    this->SetTime(78, 12, 5, 3, 12, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void MC146818::SetTrace(Trace *t) {
    m_trace = t;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int MC146818::GetTimeValue(uint8_t value) {
    if (m_regs.bits.b.bits.dm) {
        return value;
    } else {
        uint8_t l = value & 0xf, h = value >> 4;

        if (l > 9) {
            l = 9;
        }

        if (h > 9) {
            h = 9;
        }

        return h * 10 + l;
    }
}

void MC146818::SetTimeValue(uint8_t *dest, int value) {
    if (m_regs.bits.b.bits.dm) {
        *dest = (uint8_t)value;
    } else {
        *dest = (uint8_t)(value % 10 + value / 10 % 10 * 16);
    }
}

void MC146818::SetClampedTimeValue(uint8_t *dest, int value, int min_value, int max_value) {
    if (value < min_value) {
        value = min_value;
    } else if (value > max_value) {
        value = max_value;
    }

    this->SetTimeValue(dest, value);
}

// Increments time value as per the data mode. If greater than
// max_value, reset to reset_value. Returns true if value wrapped.
int MC146818::IncTimeValue(uint8_t *value, int max_value, int reset_value) {
    int n = this->GetTimeValue(*value);

    int wrap = 0;
    ++n;
    if (n > max_value) {
        n = reset_value;
        wrap = 1;
    }

    this->SetTimeValue(value, n);

    return wrap;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Hours are a pain.
int MC146818::GetHours() {
    if (m_regs.bits.b.bits.mil) {
        return this->GetTimeValue(m_regs.bits.hours);
    } else {
        int h = this->GetTimeValue(m_regs.bits.hours & 0x7f) - 1;

        if (m_regs.bits.hours & 0x80) {
            h += 12;
        }

        return h;
    }
}

void MC146818::SetHours(int h) {
    if (h < 0) {
        h = 0;
    } else if (h > 23) {
        h = 23;
    }

    if (m_regs.bits.b.bits.mil) {
        this->SetTimeValue(&m_regs.bits.hours, h);
    } else {
        this->SetTimeValue(&m_regs.bits.hours, 1 + h % 12);

        if (h >= 12) {
            m_regs.bits.hours |= 0x80;
        }
    }
}

int MC146818::IncHours() {
    int wrap = 0;
    int h = this->GetHours();

    ++h;
    if (h > 23) {
        h = 0;
        wrap = 1;
    }

    this->SetHours(h);

    return wrap;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t DAYS_PER_MONTH_N[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const uint8_t DAYS_PER_MONTH_L[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

int MC146818::IncDay() {
    int year = this->GetTimeValue(m_regs.bits.year);

    int month = this->GetTimeValue(m_regs.bits.month) - 1;
    if (month < 0) {
        month = 0;
    } else if (month > 11) {
        month = 11;
    }

    const uint8_t *days_per_month = (year & 3) == 0 ? DAYS_PER_MONTH_L : DAYS_PER_MONTH_N;

    return this->IncTimeValue(&m_regs.bits.day_of_month, days_per_month[month], 1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC146818::SetTime(const tm *t) {
    this->SetTime(t->tm_year % 100, 1 + t->tm_mon, t->tm_mday, 1 + t->tm_wday, t->tm_hour, t->tm_min, t->tm_sec);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC146818::SetTime(int year, int month, int day_of_month, int day_of_week, int hours_24h, int minutes, int seconds) {
    this->SetClampedTimeValue(&m_regs.bits.year, year, 0, 99);
    this->SetClampedTimeValue(&m_regs.bits.month, month, 1, 12);
    this->SetClampedTimeValue(&m_regs.bits.day_of_month, day_of_month, 1, 31);
    this->SetClampedTimeValue(&m_regs.bits.day_of_week, day_of_week, 1, 7);
    this->SetHours(hours_24h);
    this->SetClampedTimeValue(&m_regs.bits.minutes, minutes, 1, 60);
    this->SetClampedTimeValue(&m_regs.bits.seconds, seconds, 1, 60);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t MC146818::Read() {
    if (m_reg == MC146818Register_D) {
        m_regs.bits.d.bits.vrt = 1;
    }

    uint8_t value = m_regs.values[m_reg];

#if BBCMICRO_TRACE
    if (m_trace) {
        char d[100];
        GetRegisterDescription(d, sizeof d, m_reg);
        m_trace->AllocStringf(TraceEventSource_Host, "CMOS - Read %s: %d ($%02X)\n", d, value, value);
    }
#endif

    return value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t MC146818::GetAddress() const {
    return m_reg;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC146818::SetAddress(uint8_t value) {
    m_reg = value & 63;

#if BBCMICRO_TRACE
    if (m_trace) {
        char d[100];
        GetRegisterDescription(d, sizeof d, m_reg);

        m_trace->AllocStringf(TraceEventSource_Host, "CMOS - Set address: %s\n", d);
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC146818::SetData(uint8_t value) {
    uint8_t old_value = m_regs.values[m_reg];

    m_regs.values[m_reg] &= g_ro_masks[m_reg];
    m_regs.values[m_reg] |= value & ~g_ro_masks[m_reg];

#if LOGGING
    LOGF(RTC, "Write $%02X; %d; %s: value now: %d; $%02X\n", m_reg, m_reg, GetMC146818RegisterEnumName(m_reg), m_regs.values[m_reg], m_regs.values[m_reg]);
#endif

#if BBCMICRO_TRACE
    if (m_trace) {
        char d[100];
        GetRegisterDescription(d, sizeof d, m_reg);
        m_trace->AllocStringf(TraceEventSource_Host, "CMOS - Write %s: Value now: %d ($%02X)\n", d, m_regs.values[m_reg], m_regs.values[m_reg]);
    }
#endif

    if (m_reg >= offsetof(RegisterBits, ram) && m_regs.values[m_reg] != old_value) {
        if (m_nvram_change_callback_fn) {
            (*m_nvram_change_callback_fn)(m_nvram_change_callback_context);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> MC146818::GetRAMContents() const {
    std::vector<uint8_t> ram(m_regs.bits.ram, m_regs.bits.ram + RAM_SIZE);
    return ram;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC146818::SetRAMContents(const std::vector<uint8_t> &data) {
    size_t i;

    for (i = 0; i < data.size() && i < RAM_SIZE; ++i) {
        m_regs.bits.ram[i] = data[i];
    }

    for (; i < RAM_SIZE; ++i) {
        m_regs.bits.ram[i] = 0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC146818::Update() {
    if (m_counter > 0) {
        --m_counter;
        return;
    }

    m_counter = COUNTER;

    if (m_regs.bits.b.bits.set) {
        // Time is being set - don't update.
        return;
    }

    // Seconds.
    if (!this->IncTimeValue(&m_regs.bits.seconds, 59, 0)) {
        return;
    }

    if (!this->IncTimeValue(&m_regs.bits.minutes, 59, 0)) {
        return;
    }

    if (!this->IncHours()) {
        return;
    }

    this->IncTimeValue(&m_regs.bits.day_of_week, 7, 1);

    if (!this->IncDay()) {
        return;
    }

    if (!this->IncTimeValue(&m_regs.bits.month, 12, 1)) {
        return;
    }

    if (!this->IncTimeValue(&m_regs.bits.year, 99, 0)) {
        return;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MC146818::SetNVRAMChangeCallback(NVRAMChangeCallbackFn fn, void *context) {
    m_nvram_change_callback_fn = fn;
    m_nvram_change_callback_context = context;
}
