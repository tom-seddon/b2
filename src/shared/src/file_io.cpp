#include <shared/system.h>
#include <shared/file_io.h>
#include <shared/system_specific.h>
#include <shared/log.h>

#include <string.h>

#include <shared/enum_def.h>
#include <shared/file_io.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FILE *fopenUTF8(const char *path, const char *mode) {
#if SYSTEM_WINDOWS

    return _wfopen(GetWideString(path).c_str(), GetWideString(mode).c_str());

#else

    return fopen(path, mode); //non-UTF8 ok

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void AddError(const LogSet *logs,
                     const std::string &path,
                     const char *what1,
                     const char *what2,
                     int err) {
    if (logs) {
        logs->w.f("%s failed: %s\n", what1, path.c_str());

        if (err != 0) {
            logs->i.f("(%s: %s)\n", what2, strerror(err));
        } else {
            logs->i.f("(%s)\n", what2);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class ContType>
static bool LoadFile2(ContType *data,
                      const std::string &path,
                      const LogSet *logs,
                      uint32_t flags,
                      const char *mode) {
    static_assert(sizeof(typename ContType::value_type) == 1, "LoadFile2 can only load into a vector of bytes");
    FILE *f = NULL;
    bool good = false;
    long len;
    size_t num_bytes, num_read;

    f = fopenUTF8(path.c_str(), mode);
    if (!f) {
        if (errno == ENOENT && (flags & LoadFlag_MightNotExist)) {
            // ignore this error.
        } else {
            AddError(logs, path, "load", "open failed", errno);
        }

        goto done;
    }

    if (fseek(f, 0, SEEK_END) == -1) {
        AddError(logs, path, "load", "fseek (1) failed", errno);
        goto done;
    }

    len = ftell(f);
    if (len < 0) {
        AddError(logs, path, "load", "ftell failed", errno);
        goto done;
    }

#if LONG_MAX > SIZE_MAX
    if (len > (long)SIZE_MAX) {
        AddError(logs, path, "load", "file is too large", 0);
        goto done;
    }
#endif

    if (fseek(f, 0, SEEK_SET) == -1) {
        AddError(logs, path, "load", "fseek (2) failed", errno);
        goto done;
    }

    num_bytes = (size_t)len;
    data->resize(num_bytes);

    num_read = fread(data->data(), 1, num_bytes, f);
    if (ferror(f)) {
        AddError(logs, path, "load", "read failed", errno);
        goto done;
    }

    // Number of bytes read may be smaller if mode is rt.
    data->resize(num_read);
    good = true;

done:;
    if (!good) {
        data->clear();
    }

    if (f) {
        fclose(f);
        f = NULL;
    }

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool SaveFile2(const void *data, size_t data_size, const std::string &path, const LogSet *logs, const char *fopen_mode) {
    FILE *f = fopenUTF8(path.c_str(), fopen_mode);
    if (!f) {
        AddError(logs, path, "save", "fopen failed", errno);
        return false;
    }

    fwrite(data, 1, data_size, f);

    bool bad = !!ferror(f);
    int e = errno;

    fclose(f);
    f = nullptr;

    if (bad) {
        AddError(logs, path, "save", "write failed", e);
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadFile(std::vector<uint8_t> *data, const std::string &path, const LogSet *logs, uint32_t flags) {
    if (!LoadFile2(data, path, logs, flags, "rb")) {
        return false;
    }

    data->shrink_to_fit();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadTextFile(std::vector<char> *data, const std::string &path, const LogSet *logs, uint32_t flags) {
    if (!LoadFile2(data, path, logs, flags, "rt")) {
        return false;
    }

    data->push_back(0);
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveFile(const void *data, size_t data_size, const std::string &path, const LogSet *logs) {
    return SaveFile2(data, data_size, path, logs, "wb");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveFile(const std::vector<uint8_t> &data, const std::string &path, const LogSet *logs) {
    return SaveFile(data.data(), data.size(), path, logs);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveTextFile(const std::string &data, const std::string &path, const LogSet *logs) {
    return SaveFile2(data.c_str(), data.size(), path, logs, "wt");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
