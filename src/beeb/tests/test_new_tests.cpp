#include <shared/system.h>
#include <shared/testing.h>
#include "test_common.h"
#include <beeb/BBCMicro.h>
#include <shared/path.h>
#include <shared/debug.h>
#include <beeb/type.h>
#include <beeb/sound.h>
#include <shared/log.h>

static constexpr uint16_t WRCHV=0x20e;
static constexpr uint16_t WORDV=0x20c;

static std::vector<uint8_t> LoadFile(const std::string &path) {
    FILE *f=fopen(path.c_str(),"rb");
    TEST_NON_NULL(f);

    TEST_EQ_II(fseek(f,0,SEEK_END),0);

    long len=ftell(f);
    TEST_GE_II(len,0);

    TEST_EQ_II(fseek(f,0,SEEK_SET),0);

    std::vector<uint8_t> data;
    data.resize((size_t)len);

    TEST_EQ_UU(fread(data.data(),1,(size_t)len,f),(size_t)len);

    fclose(f);
    f=nullptr;

    return data;
}

static std::shared_ptr<const BBCMicro::ROMData> LoadROM(const std::string &name) {
    std::string path=PathJoined(ROMS_FOLDER,name);

    std::vector<uint8_t> data=LoadFile(path);

    auto rom=std::make_shared<BBCMicro::ROMData>();

    TEST_LE_UU(data.size(),rom->size());
    for(size_t i=0;i<data.size();++i) {
        (*rom)[i]=data[i];
    }

    return rom;
}

static std::unique_ptr<BBCMicro> CreateBBCMicro(const BBCMicroType *type,
                                                const DiscInterfaceDef *def,
                                                const std::vector<uint8_t> &nvram_contents,
                                                const std::map<int,std::string> &roms)
{
    auto bbc=std::make_unique<BBCMicro>(type,
                                        def,
                                        nvram_contents,
                                        nullptr,
                                        false,false,false,//no video NuLA, no ext mem, no power on tone
                                        nullptr,
                                        0);

    for(auto &&it:roms) {
        std::shared_ptr<const BBCMicro::ROMData> rom=LoadROM(it.second);
        if(it.first<0) {
            bbc->SetOSROM(std::move(rom));
        } else {
            ASSERT(it.first<16);
            bbc->SetSidewaysROM((uint8_t)it.first,std::move(rom));
        }
    }

    return bbc;
}

class OSWRCHTrapper {
public:
    std::string output;

    explicit OSWRCHTrapper(BBCMicro *m);
    ~OSWRCHTrapper();

    OSWRCHTrapper(const OSWRCHTrapper &)=delete;
    OSWRCHTrapper &operator=(const OSWRCHTrapper &)=delete;
    OSWRCHTrapper(OSWRCHTrapper &&)=delete;
    OSWRCHTrapper &operator=(OSWRCHTrapper &&)=delete;

    const std::string &GetOutput() const;
protected:
private:
    BBCMicro *m_m;

    static bool HandleOSWRCH(const BBCMicro *m,const M6502 *cpu,void *context);
};

OSWRCHTrapper::OSWRCHTrapper(BBCMicro *m):
m_m(m)
{
    m_m->AddInstructionFn(&HandleOSWRCH,&this->output);
}

OSWRCHTrapper::~OSWRCHTrapper() {
    m_m->RemoveInstructionFn(&HandleOSWRCH,&this->output);
}

bool OSWRCHTrapper::HandleOSWRCH(const BBCMicro *m,const M6502 *cpu,void *context) {
    const uint8_t *ram=m->GetRAM();

    // Rather tiresomely, BASIC 2 prints stuff with JMP (WRCHV). Who
    // comes up with this stuff? So check against WRCHV, not just
    // 0xffee.

    if(cpu->abus.b.l==ram[WRCHV+0]&&cpu->abus.b.h==ram[WRCHV+1]) {
        auto output=(std::string *)context;

        // Opcode fetch for first byte of OSWRCH
        output->push_back((char)cpu->a);
    }

    return true;
}

static std::string GetPrintable(const std::string &bbc_output) {
    std::string r;

    for(char c:bbc_output) {
        switch(c) {
            case 0:
            case 7:
            case 13:
                // ignore.
                break;

            case 95:
                r+="\xc2\xa3";//U+00A3 POUND SIGN
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

static void RunUntilOSWORD0(const std::unique_ptr<BBCMicro> &m_,
                            uint64_t max_num_cycles)
{
    BBCMicro *m=m_.get();

    const uint8_t *ram=m->GetRAM();
    const M6502 *cpu=m->GetM6502();

    VideoDataUnit video_unit;
    SoundDataUnit sound_unit;

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

        m->Update(&video_unit,&sound_unit);
        ++num_cycles;
    }

    TEST_LE_UU(num_cycles,max_num_cycles);
}

LOG_DEFINE(OUTPUT,"",&log_printer_stdout_and_debugger,true)

int main() {
    auto bbc=CreateBBCMicro(&BBC_MICRO_TYPE_B,nullptr,{},{{-1,"OS12.ROM"},{15,"BASIC2.ROM"}});
    {
        OSWRCHTrapper oswrch_trapper(bbc.get());
        RunUntilOSWORD0(bbc,10000000);
        LogDumpBytes(&LOG(OUTPUT),oswrch_trapper.output.c_str(),oswrch_trapper.output.size());
        printf("output:\n---8---\n%s\n---8<---\n",GetPrintable(oswrch_trapper.output).c_str());
    }
}
