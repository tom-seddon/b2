#include "../shared/h/shared/system.h"
#include "adapters.h"
#include "../shared/h/shared/path.h"

std::string GetAssetPath(const std::string &f0) 
{
printf ("GetAssetPath: %s\n",f0.c_str());
std::string path = "";
return path;
}
std::string GetAssetPath(const std::string &f0, const std::string &f1) 
{
std::string path = f0+PATH_SEPARATOR+f1;
printf ("GetAssetPath: %s\n",path.c_str());
return path;
}

std::string getFileNameFromPath(std::string &f0) {
  size_t ridx = f0.rfind(PATH_SEPARATOR);
    if(ridx != std::string::npos)
      return f0.substr(ridx+1);
    else
      return f0;
}

// lifted from LoadSave.cpp
bool GetFileDetails(size_t *size, bool *can_write, const char *path)
{
    FILE *fp = nullptr;
    bool good = false;
    long len;

    fp = fopenUTF8(path, "r+b");
    if (fp) {
        *can_write = true;
    } else {
        // doesn't exist, or read-only.
        fp = fopenUTF8(path, "rb");
        if (!fp) {
            // assume doesn't exist.
            goto done;
        }

        *can_write = false;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        goto done;
    }

    len = ftell(fp);
    if (len < 0) {
        goto done;
    }

    if ((unsigned long)len > SIZE_MAX) {
        *size = SIZE_MAX;
    } else {
        *size = (size_t)len;
    }

    good = true;
done:
    if (fp) {
        fclose(fp);
        fp = nullptr;
    }

    return good;
}




bool LoadFile(std::vector<uint8_t> *data,
              const std::string &path,
              Messages *messages,
              uint32_t flags)
{
printf ("LoadFile: %s\n",path.c_str());
return false;
}
bool SaveFile(const std::vector<uint8_t> &data, const std::string &path, Messages *messages)
{
printf ("SaveFile: %s\n",path.c_str());
return false;
}

class FileDialog {
  public:
    void AddFilter(std::string title, const std::vector<std::string> extensions);
};
void FileDialog::AddFilter(std::string title, std::vector<std::string> patterns) {
    (void)title;
    (void)patterns;
}
