#ifndef HEADER_B23BA1EB8849489AABE0512386F0DEA0
#define HEADER_B23BA1EB8849489AABE0512386F0DEA0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct LogSet;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// See https://github.com/cspiegel/garglk/commit/1734a95cca91d923dc3cc254b12944805c57f7df

// See https://github.com/nlohmann/json/issues/3808
//
// See https://github.com/nlohmann/json/issues/4657
//
// See https://github.com/nlohmann/json/pull/4704 (maintainers won't
// fix it)

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#elif defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5262) //implicit fall-through occurs here; are you missing a break statement? Use [[fallthrough]] when a break statement is intentionally omitted between cases
#endif

#include <nlohmann/json.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning(pop)
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Uses the file_io stuff to do its thing.
//
// TODO: maybe there's some better place for this.

bool LoadJSONFile2(nlohmann::json *j, const std::string &path, const LogSet *logs, uint32_t flags);
void HandleLoadJSONFileError(const std::string &path, const LogSet *logs, const std::string &exc_what);
bool SaveJSONFile2(const nlohmann::json &j, const std::string &path, const LogSet *logs, uint32_t flags);

template <class T>
inline bool LoadJSON(T *object, const nlohmann::json &j, std::string *exc_what) {
    try {
        *object = j.template get<T>();
        return true;
    } catch (nlohmann::json::exception &exc) {
        if (exc_what) {
            exc_what->assign(exc.what());
        }
        return false;
    }
}

template <class T>
inline bool LoadJSONFile(T *object, const std::string &path, const LogSet *logs, uint32_t flags = 0) {
    nlohmann::json j;
    if (!LoadJSONFile2(&j, path, logs, flags)) {
        return false;
    }

    std::string exc_what;
    if (!LoadJSON(object, j, &exc_what)) {
        HandleLoadJSONFileError(path, logs, exc_what);
        return false;
    }

    return true;
}

template <class T>
inline bool SaveJSONFile(const T &object, const std::string &path, const LogSet *logs, uint32_t flags = 0) {
    nlohmann::json j(object);
    bool good = SaveJSONFile2(j, path, logs, flags);
    return good;
}

std::vector<uint8_t> SaveJSONData(const nlohmann::json &j);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
