#define _DARWIN_C_SOURCE
#include <shared/system.h>
#include <shared/path.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathGlob(const std::string &folder,
              std::function<void(const std::string &path,
                                 bool is_folder)>
                  fun) {
    DIR *d = opendir(folder.c_str());
    if (!d) {
        return -1;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        std::string path = PathJoined(folder, de->d_name);

        int is_folder = PathIsFolderOnDisk(path);

        fun(path, is_folder);
    }

    closedir(d);
    d = NULL;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool IsThingOnDisk(const std::string &path, mode_t mask) {
    struct stat st;
    if (stat(path.c_str(), &st) == -1) {
        return false;
    }

    if ((st.st_mode & mask) == mask) {
        return true;
    } else {
        return false;
    }
}

bool PathIsFileOnDisk(const std::string &path) {
    return IsThingOnDisk(path, S_IFREG);
}

bool PathIsFolderOnDisk(const std::string &path) {
    return IsThingOnDisk(path, S_IFDIR);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathCreateFolder(const std::string &path) {
    int last_rc = -1;
    int last_errno = 0;

    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '/') {
            std::string tmp = path.substr(0, i + 1);

            last_rc = mkdir(tmp.c_str(), 0777);
            last_errno = errno;
        }
    }

    /* Ignore certain errors. */
    if (last_rc == -1 && (last_errno == EEXIST || last_errno == EPERM)) {
        last_rc = 0;
        last_errno = 0;
    }

    errno = last_errno;
    return last_rc == 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
