#include <shared/system.h>
#include <nlohmann/json.hpp>
#include <shared/enums.h>
#include "load_save_config_nlohmann_json.h"
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
        *result = T();
        msg->e.f("JSON error: %s\n", ex.what());
        return false;
    }
}

template <class T>
static bool TryGet(T *result, const nlohmann::json &j, const char *key, Messages *msg) {
    if (j.count(key) == 0) {
        *result = T();
        return true;
    } else {
        bool good = TryGet(result, j.at(key), msg);
        return good;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadConfigPartial(BeebConfig *config, const nlohmann::json &j, Messages *msg) {
    return TryGet(config, j, msg);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

nlohmann::json SaveConfigPartial(const BeebConfig &config) {
    return config;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadTrace(TraceUISettings *settings, const nlohmann::json &j, Messages *msg) {
    return TryGet(settings,j,msg);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

nlohmann::json SaveTrace() {
    return GetDefaultTraceUISettings();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define KEY(NAME) static const char key_##NAME[] = #NAME;

// some bits I named wrongly...
KEY(filter_bbc);
KEY(auto_scale);
KEY(manual_scale);
KEY(config);

// some annoying one-offs
KEY(popups);
KEY(keymap);
KEY(window_placement);
KEY(popup_persistent_data);

bool LoadWindows(const nlohmann::json &j, Messages *msg) {
    TryGet(&BeebWindows::defaults, j, msg);

    TryGet(&BeebWindows::defaults.display_filter, j, key_filter_bbc, msg);
    TryGet(&BeebWindows::defaults.display_auto_scale, j, key_auto_scale, msg);
    TryGet(&BeebWindows::defaults.display_manual_scale, j, key_manual_scale, msg);
    TryGet(&BeebWindows::default_config_name, j, key_config, msg);

    if (j.count(key_popups) > 0) {
        const nlohmann::json &j_popups = j.at(key_popups);
        if (j_popups.is_array()) {
            for (int i = 0; i < BeebWindowPopupType_MaxValue; ++i) {
                if (std::find(j_popups.begin(), j_popups.end(), GetBeebWindowPopupTypeEnumName(i)) != j_popups.end()) {
                    BeebWindows::defaults.popups |= (uint64_t)1 << i;
                }
            }
        }
    }

    std::string keymap_name;
    if (TryGet(&keymap_name, j, key_keymap, msg)) {
        if (!keymap_name.empty()) {
            BeebWindows::defaults.keymap = BeebWindows::FindBeebKeymapByName(keymap_name);
            if (!BeebWindows::defaults.keymap) {
                msg->w.f("default keymap unknown: %s\n", keymap_name.c_str());
                BeebWindows::defaults.keymap = BeebWindows::GetDefaultBeebKeymap();
            }
        }
    }

    std::string placement_str;
    if (TryGet(&placement_str, j, key_window_placement, nullptr)) {
        std::vector<uint8_t> placement_data;
        if (GetDataFromHexString(&placement_data, placement_str)) {
            BeebWindows::SetLastWindowPlacementData(std::move(placement_data));
        }
    }

    if (j.count(key_popup_persistent_data) > 0) {
        const nlohmann::json &j_ppds = j.at(key_popup_persistent_data);

        if (j_ppds.is_object()) {
            for (int i = 0; i < BeebWindowPopupType_MaxValue; ++i) {
                const char *popup_type_name = GetBeebWindowPopupTypeEnumName(i);
                std::shared_ptr<JSON> j_ppd;

                if (j_ppds.count(popup_type_name) > 0) {
                    j_ppd = std::make_shared<JSON>(j_ppds.at(popup_type_name));
                }

                BeebWindows::defaults.popup_persistent_data[i] = j_ppd;
            }
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

nlohmann::json SaveWindows() {
    nlohmann::json j = BeebWindows::defaults;

    j[key_filter_bbc] = BeebWindows::defaults.display_filter;
    j[key_auto_scale] = BeebWindows::defaults.display_auto_scale;
    j[key_manual_scale] = BeebWindows::defaults.display_manual_scale;
    j[key_config] = BeebWindows::default_config_name;

    if (BeebWindows::defaults.keymap) {
        j[key_keymap] = BeebWindows::defaults.keymap->GetName();
    }

    const std::vector<uint8_t> &placement_data = BeebWindows::GetLastWindowPlacementData();
    if (!placement_data.empty()) {
        j[key_window_placement] = GetHexStringFromData(placement_data);
    }

    {
        nlohmann::json j_popups = nlohmann::json::array_t{};
        for (int i = 0; i < BeebWindowPopupType_MaxValue; ++i) {
            uint64_t mask = (uint64_t)1 << i;
            if (BeebWindows::defaults.popups & mask) {
                j_popups.push_back(GetBeebWindowPopupTypeEnumName(i));
            }
        }
        j[key_popups] = std::move(j_popups);
    }

    {
        nlohmann::json j_ppds = nlohmann::json::object_t{};
        for (int i = 0; i < BeebWindowPopupType_MaxValue; ++i) {
            const std::shared_ptr<JSON> &j_ppd_ptr = BeebWindows::defaults.popup_persistent_data[i];
            if (!!j_ppd_ptr) {
                j_ppds[GetBeebWindowPopupTypeEnumName(i)] = j_ppd_ptr->AsNLohmannJSON();
            }
        }
        j[key_popup_persistent_data] = j_ppds;
    }

    return j;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
