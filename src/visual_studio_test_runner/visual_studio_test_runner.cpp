#include <shared/system.h>
#include <shared/system_specific.h>
#include <shared/path.h>
#include <stdlib.h>
#include <direct.h>
#include <string>
#include <process.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(void) {
    if (_chdir(DIR) == -1) {
        perror("chdir");
        return 1;
    }

    std::string command = PathJoined(PathGetFolder(CMAKE_COMMAND), "ctest");

    intptr_t result = _spawnl(_P_WAIT,
                              command.c_str(),
                              command.c_str(),
#ifdef SUBSET
                              "-LE", "slow",
#endif
                              "-C",
                              CONFIG,
                              nullptr);

    return (int)result;
}
