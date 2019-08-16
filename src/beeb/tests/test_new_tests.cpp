#include <shared/system.h>
#include <shared/testing.h>
#include "test_common.h"
#include <beeb/BBCMicro.h>
#include <shared/path.h>
#include <shared/debug.h>
#include <beeb/type.h>
#include <beeb/sound.h>
#include <shared/log.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(OUTPUT,"",&log_printer_stdout_and_debugger,true)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr uint16_t WRCHV=0x20e;
static constexpr uint16_t WORDV=0x20c;
static constexpr uint16_t CLIV=0x208;

static constexpr uint16_t OSWRCH_TRAP_ADDR=0xfc00;//1 byte
static constexpr uint16_t OSCLI_TRAP_ADDR=0xfc01;//3 bytes

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool LoadFile(std::vector<uint8_t> *contents,const std::string &path) {
    FILE *f=fopen(path.c_str(),"rb");
    if(!f) {
        return false;
    }

    TEST_EQ_II(fseek(f,0,SEEK_END),0);

    long len=ftell(f);
    TEST_GE_II(len,0);

    TEST_EQ_II(fseek(f,0,SEEK_SET),0);

    contents->resize((size_t)len);

    TEST_EQ_UU(fread(contents->data(),1,(size_t)len,f),(size_t)len);

    fclose(f);
    f=nullptr;

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::shared_ptr<const BBCMicro::ROMData> LoadROM(const std::string &name) {
    std::string path=PathJoined(ROMS_FOLDER,name);

    std::vector<uint8_t> data;
    TEST_TRUE(LoadFile(&data,path));

    auto rom=std::make_shared<BBCMicro::ROMData>();

    TEST_LE_UU(data.size(),rom->size());
    for(size_t i=0;i<data.size();++i) {
        (*rom)[i]=data[i];
    }

    return rom;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestBBCMicro:
public BBCMicro
{
public:
    std::string oswrch_output;
    std::string tspool_output;

    explicit TestBBCMicro(TestBBCMicroType type);

    void StartCaptureOSWRCH();
    void StopCaptureOSWRCH();

    void LoadFile(const std::string &path,uint16_t addr);

    void RunUntilOSWORD0(double max_num_seconds);

    void Paste(std::string text);

    void Update1();

    double GetSpeed() const;
protected:
private:
    bool m_tspool=false;
    size_t m_oswrch_capture_count=0;
    VideoDataUnit m_temp_video_data_unit;
    SoundDataUnit m_temp_sound_data_unit;
    uint64_t m_num_ticks=0;
    uint64_t m_num_cycles=0;

    void LoadROMsB();
    void LoadROMsMaster(const std::string &version);
    void GotOSWRCH();
    bool GotOSCLI();//true=handled, false=ok to pass on to real OSCLI
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BBCMicroType *GetBBCMicroType(TestBBCMicroType type) {
    switch(type) {
        case TestBBCMicroType_BTape:
            return &BBC_MICRO_TYPE_B;

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
    TEST_TRUE(::LoadFile(&contents,path));

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

    // InstructionFns would be another option, but in a debug build they're
    // quite a lot slower than doing this - ~3.5x BBC speed (InstructionFn) vs
    // ~7.5x (this) on my laptop.
    //
    // (A bit more can be eked out by routing the vectors to memory-mapped I/O
    // addresses with special handling, but it's a bit more fiddly and not
    // hugely quicker - ~8.0x real BBC speed in a debug build on my laptop.)
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

    if(m_tspool) {
        this->tspool_output.push_back(c);
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
        std::string::size_type cmd_end=str.find_first_of(" .",cmd_begin);
        std::string cmd=str.substr(cmd_begin,cmd_end-cmd_begin);
        LOGF(OUTPUT,"command: ``%s''\n",cmd.c_str());
        if(cmd=="TSPOOL") {
            ASSERT(!m_tspool);
            m_tspool=true;
            return true;
        } else if(cmd=="SPOOL") {
            ASSERT(m_tspool);
            m_tspool=false;
            return true;
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

static std::vector<uint8_t> LoadTestOutput(const std::string &stem,
                                           const std::string &bbc_dir)
{
    std::vector<uint8_t> contents;

    for(const std::string &dir:{bbc_dir.c_str(),"B","+","M"}) {
        std::string path=PathJoined(BBC_TESTS_FOLDER,dir+"."+stem);
        if(LoadFile(&contents,path)) {
            LOGF(OUTPUT,"Loaded output for %s.%s from %s\n",bbc_dir.c_str(),stem.c_str(),path.c_str());
            return contents;
        }
    }

    TEST_TRUE(false);
    return {};
}

static void RunTest(const std::string &test_name) {
    TestBBCMicro bbc(TestBBCMicroType_BTape);

    std::vector<uint8_t> wanted_results=LoadTestOutput(test_name,"B");

    bbc.StartCaptureOSWRCH();
    bbc.RunUntilOSWORD0(10.0);
    bbc.LoadFile(PathJoined(BBC_TESTS_FOLDER,"T."+test_name),0xe00);
    bbc.Paste("OLD\rRUN\r");
    bbc.RunUntilOSWORD0(10.0);

    {
        LOGF(OUTPUT,"All Output: ");
        LOGI(OUTPUT);
        LOG_STR(OUTPUT,GetPrintable(bbc.oswrch_output).c_str());
        LOG(OUTPUT).EnsureBOL();
    }

    if(!bbc.tspool_output.empty()) {
        {
            LOGF(OUTPUT,"TSPOOL: ");
            LOGI(OUTPUT);
            LOG_STR(OUTPUT,GetPrintable(bbc.tspool_output).c_str());
            LOG(OUTPUT).EnsureBOL();
        }

        std::string wanted_output(wanted_results.begin(),wanted_results.end());

        {
            LOGF(OUTPUT,"Wanted: ");
            LOGI(OUTPUT);
            LOG_STR(OUTPUT,GetPrintable(wanted_output).c_str());
            LOG(OUTPUT).EnsureBOL();
        }

        LOGF(OUTPUT,"Match: %d\n",wanted_output==bbc.tspool_output);
    }

    LOGF(OUTPUT,"Speed: ~%.3fx\n",bbc.GetSpeed());
}

int main() {
    RunTest("VIASTUFF1");
}
