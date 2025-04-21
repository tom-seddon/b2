#include <shared/system.h>
#include "load_save_config_nlohmann_json.h"
#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include "BeebWindow.h"
#include "BeebWindows.h"
#include "TraceUI.h"
#include <beeb/BBCMicro.h>
#include "load_save.h"
#include "BeebLinkHTTPHandler.h"
#include "b2.h"
#include "joysticks.h"
#include <beeb/DiscInterface.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ConfigExtra {
    bool video_nula = BeebConfig::DEFAULT_VIDEO_NULA;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConfigExtra, video_nula);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
static bool TryGet(T *result, const nlohmann::json &j, Messages *msg) {
    try {
        *result = j.template get<T>();
        return true;
    } catch (nlohmann::json::exception &ex) {
        msg->e.f("JSON error: %s\n", ex.what());
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadConfigExtra(BeebConfig *config, const nlohmann::json &j, Messages *msg) {
    ConfigExtra ce;
    if (!TryGet(&ce, j, msg)) {
        return false;
    }

    config->video_nula = ce.video_nula;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

nlohmann::json SaveConfigExtra(const BeebConfig &config) {
    ConfigExtra ce;
    ce.video_nula = config.video_nula;
    return ce;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
