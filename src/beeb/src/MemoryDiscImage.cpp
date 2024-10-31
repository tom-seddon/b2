#include <shared/system.h>
#include <beeb/DiscImage.h>
#include <shared/path.h>
#include <beeb/MemoryDiscImage.h>
#include <stdio.h>
#include <stdlib.h>
#include <shared/sha1.h>
#include <shared/mutex.h>
#include <shared/debug.h>
#include <inttypes.h>
#include <shared/log.h>
#include <shared/file_io.h>
#include <beeb/DiscGeometry.h>

// static const size_t BYTES_PER_SECTOR=256;
// static const size_t SECTORS_PER_TRACK=10;
//static const size_t NUM_TRACKS=80;
const uint8_t MemoryDiscImage::FILL_BYTE = 0xe5;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string MemoryDiscImage::LOAD_METHOD_FILE = "file";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The m_data pointer doesn't have to be thread safe; a given
// MemoryDiscImage is designed for use from one thread, and the caller
// must provide a mutex if it wants to be clever (and indeed the
// BeebThread does exactly this - maybe without any cleverness -
// with the unique_lock nonsense). But *m_data does need protecting,
// as multiple MemoryDiscImages can refer to the same one.
//
// I'm pretty sure it's safe to lock only on writes - since if this
// MemoryDiscImage is not the only ref, a duplicate will be made, and
// the duplicate modified, meaning concurrent reads can proceed.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// (after modifying this struct, fix up
// MemoryDiscImage::MakeDataUnique.)
struct MemoryDiscImage::Data {
    // The geometry is fixed for the lifetime of the Data. There's no
    // need to lock the mutex to read it.
    //
    // Don't change it though...
    DiscGeometry geometry;

    Mutex mut;

    size_t num_refs = 1;
    std::vector<uint8_t> data;
    std::string hash;

