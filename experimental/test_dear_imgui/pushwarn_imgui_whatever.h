// Disable whatever warnings dear imgui produces when compiled with my
// favourite warning settings.

#ifdef _MSC_VER

#pragma warning(push)
#pragma warning(disable : 4458) //declaration of 'identifier' hides class member
#pragma warning(disable : 4267) //'var' : conversion from 'size_t' to 'type', possible loss of data
#pragma warning(disable : 4305) //'identifier' : truncation from 'type1' to 'type2'
#pragma warning(disable : 4100) //'identifier' : unreferenced formal parameter
#pragma warning(disable : 4800) //'type' : forcing value to bool 'true' or 'false' (performance warning)

#elif defined __GNUC__

#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wswitch"

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif

#endif
