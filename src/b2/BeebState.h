#ifndef HEADER_C78B7626FF9A46FB958FE3B842FD9554// -*- mode:c++ -*-
#define HEADER_C78B7626FF9A46FB958FE3B842FD9554

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BBCMicro;
class DiscImage;
class Log;
struct SDL_Renderer;
struct SDL_PixelFormat;
class BeebThread;
class BeebLoadedConfig;

#include <beeb/conf.h>
#include <memory>
#include <vector>
#include <mutex>
#include <map>
#include <time.h>
#include <string>
#include "BeebConfig.h"
#include "beeb_events.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebState:
    public std::enable_shared_from_this<BeebState>
{
public:
    // Creation time.
    const struct tm creation_time;

    BeebState(std::unique_ptr<BBCMicro> beeb);
    ~BeebState();

    // Number of emulated 2MHz cycles elapsed.
    uint64_t GetEmulated2MHzCycles() const;

    // Get clone of BBCMicro with this state's state. Its DiscDrive
    // and NVRAM callbacks are indeterminate.
    std::unique_ptr<BBCMicro> CloneBBCMicro() const;

    const std::string &GetName() const;
    void SetName(std::string name);
protected:
    BeebState(BeebState &&)=default;
    BeebState &operator=(BeebState &&)=default;

    BeebState(const BeebState &)=default;
    BeebState &operator=(const BeebState &)=default;
private:
    std::unique_ptr<BBCMicro> m_beeb;

    std::string m_name;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
