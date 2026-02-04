#include <shared/system.h>
#include "b2.h"
#include <SDL.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
#if BUILD_TYPE_Final || BUILD_TYPE_RelWithDebInfo

// Seems like there's no good way to do this from CMake:
// http://stackoverflow.com/questions/8054734/
//
// But this here is simple enough.
//
// (There's no need to do anything about the entry point; the SDL2main stuff
// already ensures that the program can work either way.)
//
// (One oddity: Visual Studio seems to examine the project settings to decide
// whether to launch its own console window with the post exit
// press-return-to-exit prompt. So the RelWithDebInfo build gets a console
// window when run from Visual Studio, even though it's not actually a console
// program.)
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

#endif
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    return b2_main(argc, argv);
}
