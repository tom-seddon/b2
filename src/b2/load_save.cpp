#include <shared/system.h>
#include "load_save.h"
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include "misc.h"
#include "native_ui.h"
#include <rapidjson/error/en.h>
#include "keymap.h"
#include "keys.h"
#include <SDL.h>
#include "BeebWindows.h"
#include <shared/log.h>
#include <stdio.h>
#include "Messages.h"
#include <shared/debug.h>
#include <shared/path.h>
#include <beeb/DiscInterface.h>
#include "TraceUI.h"
#include "BeebWindow.h"
#include <shared/system_specific.h>
#if SYSTEM_WINDOWS
#include <ShlObj.h>
#endif
#include <beeb/BBCMicro.h>

#include <shared/enum_def.h>
#include "load_save.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// (sigh)
#define PRIsizetype "u"
CHECK_SIZEOF(rapidjson::SizeType,sizeof(unsigned));

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_OSX

// Forms a path from NSApplicationSupportDirectory, the bundle
// identifier, and PATH. Tries to create the resulting folder.
std::string GetOSXApplicationSupportPath(const std::string &path);

// Forms a path from NSApplicationCacheDirectory, the bundle
// identifier, and PATH. Tries to create the resulting folder.
std::string GetOSXCachePath(const std::string &path);

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(LOADSAVE,"config","LD/SV ",&log_printer_stdout_and_debugger,false)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class T>
using JSONWriter=rapidjson::PrettyWriter<T>;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
static std::string GetUTF8String(const wchar_t *str) {
    size_t len=wcslen(str);

    if(len>INT_MAX) {
        return "";
    }

    int n=WideCharToMultiByte(CP_UTF8,0,str,(int)len,nullptr,0,nullptr,nullptr);
    if(n==0) {
        return "";
    }

    std::vector<char> buffer;
    buffer.resize(n);
    WideCharToMultiByte(CP_UTF8,0,str,(int)len,buffer.data(),(int)buffer.size(),nullptr,nullptr);

    return std::string(buffer.begin(),buffer.end());
}
#endif

