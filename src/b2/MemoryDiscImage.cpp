#include <shared/system.h>
#include <beeb/DiscImage.h>
#include <shared/path.h>
#include "MemoryDiscImage.h"
#include <stdio.h>
#include <stdlib.h>
#include "download.h"
#include "misc.h"
#include <shared/sha1.h>
#include <mutex>
#include <shared/debug.h>
#include "load_save.h"
#include "Messages.h"
#include <inttypes.h>
#include "native_ui.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#elif defined _MSC_VER
#pragma warning(push)
#pragma warning(disable:4100)//C4100: 'IDENTIFIER': unreferenced formal parameter
#pragma warning(disable:4127)//C4127: conditional expression is constant
#pragma warning(disable:4244)//C4244: 'THING': conversion from 'TYPE' to 'TYPE', possible loss of data
#pragma warning(disable:4334)//C4334: 'SHIFT': result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
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

//static const DiscGeometry SSD_GEOMETRY(80,10,256);
//static const DiscGeometry DSD_GEOMETRY(80,10,256,true);
// static const DiscGeometry SDD_GEOMETRY(1,0,0,256);
// static const DiscGeometry DDD_GEOMETRY(2,0,0,256);
static const DiscGeometry ADM_GEOMETRY(80,16,256,false,true);
static const DiscGeometry ADL_GEOMETRY(80,16,256,true,true);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const DiscGeometry SDD_GEOMETRIES[]={
    DiscGeometry(80,16,256,false,true),
    DiscGeometry(40,16,256,false,true),
    DiscGeometry(80,18,256,false,true),
    DiscGeometry(40,16,256,false,true),
};
static const size_t NUM_SDD_GEOMETRIES=sizeof SDD_GEOMETRIES/sizeof SDD_GEOMETRIES[0];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const DiscGeometry DDD_GEOMETRIES[]={
    DiscGeometry(80,16,256,true,true),
    DiscGeometry(40,16,256,true,true),
    DiscGeometry(80,18,256,true,true),
    DiscGeometry(40,16,256,true,true),
};
static const size_t NUM_DDD_GEOMETRIES=sizeof DDD_GEOMETRIES/sizeof DDD_GEOMETRIES[0];

// static const size_t BYTES_PER_SECTOR=256;
// static const size_t SECTORS_PER_TRACK=10;
//static const size_t NUM_TRACKS=80;
const uint8_t MemoryDiscImage::FILL_BYTE=0xe5;

const std::string MemoryDiscImage::LOAD_METHOD_FILE="file";
const std::string MemoryDiscImage::LOAD_METHOD_ZIP="zip";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char SSD_EXT[]=".ssd";
static const char DSD_EXT[]=".dsd";
static const char SDD_EXT[]=".sdd";
static const char DDD_EXT[]=".ddd";
static const char ADM_EXT[]=".adm";
static const char ADL_EXT[]=".adl";

