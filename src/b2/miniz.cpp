// There's no b2-specific header for this (yet?). Include the relevant headers
// from the miniz submodule.

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100) //C4100: 'IDENTIFIER': unreferenced formal parameter
#pragma warning(disable : 4127) //C4127: conditional expression is constant
#pragma warning(disable : 4244) //C4244: 'THING': conversion from 'TYPE' to 'TYPE', possible loss of data
#pragma warning(disable : 4334) //C4334: 'SHIFT': result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#endif
#include <miniz.c>
#include <miniz_zip.c>
#include <miniz_tinfl.c>
#include <miniz_tdef.c>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning(pop)
#endif
