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
std::string path = f0+"/"+f1;
printf ("GetAssetPath: %s\n",path.c_str());
return path;
}

bool GetFileDetails(size_t *size, bool *can_write, const char *path)
{
printf ("GetFileDetails: %s\n",path);
}
bool LoadFile(std::vector<uint8_t> *data,
              const std::string &path,
              Messages *messages,
              uint32_t flags)
{
printf ("LoadFile: %s\n",path);
}
bool SaveFile(const std::vector<uint8_t> &data, const std::string &path, Messages *messages)
{
printf ("SaveFile: %s\n",path.c_str());
}

std::shared_ptr<const std::array<uint8_t, 16384>> LoadROM(const std::string &name) {
   // TODO: use system dir
    std::string path = "/media/storage/Documents/dev/b2-libretro/rom/"+ name;

    std::vector<uint8_t> data;
    PathLoadBinaryFile(&data, path);

    auto rom = std::make_shared<std::array<uint8_t, 16384>>();

    printf("rom size check: %s, %d, %d\n",name.c_str(),data.size(), rom->size());
    for (size_t i = 0; i < data.size(); ++i) {
        (*rom)[i] = data[i];
    }

    return rom;
}