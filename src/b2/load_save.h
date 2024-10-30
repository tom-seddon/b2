#ifndef HEADER_74221AA5C67346ECAD9C7B417B8ECE3C // -*- mode:c++ -*-
#define HEADER_74221AA5C67346ECAD9C7B417B8ECE3C

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Random grab bag of serialization bits and pieces.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>
#include <stdio.h>

struct LogSet;
class Messages;
struct SDL_Surface;

#include <shared/enum_decl.h>
#include "load_save.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetAssetPath(const std::string &f0);
std::string GetAssetPath(const std::string &f0, const std::string &f1);

// Set folder for config files explicitly. If never set, a system-specific
// default will be used.
void SetConfigFolder(std::string folder);

// Get path to user-specific config file. This will be stored
// somewhere persistent, that may follow the user around.
//
// Windows: APPDATA
// OS X: ~/Library/Application Support/IDENTIFIER/
// Linux: XDG_CONFIG_HOME
std::string GetConfigPath(const std::string &path);

// Get path to user-specific cache file. This will stick around,
// probably.
//
// Windows: LOCALAPPDATA
// OS X: ~/Library/Caches/IDENTIFIER
// Linux: XDG_CACHE_HOME (but do see: https://wiki.debian.org/XDGBaseDirectorySpecification#state)
std::string GetCachePath(const std::string &path);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// File names are assumed to be UTF-8.

bool LoadFile(std::vector<uint8_t> *data, const std::string &path, const LogSet &logs, uint32_t flags = 0);
bool LoadFile(std::vector<uint8_t> *data, const std::string &path, Messages *messages, uint32_t flags = 0);

bool LoadTextFile(std::vector<char> *data, const std::string &path, const LogSet &logs, uint32_t flags = 0);
bool LoadTextFile(std::vector<char> *data, const std::string &path, Messages *messages, uint32_t flags = 0);

bool SaveFile(const void *data, size_t data_size, const std::string &path, const LogSet &logs);
bool SaveFile(const void *data, size_t data_size, const std::string &path, Messages *messages);

bool SaveFile(const std::vector<uint8_t> &data, const std::string &path, const LogSet &logs);
bool SaveFile(const std::vector<uint8_t> &data, const std::string &path, Messages *messages);

bool SaveTextFile(const std::string &data, const std::string &path, const LogSet &logs);
bool SaveTextFile(const std::string &data, const std::string &path, Messages *messages);

bool SaveSDLSurface(SDL_Surface *surface, const std::string &path, Messages *messages);

// Free result using free.
unsigned char *SaveSDLSurfaceToPNGData(SDL_Surface *surface, size_t *png_size_out, Messages *messages);

bool GetFileDetails(size_t *size, bool *can_write, const char *path);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// bool LoadGlobalConfig(std::vector<char> *data,
//                       Messages *messages);

// std::string SaveGlobalConfig();

bool LoadGlobalConfig(Messages *messages);
bool SaveGlobalConfig(Messages *messages);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_OSX

// These don't do any JSON stuff, but they're kind of load/save
// related, and they do need to go somewhere...

// C++-friendly wrappers around [NSWindow saveFrameUsingName] and
// [NSWindow setFrameUsingName].
void SaveCocoaFrameUsingName(void *nswindow, const std::string &name);
bool SetCocoaFrameUsingName(void *nswindow, const std::string &name);

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
