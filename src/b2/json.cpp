#include <shared/system.h>
#include <shared/log.h>
#include <shared/file_io.h>
#include "json.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadJSONFile2(nlohmann::json *j, const std::string &path, const LogSet *logs, uint32_t flags) {
    std::string str;
    if (!LoadTextFile(&str, path, logs, flags)) {
        return false;
    }

    try {
        *j = nlohmann::json::parse(str);
    } catch (nlohmann::json::exception &exc) {
        logs->e.f("%s: failed to parse: %s\n", path.c_str(), exc.what());
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HandleLoadJSONFileError(const std::string &path, const LogSet *logs, const std::string &exc_what) {
    logs->e.f("%s: failed to load: %s\n", path.c_str(), exc_what.c_str());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveJSONFile2(const nlohmann::json &j, const std::string &path, const LogSet *logs, uint32_t flags) {
    std::string str = j.dump(4);
    if (!SaveTextFile(str, path, logs, flags)) {
        return false;
    }

    return true;
}
