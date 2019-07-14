#ifndef HEADER_8E9F48F66B734BF09EFD8580786ECA18// -*- mode:c++ -*-
#define HEADER_8E9F48F66B734BF09EFD8580786ECA18

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <memory>
#include <string>
#include <vector>
#include <atomic>

class BeebWindow;
class SettingsUI;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TimerDefs should be global objects. Anything else is unsupported and you'll
// just get a big mess.
//
// The TimerDef nesting should reflect the Timer nesting, but there are no
// checks.

class TimerDef {
public:
    const std::string name;

    explicit TimerDef(std::string name,TimerDef *parent=nullptr);
    ~TimerDef();

    TimerDef(const TimerDef &)=delete;
    TimerDef &operator=(const TimerDef &)=delete;

    TimerDef(TimerDef &&)=delete;
    TimerDef &operator=(TimerDef &&)=delete;

    uint64_t GetTotalNumTicks() const;
    uint64_t GetNumSamples() const;

    void AddTicks(uint64_t num_ticks);

    void DoImGui();
protected:
private:
    TimerDef *m_parent=nullptr;
    std::vector<TimerDef *> m_children;
    
    std::atomic<uint64_t> m_total_num_ticks{0};
    std::atomic<uint64_t> m_num_samples{0};
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Timer {
public:
    explicit inline Timer(TimerDef *def):
    m_def(def),
    m_begin_ticks(GetCurrentTickCount())
    {
    }

    inline ~Timer() {
        uint64_t end_ticks=GetCurrentTickCount();

        m_def->AddTicks(end_ticks-m_begin_ticks);
    }

    Timer(const Timer &)=delete;
    Timer &operator=(const Timer &)=delete;

    // (could be implemented if required.)
    Timer(Timer &&)=delete;
    Timer &operator=(Timer &&)=delete;
protected:
private:
    TimerDef *m_def=nullptr;
    uint64_t m_begin_ticks=0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateDataRateUI(BeebWindow *beeb_window);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
