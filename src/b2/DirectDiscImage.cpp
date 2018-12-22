#include <shared/system.h>
#include <string>
#include "misc.h"
#include "load_save.h"
#include "DirectDiscImage.h"
#include "native_ui.h"
#include "Messages.h"
#include <limits.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string DirectDiscImage::LOAD_METHOD_DIRECT="direct";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DirectDiscImage> DirectDiscImage::CreateForFile(std::string path,
                                                                Messages *msg)
{
    size_t size;
    bool can_write;
    if(!GetFileDetails(&size,&can_write,path.c_str())) {
        msg->e.f("Couldn't get details for file: %s\n",path.c_str());
        return nullptr;
    }

    DiscGeometry geometry;
    if(!FindDiscGeometryFromFileDetails(&geometry,path.c_str(),size,msg)) {
        return nullptr;
    }

    return std::shared_ptr<DirectDiscImage>(new DirectDiscImage(std::move(path),geometry,!can_write));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::CanClone() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::CanSave() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DiscImage> DirectDiscImage::Clone() const {
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string DirectDiscImage::GetHash() const {
    return std::string();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string DirectDiscImage::GetName() const {
    return m_path;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string DirectDiscImage::GetLoadMethod() const {
    return LOAD_METHOD_DIRECT;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO - basically the same as MemoryDiscImage.
std::string DirectDiscImage::GetDescription() const {
    return strprintf("%s %s %zuT x %zuS",
                     m_geometry.double_sided?"DS":"SS",
                     m_geometry.double_density?"DD":"SD",
                     m_geometry.num_tracks,
                     m_geometry.sectors_per_track);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO - basically the same as MemoryDiscImage.
void DirectDiscImage::AddFileDialogFilter(FileDialog *fd) const {
    if(const char *ext=GetExtensionFromDiscGeometry(m_geometry)) {
        std::string pattern=std::string("*")+ext;
        fd->AddFilter("BBC disc image",{pattern});
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::SaveToFile(const std::string &file_name,Messages *msg) const {
    std::vector<uint8_t> data;
    if(!LoadFile(&data,m_path,msg)) {
        return false;
    }

    if(!SaveFile(data,file_name,msg)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::Read(uint8_t *value,
                           uint8_t side,
                           uint8_t track,
                           uint8_t sector,
                           size_t offset) const
{
    FILE *fp=this->fopenAndSeek("rb",side,track,sector,offset);
    if(!fp) {
        return false;
    }

    int c=fgetc(fp);
    if(c==EOF) {
        // This case is OK - the disc image is logically its full size, even
        // when truncated.
        //
        // Returns 0s rather than a fill byte - writing past end supposedly
        // fills the gap with zeroes (https://en.cppreference.com/w/c/io/fseek)
        *value=0;
    } else {
        *value=(uint8_t)c;
    }

    fclose(fp);
    fp=nullptr;

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::Write(uint8_t side,
                            uint8_t track,
                            uint8_t sector,
                            size_t offset,
                            uint8_t value)
{
    FILE *fp=this->fopenAndSeek("r+b",side,track,sector,offset);
    if(!fp) {
        return false;
    }

    bool good=false;
    if(fputc(value,fp)!=EOF) {
        good=true;
    }

    fclose(fp);
    fp=nullptr;

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TDOO - same as MemoryDiscImage.
bool DirectDiscImage::GetDiscSectorSize(size_t *size,
                                        uint8_t side,
                                        uint8_t track,
                                        uint8_t sector,
                                        bool double_density) const
{
    if(double_density!=m_geometry.double_density) {
        return false;
    }

    size_t index;
    if(!m_geometry.GetIndex(&index,side,track,sector,0)) {
        return false;
    }

    *size=m_geometry.bytes_per_sector;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DirectDiscImage::IsWriteProtected() const {
    return m_write_protected;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DirectDiscImage::DirectDiscImage(std::string path,
                                 const DiscGeometry &geometry,
                                 bool write_protected):
m_path(std::move(path)),
m_geometry(geometry),
m_write_protected(write_protected)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FILE *DirectDiscImage::fopenAndSeek(const char *mode,
                                    uint8_t side,
                                    uint8_t track,
                                    uint8_t sector,
                                    size_t offset) const {
    size_t index;
    if(!m_geometry.GetIndex(&index,side,track,sector,offset)) {
        return nullptr;
    }

    if(index>LONG_MAX) {
        return nullptr;
    }

    FILE *fp=fopenUTF8(m_path.c_str(),mode);
    if(!fp) {
        return nullptr;
    }

    if(fseek(fp,(long)index,SEEK_SET)!=0) {
        fclose(fp);
        return nullptr;
    }

    return fp;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
