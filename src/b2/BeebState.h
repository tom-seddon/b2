#ifndef HEADER_C78B7626FF9A46FB958FE3B842FD9554 // -*- mode:c++ -*-
#define HEADER_C78B7626FF9A46FB958FE3B842FD9554

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DiscImage;
class Log;
struct SDL_Renderer;
struct SDL_PixelFormat;
class BeebThread;
class BeebLoadedConfig;
class TVOutput;
struct BBCMicroType;
class BBCMicroUniqueState;

#include <beeb/conf.h>
#include <beeb/BBCMicro.h>
#include <memory>
#include <vector>
#include <shared/mutex.h>
#include <map>
#include <time.h>
#include <string>
#include "BeebConfig.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Holds a named BBCMicroUniqueState and exposes shared_from_this.

class BeebState : public std::enable_shared_from_this<BeebState> {
  public:
    // Creation time.
    const struct tm creation_time;

    explicit BeebState(std::unique_ptr<BBCMicroUniqueState> state);
    ~BeebState();

    // Number of emulated cycles elapsed.
    CycleCount GetCycleCount() const;

    // Get clone of BBCMicro with this state's state. Its DiscDrive
    // and NVRAM callbacks are indeterminate.
    std::unique_ptr<BBCMicro> CloneBBCMicro() const;

    const BBCMicroType *GetBBCMicroType() const;
    std::shared_ptr<const DiscImage> GetDiscImageByDrive(int drive) const;

    const std::string &GetName() const;
    void SetName(std::string name);

  protected:
    BeebState(BeebState &&) = delete;
    BeebState &operator=(BeebState &&) = delete;

    BeebState(const BeebState &) = delete;
    BeebState &operator=(const BeebState &) = delete;

  private:
    std::unique_ptr<BBCMicroUniqueState> m_state;
    std::string m_name;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
