#include <shared/system.h>
#include <shared/system_specific.h>
#include <shared/path.h>
#include <stdlib.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string PathGetEXEFileName(void) {
    // Getting the right buffer size is a pain compared to most Win32
    // APIs - so just have a buffer that's likely larger than anything
    // you'll see in practice.
    wchar_t buf[8192];

    if (GetModuleFileNameW(GetModuleHandle(NULL), buf, ARRAYSIZE(buf) - 1) == ARRAYSIZE(buf) - 1) {
        return "";
    }

    buf[ARRAYSIZE(buf) - 1] = 0;

    return GetUTF8String(buf);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathGlob(const std::string &folder, std::function<void(const std::string &path, bool is_folder)> fun) {
    std::wstring spec = GetWideString(PathJoined(folder, "*"));

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(spec.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::string path = PathJoined(folder, GetUTF8String(fd.cFileName));
            bool is_folder = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

            fun(path, is_folder);
        } while (FindNextFileW(h, &fd));

        FindClose(h);
        h = INVALID_HANDLE_VALUE;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool GetFindDataForPath(WIN32_FIND_DATAW *fd, const std::string &path) {
    HANDLE h = FindFirstFileW(GetWideString(path).c_str(), fd);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    FindClose(h);
    h = INVALID_HANDLE_VALUE;

    return true;
}

bool PathIsFileOnDisk(const std::string &path, uint64_t *file_size, bool *can_write) {
    WIN32_FIND_DATAW fd;
    if (!GetFindDataForPath(&fd, path)) {
        return false;
    }

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return false;
    }

    if (file_size) {
        ULARGE_INTEGER file_size_ui;
        file_size_ui.HighPart = fd.nFileSizeHigh;
        file_size_ui.LowPart = fd.nFileSizeLow;
        *file_size = file_size_ui.QuadPart;
    }

    if (can_write) {
        *can_write = !(fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY);
    }

    return true;
}

bool PathIsFolderOnDisk(const std::string &path) {
    WIN32_FIND_DATAW fd;
    if (!GetFindDataForPath(&fd, path)) {
        return false;
    }

    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathCreateFolder(const std::string &folder) {
    // There's not much point checking for errors as it goes. Just
    // save the result of the last one; if it succeeded, the directory
    // was created.
    DWORD last_error = ERROR_SUCCESS;

    for (size_t i = 0; i < folder.size(); ++i) {
        if (i == folder.size() - 1 || PathIsSeparatorChar(folder[i])) {
            // Leave the separator in place, so that things like
            // "C:\\" are handled properly.
            std::wstring tmp = GetWideString(folder.substr(0, i + 1));

            if (!CreateDirectoryW(tmp.c_str(), NULL)) {
                last_error = GetLastError();
            } else {
                last_error = ERROR_SUCCESS;
            }
        }
    }

    switch (last_error) {
    case ERROR_SUCCESS:
    case ERROR_ALREADY_EXISTS:
        return true;

    case ERROR_ACCESS_DENIED:
        errno = EPERM;
        return false;

    default:
        errno = EINVAL; //???
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
