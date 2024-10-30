#include <shared/system.h>
#include <beeb/Trace.h>

#if BBCMICRO_TRACE

#include <shared/debug.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <shared/log.h>

#include <shared/enum_def.h>
#include <beeb/Trace.inl>
#include <shared/enum_end.h>

struct M6502Config;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const size_t CHUNK_SIZE = 16777216;

static constexpr size_t NUM_TIME_DELTA_BITS = 5;
static constexpr size_t MAX_TIME_DELTA = (1 << NUM_TIME_DELTA_BITS) - 1;

static constexpr size_t NUM_SOURCE_BITS = 2;
static constexpr size_t MAX_SOURCE = (1 << NUM_SOURCE_BITS) - 1;
static_assert(TraceEventSource_Count <= MAX_SOURCE, "");

static const size_t MAX_EVENT_SIZE = 65535;

static_assert(MAX_EVENT_SIZE <= CHUNK_SIZE, "chunks must be large enough for at least one event");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
struct EventHeader {
    uint8_t type;
    uint8_t time_delta : NUM_TIME_DELTA_BITS;
    uint8_t source : NUM_SOURCE_BITS;
    uint8_t canceled : 1;
};
#include <shared/poppack.h>
CHECK_SIZEOF(EventHeader, 2);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
struct EventWithSizeHeader {
    EventHeader h;
    uint16_t size;
};
#include <shared/poppack.h>
CHECK_SIZEOF(EventWithSizeHeader, 4);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static TraceEventType *g_trace_event_types[256];
static size_t g_trace_next_id = 0;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TraceEventType::TraceEventType(const char *name, size_t size_, TraceEventSource default_source_)
    : type_id((uint8_t)g_trace_next_id++)
    , size(size_)
    , default_source(default_source_)
    , m_name(name) {
    ASSERT(this->size <= MAX_EVENT_SIZE);
    ASSERT(g_trace_next_id <= sizeof g_trace_event_types / sizeof g_trace_event_types[0]);
    g_trace_event_types[this->type_id] = this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TraceEventType::~TraceEventType() {
    ASSERT(g_trace_event_types[this->type_id] == this);

    g_trace_event_types[this->type_id] = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &TraceEventType::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//const TraceEventType Trace::BLANK_LINE_EVENT("_blank_line", 0, TraceEventSource_None);
const TraceEventType Trace::STRING_EVENT("_string", 0, TraceEventSource_None);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
struct DiscontinuityTraceEvent {
    CycleCount new_time;
};
typedef struct DiscontinuityTraceEvent DiscontinuityTraceEvent;
#include <shared/poppack.h>

const TraceEventType Trace::DISCONTINUITY_EVENT("_discontinuity", sizeof(DiscontinuityTraceEvent), TraceEventSource_None);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const TraceEventType Trace::WRITE_ROMSEL_EVENT("_write_romsel", sizeof(WriteROMSELEvent), TraceEventSource_Host);
const TraceEventType Trace::WRITE_ACCCON_EVENT("_write_acccon", sizeof(WriteACCCONEvent), TraceEventSource_Host);
const TraceEventType Trace::PARASITE_BOOT_MODE_EVENT("_parasite_boot_mode", sizeof(ParasiteBootModeEvent), TraceEventSource_Parasite);
const TraceEventType Trace::SET_MAPPER_REGION_EVENT("_set_mapper_region", sizeof(SetMapperRegionEvent), TraceEventSource_Host);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Trace::Chunk {
    struct Chunk *next = nullptr;
    size_t size = 0;
    size_t capacity = 0;
    size_t num_events = 0;

    CycleCount initial_time = {};
    CycleCount last_time = {};

    PagingState initial_paging;
    bool initial_parasite_boot_mode = false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Trace::Trace(size_t max_num_bytes,
             std::shared_ptr<const BBCMicroType> bbc_micro_type,
             const PagingState &initial_paging,
             BBCMicroParasiteType parasite_type,
             const M6502Config *parasite_m6502_config,
             bool initial_parasite_boot_mode)
    : m_max_num_bytes(max_num_bytes)
    , m_bbc_micro_type(std::move(bbc_micro_type))
    , m_paging(initial_paging)
    , m_parasite_type(parasite_type)
    , m_parasite_m6502_config(parasite_m6502_config)
    , m_parasite_boot_mode(initial_parasite_boot_mode) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Trace::~Trace() {
    Chunk *c = m_head;
    while (c) {
        Chunk *next = c->next;

        static_assert(std::is_trivially_destructible<decltype(*c)>::value);
        free(c);

        c = next;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::SetTime(const CycleCount *time_ptr) {
    m_time_ptr = time_ptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *Trace::AllocEvent(const TraceEventType &type, TraceEventSource source) {
    ASSERT(type.size > 0);
    ASSERT(source >= 0 && source <= MAX_SOURCE);

    CycleCount time = m_last_time;
    if (m_time_ptr) {
        time = *m_time_ptr;
    }

    auto h = (EventHeader *)this->Alloc(time, sizeof(EventHeader) + type.size);

    h->type = type.type_id;
    ASSERT(time.n >= m_last_time.n);
    h->source = source;
    h->time_delta = (uint8_t)(time.n - m_last_time.n);
    h->canceled = 0;

    m_tail->last_time = time;
    m_last_time = time;

    this->Check();

    if (type.size > 0) {
        return h + 1;
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::CancelEvent(const TraceEventType &type, void *data) {
    if (type.size == 0) {
        EventWithSizeHeader *h = (EventWithSizeHeader *)data - 1;

        h->h.canceled = 1;
    } else {
        EventHeader *h = (EventHeader *)data - 1;

        h->canceled = 1;
    }

    this->Check();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void Trace::AllocBlankLineEvent() {
//    this->AllocEventWithSize(BLANK_LINE_EVENT, 0);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void Trace::AllocStringf(const char *fmt, ...) {
//    va_list v;
//
//    va_start(v, fmt);
//    this->AllocStringv(TraceEventSource_Host, fmt, v);
//    va_end(v);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void Trace::AllocStringv(const char *fmt, va_list v) {
//    char buf[1024];
//
//    vsnprintf(buf, sizeof buf, fmt, v);
//
//    this->AllocString(buf);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void Trace::AllocString(const char *str) {
//    this->AllocString2(str, strlen(str));
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t Trace::AllocStringf(TraceEventSource source, const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    size_t n = this->AllocStringv(source, fmt, v);
    va_end(v);

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t Trace::AllocStringv(TraceEventSource source, const char *fmt, va_list v) {
    char buf[1024];

    int n = vsnprintf(buf, sizeof buf, fmt, v);
    if (n < 0) {
        return 0;
    }

    if ((size_t)n >= sizeof(buf)) {
        n = (int)sizeof buf - 1;
    }

    this->AllocString2(source, buf, (size_t)n);

    return (size_t)n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t Trace::AllocString(TraceEventSource source, const char *str) {
    size_t n = strlen(str);
    this->AllocString2(source, str, n);
    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::AllocStringn(TraceEventSource source, const char *str, size_t n) {
    this->AllocString2(source, str, n);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::AllocWriteROMSELEvent(ROMSEL romsel) {
    auto ev = (WriteROMSELEvent *)this->AllocEvent(WRITE_ROMSEL_EVENT);

    ev->romsel = romsel;
    m_paging.romsel = romsel;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::AllocWriteACCCONEvent(ACCCON acccon) {
    auto ev = (WriteACCCONEvent *)this->AllocEvent(WRITE_ACCCON_EVENT);

    ev->acccon = acccon;
    m_paging.acccon = acccon;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::AllocParasiteBootModeEvent(bool parasite_boot_mode) {
    auto ev = (ParasiteBootModeEvent *)this->AllocEvent(PARASITE_BOOT_MODE_EVENT);

    ev->parasite_boot_mode = parasite_boot_mode;
    m_parasite_boot_mode = parasite_boot_mode;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::AllocSetMapperRegionEvent(uint8_t region) {
    auto ev = (SetMapperRegionEvent *)this->AllocEvent(SET_MAPPER_REGION_EVENT);

    ev->region = region;
    m_paging.rom_regions[m_paging.romsel.b_bits.pr] = region;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogPrinter *Trace::GetLogPrinter(TraceEventSource source, size_t max_len) {
    if (max_len > MAX_EVENT_SIZE) {
        max_len = MAX_EVENT_SIZE;
    }

    m_log_data = this->AllocString2(source, NULL, max_len);
    m_log_len = 0;
    m_log_max_len = max_len;

    return &m_log_printer;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::FinishLog(Log *log) {
    ASSERT(log->GetLogPrinter() == &m_log_printer);
    ASSERT(m_last_alloc);

    log->Flush();

    if (m_log_data != (char *)m_last_alloc + sizeof(EventWithSizeHeader)) {
        // Can't truncate - there are further allocations.
        return;
    }

    EventWithSizeHeader *h = (EventWithSizeHeader *)m_last_alloc;

#if ASSERT_ENABLED
    const char *tail_data = (const char *)(m_tail + 1);
#endif

    ASSERT(m_log_data + h->size == tail_data + m_tail->size);

    ASSERT(m_log_max_len >= m_log_len);
    size_t delta = m_log_max_len - m_log_len;

    ASSERT(h->size >= delta);
    h->size -= (uint16_t)delta;

    ASSERT(m_tail->size >= delta);
    m_tail->size -= delta;

    ASSERT(m_stats.num_used_bytes >= delta);
    m_stats.num_used_bytes -= delta;

    ASSERT(m_log_data[h->size - 1] == 0);

    m_log_data = NULL;

    this->Check();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::GetStats(TraceStats *stats) const {
    *stats = m_stats;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const BBCMicroType> Trace::GetBBCMicroType() const {
    return m_bbc_micro_type;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const PagingState &Trace::GetInitialPagingState() const {
    if (m_head) {
        return m_head->initial_paging;
    } else {
        return m_paging;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicroParasiteType Trace::GetParasiteType() const {
    return m_parasite_type;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Trace::GetInitialParasiteBootMode() const {
    if (m_head) {
        return m_head->initial_parasite_boot_mode;
    } else {
        return m_parasite_boot_mode;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const M6502Config *Trace::GetParasiteM6502Config() const {
    return m_parasite_m6502_config;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int Trace::ForEachEvent(ForEachEventFn fn, void *context) {
    if (!m_head) {
        return 1;
    }

    TraceEvent e;
    e.time = m_head->initial_time;

    Chunk *c = m_head;
    while (c) {
        const uint8_t *p = (uint8_t *)(c + 1);
        const uint8_t *end = p + c->size;

        while (p < end) {
            const EventHeader *h = (const EventHeader *)p;
            p += sizeof(EventHeader);

            ASSERT(h->type < g_trace_next_id);

            e.type = g_trace_event_types[h->type];
            ASSERT(e.type);

            e.size = e.type->size;
            if (e.size == 0) {
                const EventWithSizeHeader *hs = (const EventWithSizeHeader *)h;

                e.size = hs->size;
                p += sizeof(EventWithSizeHeader) - sizeof(EventHeader);
            }

            e.event = p;

            if (e.type == &DISCONTINUITY_EVENT) {
                auto de = (const DiscontinuityTraceEvent *)e.event;

                // Shouldn't be able to find one to cancel it...
                ASSERT(!h->canceled);

                e.time = de->new_time;
            } else {
                e.time.n += h->time_delta;

                if (!h->canceled) {
                    e.source = (TraceEventSource)h->source;
                    if (!(*fn)(this, &e, context)) {
                        return 0;
                    }
                }
            }

            p += e.size;
        }

        ASSERT(p == end);

        c = c->next;
    }

    return 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *Trace::AllocEventWithSize(const TraceEventType &type, TraceEventSource source, size_t size) {
    ASSERT(type.size == 0);
    ASSERT(source >= 0 && source <= MAX_SOURCE);

    if (size > MAX_EVENT_SIZE) {
        return nullptr;
    }

    CycleCount time = m_last_time;
    if (m_time_ptr) {
        time = *m_time_ptr;
    }

    auto h = (EventWithSizeHeader *)this->Alloc(time, sizeof(EventWithSizeHeader) + size);

    h->h.type = type.type_id;
    h->h.time_delta = (uint8_t)(time.n - m_last_time.n);
    h->h.source = source;
    h->h.canceled = 0;
    h->size = (uint16_t)size;

    m_last_time = time;

    this->Check();

    return h + 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

char *Trace::AllocString2(TraceEventSource source, const char *str, size_t len) {
    if (len + 1 > MAX_EVENT_SIZE) {
        return nullptr;
    }

    auto p = (char *)this->AllocEventWithSize(STRING_EVENT, source, len + 1);
    if (p) {
        if (str) {
            memcpy(p, str, len + 1);
        } else {
            *p = 0;
        }
    }

    this->Check();

    return p;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *Trace::Alloc(CycleCount time, size_t n) {
    //ASSERT(ENABLED(t));
    this->Check();

    if (n > MAX_EVENT_SIZE) {
        return nullptr;
    }

    if (!m_tail || m_tail->size + n > m_tail->capacity) {
        size_t size = CHUNK_SIZE;

        Chunk *c = (Chunk *)malloc(sizeof *c + size);
        if (!c) {
            return nullptr;
        }

        if (m_stats.num_allocated_bytes + size > m_max_num_bytes) {
            if (m_head == m_tail) {
                // Always leave at least one used chunk around.
            } else {
                Chunk *old_head = m_head;

                this->Check();

                m_head = m_head->next;

                ASSERT(m_stats.num_used_bytes >= old_head->size);
                m_stats.num_used_bytes -= old_head->size;

                ASSERT(m_stats.num_allocated_bytes >= old_head->capacity);
                m_stats.num_allocated_bytes -= old_head->capacity;

                ASSERT(m_stats.num_events >= old_head->num_events);
                m_stats.num_events -= old_head->num_events;

                // Could/should maybe reuse the old head instead...
                free(old_head);
                old_head = nullptr;

                this->Check();
            }
        }

        new (c) Chunk;
        c->capacity = size;
        c->initial_time = c->last_time = time;
        c->initial_paging = m_paging;
        c->initial_parasite_boot_mode = m_parasite_boot_mode;

        if (!m_head) {
            m_head = c;
        } else {
            m_tail->next = c;
        }

        m_tail = c;

        // Don't bother accounting for the header... it's just noise.
        m_stats.num_allocated_bytes += m_tail->capacity;

        this->Check();
    }

    if (time.n > m_stats.max_time.n) {
        m_stats.max_time = time;
    }

    if (time.n < m_last_time.n || time.n - m_last_time.n > MAX_TIME_DELTA) {
        // Insert a discontinuity event. Ensure it has a time_delta of
        // 0.
        m_last_time = time;

        auto de = (DiscontinuityTraceEvent *)this->AllocEvent(DISCONTINUITY_EVENT);

        de->new_time = time;
    }
    ASSERT(time.n >= m_last_time.n && time.n - m_last_time.n <= MAX_TIME_DELTA);

    uint8_t *p = (uint8_t *)(m_tail + 1) + m_tail->size;

    m_tail->size += n;
    m_stats.num_used_bytes += n;

    ++m_tail->num_events;
    ++m_stats.num_events;

    m_tail->last_time = time;

    m_last_alloc = p;

    this->Check();

    return p;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::Check() {
#if ASSERT_ENABLED
//    size_t total_num_used_bytes=0;
//    size_t total_num_allocated_bytes=0;
//    size_t total_num_events=0;
//
//    for(const Chunk *c=m_head;c;c=c->next) {
//        total_num_used_bytes+=c->size;
//        total_num_allocated_bytes+=c->capacity;
//        total_num_events+=c->num_events;
//    }
//
//    ASSERT(total_num_used_bytes==m_stats.num_used_bytes);
//    ASSERT(total_num_allocated_bytes==m_stats.num_allocated_bytes);
//    ASSERT(total_num_events==m_stats.num_events);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Trace::LogPrinterTrace::LogPrinterTrace(Trace *t)
    : m_t(t) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::LogPrinterTrace::Print(const char *str, size_t str_len) {
    if (!m_t->m_log_data) {
        // This could actually be a stale log... it's not actually a
        // problem.
        return;
    }

    if (m_t->m_log_len == m_t->m_log_max_len) {
        return;
    }

    if (str_len > m_t->m_log_max_len - m_t->m_log_len) {
        str_len = m_t->m_log_max_len - m_t->m_log_len;
    }

    memcpy(m_t->m_log_data + m_t->m_log_len, str, str_len + 1);
    m_t->m_log_len += str_len;
    ASSERT(m_t->m_log_data[m_t->m_log_len] == 0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
