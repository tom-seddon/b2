#include <shared/system.h>
#include "misc.h"
#include <shared/path.h>
#include <shared/log.h>
#include <SDL.h>
//#include <parson.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>
#include <beeb/BBCMicro.h>
#include <beeb/Trace.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DumpRendererInfo(Log *log,const SDL_RendererInfo *info) {
    LogIndenter indent(log);
    
    log->f("Name: %s\n",info->name);
#define F(X) info->flags&SDL_RENDERER_##X?" " #X:""
    log->f("Flags:%s%s%s%s\n",F(SOFTWARE),F(ACCELERATED),F(PRESENTVSYNC),F(TARGETTEXTURE));
#undef F
    log->f("Max Texture Size: %dx%d\n",info->max_texture_width,info->max_texture_width);
    log->f("Texture Formats:");
    for(size_t i=0;i<info->num_texture_formats;++i) {
        log->f(" %s",SDL_GetPixelFormatName(info->texture_formats[i]));
    }
    log->f("\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string strprintf(const char *fmt,...) {
    va_list v;

    va_start(v,fmt);
    std::string result=strprintfv(fmt,v);
    va_end(v);

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string strprintfv(const char *fmt,va_list v) {
    char *str;
    vasprintf(&str,fmt,v);

    std::string result(str);

    free(str);
    str=NULL;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetFlagsString(uint32_t value,const char *(*get_name_fn)(int)) {
    std::string str;

    for(uint32_t mask=1;mask!=0;mask<<=1) {
        if(value&mask) {
            const char *name=(*get_name_fn)((int)mask);

            if(!str.empty()) {
                str+="|";
            }

            if(name[0]=='?') {
                str+=strprintf("0x%" PRIx32,mask);
            } else {
                str+=name;
            }
        }
    }

    if(str.empty()) {
        str="0";
    }

    return str;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetMicrosecondsString(uint64_t num_microseconds) {
    char str[500];

    uint64_t n=num_microseconds;

    unsigned us=n%1000;
    n/=1000;

    unsigned ms=n%1000;
    n/=1000;

    unsigned secs=n%60;
    n/=60;

    uint64_t minutes=n;

    snprintf(str,sizeof str,"%" PRIu64 " min %02u sec %03u ms %03u usec",minutes,secs,ms,us);

    return str;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string Get2MHzCyclesString(uint64_t num_2MHz_cycles) {
    char str[500];

    uint64_t n=num_2MHz_cycles;

    unsigned cycles=n%2;
    n/=2;

    unsigned us=n%1000;
    n/=1000;

    unsigned ms=n%1000;
    n/=1000;

    unsigned secs=n%60;
    n/=60;

    uint64_t minutes=n;

    snprintf(str,sizeof str,"%" PRIu64 " min %02u sec %03u ms %03u.%d usec",minutes,secs,ms,us,cycles?5:0);

    return str;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_Window *w) const {
    SDL_DestroyWindow(w);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_Renderer *r) const {
    SDL_DestroyRenderer(r);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_Texture *t) const {
    SDL_DestroyTexture(t);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_Surface *s) const {
    SDL_FreeSurface(s);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SDL_Deleter::operator()(SDL_PixelFormat *p) const {
    SDL_FreeFormat(p);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SDL_PixelFormat *ClonePixelFormat(const SDL_PixelFormat *pixel_format) {
    return SDL_AllocFormat(pixel_format->format);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetUniqueName(std::string suggested_name,
                          std::function<const void *(const std::string &)> find,
                          const void *ignore)
{
    const void *p=find(suggested_name);
    if(!p||p==ignore) {
        return suggested_name;
    }

    uint64_t suffix=2;

    // decode and remove any existing suffix.
    if(!suggested_name.empty()) {
        if(suggested_name.back()==')') {
            std::string::size_type op=suggested_name.find_last_of("(");
            if(op!=std::string::npos) {
                std::string suffix_str=suggested_name.substr(op+1,(suggested_name.size()-1)-(op+1));
                if(suffix_str.find_first_not_of("0123456789")==std::string::npos) {
                    suffix=strtoull(suffix_str.c_str(),nullptr,0);

                    suggested_name=suggested_name.substr(0,op);

                    while(!suggested_name.empty()&&isspace(suggested_name.back())) {
                        suggested_name.pop_back();
                    }
                }
            }
        }
    }

    for(;;) {
        std::string new_name=suggested_name+" ("+std::to_string(suffix)+")";
        const void *existing_item=find(new_name);
        if(!existing_item||existing_item==ignore) {
            return new_name;
        }

        ++suffix;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct tm GetUTCTimeNow() {
    time_t now;
    time(&now);

    struct tm utc;
    gmtime_r(&now,&utc);

    return utc;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct tm GetLocalTimeNow() {
    time_t now;
    time(&now);

    struct tm local;
    localtime_r(&now,&local);

    return local;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetTimeString(const struct tm &t) {
    char time_str[500];
    strftime(time_str,sizeof time_str,"%c",&t);

    return time_str;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

AudioDeviceLock::AudioDeviceLock(uint32_t device):
    m_device(device)
{
    if(m_device!=0) {
        SDL_LockAudioDevice(m_device);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

AudioDeviceLock::~AudioDeviceLock() {
    if(m_device!=0) {
        SDL_UnlockAudioDevice(m_device);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
