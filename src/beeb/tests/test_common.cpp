#include <shared/system.h>
#include <shared/testing.h>
#include <shared/path.h>
#include <shared/debug.h>
#include "test_common.h"

#include <shared/enum_def.h>
#include "test_common.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(OUTPUT,"",&log_printer_stdout_and_debugger,true)
LOG_DEFINE(BBC_OUTPUT,"",&log_printer_stdout_and_debugger,false)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr uint16_t WRCHV=0x20e;
static constexpr uint16_t WORDV=0x20c;
static constexpr uint16_t CLIV=0x208;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string strprintfv(const char *fmt,va_list v) {
    char *str;
    if(vasprintf(&str,fmt,v)==-1) {
        // Better suggestions welcome... please.
        return std::string("vasprintf failed - ")+strerror(errno)+" ("+std::to_string(errno)+")";
    } else {
        std::string result(str);

        free(str);
        str=NULL;

        return result;
    }
}

std::string PRINTF_LIKE(1,2) strprintf(const char *fmt,...) {
    va_list v;

    va_start(v,fmt);
    std::string result=strprintfv(fmt,v);
    va_end(v);

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool SaveFile2(const void *contents,size_t contents_size,const std::string &path,const char *mode) {
    if(!PathCreateFolder(PathGetFolder(path))) {
        return false;
    }

    FILE *f=fopen(path.c_str(),mode);
    if(!f) {
        return false;
    }

    size_t n=fwrite(contents,1,contents_size,f);

    fclose(f);
    f=nullptr;

    if(n!=contents_size) {
        return false;
    }

    return true;
}

template<class T>
static bool SaveBinaryFile(const T &contents,const std::string &path) {
    return SaveFile2(contents.data(),contents.size()*sizeof(typename T::value_type),path,"wb");
}

static bool SaveTextFile(const std::string &contents,const std::string &path) {
    return SaveFile2(contents.data(),contents.size(),path,"wt");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::shared_ptr<const BBCMicro::ROMData> LoadROM(const std::string &name) {
    std::string path=PathJoined(ROMS_FOLDER,name);

    std::vector<uint8_t> data;
    TEST_TRUE(PathLoadBinaryFile(&data,path));

    auto rom=std::make_shared<BBCMicro::ROMData>();

    TEST_LE_UU(data.size(),rom->size());
    for(size_t i=0;i<data.size();++i) {
        (*rom)[i]=data[i];
    }

    return rom;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BBCMicroType *GetBBCMicroType(TestBBCMicroType type) {
    switch(type) {
        case TestBBCMicroType_BTape:
            return &BBC_MICRO_TYPE_B;

        case TestBBCMicroType_BPlusTape:
            return &BBC_MICRO_TYPE_B_PLUS;

        case TestBBCMicroType_Master128MOS320:
        case TestBBCMicroType_Master128MOS350:
            return &BBC_MICRO_TYPE_MASTER;
    }

    ASSERT(false);
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const DiscInterfaceDef *GetDiscInterfaceDef(TestBBCMicroType type) {
    switch(type) {
        case TestBBCMicroType_BTape:
        case TestBBCMicroType_BPlusTape:
            return nullptr;

        case TestBBCMicroType_Master128MOS320:
        case TestBBCMicroType_Master128MOS350:
            return &DISC_INTERFACE_MASTER128;
    }

    ASSERT(false);
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<uint8_t> GetNVRAMContents(TestBBCMicroType type) {
    switch(type) {
        default:
            return {};

        case TestBBCMicroType_Master128MOS320:
        case TestBBCMicroType_Master128MOS350:
        {
            std::vector<uint8_t> nvram;

            nvram.resize(50);

            nvram[5]=0xC9;// 5 - LANG 12; FS 9
            nvram[6]=0xFF;// 6 - INSERT 0 ... INSERT 7
            nvram[7]=0xFF;// 7 - INSERT 8 ... INSERT 15
            nvram[8]=0x00;// 8
            nvram[9]=0x00;// 9
            nvram[10]=0x17;//10 - MODE 7; SHADOW 0; TV 0 1
            nvram[11]=0x80;//11 - FLOPPY
            nvram[12]=55;//12 - DELAY 55
            nvram[13]=0x03;//13 - REPEAT 3
            nvram[14]=0x00;//14
            nvram[15]=0x00;//15
            nvram[16]=0x02;//16 - LOUD

            return nvram;
        }
            break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TestBBCMicro::TestBBCMicro(TestBBCMicroType type):
BBCMicro(GetBBCMicroType(type),
         GetDiscInterfaceDef(type),
         GetNVRAMContents(type),
         nullptr,
         false,
         false,
         false,
         nullptr,
         0)
{
    switch(type) {
        case TestBBCMicroType_BTape:
            this->LoadROMsB();
            break;

        case TestBBCMicroType_BPlusTape:
            this->LoadROMsBPlus();
            break;

        case TestBBCMicroType_Master128MOS320:
            this->LoadROMsMaster("3.20");
            break;

        case TestBBCMicroType_Master128MOS350:
            this->LoadROMsMaster("3.50");
            break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::StartCaptureOSWRCH() {
    ++m_oswrch_capture_count;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::StopCaptureOSWRCH() {
    ASSERT(m_oswrch_capture_count>0);
    --m_oswrch_capture_count;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::LoadFile(const std::string &path,uint16_t addr) {
    std::vector<uint8_t> contents;
    TEST_TRUE(PathLoadBinaryFile(&contents,path));

    // try to just hide the fact that BBCMicro::TestSetByte doesn't actually
    // use proper BBC addresses...
    TEST_LE_UU(addr+contents.size(),0x8000);
    for(size_t i=0;i<contents.size();++i) {
        this->TestSetByte((uint16_t)(addr+i),contents[i]);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::RunUntilOSWORD0(double max_num_seconds) {
    const uint8_t *ram=this->GetRAM();
    const M6502 *cpu=this->GetM6502();

    uint64_t max_num_cycles=(uint64_t)(max_num_seconds*2e6);

    uint64_t start_ticks=GetCurrentTickCount();

    uint64_t num_cycles=0;
    while(num_cycles<max_num_cycles) {
        if(M6502_IsAboutToExecute(cpu)) {
            if(cpu->abus.b.l==ram[WORDV+0]&&
               cpu->abus.b.h==ram[WORDV+1]&&
               cpu->a==0)
            {
                break;
            }
        }

        this->Update1();
        ++num_cycles;
    }

    m_num_ticks+=GetCurrentTickCount()-start_ticks;

    TEST_LE_UU(num_cycles,max_num_cycles);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::Paste(std::string text) {
    this->StartPaste(std::make_shared<std::string>(std::move(text)));

    uint64_t start_ticks=GetCurrentTickCount();

    while(this->IsPasting()) {
        this->Update1();
    }

    m_num_ticks+=GetCurrentTickCount()-start_ticks;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::Update1() {
    const M6502 *cpu=this->GetM6502();

    if(M6502_IsAboutToExecute(cpu)) {
        const uint8_t *ram=this->GetRAM();

        if(cpu->abus.b.l==ram[WRCHV+0]&&cpu->abus.b.h==ram[WRCHV+1]) {
            this->GotOSWRCH();
        } else if(cpu->abus.b.l==ram[CLIV+0]&&cpu->abus.b.h==ram[CLIV+1]) {
            if(this->GotOSCLI()) {
                this->TestRTS();
            }
        }
    }

    this->Update(&m_temp_video_data_unit,&m_temp_sound_data_unit);
    ++m_num_cycles;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double TestBBCMicro::GetSpeed() const {
    double num_seconds=GetSecondsFromTicks(m_num_ticks);
    return m_num_cycles/(num_seconds*2e6);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::LoadROMsB() {
    this->SetOSROM(LoadROM("OS12.ROM"));
    this->SetSidewaysROM(15,LoadROM("BASIC2.ROM"));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::LoadROMsBPlus() {
    this->SetOSROM(LoadROM("B+MOS.ROM"));
    this->SetSidewaysROM(15,LoadROM("BASIC2.ROM"));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::LoadROMsMaster(const std::string &version) {
    this->SetOSROM(LoadROM(PathJoined("m128",version,"mos.rom")));
    this->SetSidewaysROM(15,LoadROM(PathJoined("m128",version,"terminal.rom")));
    this->SetSidewaysROM(14,LoadROM(PathJoined("m128",version,"view.rom")));
    this->SetSidewaysROM(13,LoadROM(PathJoined("m128",version,"adfs.rom")));
    this->SetSidewaysROM(12,LoadROM(PathJoined("m128",version,"basic4.rom")));
    this->SetSidewaysROM(11,LoadROM(PathJoined("m128",version,"edit.rom")));
    this->SetSidewaysROM(10,LoadROM(PathJoined("m128",version,"viewsht.rom")));
    this->SetSidewaysROM(9,LoadROM(PathJoined("m128",version,"dfs.rom")));

    for(size_t i=4;i<8;++i) {
        this->SetSidewaysRAM(i,nullptr);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestBBCMicro::GotOSWRCH() {
    const M6502 *cpu=this->GetM6502();
    auto c=(char)cpu->a;

    if(m_oswrch_capture_count>0) {
        this->oswrch_output.push_back(c);
    }

    if(m_spooling) {
        this->spool_output.push_back(c);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TestBBCMicro::GotOSCLI() {
    const M6502 *cpu=this->GetM6502();
    const uint8_t *ram=this->GetRAM();

    M6502Word addr;
    addr.b.l=cpu->x;
    addr.b.h=cpu->y;
    TEST_EQ_UU(addr.w&0x8000,0);//must be in main RAM...

    std::string str;
    for(size_t i=0;i<256;++i) {
        if(ram[addr.w]==13) {
            break;
        }

        str.push_back((char)ram[addr.w]);
        ++addr.w;
    }

    bool handled=false;

    std::string::size_type cmd_begin=str.find_first_not_of("* ");
    if(cmd_begin!=std::string::npos) {
        std::string::size_type cmd_end=str.find_first_of(" ",cmd_begin);
        std::string cmd=str.substr(cmd_begin,cmd_end-cmd_begin);

        std::string::size_type args_begin=str.find_first_not_of(" ",cmd_end);
        if(args_begin==std::string::npos) {
            args_begin=str.size();
        }
        std::string args=str.substr(args_begin);
        LOGF(OUTPUT,"command: ``%s''\n",cmd.c_str());
        LOGF(OUTPUT,"args: ``%s''\n",args.c_str());
        if(cmd=="SPOOL") {
            if(!args.empty()) {
                ASSERT(!m_spooling);
                m_spooling=true;
                spool_output_name=args;
                return true;
            } else {
                ASSERT(m_spooling);
                m_spooling=false;
                return true;
            }
        } else {
            TEST_TRUE(false);
        }
    }

    if(!handled) {
        LOGF(OUTPUT,"ignoring OSCLI: ``%s''\n",str.c_str());
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetPrintable(const std::string &bbc_output) {
    std::string r;

    for(char c:bbc_output) {
        switch(c) {
            case 0:
            case 7:
            case 13:
                // ignore.
                break;

            case '`':
                r+=u8"\u00a3";//POUND SIGN
                break;

            default:
                if(c<32||c>=127) {
                    char tmp[100];
                    snprintf(tmp,sizeof tmp,"`%02x`",c);
                    r+=tmp;
                } else {
                case 10:
                    r.push_back(c);
                }
                break;
        }
    }

    return r;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SaveTextOutput2(const std::string &contents,
                            const std::string &test_name,
                            const std::string &type,
                            const std::string &suffix)
{
    SaveTextFile(contents,PathJoined(BBC_TESTS_OUTPUT_FOLDER,test_name+"."+type+"_"+suffix));
    SaveTextFile(contents,PathJoined(BBC_TESTS_OUTPUT_FOLDER,type+"/"+test_name+"."+suffix));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SaveTextOutput(const std::string &output,const std::string &test_name,const std::string &type) {
    std::string printable_output=GetPrintable(output);
    SaveTextOutput2(GetPrintable(output),test_name,type,"ascii.txt");
    SaveTextOutput2(output,test_name,type,"raw.dat");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

 void RunStandardTest(const std::string &test_name,
                            TestBBCMicroType type)
{
    TestBBCMicro bbc(type);

    bbc.StartCaptureOSWRCH();
    bbc.RunUntilOSWORD0(10.0);
    bbc.LoadFile(PathJoined(BBC_TESTS_FOLDER,"T."+test_name),0xe00);
    bbc.Paste("OLD\rRUN\r");
    bbc.RunUntilOSWORD0(10.0);

    {
        LOGF(BBC_OUTPUT,"All Output: ");
        LOGI(BBC_OUTPUT);
        LOG_STR(BBC_OUTPUT,GetPrintable(bbc.oswrch_output).c_str());
        LOG(BBC_OUTPUT).EnsureBOL();
    }

    std::string stem=strprintf("%s.%s",test_name.c_str(),GetTestBBCMicroTypeEnumName(type));

    TEST_TRUE(SaveTextFile(bbc.oswrch_output,PathJoined(BBC_TESTS_OUTPUT_FOLDER,
                                                        strprintf("%s.all_output.txt",stem.c_str()))));

    if(!bbc.spool_output.empty()) {
        {
            LOGF(BBC_OUTPUT,"Spooled: ");
            LOGI(BBC_OUTPUT);
            LOG_STR(BBC_OUTPUT,GetPrintable(bbc.spool_output).c_str());
            LOG(BBC_OUTPUT).EnsureBOL();
        }

        std::vector<uint8_t> wanted_results;
        TEST_TRUE(PathLoadBinaryFile(&wanted_results,
                                     PathJoined(BBC_TESTS_FOLDER,bbc.spool_output_name)));

        std::string wanted_output(wanted_results.begin(),wanted_results.end());

        {
            LOGF(BBC_OUTPUT,"Wanted: ");
            LOGI(BBC_OUTPUT);
            LOG_STR(BBC_OUTPUT,GetPrintable(wanted_output).c_str());
            LOG(BBC_OUTPUT).EnsureBOL();
        }

        SaveTextOutput(wanted_output,stem,"wanted");
        SaveTextOutput(bbc.spool_output,stem,"got");

        TEST_EQ_SS(wanted_output,bbc.spool_output);
        //LOGF(OUTPUT,"Match: %d\n",wanted_output==bbc.tspool_output);
    }

    LOGF(OUTPUT,"Speed: ~%.3fx\n",bbc.GetSpeed());
}
