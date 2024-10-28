#ifndef HEADER_4466BEFE653742FAA5F44358E1E758AB // -*- mode:c++ -*-
#define HEADER_4466BEFE653742FAA5F44358E1E758AB

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <vector>
#include <string>

#include "enum_decl.h"
#include "file_io.inl"
#include "enum_end.h"

struct LogSet;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FILE *fopenUTF8(const char *path, const char *mode);

bool LoadFile(std::vector<uint8_t> *data, const std::string &path, const LogSet *logs, uint32_t flags = 0);
bool LoadTextFile(std::vector<char> *data, const std::string &path, const LogSet *logs, uint32_t flags = 0);

bool SaveFile(const void *data, size_t data_size, const std::string &path, const LogSet *logs);
bool SaveFile(const std::vector<uint8_t> &data, const std::string &path, const LogSet *logs);
bool SaveTextFile(const std::string &data, const std::string &path, const LogSet *logs);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
