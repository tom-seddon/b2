#ifndef HEADER_944C1DF3E98D466A9F1095BAA8100D15 // -*- mode:c++ -*-
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
#include <beeb/DiscImage.h>

#include <shared/enum_decl.h>
#include "test_common.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct TestBBCMicroArgs {
    uint32_t flags = 0;
};

class TestBBCMicro : public BBCMicro {
  public:
    std::string oswrch_output;
    std::string spool_output;
    std::string spool_output_name;

    // Flags are a combination of TestBBCMicroFlags
    explicit TestBBCMicro(TestBBCMicroType type, const TestBBCMicroArgs &args = {});

    void StartCaptureOSWRCH();
    void StopCaptureOSWRCH();

    void LoadFile(const std::string &path, uint32_t addr);
    void LoadSSD(int drive, const std::string &path);

    void RunUntilOSWORD0(double max_num_seconds);

    // return value is video output.
    std::vector<uint32_t> RunForNFrames(size_t num_frames);

    void Paste(std::string text);

    uint32_t Update1();

    double GetSpeed() const;

    // flags to be used when code writes to $fc10.
    uint32_t GetTestTraceFlags() const;
    void SetTestTraceFlags(uint32_t flags);

    void SaveTestTrace(const std::string &stem);

  protected:
    void GotOSWRCH();
    virtual bool GotOSCLI(); //true=handled, false=ok to pass on to real OSCLI
  private:
    bool m_spooling = false;
    size_t m_oswrch_capture_count = 0;
    size_t m_video_data_unit_idx = 0;
    std::vector<VideoDataUnit> m_video_data_units;
    SoundDataUnit m_temp_sound_data_unit;
    uint64_t m_num_ticks = 0;
    CycleCount m_num_cycles = {0};
#if BBCMICRO_TRACE
    std::shared_ptr<Trace> m_test_trace;
    uint32_t m_trace_flags = 0;
#else
    const uint32_t m_trace_flags = 0;
#endif

    void LoadROMsB();
    void LoadROMsBPlus();
    void LoadROMsMaster(const std::string &version);
    void LoadParasiteOS(const std::string &name);

    static uint8_t ReadTestCommand(void *context, M6502Word addr);
    static void WriteTestCommand(void *context, M6502Word addr, uint8_t value);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string strprintfv(const char *fmt, va_list v);
std::string PRINTF_LIKE(1, 2) strprintf(const char *fmt, ...);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//
std::string GetTestFileName(const std::string &beeblink_volume_path,
                            const std::string &drive,
                            const std::string &name);

std::string GetOutputFileName(const std::string &path);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestSpooledOutput(const TestBBCMicro &bbc,
                       const std::string &beeblink_volume_path,
                       const std::string &beeblink_drive,
                       const std::string &test_name);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Runs an image-based test.
//
// WANTED_PNG_PATH is the full path to a known good image, checked by eye, as
// saved out from a previous run of this test.
//
// PNG_NAME is the name part of two .png files to save in the b2_tests_output
// folder: (PNG_NAME).png, the actual output, and (PNG_NAME).differences.png,
// an image with a white pixel anywhere there's a difference between the output
// and the known good image.
//
// BEEB is the Beeb that's in a state where it's showing the image of interest
// on its emulated display.
//
// If the known good image isn't available, the test fails - but the output
// is still saved, so copy it to the known good location.
void RunImageTest(const std::string &wanted_png_path,
                  const std::string &png_name,
                  TestBBCMicro *beeb);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveFileInternal(const void *contents,
                      size_t contents_size,
                      const std::string &path,
                      const char *mode);

template <class T>
static bool SaveBinaryFile(const T &contents, const std::string &path) {
    return SaveFileInternal(contents.data(),
                            contents.size() * sizeof(typename T::value_type),
                            path,
                            "wb");
}

bool SaveTextFile(const std::string &contents, const std::string &path);

std::string GetPrintable(const std::string &bbc_output);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
