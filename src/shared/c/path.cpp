#include <shared/system.h>
#include <shared/path.h>
#include <shared/debug.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathIsSeparatorChar(char c) {
    return c=='/'||c=='\\';
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SplitPath(std::string::size_type *last_sep_,std::string::size_type *ext_,const std::string &path) {
    std::string::size_type last_sep=path.find_last_of("\\/");
    if(last_sep_) {
        *last_sep_=last_sep;
    }

    if(last_sep==std::string::npos) {
        last_sep=0;
    }

    std::string::size_type ext=path.find_last_of(".");
    if(ext<last_sep) {
        ext=std::string::npos;
    }

    if(ext_) {
        *ext_=ext;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathWithoutExtension(const std::string &path) {
    std::string::size_type dot;
    SplitPath(nullptr,&dot,path);

    if(dot==std::string::npos) {
        return path;
    } else {
        return path.substr(0,dot);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathGetFolder(const std::string &path) {
    std::string::size_type last_sep;
    SplitPath(&last_sep,nullptr,path);

    if(last_sep==std::string::npos) {
        return "";
    } else {
        return path.substr(0,last_sep+1);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathWithoutTrailingSeparators(const std::string &path) {
    size_t i=path.size();
    while(i>0&&PathIsSeparatorChar(path[i-1])) {
        --i;
    }

    return path.substr(0,i);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathJoined(const std::string &a,const std::string &b) {
    if(a.empty()) {
        return b;
    }
    
    if(b.empty()) {
        return a;
    }

    if(PathIsSeparatorChar(b[0])) {
        return b;
    }

    if(PathIsSeparatorChar(a.back())) {
        return a+b;
    } else {
        return a+"/"+b;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathJoined(const std::string &a,const std::string &b,const std::string &c) {
    return PathJoined(PathJoined(a,b),c);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathGetName(const std::string &path) {
    std::string::size_type last_sep;
    SplitPath(&last_sep,nullptr,path);

    if(last_sep==std::string::npos) {
        return path;
    } else {
        return path.substr(last_sep+1);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathGetExtension(const std::string &path) {
    std::string::size_type ext;
    SplitPath(nullptr,&ext,path);

    if(ext==std::string::npos) {
        return "";
    } else {
        return path.substr(ext);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int PathCompare(const std::string &a,const std::string &b) {
    size_t ai=0,bi=0;

    for(;;) {
        bool ea=ai==a.size();
        bool eb=bi==b.size();

        if(ea&&eb) {
            break;
        }

        int ca=a[ai];
        int cb=b[bi];

#if SYSTEM_OSX||SYSTEM_WINDOWS
        ca=tolower(ca);
        cb=tolower(cb);
#endif

#if SYSTEM_WINDOWS
        if(ca=='\\') {
            ca='/';
        }

        if(cb=='\\') {
            cb='/';
        }
#endif

        if(ea||ca<cb) {
            return -1;
        } else if(eb||ca>cb) {
            return 1;
        }

        ++ai;
        ++bi;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathLoadBinaryFile(std::vector<uint8_t> *contents,const std::string &path) {
    FILE *f=NULL;
    bool good=false;
    size_t num_read;
    long len;

    f=fopen(path.c_str(),"rb");
    if(!f) {
        goto done;
    }

    if(fseek(f,0,SEEK_END)==-1) {
        goto done;
    }

    len=ftell(f);
    if(len<0) {
        goto done;
    }

#if LONG_MAX>SIZE_MAX
    if(len>(long)SIZE_MAX) {
        goto done;
    }
#endif
    
    if(fseek(f,0,SEEK_SET)==-1) {
        goto done;
    }

    contents->resize((size_t)len);

    num_read=fread(contents->data(),1,contents->size(),f);
    if(num_read!=contents->size()) {
        goto done;
    }

    good=true;

done:;
    int e=errno;

    if(f) {
        fclose(f);
        f=NULL;
    }

    if(!good) {
        contents->clear();//may as well...
    }

    errno=e;
    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
