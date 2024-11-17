#define _DARWIN_C_SOURCE
#include <shared/system.h>
#include <shared/path.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

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

        bool is_folder = PathIsFolderOnDisk(path);

        fun(path, is_folder);
    }

    closedir(d);
    d = NULL;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PathIsFileOnDisk(const std::string &path, uint64_t *file_size, bool *can_write) {
    struct stat st;
    if (stat(path.c_str(), &st) == -1) {
        return false;
    }

    if ((st.st_mode & S_IFREG) != S_IFREG) {
        return false;
    }

    if (file_size) {
        if (st.st_size < 0) {
            *file_size = 0; //???
        } else {
            *file_size = (uint64_t)st.st_size;
        }
    }

    if (can_write) {
        *can_write = access(path.c_str(), W_OK) == 0;
    }

    return true;
}

bool PathIsFolderOnDisk(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) == -1) {
        return false;
    }

    if ((st.st_mode & S_IFDIR) != S_IFDIR) {
        return false;
    }

    return true;
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
