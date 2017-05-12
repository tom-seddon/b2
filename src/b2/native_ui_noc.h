#ifndef HEADER_21AB78E291484F5CA40BCD5FB8CD03A1
#define HEADER_21AB78E291484F5CA40BCD5FB8CD03A1
#include <shared/cpp_begin.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* native_ui_noc.* just defers to noc_file_dialog.h, after setting up
 * the appropriate defines.
 *
 * (The noc OS X code is Objective-C, so it needs to go in its own .m
 * file. So the Win32 and Linux code might as well have its own .c
 * file, too - because then at least that way there's no risk of
 * introducing C++isms when modifying it.)
 */

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
#define NOC_FILE_DIALOG_WIN32
#elif SYSTEM_OSX
#define NOC_FILE_DIALOG_OSX
#elif SYSTEM_LINUX
#define NOC_FILE_DIALOG_GTK
#else
#error
#endif

#include <noc_file_dialog.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/cpp_end.h>
#endif
