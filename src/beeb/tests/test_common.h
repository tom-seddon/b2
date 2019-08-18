#ifndef HEADER_944C1DF3E98D466A9F1095BAA8100D15// -*- mode:c++ -*-
#define HEADER_944C1DF3E98D466A9F1095BAA8100D15

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Random grab-bag of test bits.
//
// There's no real rhyme or reason to any of this. It's just whatever was
// convenient.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <beeb/BBCMicro.h>
#include <beeb/sound.h>
#include <string>

#include <shared/enum_decl.h>
#include "test_common.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestBBCMicro:
public BBCMicro
{
public:
    std::string oswrch_output;
    std::string spool_output;
    std::string spool_output_name;

    explicit TestBBCMicro(TestBBCMicroType type);

    void StartCaptureOSWRCH();
    void StopCaptureOSWRCH();

    void LoadFile(const std::string &path,uint16_t addr);

    void RunUntilOSWORD0(double max_num_seconds);

    void Paste(std::string text);

    void Update1();

    double GetSpeed() const;
protected:
    void GotOSWRCH();
    virtual bool GotOSCLI();//true=handled, false=ok to pass on to real OSCLI
private:
    bool m_spooling=false;
    size_t m_oswrch_capture_count=0;
    VideoDataUnit m_temp_video_data_unit;
    SoundDataUnit m_temp_sound_data_unit;
    uint64_t m_num_ticks=0;
    uint64_t m_num_cycles=0;

    void LoadROMsB();
    void LoadROMsBPlus();
    void LoadROMsMaster(const std::string &version);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Runs one of the standard tests.
//
// Does a _exit(1) if any of the tests fail.
//
// See etc/b2_tests/README.md in the working copy for more about this.
void RunStandardTest(const std::string &test_name,TestBBCMicroType type);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
