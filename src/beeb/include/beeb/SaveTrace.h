    #ifndef HEADER_413466CDB46E4A138B70F24354302A47 // -*- mode:c++ -*-
#define HEADER_413466CDB46E4A138B70F24354302A47

#include "conf.h"

// This stuff exists even when tracing is compiled out, so that the
// settings can still be serialized.

#include <shared/enum_decl.h>
#include "SaveTrace.inl"
#include <shared/enum_end.h>

static constexpr uint32_t DEFAULT_TRACE_OUTPUT_FLAGS = TraceOutputFlags_Cycles | TraceOutputFlags_RegisterNames;

#if BBCMICRO_TRACE

#include <memory>
#include <thread>
#include <atomic>

class Trace;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SaveTraceProgress {
    std::atomic<uint64_t> num_bytes_written{0};
    std::atomic<uint64_t> num_events_handled{0};
    std::atomic<uint64_t> num_events{0};
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

typedef bool (*SaveTraceWasCanceledFn)(void *context);
typedef bool (*SaveTraceSaveDataFn)(const void *data, size_t num_bytes, void *context);

bool SaveTrace(std::shared_ptr<Trace> trace,
               uint32_t output_flags, //combination of TraceOutputFlags
               SaveTraceSaveDataFn save_data_fn,
               void *save_data_context,
               SaveTraceWasCanceledFn was_canceled_fn,
               void *was_canceled_context,
               SaveTraceProgress *progress);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
