#include <shared/system.h>
#include "LoadMemoryDiscImage.h"
#include <beeb/MemoryDiscImage.h>
#include <beeb/DiscGeometry.h>
#include <shared/path.h>
#include <shared/file_io.h>
#include <shared/log.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100) //C4100: 'IDENTIFIER': unreferenced formal parameter
#pragma warning(disable : 4127) //C4127: conditional expression is constant
#pragma warning(disable : 4244) //C4244: 'THING': conversion from 'TYPE' to 'TYPE', possible loss of data
#pragma warning(disable : 4334) //C4334: 'SHIFT': result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#endif
#include <miniz.c>
#include <miniz_zip.c>
#include <miniz_tinfl.c>
#include <miniz_tdef.c>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning(pop)
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string LOAD_METHOD_ZIP = "zip";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const mz_uint BAD_INDEX = ~(mz_uint)0;

static bool LoadDiscImageFromZipFile(
    std::string *image_name,
    std::vector<uint8_t> *data,
    DiscGeometry *geometry,
    const std::string &zip_file_name,
    const LogSet &logs) {
    mz_zip_archive za;
    bool za_opened = 0;
    bool good = 0;
    mz_uint image_index = BAD_INDEX;
    mz_zip_archive_file_stat image_stat = {};
    DiscGeometry image_geometry;
    //    void *data=NULL;
    //
    memset(&za, 0, sizeof za);
    if (!mz_zip_reader_init_file(&za, zip_file_name.c_str(), 0)) {
        logs.e.f("failed to init zip file reader\n");
        goto done;
    }

    za_opened = true;

    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&za); ++i) {
        // the zip file name length is 16 bits or so, so there'll
        // never be any mz_uint wrap or overflow.
        mz_uint name_size = mz_zip_reader_get_filename(&za, i, NULL, 0);

        std::vector<char> name(name_size + 1);

        mz_zip_reader_get_filename(&za, i, name.data(), (mz_uint)name.size());

        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&za, i, &stat)) {
            logs.e.f("failed to get file info from zip file: %s\n", zip_file_name.c_str());
            logs.i.f("(problem file: %s)\n", name.data());
            goto done;
        }

        if (stat.m_uncomp_size > SIZE_MAX) {
            logs.e.f("file is too large in zip file: %s\n", zip_file_name.c_str());
            logs.i.f("(problem file: %s)\n", name.data());
            goto done;
        }

        DiscGeometry g;
        if (FindDiscGeometryFromFileDetails(&g, name.data(), stat.m_uncomp_size, nullptr)) {
            if (image_index != BAD_INDEX) {
                logs.e.f("zip file contains multiple disc images: %s\n", zip_file_name.c_str());
                logs.i.f("(at least: %s, %s)\n", name.data(), image_name->c_str());
                goto done;
            }

            *image_name = name.data();
            image_index = i;
            image_geometry = g;
            image_stat = stat;
        }
    }

    if (image_index == BAD_INDEX) {
        logs.e.f("zip file contains no disc images: %s\n", zip_file_name.c_str());
        goto done;
    }

    data->resize((size_t)image_stat.m_uncomp_size);
    if (!mz_zip_reader_extract_to_mem(&za, image_index, data->data(), data->size(), 0)) {
        logs.e.f("failed to extract disc image from zip: %s\n", zip_file_name.c_str());
        logs.i.f("(disc image: %s)\n", image_name->c_str());
        goto done;
    }

    good = true;
    *geometry = image_geometry;

done:;
    if (za_opened) {
        mz_zip_reader_end(&za);
        za_opened = 0;
    }

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<MemoryDiscImage> LoadMemoryDiscImage(std::string path, const LogSet &logs) {
    std::vector<uint8_t> data;
    DiscGeometry geometry;
    std::string method;

    if (PathCompare(PathGetExtension(path), ".zip") == 0) {
        std::string name;
        if (!LoadDiscImageFromZipFile(&name, &data, &geometry, path, logs)) {
            return nullptr;
        }

        method = LOAD_METHOD_ZIP;

        // Just some fairly arbitrary separator that's easy to find
        // later and rather unlikely to appear in a file name.
        path += "::" + name;
    } else {
        if (!LoadFile(&data, path, &logs)) {
            return nullptr;
        }

        if (!FindDiscGeometryFromFileDetails(&geometry, path.c_str(), data.size(), &logs)) {
            return nullptr;
        }

        method = MemoryDiscImage::LOAD_METHOD_FILE;
    }

    return MemoryDiscImage::LoadFromBuffer(path, method, data.data(), data.size(), geometry, logs);
}
