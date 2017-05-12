#include <shared/system.h>
#include <shared/system_specific.h>
#include <stdlib.h>
#include <direct.h>
#include <string>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(void) {
    if(_chdir(DIR)==-1) {
        perror("chdir");
        return 1;
    }

    std::string c="ctest";

#ifdef SUBSET
    c+=" -LE slow";
#endif

    c+=" -C \""+std::string(CONFIG)+"\"";

    int result=system(c.c_str());

    return result;
}
