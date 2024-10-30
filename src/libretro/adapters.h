#ifndef HEADER_ADAPTER
#define HEADER_ADAPTER
#include <vector>
#include <string>
#include <stdint.h>
#include "../b2/Messages.h"

#ifdef _WIN32
const char PATH_SEPARATOR = '\\';
#else
const char PATH_SEPARATOR = '/';
#endif // _WIN32

std::string GetAssetPath(const std::string &f0);
std::string GetAssetPath(const std::string &f0, const std::string &f1);
std::string getFileNameFromPath(std::string &f0);
bool GetFileDetails(size_t *size, bool *can_write, const char *path);
std::string strprintf(const char *fmt, ...) PRINTF_LIKE(1, 2);
std::string strprintfv(const char *fmt, va_list v);
//void log_info_OUTPUT(const char *fmt, ...);

bool LoadFile(std::vector<uint8_t> *data,
              const std::string &path,
              Messages *messages,
              uint32_t flags = 0);
bool SaveFile(const std::vector<uint8_t> &data, const std::string &path, Messages *messages);

class FileDialog;

#endif