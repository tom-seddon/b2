#include <shared/system.h>
#include <shared/path.h>
#include <mach-o/dyld.h>
#include <stdlib.h>
#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathGetEXEFileName(void) {
    uint32_t size;
    int rc;

    char tmp[1];
    size=1;
    rc=_NSGetExecutablePath(tmp,&size);

    std::vector<char> buf;
    buf.resize(size);
    rc=_NSGetExecutablePath(buf.data(),&size);
    if(rc==-1) {
        return "";
    }

    return buf.data();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
