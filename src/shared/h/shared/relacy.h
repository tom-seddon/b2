#ifdef HEADER_041EF256A6D140CB9E42AB5BFE98CE21
#if USE_RELACY
#ifdef OFF_041EF256A6D140CB9E42AB5BFE98CE21
#error USE_RELACY mismatch
#endif
#endif
#else
#define HEADER_041EF256A6D140CB9E42AB5BFE98CE21

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Relacy wrappers.
//
// How to use:
//
// 1. Include this file rather than <atomic> - the Relacy header
// defines make a mess
//
// 2. Define USE_RELACY 1 before including this file when using
// Relacy.
//
// 3. Use the macros:
//
//    - RVAR(T) for the type of a rl::var<T>
//    - RVAL(X) to do X($)
//    - RDELETED(BLAH) if you want BLAH=delete
//    - RMO_ACQUIRE, RMO_ACQ_REL, etc., rather than the Relacy defines
//
// It looks like Relacy is supposed to make this stuff work relatively
// transparently, but it doesn't seem to have quite worked for me.
// Looks like VS2015 support is hit-or-miss anyway:
// https://github.com/dvyukov/relacy/pull/2 (applied)
//
// However these macros do seem to make things roughly hold
// together...

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if USE_RELACY

#define ON_041EF256A6D140CB9E42AB5BFE98CE21

#define RVAR(T) rl::var<T>
//#define RLOAD(V,MO) ((V)($).load(MO))
#define RVAL(X) ((X)($))

#define RMO_ACQUIRE (rl::mo_acquire)
#define RMO_ACQ_REL (rl::mo_acq_rel)

// Relacy redefines "delete", so what can you do. This isn't great,
// but you'll at least get a linker error if it's ever used.
#define RDELETED(...) __VA_ARGS__

#else

#include <atomic>

#define OFF_041EF256A6D140CB9E42AB5BFE98CE21

#define RVAR(T) T
//#define RLOAD(V,MO) ((V).load(MO))
#define RVAL(X) (X)

#define RMO_ACQUIRE (std::memory_order_acquire)
#define RMO_ACQ_REL (std::memory_order_acq_rel)

#define RDELETED(...) __VA_ARGS__ = delete

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