#if SYSTEM_WINDOWS
static std::wstring GetWideString(const char *str) {
    size_t len=strlen(str);

    if(len>INT_MAX) {
        return L"";
    }

    int n=MultiByteToWideChar(CP_UTF8,0,str,(int)len,nullptr,0);
    if(n==0) {
        return L"";
    }

    std::vector<wchar_t> buffer;
    buffer.resize(n);
    MultiByteToWideChar(CP_UTF8,0,str,(int)len,buffer.data(),(int)buffer.size());

    return std::wstring(buffer.begin(),buffer.end());
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS
static std::string GetWindowsPath(const GUID &known_folder,const std::string &path) {
    std::string result;
    WCHAR *wpath;

    wpath=nullptr;
    if(FAILED(SHGetKnownFolderPath(known_folder,KF_FLAG_CREATE,nullptr,&wpath))) {
        goto bad;
    }

    result=GetUTF8String(wpath);

    CoTaskMemFree(wpath);
    wpath=NULL;

    if(result.empty()) {
        goto bad;
    }

    return PathJoined(result,"b2",path);

bad:;
    return path;
}
#endif

#if SYSTEM_LINUX
static std::string GetXDGPath(const char *env_name,const char *folder_name,const std::string &path) {
    std::string result;

    if(const char *env_value=getenv(env_name)) {
        result=env_value;
    } else if(const char *home=getenv("HOME")) {
        result=PathJoined(home,folder_name);
    } else {
        result="."; // *sigh*
    }

    result=PathJoined(result,"b2",path);
    return result;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetConfigPath(const std::string &path) {
#if SYSTEM_WINDOWS

    // The config file includes a bunch of paths to local files, so it
    // should probably be Local rather than Roaming. But it's
    // debatable.
    return GetWindowsPath(FOLDERID_LocalAppData,path);

#elif SYSTEM_LINUX

    return GetXDGPath("XDG_CONFIG_HOME",".config",path);

#elif SYSTEM_OSX

    return GetOSXApplicationSupportPath(path);

#else
#error
#endif
    }

std::string GetCachePath(const std::string &path) {
#if SYSTEM_WINDOWS

    return GetWindowsPath(FOLDERID_LocalAppData,path);

#elif SYSTEM_LINUX

    return GetXDGPath("XDG_CACHE_HOME",".cache",path);

#elif SYSTEM_OSX

    return GetOSXCachePath(path);

#else
#error
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetConfigFileName() {
    return GetConfigPath("b2.json");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char ASSETS_FOLDER[]="assets";

static std::string GetAssetPathInternal(const std::string *f0,...) {
    std::string path=PathJoined(PathGetFolder(PathGetEXEFileName()),ASSETS_FOLDER);

    va_list v;
    va_start(v,f0);

    for(const std::string *f=f0;f;f=va_arg(v,const std::string *)) {
        path=PathJoined(path,*f);
    }

    va_end(v);

    return path;
}

std::string GetAssetPath(const std::string &f0) {
    return GetAssetPathInternal(&f0,nullptr);
}

std::string GetAssetPath(const std::string &f0,const std::string &f1) {
    return GetAssetPathInternal(&f0,&f1,nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static char GetHexChar(uint8_t value) {
    ASSERT(value<16);

    return "0123456789abcdef"[value];
}

static int GetHexCharValue(char c) {
    if(c>='0'&&c<='9') {
        return c-'0';
    } else if(c>='a'&&c<='f') {
        return c-'a'+10;
    } else if(c>='A'&&c<='F') {
        return c-'A'+10;
    } else {
        return -1;
    }
}

static std::string GetHexStringFromData(const std::vector<uint8_t> &data) {
    std::string hex;

    hex.reserve(data.size()*2);

    for(uint8_t byte:data) {
        hex.append(1,GetHexChar(byte>>4));
        hex.append(1,GetHexChar(byte&15));
    }

    return hex;
}

static bool GetDataFromHexString(std::vector<uint8_t> *data,const std::string &str) {
    if(str.size()%2!=0) {
        return false;
    }

    data->reserve(str.size()/2);

    for(size_t i=0;i<str.size();i+=2) {
        int a=GetHexCharValue(str[i+0]);
        if(a<0) {
            return false;
        }

        int b=GetHexCharValue(str[i+1]);
        if(b<0) {
            return false;
        }

        data->push_back((uint8_t)a<<4|(uint8_t)b);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void AddError(Messages *msg,
    const std::string &path,
    const char *what1,
    const char *what2,
    int err)
{
    msg->w.f("%s failed: %s\n",what1,path.c_str());

    if(err!=0) {
        msg->i.f("(%s: %s)\n",what2,strerror(err));
    } else {
        msg->i.f("(%s)\n",what2);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static FILE *fopenUTF8(const char *path,const char *mode) {
#if SYSTEM_WINDOWS

    return _wfopen(GetWideString(path).c_str(),GetWideString(mode).c_str());

#else

    return fopen(path,mode);

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class ContType>
static bool LoadFile2(ContType *data,
    const std::string &path,
    Messages *msg,
    uint32_t flags,
    const char *mode)
{
    static_assert(sizeof(typename ContType::value_type)==1,"LoadFile2 can only load into a vector of bytes");
    FILE *f=NULL;
    bool good=false;
    long len;
    size_t num_bytes,num_read;

    f=fopenUTF8(path.c_str(),mode);
    if(!f) {
        if(errno==ENOENT&&(flags&LoadFlag_MightNotExist)) {
            // ignore this error.
        } else {
            AddError(msg,path,"load","open failed",errno);
        }

        goto done;
    }

    if(fseek(f,0,SEEK_END)==-1) {
        AddError(msg,path,"load","fseek (1) failed",errno);
        goto done;
    }

    len=ftell(f);
    if(len<0) {
        AddError(msg,path,"load","ftell failed",errno);
        goto done;
    } else if((size_t)len>SIZE_MAX) {
        AddError(msg,path,"load","file is too large",0);
        goto done;
    }

    if(fseek(f,0,SEEK_SET)==-1) {
        AddError(msg,path,"load","fseek (2) failed",errno);
        goto done;
    }

    num_bytes=(size_t)len;
    data->resize(num_bytes);

    num_read=fread(data->data(),1,num_bytes,f);
    if(ferror(f)) {
        AddError(msg,path,"load","read failed",errno);
        goto done;
    }

    // Number of bytes read may be smaller if mode is rt.
    data->resize(num_read);
    good=true;

done:;
    if(!good) {
        data->clear();
    }

    if(f) {
        fclose(f);
        f=NULL;
    }

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadFile(std::vector<uint8_t> *data,
    const std::string &path,
    Messages *messages,
    uint32_t flags)
{
    if(!LoadFile2(data,path,messages,flags,"rb")) {
        return false;
    }

    data->shrink_to_fit();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadTextFile(std::vector<char> *data,
    const std::string &path,
    Messages *messages,
    uint32_t flags)
{
    if(!LoadFile2(data,path,messages,flags,"rt")) {
        return false;
    }

    data->push_back(0);
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool SaveFile2(const void *data,size_t data_size,const std::string &path,Messages *messages,const char *fopen_mode) {
    FILE *f=fopen(path.c_str(),fopen_mode);
    if(!f) {
        AddError(messages,path,"save","fopen failed",errno);
        return false;
    }

    fwrite(data,1,data_size,f);

    bool bad=!!ferror(f);
    int e=errno;

    fclose(f);
    f=nullptr;

    if(bad) {
        AddError(messages,path,"save","write failed",e);
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveFile(const void *data,size_t data_size,const std::string &path,Messages *messages) {
    return SaveFile2(data,data_size,path,messages,"wb");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveFile(const std::vector<uint8_t> &data,const std::string &path,Messages *messages) {
    return SaveFile(data.data(),data.size(),path,messages);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveTextFile(const std::string &data,const std::string &path,Messages *messages) {
    return SaveFile2(data.c_str(),data.size(),path,messages,"wt");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// RapidJSON-friendly stream class that writes its output into a
// std::string.

class StringStream {
public:
    typedef std::string::value_type Ch;

    StringStream(std::string *str);

    void Put(char c);
    void Flush();
protected:
private:
    std::string *m_str;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

StringStream::StringStream(std::string *str):
    m_str(str)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void StringStream::Put(char c)
{
    m_str->push_back(c);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void StringStream::Flush()
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindMember(rapidjson::Value *member,rapidjson::Value *object,const char *key,Messages *msg,bool (rapidjson::Value::*is_type_mfn)() const,const char *type_name) {
    rapidjson::Document::MemberIterator it=object->FindMember(key);
    if(it==object->MemberEnd()) {
        if(msg) {
            msg->w.f("%s not found: %s\n",type_name,key);
        }

        return false;
    }

    if(!(it->value.*is_type_mfn)()) {
        if(msg) {
            msg->w.f("not %s: %s\n",type_name,key);
        }

        return false;
    }

    *member=it->value;
    return true;
}

static bool FindArrayMember(rapidjson::Value *arr,rapidjson::Value *src,const char *key,Messages *msg) {
    return FindMember(arr,src,key,msg,&rapidjson::Value::IsArray,"array");
}

static bool FindObjectMember(rapidjson::Value *obj,rapidjson::Value *src,const char *key,Messages *msg) {
    return FindMember(obj,src,key,msg,&rapidjson::Value::IsObject,"object");
}

static bool FindStringMember(rapidjson::Value *value,rapidjson::Value *object,const char *key,Messages *msg) {
    return FindMember(value,object,key,msg,&rapidjson::Value::IsString,"string");
}

static bool FindStringMember(std::string *value,rapidjson::Value *object,const char *key,Messages *msg) {
    rapidjson::Value tmp;
    if(!FindMember(&tmp,object,key,msg,&rapidjson::Value::IsString,"string")) {
        return false;
    }

    *value=tmp.GetString();
    return true;
}

static bool FindBoolMember(bool *value,rapidjson::Value *object,const char *key,Messages *msg) {
    rapidjson::Value tmp;
    if(!FindMember(&tmp,object,key,msg,&rapidjson::Value::IsBool,"bool")) {
        return false;
    }

    *value=tmp.GetBool();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static bool FindDoubleMember(double *value,rapidjson::Value *object,const char *key,Messages *msg) {
//    rapidjson::Value tmp;
//    if(!FindMember(&tmp,object,key,msg,&rapidjson::Value::IsNumber,"number")) {
//        return false;
//    }
//
//    *value=tmp.GetDouble();
//    return true;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindFloatMember(float *value,rapidjson::Value *object,const char *key,Messages *msg) {
    rapidjson::Value tmp;
    if(!FindMember(&tmp,object,key,msg,&rapidjson::Value::IsNumber,"number")) {
        return false;
    }

    *value=tmp.GetFloat();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SaveFlags(JSONWriter<StringStream> *writer,uint32_t flags,const char *(*get_name_fn)(int)) {
    for(uint32_t mask=1;mask!=0;mask<<=1) {
        const char *name=(*get_name_fn)((int)mask);
        if(name[0]=='?') {
            continue;
        }

        if(!(flags&mask)) {
            continue;
        }

        writer->String(name);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadFlags(uint32_t *flags,const rapidjson::Value &json,const char *what,const char *(*get_name_fn)(int),Messages *msg) {
    ASSERT(json.IsArray());

    bool good=true;
    *flags=0;

    for(rapidjson::SizeType i=0;i<json.Size();++i) {
        if(json[i].IsString()) {
            bool found=false;
            const char *flag_name=json[i].GetString();

            for(uint32_t mask=1;mask!=0;mask<<=1) {
                const char *name=(*get_name_fn)((int)mask);
                if(name[0]=='?') {
                    continue;
                }

                if(strcmp(flag_name,name)==0) {
                    found=true;
                    *flags|=mask;
                    break;
                }
            }

            if(!found) {
                msg->e.f("unknown %s: %s\n",what,flag_name);
                good=false;
            }
        }
    }

    return good;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool FindFlagsMember(uint32_t *flags,rapidjson::Value *object,const char *key,const char *what,const char *(*get_name_fn)(int),Messages *msg) {
    rapidjson::Value array;
    if(!FindArrayMember(&array,object,key,msg)) {
        return false;
    }

    if(!LoadFlags(flags,array,what,get_name_fn,msg)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class T>
static void SaveEnum(JSONWriter<StringStream> *writer,T value,const char *(*get_name_fn)(int)) {
    const char *name=(*get_name_fn)((int)value);
    if(name[0]!='?') {
        writer->String(name);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// To be suitable for use with this function, thie enum values must
// start from 0 and be contiguous.
template<class T>
static bool LoadEnum(T *value,const std::string &str,const char *what,const char *(*get_name_fn)(int),Messages *msg) {
    int i=0;
    for(;;) {
        const char *name=(*get_name_fn)(i);
        if(name[0]=='?') {
            break;
        }

        if(str==name) {
            *value=(T)i;
            return true;
        }

        ++i;
    }

    msg->e.f("unknown %s: %s\n",what,str.c_str());
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class T>
static bool FindEnumMember(T *value,rapidjson::Value *object,const char *key,const char *what,const char *(*get_name_fn)(int),Messages *msg) {
    std::string str;
    if(!FindStringMember(&str,object,key,msg)) {
        return false;
    }

    if(!LoadEnum(value,str,what,get_name_fn,msg)) {
        return false;
    }

    return true;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::unique_ptr<rapidjson::Document> LoadDocument(
    std::vector<char> *data,
    Messages *msg)
{
    std::unique_ptr<rapidjson::Document> doc=std::make_unique<rapidjson::Document>();

    doc->ParseInsitu(data->data());

    if(doc->HasParseError()) {
        msg->e.f("JSON error: +%zu: %s\n",
            doc->GetErrorOffset(),
            rapidjson::GetParseError_En(doc->GetParseError()));
        return nullptr;
    }

    return doc;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct WriterHelper {
    bool valid=true;
    std::function<void(void)> fun;

    WriterHelper(std::function<void(void)> fun_):
        fun(fun_)
    {
    }

    WriterHelper(WriterHelper &&oth):
        valid(oth.valid),
        fun(oth.fun)
    {
        oth.valid=false;
    }

    ~WriterHelper() {
        if(this->valid) {
            this->fun();
        }
    }
};

template<class WriterType>
static WriterHelper DoWriter(WriterType *writer,
    const char *name,
    bool (WriterType::*begin_mfn)(),
    bool (WriterType::*end_mfn)(rapidjson::SizeType))
{
    if(name) {
        writer->Key(name);
    }

    (writer->*begin_mfn)();

    return WriterHelper(
        [writer,end_mfn]() {
        (writer->*end_mfn)(0);
    });
}

template<class WriterType>
static WriterHelper ObjectWriter(WriterType *writer,
    const char *name=nullptr)
{
    return DoWriter(writer,name,&WriterType::StartObject,&WriterType::EndObject);
}

template<class WriterType>
static WriterHelper ArrayWriter(WriterType *writer,
    const char *name=nullptr)
{
    return DoWriter(writer,name,&WriterType::StartArray,&WriterType::EndArray);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char RECENT_PATHS[]="recent_paths";
static const char PATHS[]="paths";
static const char KEYMAPS[]="keymaps";
static const char KEYS[]="keys";
static const char PLACEMENT[]="window_placement";
static const char CONFIGS[]="configs";
static const char OS[]="os";
static const char ROMS[]="roms";
static const char WRITEABLE[]="writeable";
static const char FILE_NAME[]="file_name";
static const char NAME[]="name";
static const char DEFAULT_CONFIG[]="default_config";
static const char DISC_INTERFACE[]="disc_interface";
static const char TRACE[]="trace";
static const char FLAGS[]="flags";
static const char START[]="start";
static const char STOP[]="stop";
static const char WINDOWS[]="windows";
static const char KEYMAP[]="keymap";
static const char KEYSYMS[]="keysyms";
static const char KEYCODE[]="keycode";
static const char BBC_VOLUME[]="bbc_volume";
static const char DISC_VOLUME[]="disc_volume";
static const char SAVE_STATE_SHORTCUT[]="save_state_shortcut";
static const char LOAD_LAST_STATE_SHORTCUT[]="load_last_state_shortcut";
static const char DISPLAY_AUTO_SCALE[]="display_auto_scale";
static const char DISPLAY_OVERALL_SCALE[]="display_overall_scale";
static const char DISPLAY_SCALE_X[]="display_scale_x";
static const char DISPLAY_SCALE_Y[]="display_scale_y";
static const char DISPLAY_ALIGNMENT_X[]="display_alignment_x";
static const char DISPLAY_ALIGNMENT_Y[]="display_alignment_y";
static const char FILTER_BBC[]="filter_bbc";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadKeycodeFromObject(uint32_t *keycode,rapidjson::Value *keycode_json,Messages *msg,const char *name_fmt,...) {
    if(!keycode_json->IsObject()) {
        msg->e.f("not an object: ");

        va_list v;
        va_start(v,name_fmt);
        msg->e.v(name_fmt,v);
        va_end(v);

        msg->e.f("\n");
        return false;
    }

    std::string keycode_name;
    if(!FindStringMember(&keycode_name,keycode_json,KEYCODE,nullptr)) {
        *keycode=0;
        return true;
    }

    *keycode=(uint32_t)SDL_GetKeyFromName(keycode_name.c_str());
    if(*keycode==0) {
        msg->w.f("unknown keycode: %s\n",keycode_name.c_str());
        return false;
    }

    for(uint32_t mask=PCKeyModifier_Begin;mask!=PCKeyModifier_End;mask<<=1) {
        bool value;
        if(FindBoolMember(&value,keycode_json,GetPCKeyModifierEnumName((int)mask),nullptr)) {
            if(value) {
                *keycode|=mask;
            }
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SaveKeycodeObject(JSONWriter<StringStream> *writer,uint32_t keycode) {
    auto keycode_json=ObjectWriter(writer);

    if((keycode&~PCKeyModifier_All)!=0) {
        for(uint32_t mask=PCKeyModifier_Begin;mask!=PCKeyModifier_End;mask<<=1) {
            if(keycode&mask) {
                writer->Key(GetPCKeyModifierEnumName((int)mask));
                writer->Bool(true);
            }
        }

        writer->Key(KEYCODE);

        uint32_t sdl_keycode=keycode&~PCKeyModifier_All;
        const char *keycode_name=SDL_GetKeyName((SDL_Keycode)sdl_keycode);
        if(strlen(keycode_name)==0) {
            writer->Int64((int64_t)sdl_keycode);
        } else {
            writer->String(keycode_name);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadRecentPaths(rapidjson::Value *recent_paths,
    Messages *msg)
{
    for(rapidjson::Value::MemberIterator it=recent_paths->MemberBegin();
        it!=recent_paths->MemberEnd();
        ++it)
    {
        std::string tag=it->name.GetString();

        LOGF(LOADSAVE,"Loading recent paths for: %s\n",tag.c_str());

        rapidjson::Value paths;
        if(!FindArrayMember(&paths,&it->value,PATHS,nullptr)) {
            LOGF(LOADSAVE,"(no data found.)\n");
            return false;
        }

        RecentPaths recents;

        for(rapidjson::SizeType json_index=0;json_index<paths.Size();++json_index) {
            rapidjson::SizeType i=paths.Size()-1-json_index;

            if(!paths[i].IsString()) {
                msg->e.f("not a string: %s.%s.paths[%u]\n",
                    RECENT_PATHS,tag.c_str(),i);
                return false;
            }

            LOGF(LOADSAVE,"    %" PRIsizetype ". %s\n",i,paths[i].GetString());
            recents.AddPath(paths[i].GetString());
        }

        SetRecentPathsByTag(std::move(tag),std::move(recents));
    }

    return true;
}

static bool LoadKeymaps(rapidjson::Value *keymaps_json,Messages *msg) {
    for(rapidjson::SizeType keymap_idx=0;keymap_idx<keymaps_json->Size();++keymap_idx) {
        rapidjson::Value *keymap_json=&(*keymaps_json)[keymap_idx];

        if(!keymap_json->IsObject()) {
            msg->e.f("not an object: %s[%" PRIsizetype "]\n",
                KEYMAPS,keymap_idx);
            continue;
        }

        std::string keymap_name;
        if(!FindStringMember(&keymap_name,keymap_json,NAME,msg)) {
            continue;
        }

        rapidjson::Value keys_json;
        if(!FindObjectMember(&keys_json,keymap_json,KEYS,msg)) {
            LOGF(LOADSAVE,"(no data found.)\n");
            return false;
        }

        bool keysyms;
        if(!FindBoolMember(&keysyms,keymap_json,KEYSYMS,msg)) {
            return false;
        }

        Keymap keymap("",keysyms);//need better ctors than this
        if(keysyms) {
            for(rapidjson::Value::MemberIterator keys_it=keys_json.MemberBegin();
                keys_it!=keys_json.MemberEnd();
                ++keys_it)
            {
                const char *beeb_sym_name=keys_it->name.GetString();
                uint8_t beeb_sym=GetBeebKeySymByName(beeb_sym_name);
                if(beeb_sym==BeebSpecialKey_None) {
                    msg->w.f("unknown BBC keysym: %s\n",beeb_sym_name);
                    continue;
                }

                if(!keys_it->value.IsArray()) {
                    msg->e.f("not an array: %s.%s.keys.%s\n",
                        KEYS,keymap_name.c_str(),beeb_sym_name);
                    continue;
                }

                for(rapidjson::SizeType i=0;i<keys_it->value.Size();++i) {
                    uint32_t keycode;
                    if(!LoadKeycodeFromObject(&keycode,&keys_it->value[i],msg,"%s.%s.keys[%" PRIsizetype "]",KEYS,keymap_name.c_str(),i)) {
                        continue;
                    }

                    keymap.SetMapping(keycode,beeb_sym,true);
                }
            }
        } else {
            for(rapidjson::Value::MemberIterator keys_it=keys_json.MemberBegin();
                keys_it!=keys_json.MemberEnd();
                ++keys_it)
            {
                LOGF(LOADSAVE,"    Loading scancodes for: %s\n",keys_it->name.GetString());
                const char *beeb_key_name=keys_it->name.GetString();
                uint8_t beeb_key=GetBeebKeyByName(beeb_key_name);
                if(beeb_key==BeebSpecialKey_None) {
                    msg->w.f("unknown BBC key: %s\n",beeb_key_name);
                    continue;
                }

                if(!keys_it->value.IsArray()) {
                    msg->e.f("not an array: %s.%s.keys.%s\n",
                        KEYS,keymap_name.c_str(),beeb_key_name);
                    continue;
                }

                for(rapidjson::SizeType i=0;i<keys_it->value.Size();++i) {
                    if(keys_it->value[i].IsNumber()) {
                        uint32_t scancode=(uint32_t)keys_it->value[i].GetInt64();
                        keymap.SetMapping(scancode,beeb_key,true);
                    } else if(keys_it->value[i].IsString()) {
                        const char *scancode_name=keys_it->value[i].GetString();
                        uint32_t scancode=SDL_GetScancodeFromName(scancode_name);
                        if(scancode==SDL_SCANCODE_UNKNOWN) {
                            msg->w.f("unknown scancode: %s\n",
                                scancode_name);
                        } else {
                            keymap.SetMapping(scancode,beeb_key,true);
                        }
                    } else {
                        msg->e.f("not number/string: %s.%s.keys.%s[%" PRIsizetype "]\n",
                            KEYS,keymap_name.c_str(),beeb_key_name,i);
                        continue;
                    }
                }
            }
        }

        Keymap *keymap_ptr=BeebWindows::AddKeymap(std::move(keymap));
        BeebWindows::SetKeymapName(keymap_ptr,keymap_name);
    }

    return true;
}

static void LoadKeycode(uint32_t *keycode,rapidjson::Value *windows,const char *key,Messages *msg) {
    rapidjson::Value shortcut;
    if(FindObjectMember(&shortcut,windows,key,msg)) {
        LoadKeycodeFromObject(keycode,&shortcut,msg,"%s.%s",WINDOWS,key);
    }
}

static bool LoadWindows(rapidjson::Value *windows,Messages *msg) {
    {
        std::string placement_str;
        if(FindStringMember(&placement_str,windows,PLACEMENT,nullptr)) {
            std::vector<uint8_t> placement_data;
            if(!GetDataFromHexString(&placement_data,placement_str)) {
                msg->e.f("invalid placement data\n");
            } else {
                BeebWindows::SetLastWindowPlacementData(std::move(placement_data));
            }
        }
    }

    FindFlagsMember(&BeebWindows::defaults.ui_flags,windows,FLAGS,"UI flag",&GetBeebWindowUIFlagEnumName,msg);
    FindFloatMember(&BeebWindows::defaults.bbc_volume,windows,BBC_VOLUME,msg);
    FindFloatMember(&BeebWindows::defaults.disc_volume,windows,DISC_VOLUME,msg);
    FindBoolMember(&BeebWindows::defaults.display_auto_scale,windows,DISPLAY_AUTO_SCALE,msg);
    FindFloatMember(&BeebWindows::defaults.display_overall_scale,windows,DISPLAY_OVERALL_SCALE,msg);
    FindFloatMember(&BeebWindows::defaults.display_scale_x,windows,DISPLAY_SCALE_X,msg);
    FindFloatMember(&BeebWindows::defaults.display_scale_y,windows,DISPLAY_SCALE_Y,msg);
    FindEnumMember(&BeebWindows::defaults.display_alignment_x,windows,DISPLAY_ALIGNMENT_X,"window alignment",GetBeebWindowDisplayAlignmentEnumName,msg);
    FindEnumMember(&BeebWindows::defaults.display_alignment_y,windows,DISPLAY_ALIGNMENT_Y,"window alignment",GetBeebWindowDisplayAlignmentEnumName,msg);
    FindBoolMember(&BeebWindows::defaults.display_filter,windows,FILTER_BBC,nullptr);
    
    {
        std::string keymap_name;
        if(FindStringMember(&keymap_name,windows,KEYMAP,msg)) {
            const Keymap *keymap=BeebWindows::ForEachKeymap([&](const Keymap *keymap,Keymap *) {
                return keymap->GetName()!=keymap_name;
            });
            if(keymap) {
                BeebWindows::SetDefaultKeymap(keymap);
            } else {
                msg->w.f("default keymap unknown: %s\n",keymap_name.c_str());

                // But it's OK - a sensible one will be selected.
            }
        }
    }

    LoadKeycode(&BeebWindows::save_state_shortcut_key,windows,SAVE_STATE_SHORTCUT,msg);
    LoadKeycode(&BeebWindows::load_last_state_shortcut_key,windows,LOAD_LAST_STATE_SHORTCUT,msg);

    return true;
}


static bool LoadConfigs(rapidjson::Value *configs_json,Messages *msg) {
    for(rapidjson::SizeType config_idx=0;config_idx<configs_json->Size();++config_idx) {
        rapidjson::Value *config_json=&(*configs_json)[config_idx];

        if(!config_json->IsObject()) {
            msg->e.f("not an object: %s[%" PRIsizetype "]\n",CONFIGS,config_idx);
            continue;
        }

        BeebConfig config;

        if(!FindStringMember(&config.name,config_json,NAME,msg)) {
            continue;
        }

        FindStringMember(&config.os_file_name,config_json,OS,msg);

        std::string disc_interface_name;
        if(FindStringMember(&disc_interface_name,config_json,DISC_INTERFACE,nullptr)) {
            config.disc_interface=FindDiscInterfaceByName(disc_interface_name.c_str());
            if(!config.disc_interface) {
                msg->w.f("unknown disc interface: %s\n",disc_interface_name.c_str());
            }
        }

        rapidjson::Value roms;
        if(!FindArrayMember(&roms,config_json,ROMS,msg)) {
            continue;
        }

        if(roms.Size()!=16) {
            msg->e.f("not an array with 16 entries: %s[%" PRIsizetype "].%s\n",
                CONFIGS,config_idx,ROMS);
            continue;
        }

        for(rapidjson::SizeType i=0;i<16;++i) {
            if(roms[i].IsNull()) {
                // ignore...
            } else if(roms[i].IsObject()) {
                BeebConfig::ROM *rom=&config.roms[i];

                FindBoolMember(&rom->writeable,&roms[i],WRITEABLE,nullptr);
                FindStringMember(&rom->file_name,&roms[i],FILE_NAME,nullptr);
            } else {
                msg->e.f("not null or object: %s[%" PRIsizetype "].%s[%" PRIsizetype "]\n",
                    CONFIGS,config_idx,ROMS,i);
                continue;
            }
        }

        BeebWindows::AddConfig(std::move(config));
    }

    return true;
}

static bool LoadTrace(rapidjson::Value *trace_json,Messages *msg) {
    TraceUI::Settings settings;

    FindFlagsMember(&settings.flags,trace_json,FLAGS,"trace flag",&GetBBCMicroTraceFlagEnumName,msg);
    FindEnumMember(&settings.start,trace_json,START,"start condition",&GetTraceUIStartConditionEnumName,msg);
    FindEnumMember(&settings.stop,trace_json,STOP,"stop condition",&GetTraceUIStopConditionEnumName,msg);

    TraceUI::SetDefaultSettings(settings);

    return true;
}

bool LoadGlobalConfig(Messages *msg)
{
    std::string fname=GetConfigFileName();
    if(fname.empty()) {
        msg->e.f("failed to load config file\n");
        msg->i.f("(couldn't get file name)\n");
        return false;
    }

    std::vector<char> data;
    if(!LoadTextFile(&data,fname,msg,LoadFlag_MightNotExist)) {
        return true;
    }

    std::unique_ptr<rapidjson::Document> doc=LoadDocument(&data,msg);
    if(!doc) {
        return false;
    }

    //LogDumpBytes(&LOG(LOADSAVE),data->data(),data->size());

    rapidjson::Value recent_paths;
    if(FindObjectMember(&recent_paths,doc.get(),RECENT_PATHS,msg)) {
        LOGF(LOADSAVE,"Loading recent paths.\n");

        if(!LoadRecentPaths(&recent_paths,msg)) {
            return false;
        }
    }

    rapidjson::Value keymaps;
    if(FindArrayMember(&keymaps,doc.get(),KEYMAPS,msg)) {
        LOGF(LOADSAVE,"Loading keymaps.\n");

        if(!LoadKeymaps(&keymaps,msg)) {
            return false;
        }
    }

    rapidjson::Value windows;
    if(FindObjectMember(&windows,doc.get(),WINDOWS,msg)) {
        if(!LoadWindows(&windows,msg)) {
            return false;
        }
    }

    rapidjson::Value configs;
    if(FindArrayMember(&configs,doc.get(),CONFIGS,msg)) {
        LOGF(LOADSAVE,"Loading configs.\n");

        if(!LoadConfigs(&configs,msg)) {
            return false;
        }
    }

    rapidjson::Value trace;
    if(FindObjectMember(&trace,doc.get(),TRACE,msg)) {
        if(!LoadTrace(&trace,msg)) {
            return false;
        }
    }

    // this must be done after loading the configs.
    rapidjson::Value default_config;
    if(FindStringMember(&default_config,doc.get(),DEFAULT_CONFIG,msg)) {
        BeebWindows::SetDefaultConfig(default_config.GetString());
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SaveRecentPaths(JSONWriter<StringStream> *writer) {
    auto recent_paths_json=ObjectWriter(writer,RECENT_PATHS);

    ForEachRecentPaths(
        [writer](const std::string &tag,
            const RecentPaths &recents)
    {
        auto tag_json=ObjectWriter(writer,tag.c_str());

        auto paths_json=ArrayWriter(writer,PATHS);

        for(size_t i=0;i<recents.GetNumPaths();++i) {
            const std::string &path=recents.GetPathByIndex(i);
            writer->String(path.c_str());
        }
    });
}

static void SaveKeymaps(JSONWriter<StringStream> *writer) {
    auto keymaps_json=ArrayWriter(writer,KEYMAPS);

    BeebWindows::ForEachKeymap([&](const Keymap *keymap,Keymap *editable_keymap) {
        if(!editable_keymap) {
            // don't serialize stock keymaps.
            return true;
        }

        auto keymap_json=ObjectWriter(writer);

        writer->Key(NAME);
        writer->String(keymap->GetName().c_str());

        writer->Key(KEYSYMS);
        writer->Bool(keymap->IsKeySymMap());

        auto keys_json=ObjectWriter(writer,KEYS);

        if(keymap->IsKeySymMap()) {
            for(uint8_t beeb_sym=0;beeb_sym<BeebSpecialKey_None;++beeb_sym) {
                const char *beeb_sym_name=GetBeebKeySymName(beeb_sym);
                if(!beeb_sym_name) {
                    continue;
                }

                const uint32_t *keycodes=keymap->GetPCKeysForBeebKey(beeb_sym);
                if(!keycodes) {
                    continue;
                }

                auto key_json=ArrayWriter(writer,beeb_sym_name);

                for(const uint32_t *keycode=keycodes;*keycode!=0;++keycode) {
                    SaveKeycodeObject(writer,*keycode);
                }
            }
        } else {
            for(uint8_t beeb_key=0;
                beeb_key<BeebSpecialKey_None;
                ++beeb_key)
            {
                const char *beeb_key_name=GetBeebKeyName(beeb_key);
                if(!beeb_key_name) {
                    continue;
                }

                const uint32_t *pc_keys=keymap->GetPCKeysForBeebKey(beeb_key);
                if(!pc_keys) {
                    continue;
                }

                auto key_json=ArrayWriter(writer,beeb_key_name);

                for(const uint32_t *scancode=pc_keys;
                    *scancode!=0;
                    ++scancode)
                {
                    const char *scancode_name=SDL_GetScancodeName((SDL_Scancode)*scancode);
                    if(strlen(scancode_name)==0) {
                        writer->Int64((int64_t)*scancode);
                    } else {
                        writer->String(scancode_name);
                    }
                }
            }
        }

        return true;
    });
}

static void SaveConfigs(JSONWriter<StringStream> *writer) {
    {
        auto configs_json=ArrayWriter(writer,CONFIGS);

        BeebWindows::ForEachConfig([&](const BeebConfig *config,const BeebLoadedConfig *) {
            if(!config) {
                // don't serialize stock configs.
                return true;
            }

            auto config_json=ObjectWriter(writer);

            writer->Key(NAME);
            writer->String(config->name.c_str());

            writer->Key(OS);
            writer->String(config->os_file_name.c_str());

            writer->Key(DISC_INTERFACE);
            if(!config->disc_interface) {
                writer->Null();
            } else {
                writer->String(config->disc_interface->name.c_str());
            }

            auto roms_json=ArrayWriter(writer,ROMS);

            for(size_t j=0;j<16;++j) {
                const BeebConfig::ROM *rom=&config->roms[j];

                if(!rom->writeable&&rom->file_name.empty()) {
                    writer->Null();
                } else {
                    auto rom_json=ObjectWriter(writer);

                    if(rom->writeable) {
                        writer->Key(WRITEABLE);
                        writer->Bool(rom->writeable);
                    }

                    if(!rom->file_name.empty()) {
                        writer->Key(FILE_NAME);
                        writer->String(rom->file_name.c_str());
                    }
                }
            }

            return true;
        });
    }

    writer->Key(DEFAULT_CONFIG);
    writer->String(BeebWindows::GetDefaultConfig()->name.c_str());
}

static void SaveKeycode(JSONWriter<StringStream> *writer,const char *key,uint32_t keycode) {
    writer->Key(key);
    SaveKeycodeObject(writer,keycode);
}

static void SaveWindows(JSONWriter<StringStream> *writer) {
    {
        auto windows_json=ObjectWriter(writer,WINDOWS);

        {
            const std::vector<uint8_t> &placement_data=BeebWindows::GetLastWindowPlacementData();
            if(!placement_data.empty()) {
                writer->Key(PLACEMENT);
                writer->String(GetHexStringFromData(placement_data).c_str());
            }
        }

        {
            auto ui_flags_json=ArrayWriter(writer,FLAGS);

            SaveFlags(writer,BeebWindows::defaults.ui_flags,&GetBeebWindowUIFlagEnumName);
        }

        writer->Key(KEYMAP);
        writer->String(BeebWindows::GetDefaultKeymap()->GetName().c_str());

        writer->Key(BBC_VOLUME);
        writer->Double(BeebWindows::defaults.bbc_volume);

        writer->Key(DISC_VOLUME);
        writer->Double(BeebWindows::defaults.disc_volume);

        SaveKeycode(writer,SAVE_STATE_SHORTCUT,BeebWindows::save_state_shortcut_key);
        SaveKeycode(writer,LOAD_LAST_STATE_SHORTCUT,BeebWindows::load_last_state_shortcut_key);

        writer->Key(DISPLAY_AUTO_SCALE);
        writer->Bool(BeebWindows::defaults.display_auto_scale);

        writer->Key(DISPLAY_OVERALL_SCALE);
        writer->Double(BeebWindows::defaults.display_overall_scale);

        writer->Key(DISPLAY_SCALE_X);
        writer->Double(BeebWindows::defaults.display_scale_x);

        writer->Key(DISPLAY_SCALE_Y);
        writer->Double(BeebWindows::defaults.display_scale_y);

        writer->Key(DISPLAY_ALIGNMENT_X);
        SaveEnum(writer,BeebWindows::defaults.display_alignment_x,&GetBeebWindowDisplayAlignmentEnumName);

        writer->Key(DISPLAY_ALIGNMENT_Y);
        SaveEnum(writer,BeebWindows::defaults.display_alignment_y,&GetBeebWindowDisplayAlignmentEnumName);

        writer->Key(FILTER_BBC);
        writer->Bool(BeebWindows::defaults.display_filter);
    }
}

static void SaveTrace(JSONWriter<StringStream> *writer) {
    const TraceUI::Settings &settings=TraceUI::GetDefaultSettings();

    {
        auto trace_json=ObjectWriter(writer,TRACE);

        {
            auto default_flags_json=ArrayWriter(writer,FLAGS);

            SaveFlags(writer,settings.flags,&GetBBCMicroTraceFlagEnumName);
        }

        writer->Key(START);
        SaveEnum(writer,settings.start,&GetTraceUIStartConditionEnumName);

        writer->Key(STOP);
        SaveEnum(writer,settings.stop,&GetTraceUIStopConditionEnumName);
    }
}

bool SaveGlobalConfig(Messages *messages) {
    std::string fname=GetConfigFileName();
    if(fname.empty()) {
        messages->e.f("failed to save config file: %s\n",fname.c_str());
        messages->i.f("(couldn't get file name)\n");
        return false;
    }

    if(!PathCreateFolder(PathGetFolder(fname))) {
        int e=errno;

        messages->e.f("failed to save config file: %s\n",fname.c_str());
        messages->i.f("(failed to create folder: %s)\n",strerror(e));
        return false;
    }

    std::string json;
    {
        StringStream stream(&json);
        JSONWriter<StringStream> writer(stream);

        auto root=ObjectWriter(&writer);

        SaveRecentPaths(&writer);

        SaveKeymaps(&writer);

        SaveWindows(&writer);

        SaveTrace(&writer);

        SaveConfigs(&writer);
    }

    if(!SaveTextFile(json,fname,messages)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
