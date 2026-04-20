#include <shared/system.h>
#include <shared/log.h>
#include <shared/file_io.h>
#include "json.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadJSONData2(nlohmann::json *j, const std::vector<uint8_t> &data, const LogSet *logs, const char *notional_path) {
    try {
        *j = nlohmann::json::parse(data);
        return true;
    } catch (nlohmann::json::exception &exc) {
        HandleLoadJSONError(notional_path, logs, exc.what());
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadJSONFile2(nlohmann::json *j, const std::string &path, const LogSet *logs, uint32_t flags) {
    std::string str;
    if (!LoadTextFile(&str, path, logs, flags)) {
        return false;
    }

    try {
        *j = nlohmann::json::parse(str);
        return true;
    } catch (nlohmann::json::exception &exc) {
        HandleLoadJSONError(path.c_str(), logs, exc.what());
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HandleLoadJSONError(const char *notional_path, const LogSet *logs, const char *exc_what) {
    if (notional_path) {
        logs->e.f("%s: ", notional_path);
    }

    logs->e.f("failed to load: %s\n", exc_what);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveJSONFile(const nlohmann::json &j, const std::string &path, const LogSet *logs, uint32_t flags) {
    std::string str = j.dump(4);
    if (!SaveTextFile(str, path, logs, flags)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> SaveJSONData(const nlohmann::json &j) {
    std::string str = j.dump(4);
    return std::vector<uint8_t>(str.begin(), str.end());
}