    // (now go and fix up MakeDataUnique.)
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<MemoryDiscImage> MemoryDiscImage::LoadFromBuffer(
    std::string path,
    std::string load_method,
    const void *data, size_t data_size,
    const DiscGeometry &geometry,
    const LogSet &logs) {
    if (data_size == 0) {
        logs.e.f("%s: disc image is empty\n", path.c_str());
        return nullptr;
    }

    if (data_size % geometry.bytes_per_sector != 0) {
        logs.e.f("%s: not a multiple of sector size (%zu)\n",
                 path.c_str(), geometry.bytes_per_sector);
        return nullptr;
    }

    return std::shared_ptr<MemoryDiscImage>(new MemoryDiscImage(std::move(path), std::move(load_method), data, data_size, geometry));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MemoryDiscImage::MemoryDiscImage()
    : m_data(new Data) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MemoryDiscImage::MemoryDiscImage(std::string path,
                                 std::string load_method,
                                 const void *data,
                                 size_t data_size,
                                 const DiscGeometry &geometry)
    : m_data(new Data)
    , m_load_method(std::move(load_method)) {
    m_data->data.assign((const uint8_t *)data, (const uint8_t *)data + data_size);
    m_data->geometry = geometry;
    this->SetName(std::move(path));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MemoryDiscImage::MemoryDiscImage(Data *data, std::string name, std::string load_method)
    : m_data(data)
    , m_load_method(std::move(load_method)) {
    this->SetName(std::move(name));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MemoryDiscImage::~MemoryDiscImage() {
    this->ReleaseData(&m_data);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::CanClone() const {
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::CanSave() const {
    return m_load_method == LOAD_METHOD_FILE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DiscImage> MemoryDiscImage::Clone() const {
    LockGuard<Mutex> lock(m_data->mut);

    ++m_data->num_refs;

    return std::shared_ptr<DiscImage>(new MemoryDiscImage(m_data, m_name, m_load_method));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string MemoryDiscImage::GetHash() const {
    LockGuard<Mutex> lock(m_data->mut);

    if (m_data->hash.empty()) {
        char hash_str[SHA1::DIGEST_STR_SIZE];
        SHA1::HashBuffer(nullptr, hash_str, m_data->data.data(), m_data->data.size());

        m_data->hash = hash_str;
    }

    return m_data->hash;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string MemoryDiscImage::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string MemoryDiscImage::GetLoadMethod() const {
    return m_load_method;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void MemoryDiscImage::SetLoadMethod(std::string load_method) {
//    m_load_method=std::move(load_method);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string MemoryDiscImage::GetDescription() const {
    char description[100];
    snprintf(description, sizeof description, "%s %s %zuT x %zuS",
             m_data->geometry.double_sided ? "DS" : "SS",
             m_data->geometry.double_density ? "DD" : "SD",
             m_data->geometry.num_tracks,
             m_data->geometry.sectors_per_track);

    return description;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<FileDialogFilter> MemoryDiscImage::GetFileDialogFilters() const {
    if (const char *ext = GetExtensionFromDiscGeometry(m_data->geometry)) {
        return {{"BBC disc image", {ext}}};
    }

    return {};
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::SaveToFile(const std::string &file_name, const LogSet &logs) const {
    return SaveFile(m_data->data, file_name, &logs);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void MemoryDiscImage::SetNameAndLoadMethod(std::string name,std::string load_method) {
//    m_name=std::move(name);
//    m_load_method=std::move(load_method);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::Read(uint8_t *value,
                           uint8_t side,
                           uint8_t track,
                           uint8_t sector,
                           size_t offset) const {
    LockGuard<Mutex> lock(m_data->mut);

    size_t index;
    if (!m_data->geometry.GetIndex(&index, side, track, sector, offset)) {
        return false;
    }

    if (index >= m_data->data.size()) {
        *value = FILL_BYTE;
        return true;
    }

    *value = m_data->data[index];
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::Write(uint8_t side,
                            uint8_t track,
                            uint8_t sector,
                            size_t offset,
                            uint8_t value) {
    size_t index;
    if (!m_data->geometry.GetIndex(&index, side, track, sector, offset)) {
        return false;
    }

    this->MakeDataUnique();

    LockGuard<Mutex> lock(m_data->mut);

    if (index >= m_data->data.size()) {
        // Round up to the next sector boundary, but don't try to be
        // any cleverer than that...
        m_data->data.resize((index + m_data->geometry.bytes_per_sector) / m_data->geometry.bytes_per_sector * m_data->geometry.bytes_per_sector, FILL_BYTE);
    }

    if (m_data->data[index] != value) {
        m_data->data[index] = value;
        m_data->hash.clear();
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MemoryDiscImage::Flush() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::GetDiscSectorSize(size_t *size,
                                        uint8_t side,
                                        uint8_t track,
                                        uint8_t sector,
                                        bool double_density) const {
    if (double_density != m_data->geometry.double_density) {
        return false;
    }

    size_t index;
    if (!m_data->geometry.GetIndex(&index, side, track, sector, 0)) {
        return false;
    }

    *size = m_data->geometry.bytes_per_sector;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::IsWriteProtected() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MemoryDiscImage::MakeDataUnique() {
    Data *old_data;

    {
        LockGuard<Mutex> lock(m_data->mut);

        if (m_data->num_refs == 1) {
            // This can't be racing anything else; there's no other
            // instance with with a ref to race.
            return;
        }

        old_data = m_data;
        m_data = new Data;

        m_data->geometry = old_data->geometry;
        m_data->data = old_data->data;
    }

    this->ReleaseData(&old_data);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MemoryDiscImage::ReleaseData(Data **data_ptr) {
    Data *data = *data_ptr;
    *data_ptr = nullptr;

    UniqueLock<Mutex> lock(data->mut);

    ASSERT(data->num_refs > 0);
    --data->num_refs;
    if (data->num_refs == 0) {
        lock.unlock();

        delete data;
        data = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MemoryDiscImage::SetName(std::string name) {
    m_name = std::move(name);

    MUTEX_SET_NAME(m_data->mut, ("MemoryDiscImage: " + m_name));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
