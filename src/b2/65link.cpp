#include <shared/system.h>
#include "65link.h"
#include <shared/path.h>
#include <shared/load_store.h>
#include <shared/debug.h>
#include "MemoryDiscImage.h"
#include <string.h>
#include <stdlib.h>
#include "misc.h"
#include "load_save.h"
#include "Messages.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const size_t NUM_SECTORS=800;
static const size_t MAX_FILE_SIZE=(1<<18)-1;
static const size_t MAX_NUM_FILES=31;

// in the absence of any other info: if there's a $.!BOOT file, this
// option is applied, otherwise it's option 0.
static const uint8_t DEFAULT_BOOT_OPT=3;

const std::string LOAD_METHOD_65LINK="65link";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct CharMapping {
    char code[3];
    char c;
};
typedef struct CharMapping CharMapping;

static const CharMapping g_char_mappings[]={
    {"sp",' ',},
    {"xm",'!',},
    {"dq",'"',},
    {"ha",'#',},
    {"do",'$',},
    {"pc",'%',},
    {"am",'&',},
    {"sq",'\'',},
    {"rb",'(',},
    {"lb",')',},
    {"as",'*',},
    {"pl",'+',},
    {"cm",',',},
    {"mi",'-',},
    {"pd",'.',},
    {"fs",'/',},
    {"co",':',},
    {"sc",';',},
    {"st",'<',},
    {"eq",'=',},
    {"lt",'>',},
    {"qm",'?',},
    {"at",'@',},
    {"hb",'[',},
    {"bs",'\\',},
    {"bh",']',},
    {"po",'^',},
    {"un",'_',},
    {"bq",'`',},
    {"cb",'{',},
    {"ba",'|',},
    {"bc",'}',},
    {"no",'~',},
    {{0}},
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BBCFile {
    std::string pc_fname;

    // in DFS format. 0-6=name, 7=dir.
    char bbc_name[8]={};

    uint32_t load=0,exec=0,attr=0;

    std::vector<uint8_t> data;

    size_t sector;
};
typedef struct BBCFile BBCFile;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetPCChar(uint8_t bbc) {
    bbc&=0x7f;

    for(const CharMapping *m=g_char_mappings;m->c!=0;++m) {
        if(m->c==bbc) {
            return std::string("_")+m->code;
        }
    }

    if(bbc==0) {
        return "";
    } else {
        return std::string(1,bbc&0x7f);
    }
}

// name is in DFS format: 8 bytes, 0-6=name, 7=dir
static std::string GetPCName(const uint8_t *dfs_name) {
    size_t n=7;
    while(n>0&&dfs_name[n-1]==' ') {
        --n;
    }

    std::string result=GetPCChar(dfs_name[7]);

    for(size_t i=0;i<n;++i) {
        result+=GetPCChar(dfs_name[i]);
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int GetBBCName(char *bbc_name,const std::string &pc_name) {
    const char *p=pc_name.c_str();
    size_t bbc_idx=0;

    char tmp[8];

    while(*p!=0) {
        char bbc_char=0;

        if(*p=='_') {
            const CharMapping *m;

            for(m=g_char_mappings;m->c!=0;++m) {
                if(p[1]==m->code[0]&&p[2]==m->code[1]) {
                    break;
                }
            }

            bbc_char=m->c;
            p+=3;
        } else {
            bbc_char=*p++;
        }

        if(bbc_char<32||bbc_char>126) {
            return 0;
        }

        if(bbc_idx==8) {
            return 0;
        }

        tmp[bbc_idx++]=bbc_char;
    }

    if(bbc_idx==0) {
        return 0;
    }

    memset(bbc_name,' ',8);
    memcpy(bbc_name,tmp+1,bbc_idx-1);
    bbc_name[7]=tmp[0];

    return 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SaveBBCFile(std::vector<BBCFile> *bbc_files,const std::string &path,int is_folder) {
    if(is_folder) {
        return;
    }

    if(!PathGetExtension(path).empty()) {
        return;
    }

    std::string fname=PathGetName(path);
    if(fname.empty()) {
        return;
    }

    BBCFile bbc_file;

    if(!GetBBCName(bbc_file.bbc_name,fname)) {
        return;
    }

    bbc_file.pc_fname=path;

    bbc_files->push_back(bbc_file);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadDiscImageSideDataFrom65LinkFolder(
    std::vector<uint8_t> *data,
    const std::string &path,
    int opt,
    bool ok_if_empty,
    Messages *msg)
{
    ASSERT(opt<=3);

    std::vector<BBCFile> bbc_files;
    PathGlob(path,[&](const std::string &path,bool is_folder) {
        SaveBBCFile(&bbc_files,path,is_folder);
    });

    if(bbc_files.empty()) {
        if(ok_if_empty) {
            return true;
        } else {
            msg->e.f("no BBC files in folder: %s\n",path.c_str());
            return false;
        }
    } else if(bbc_files.size()>MAX_NUM_FILES) {
        msg->e.f("too many BBC files in folder: %s\n",path.c_str());
        msg->i.f("(total: %zu; DFS maximum: %zu)\n",bbc_files.size(),MAX_NUM_FILES);
        return false;
    }

    size_t num_sectors=2;
    for(BBCFile &bbc_file:bbc_files) {
        if(!LoadFile(&bbc_file.data,bbc_file.pc_fname,msg)) {
            return false;
        }

        if(memcmp(bbc_file.bbc_name,"!BOOT  $",8)==0) {
            if(opt<0) {
                opt=DEFAULT_BOOT_OPT;
            }
        }

        if(bbc_file.data.size()>MAX_FILE_SIZE) {
            msg->e.f("file too large for DFS: %s (%zu bytes; max: %zu)\n",
                bbc_file.pc_fname.c_str(),
                bbc_file.data.size(),
                MAX_FILE_SIZE);

            return false;
        }

        std::string lea_fname=strprintf("%s.lea",bbc_file.pc_fname.c_str());

        std::vector<uint8_t> lea_data;
        if(LoadFile(&lea_data,lea_fname,msg,LoadFlag_MightNotExist)) {
            if(lea_data.size()!=12) {
                msg->e.f("invalid LEA file: %s\n",lea_fname.c_str());
                return false;
            }

            bbc_file.load=Load32LE(&lea_data[0]);

            bbc_file.exec=Load32LE(&lea_data[4]);

            bbc_file.attr=Load32LE(&lea_data[8]);
        }

        bbc_file.sector=num_sectors;
        num_sectors+=(bbc_file.data.size()+255)/256;
    }

    if(opt<0) {
        opt=0;
    }

    if(num_sectors>NUM_SECTORS-2) {
        msg->e.f("too much data: %zu sectors (max: %zu)\n",
            num_sectors,
            NUM_SECTORS-2);
        return false;
    }

    data->resize(512,0);

    // Title.
    // (already 0)


    // Disc metadata.
    data->at(256+4)=0;//Disc write count
    data->at(256+5)=(uint8_t)(bbc_files.size()*8);
    data->at(256+6)=(((unsigned)opt&3)<<4)|(NUM_SECTORS>>8&3);
    data->at(256+7)=NUM_SECTORS&255;

    // Catalogue.
    for(size_t i=0;i<bbc_files.size();++i) {
        const BBCFile *bbc_file=&bbc_files[i];

        // Files are stored in reverse order of their start sector.
        size_t offset=8+8*(bbc_files.size()-1-i);

        memcpy(&data->at(0+offset),bbc_file->bbc_name,8);

        Store16LE(&data->at(256+offset+0),(uint16_t)bbc_file->load);
        Store16LE(&data->at(256+offset+2),(uint16_t)bbc_file->exec);
        Store16LE(&data->at(256+offset+4),(uint16_t)bbc_file->data.size());

        data->at(256+offset+6)=0;

        if(bbc_file->exec&0xffff0000) {
            data->at(256+offset+6)|=3<<6;
        }

        ASSERT((bbc_file->data.size()>>18)==0);
        data->at(56+offset+6)|=(bbc_file->data.size()>>16&3)<<4;

        if(bbc_file->load&0xffff0000) {
            data->at(256+offset+6)|=3<<2;
        }

        ASSERT((bbc_file->sector>>10)==0);
        data->at(256+offset+6)|=(bbc_file->sector>>8)&3;

        data->at(256+offset+7)=(uint8_t)bbc_file->sector;
    }

    for(const BBCFile &bbc_file:bbc_files) {
        data->insert(data->end(),bbc_file.data.begin(),bbc_file.data.end());

        while(data->size()%256!=0) {
            data->push_back(0);
        }
    }

    return true;
}

std::unique_ptr<DiscImage> LoadDiscImageFrom65LinkFolder(const std::string &path,Messages *msg) {
    std::vector<uint8_t> side_data[2];
    std::vector<uint8_t> bootopts;

    std::string drive0=PathJoined(path,"0");
    if(PathIsFolderOnDisk(drive0)) {
        // probably the volume root.
        LoadFile(&bootopts,PathJoined(path,"BootOpt"),msg,LoadFlag_MightNotExist);

        int opt0=-1;
        if(bootopts.size()>0) {
            opt0=bootopts[0];
        }

        if(!LoadDiscImageSideDataFrom65LinkFolder(&side_data[0],drive0,opt0,false,msg)) {
            return nullptr;
        }

        std::string drive2=PathJoined(path,"2");
        if(PathIsFolderOnDisk(drive2)) {
            int opt2=-1;
            if(bootopts.size()>2) {
                opt2=bootopts[2];
            }

            // If drive 2 is empty, that's OK. It will be loaded as a
            // single-sided image.
            if(!LoadDiscImageSideDataFrom65LinkFolder(&side_data[1],drive2,opt2,true,msg)) {
                return nullptr;
            }
        }
    } else {
        // probably an individual volume folder.
        std::vector<uint8_t> bootops;
        LoadFile(&bootopts,PathJoined(path,"..","BootOpt"),msg,LoadFlag_MightNotExist);

        int opt0=-1;
        if(bootopts.size()>0) {
            opt0=bootopts[0];
        }

        if(!LoadDiscImageSideDataFrom65LinkFolder(&side_data[0],path,opt0,false,msg)) {
            return nullptr;
        }
    }

    std::vector<uint8_t> data;
    DiscGeometry geometry(80,10,256,false);
    if(side_data[1].empty()) {
        data=std::move(side_data[0]);
    } else {
        if(side_data[0].size()<side_data[1].size()) {
            side_data[0].resize(side_data[1].size(),MemoryDiscImage::FILL_BYTE);
        } else if(side_data[1].size()<side_data[0].size()) {
            side_data[1].resize(side_data[0].size(),MemoryDiscImage::FILL_BYTE);
        }
        ASSERT(side_data[0].size()==side_data[1].size());

        // Interleave the tracks.
        //
        // (The DiscGeometry/MemoryDiscImage stuff should probably be more
        // clever, but this will do for now.)
        size_t bytes_per_track=geometry.sectors_per_track*geometry.bytes_per_sector;

        size_t a=0;
        while(a<side_data[0].size()) {
            size_t b=a+bytes_per_track;

            if(b>side_data[0].size()) {
                b=side_data[0].size();
            }

            for(size_t i=0;i<2;++i) {
                const uint8_t *begin=side_data[i].data()+a;
                const uint8_t *end=side_data[i].data()+b;
                data.insert(data.end(),begin,end);
            }

            a=b;
        }

        geometry.double_sided=true;
    }

    //SaveFile(data,"C:/temp/test.dsd",msg);

    std::unique_ptr<DiscImage> disc_image=MemoryDiscImage::LoadFromBuffer(path,LOAD_METHOD_65LINK,data.data(),data.size(),geometry,msg);
    return disc_image;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool SaveDiscSideTo65LinkDriveFolder(int side,int drive,const std::vector<uint8_t> &data,const std::string &path,Messages *messages) {
    if(data.size()<512) {
        messages->e.f("disc image too small on side %d: %zu bytes\n",side,data.size());
        return false;
    }

    uint8_t num_files=data[0x105];
    if((num_files&7)) {
        messages->e.f("invalid file count byte on side %d: 0x%02X\n",side,num_files);
        return false;
    }

    num_files>>=3;

    //uint8_t num_sectors=data[0x107]|(data[0x106]&3)<<8;

    for(size_t i=0;i<num_files;++i) {
        std::string name=GetPCName(&data[8+i*8]);

        uint32_t load=Load16LE(&data[0x108+i*8+0]);
        uint32_t exec=Load16LE(&data[0x108+i*8+2]);
        uint32_t length=Load16LE(&data[0x108+i*8+4]);
        uint32_t sector=Load8LE(&data[0x108+i*8+7]);

        uint8_t extra=Load8LE(&data[0x108+i*8+6]);

        if(extra&0xc0) {
            exec|=0xffff0000u;
        }

        length|=((uint32_t)extra&0x30)>>4<<16;

        if(extra&0x0c) {
            load|=0xffff0000u;
        }

        sector|=((uint32_t)extra&0x03)<<8;

        bool locked=!!(data[8+i*8+7]&0x7f);

        if(sector*256+length>data.size()) {
            messages->e.f("invalid start and/or length for file: %s\n",name.c_str());
            return false;
        }

        std::string fname=PathJoined(path,name);
        if(!SaveFile(data.data()+sector*256,length,fname,messages)) {
            return false;
        }

        uint32_t lea[3]={
            load,
            exec,
            locked?8u:0,
        };

        if(!SaveFile(lea,sizeof lea,fname+".lea",messages)) {
            return false;
        }
    }

    std::string bootopt_fname=PathJoined(path,"../BootOpt");
    std::vector<uint8_t> bootopt;
    if(LoadFile(&bootopt,bootopt_fname,messages,LoadFlag_MightNotExist)) {
        if(bootopt.size()<8) {
            bootopt.resize(8);
        }

        bootopt[(size_t)drive]=data[0x106]>>4&3;

        SaveFile(bootopt,bootopt_fname,messages);
    }

    return true;
}

bool SaveDiscImageTo65LinkFolder(const std::shared_ptr<const DiscImage> &disc_image,const std::string &path,Messages *messages) {
    std::vector<uint8_t> sides[2];
    std::string folders[2];
    int drives[2]={-1,-1};

    for(uint8_t side=0;side<2;++side) {
        for(uint8_t track=0;track<80;++track) {
            for(uint8_t sector=0;sector<10;++sector) {
                size_t sector_size;
                if(!disc_image->GetDiscSectorSize(&sector_size,side,track,sector,false)||sector_size!=256) {
                    goto side_done;
                }

                for(size_t i=0;i<sector_size;++i) {
                    uint8_t value;
                    if(!disc_image->Read(&value,side,track,sector,i)) {
                        messages->e.f("error reading data: side %u, T%u, S%02u +%zu\n",side,track,sector,i);
                        return false;
                    }

                    sides[side].push_back(value);
                }
            }
        }
    side_done:;
    }

    // 0-sided discs are no good.
    if(sides[0].empty()&&sides[1].empty()) {
        messages->e.f("no data on disc image.\n");
        return false;
    }

    // Does the path appear to be a drive folder?
    int drive=-1;
    {
        std::string tmp=PathGetName(PathWithoutTrailingSeparators(path));
        if(tmp.size()==1&&tmp[0]>='0'&&tmp[0]<'8') {
            drive=tmp[0]-'0';
        }
    }

    if(drive>=0) {
        if(!sides[0].empty()&&!sides[1].empty()) {
            // Double-sided discs have to be saved into a volume folder.
            messages->e.f("not saving double-sided disc into 65Link drive folder.\n");
            return false;
        }

        if(!sides[0].empty()) {
            folders[0]=path;
            drives[0]=drive;
        } else {
            folders[1]=path;
            drives[1]=drive;
        }
    } else {
        // Create folders for each side.
        if(!sides[0].empty()) {
            drives[0]=0;
        }

        if(!sides[1].empty()) {
            drives[1]=2;
        }

        for(int i=0;i<2;++i) {
            if(!sides[i].empty()) {
                folders[i]=PathJoined(path,strprintf("%d",drives[i]));
                PathCreateFolder(folders[i].c_str());
            }
        }
    }

    bool good=true;

    for(int i=0;i<2;++i) {
        if(!sides[i].empty()) {
            if(!SaveDiscSideTo65LinkDriveFolder(i,drives[i],sides[i],folders[i],messages)) {
                good=false;
            }
        }
    }

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