static const char *const ALL_EXTS[]={SSD_EXT,DSD_EXT,SDD_EXT,DDD_EXT,ADM_EXT,ADL_EXT,".zip",nullptr};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void AddDiscImagesFileDialogFilter(FileDialog *fd) {
    std::string patterns;

    for(size_t i=0;ALL_EXTS[i];++i) {
        if(i>0) {
            patterns+=";";
        }

        patterns+="*";
        patterns+=ALL_EXTS[i];
    }

    fd->AddFilter("BBC disc images",patterns);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DiscGeometry::DiscGeometry() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DiscGeometry::DiscGeometry(size_t num_tracks_,
    size_t sectors_per_track_,
    size_t bytes_per_sector_,
    bool double_sided_,
    bool double_density_):
    double_sided(double_sided_),
    double_density(double_density_),
    num_tracks(num_tracks_),
    sectors_per_track(sectors_per_track_),
    bytes_per_sector(bytes_per_sector_)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t DiscGeometry::GetTotalNumBytes() const {
    size_t n=this->num_tracks*this->sectors_per_track*this->bytes_per_sector;

    if(this->double_sided) {
        n*=2;
    }

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool operator==(const DiscGeometry &a,const DiscGeometry &b) {
    if(a.double_sided!=b.double_sided) {
        return false;
    }

    if(a.double_density!=b.double_density) {
        return false;
    }

    if(a.num_tracks!=b.num_tracks) {
        return false;
    }

    if(a.sectors_per_track!=b.sectors_per_track) {
        return false;
    }

    if(a.bytes_per_sector!=b.bytes_per_sector) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool operator!=(const DiscGeometry &a,const DiscGeometry &b) {
    return !(a==b);
}

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
// But it's locked for both for now. Lame (until proven otherwise).
// The locking in general isn't very clever anyway... but I don't
// think this is a big deal. It's not the end of the world if
// emulation is less efficient when there's disk access going on.

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

    std::mutex mut;

    size_t num_refs=1;
    std::vector<uint8_t> data;
    std::string hash;

    // (now go and fix up MakeDataUnique.)
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDiscImage> MemoryDiscImage::LoadFromBuffer(
    std::string path,
    std::string load_method,
    const void *data,size_t data_size,
    const DiscGeometry &geometry,
    Messages *msg)
{
    if(data_size==0) {
        msg->e.f("%s: disc image is empty\n",path.c_str());
        return nullptr;
    }

    if(data_size%geometry.bytes_per_sector!=0) {
        msg->e.f("%s: not a multiple of sector size (%zu)\n",
            path.c_str(),geometry.bytes_per_sector);
        return nullptr;
    }

    return std::unique_ptr<MemoryDiscImage>(new MemoryDiscImage(std::move(path),std::move(load_method),data,data_size,geometry));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindDiscGeometryFromFileSize(
    DiscGeometry *geometry,
    const DiscGeometry *geometries,
    size_t num_geometries,
    size_t size,
    const char *name,
    Messages *msg)
{
    for(size_t i=0;i<num_geometries;++i) {
        const DiscGeometry *g=&geometries[i];

        if(size==g->GetTotalNumBytes()) {
            *geometry=*g;
            return true;
        }
    }

    msg->e.f("invalid size for file: %s\n",name);

    msg->i.f("(size is %zu; valid sizes are: ",size);
    for(size_t i=0;i<num_geometries;++i) {
        if(i>0) {
            msg->i.f("; ");
        }

        msg->i.f("%zu",geometries[i].GetTotalNumBytes());
    }
    msg->i.f(")\n");

    return false;
}

static bool GetSingleDensityDiscGeometry(
    DiscGeometry *geometry,
    bool double_sided,
    const char *name,
    size_t size,
    Messages *msg)
{
    *geometry=DiscGeometry(80,10,256,double_sided);

    if(size%256!=0) {
        msg->e.f("invalid size for file: %s\n",name);
        msg->i.f("(length %zu not a multiple of sector size 256)\n",
            size);
        return false;
    }

    if(size>geometry->GetTotalNumBytes()) {
        msg->e.f("invalid size for file: %s\n",name);
        msg->i.f("(length %zu larger than maximum %zu for %d*%zu*%zu sectors)\n",
            size,
            geometry->GetTotalNumBytes(),
            geometry->double_sided?2:1,
            geometry->num_tracks,
            geometry->sectors_per_track);
        return false;
    }

    return true;
}

static bool GetDiscGeometryFromFileDetails(
    DiscGeometry *geometry,
    const char *name,
    size_t size,
    Messages *msg)
{
    std::string ext=PathGetExtension(name);

    //LOGF(OUTPUT,"%s: %s: %zu bytes\n",__func__,name,size);

    if(PathCompare(ext,SSD_EXT)==0) {
        return GetSingleDensityDiscGeometry(geometry,false,name,size,msg);
    } else if(PathCompare(ext,DSD_EXT)==0) {
        return GetSingleDensityDiscGeometry(geometry,true,name,size,msg);
    } else if(PathCompare(ext,ADL_EXT)==0) {
        return FindDiscGeometryFromFileSize(geometry,&ADL_GEOMETRY,1,size,name,msg);
    } else if(PathCompare(ext,ADM_EXT)==0) {
        return FindDiscGeometryFromFileSize(geometry,&ADM_GEOMETRY,1,size,name,msg);
    } else if(PathCompare(ext,SDD_EXT)==0) {
        return FindDiscGeometryFromFileSize(geometry,SDD_GEOMETRIES,NUM_SDD_GEOMETRIES,size,name,msg);
    } else if(PathCompare(ext,DDD_EXT)==0) {
        return FindDiscGeometryFromFileSize(geometry,DDD_GEOMETRIES,NUM_DDD_GEOMETRIES,size,name,msg);
    } else {
        msg->e.f("unknown extension: %s\n",ext.c_str());
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool IsGeometryInList(const DiscGeometry &geometry,const DiscGeometry *geometries,size_t num_geometries) {
    for(size_t i=0;i<num_geometries;++i) {
        if(geometries[i]==geometry) {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *GetExtensionFromDiscGeometry(const DiscGeometry &geometry) {
    if(geometry.double_density) {
        if(geometry==ADL_GEOMETRY) {
            return ADL_EXT;
        } else if(geometry==ADM_GEOMETRY) {
            return ADM_EXT;
        } else if(geometry.double_sided) {
            if(IsGeometryInList(geometry,DDD_GEOMETRIES,NUM_DDD_GEOMETRIES)) {
                return DDD_EXT;
            }
        } else {
            if(IsGeometryInList(geometry,SDD_GEOMETRIES,NUM_SDD_GEOMETRIES)) {
                return SDD_EXT;
            }
        }
    } else {
        if(geometry.double_sided) {
            return DSD_EXT;
        } else {
            return SSD_EXT;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const mz_uint BAD_INDEX=~(mz_uint)0;

static bool LoadDiscImageFromZipFile(
    std::string *image_name,
    std::vector<uint8_t> *data,
    DiscGeometry *geometry,
    const std::string &zip_file_name,
    Messages *msg)
{
    mz_zip_archive za;
    bool za_opened=0;
    bool good=0;
    mz_uint image_index=BAD_INDEX;
    mz_zip_archive_file_stat image_stat={};
    DiscGeometry image_geometry;
    //    void *data=NULL;
    //
    memset(&za,0,sizeof za);
    if(!mz_zip_reader_init_file(&za,zip_file_name.c_str(),0)) {
        msg->e.f("failed to init zip file reader\n");
        goto done;
    }

    za_opened=true;

    for(mz_uint i=0;i<mz_zip_reader_get_num_files(&za);++i) {
        // the zip file name length is 16 bits or so, so there'll
        // never be any mz_uint wrap or overflow.
        mz_uint name_size=mz_zip_reader_get_filename(&za,i,NULL,0);

        std::vector<char> name(name_size+1);

        mz_zip_reader_get_filename(&za,i,name.data(),(mz_uint)name.size());

        mz_zip_archive_file_stat stat;
        if(!mz_zip_reader_file_stat(&za,i,&stat)) {
            msg->e.f("failed to get file info from zip file: %s\n",zip_file_name.c_str());
            msg->i.f("(problem file: %s)\n",name.data());
            goto done;
        }

        if(stat.m_uncomp_size>SIZE_MAX) {
            msg->e.f("file is too large in zip file: %s\n",zip_file_name.c_str());
            msg->i.f("(problem file: %s)\n",name.data());
            goto done;
        }

        DiscGeometry g;
        if(GetDiscGeometryFromFileDetails(&g,name.data(),stat.m_uncomp_size,nullptr)) {
            if(image_index!=BAD_INDEX) {
                msg->e.f("zip file contains multiple disc images: %s\n",zip_file_name.c_str());
                msg->i.f("(at least: %s, %s)\n",name.data(),image_name->c_str());
                goto done;
            }

            *image_name=name.data();
            image_index=i;
            image_geometry=g;
            image_stat=stat;
        }
    }

    if(image_index==BAD_INDEX) {
        msg->e.f("zip file contains no disc images: %s\n",zip_file_name.c_str());
        goto done;
    }

    data->resize((size_t)image_stat.m_uncomp_size);
    if(!mz_zip_reader_extract_to_mem(&za,image_index,data->data(),data->size(),0)) {
        msg->e.f("failed to extract disc image from zip: %s\n",zip_file_name.c_str());
        msg->i.f("(disc image: %s)\n",image_name->c_str());
        goto done;
    }

    good=true;
    *geometry=image_geometry;


done:;
    if(za_opened) {
        mz_zip_reader_end(&za);
        za_opened=0;
    }

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadDiscImage(std::vector<uint8_t> *data,
    DiscGeometry *geometry,
    const std::string &path,
    Messages *msg)
{
    if(!LoadFile(data,path,msg)) {
        return false;
    }

    if(!GetDiscGeometryFromFileDetails(geometry,
        path.c_str(),
        data->size(),
        msg))
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDiscImage> MemoryDiscImage::LoadFromFile(
    std::string path,
    Messages *msg)
{
    std::vector<uint8_t> data;
    DiscGeometry geometry;
    std::string method;

    if(PathCompare(PathGetExtension(path),".zip")==0) {
        std::string name;
        if(!LoadDiscImageFromZipFile(&name,&data,&geometry,path,msg)) {
            return nullptr;
        }

        method=LOAD_METHOD_ZIP;

        // Just some fairly arbitrary separator that's easy to find
        // later and rather unlikely to appear in a file name.
        path+="::"+name;
    } else {
        if(!LoadDiscImage(&data,&geometry,path,msg)) {
            return nullptr;
        }

        method=LOAD_METHOD_FILE;
    }

    return LoadFromBuffer(path,method,data.data(),data.size(),geometry,msg);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MemoryDiscImage::MemoryDiscImage():
    m_data(new Data)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MemoryDiscImage::MemoryDiscImage(std::string path,std::string load_method,const void *data,size_t data_size,const DiscGeometry &geometry):
    m_data(new Data),
    m_name(std::move(path)),
    m_load_method(std::move(load_method))
{
    m_data->data.assign((const uint8_t *)data,(const uint8_t *)data+data_size);
    m_data->geometry=geometry;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MemoryDiscImage::MemoryDiscImage(Data *data,std::string name,std::string load_method):
    m_data(data),
    m_name(std::move(name)),
    m_load_method(std::move(load_method))
{
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

std::shared_ptr<DiscImage> MemoryDiscImage::Clone() const {
    std::lock_guard<std::mutex> lock(m_data->mut);

    ++m_data->num_refs;

    return std::shared_ptr<DiscImage>(new MemoryDiscImage(m_data,m_name,m_load_method));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string MemoryDiscImage::GetHash() const {
    std::lock_guard<std::mutex> lock(m_data->mut);

    if(m_data->hash.empty()) {
        char hash_str[SHA1::DIGEST_STR_SIZE];
        SHA1::HashBuffer(nullptr,hash_str,m_data->data.data(),m_data->data.size());

        m_data->hash=hash_str;
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

void MemoryDiscImage::SetName(std::string name) {
    m_name=std::move(name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string MemoryDiscImage::GetLoadMethod() const {
    return m_load_method;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MemoryDiscImage::SetLoadMethod(std::string load_method) {
    m_load_method=std::move(load_method);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string MemoryDiscImage::GetDescription() const {
    return strprintf("%s %s %zuT x %zuS",
        m_data->geometry.double_sided?"DS":"SS",
        m_data->geometry.double_density?"DD":"SD",
        m_data->geometry.num_tracks,
        m_data->geometry.sectors_per_track);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MemoryDiscImage::AddFileDialogFilter(FileDialog *fd) const {
    if(const char *ext=GetExtensionFromDiscGeometry(m_data->geometry)) {
        std::string pattern=std::string("*")+ext;
        fd->AddFilter("BBC disc image",pattern);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::SaveToFile(const std::string &file_name,Messages *msg) const {
    return SaveFile(m_data->data,file_name,msg);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::Read(uint8_t *value,uint8_t side,uint8_t track,uint8_t sector,size_t offset) const {
    std::lock_guard<std::mutex> lock(m_data->mut);

    size_t index;
    if(!this->GetIndex(&index,side,track,sector,offset)) {
        return false;
    }

    if(index>=m_data->data.size()) {
        *value=FILL_BYTE;
        return true;
    }

    *value=m_data->data[index];
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::Write(uint8_t side,uint8_t track,uint8_t sector,size_t offset,uint8_t value) {
    size_t index;
    if(!this->GetIndex(&index,side,track,sector,offset)) {
        return false;
    }

    this->MakeDataUnique();

    std::lock_guard<std::mutex> lock(m_data->mut);

    if(index>=m_data->data.size()) {
        // Round up to the next sector boundary, but don't try to be
        // any cleverer than that...
        m_data->data.resize((index+m_data->geometry.bytes_per_sector)/m_data->geometry.bytes_per_sector*m_data->geometry.bytes_per_sector,FILL_BYTE);
    }

    if(m_data->data[index]!=value) {
        m_data->data[index]=value;
        m_data->hash.clear();
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::GetDiscSectorSize(size_t *size,uint8_t side,uint8_t track,uint8_t sector,bool double_density) const {
    if(double_density!=m_data->geometry.double_density) {
        return false;
    }

    size_t index;
    if(!this->GetIndex(&index,side,track,sector,0)) {
        return false;
    }

    *size=m_data->geometry.bytes_per_sector;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::IsWriteProtected() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MemoryDiscImage::GetIndex(size_t *index,
    uint8_t side,
    uint8_t track,
    uint8_t sector,
    size_t offset) const
{
    if(side>=(m_data->geometry.double_sided?2:1)) {
        return false;
    }

    if(track>=m_data->geometry.num_tracks) {
        return false;
    }

    if(sector>=m_data->geometry.sectors_per_track) {
        return false;
    }

    if(offset>=m_data->geometry.bytes_per_sector) {
        return false;
    }

    *index=0;
    *index+=track;              // in tracks
    if(m_data->geometry.double_sided) {
        *index*=2;
        *index+=side;           // adjusted for track interleaving
    }
    *index*=m_data->geometry.sectors_per_track; // in sectors
    *index+=sector;
    *index*=m_data->geometry.bytes_per_sector;  // in bytes
    *index+=offset;                             // +offset

    ASSERT(*index<m_data->geometry.GetTotalNumBytes());

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MemoryDiscImage::MakeDataUnique() {
    Data *old_data;

    {
        std::lock_guard<std::mutex> lock(m_data->mut);

        if(m_data->num_refs==1) {
            // This can't be racing anything else; there's no other
            // instance with with a ref to race.
            return;
        }

        old_data=m_data;
        m_data=new Data;

        m_data->geometry=old_data->geometry;
        m_data->data=old_data->data;
    }

    this->ReleaseData(&old_data);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MemoryDiscImage::ReleaseData(Data **data_ptr) {
    Data *data=*data_ptr;
    *data_ptr=nullptr;

    std::unique_lock<std::mutex> lock(data->mut);

    ASSERT(data->num_refs>0);
    --data->num_refs;
    if(data->num_refs==0) {
        lock.unlock();

        delete data;
        data=nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
