#include <shared/system.h>
#include "VBlankMonitorDefault.h"
#include <shared/debug.h>
#include <thread>
#include "VBlankMonitor.h"
#include <atomic>
#include "Messages.h"
#include <shared/log.h>
#include <system_error>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VBlankMonitorDefault : public VBlankMonitor {
  public:
    explicit VBlankMonitorDefault(std::chrono::microseconds interval)
        : m_interval(interval) {
    }

    ~VBlankMonitorDefault() {
        if (m_thread.joinable()) {
            m_stop_thread = true;

            m_thread.join();
        }

        m_handler->FreeDisplayData(1, m_display_data);
    }

    bool Init(Handler *handler, Messages *messages) override {
        m_handler = handler;

        m_display_data = m_handler->AllocateDisplayData(1);
        if (!m_display_data) {
            messages->e.f("AllocateDisplayData failed\n");
            return false;
        }

        try {
            m_thread = std::thread([this]() {
                this->ThreadFunc();
            });
        } catch (const std::system_error &e) {
            messages->e.f("failed to create vblank thread: %s\n", e.what());
            return false;
        }

        return true;
    }

    virtual void *GetDisplayDataForDisplayID(uint32_t display_id) const override {
        if (display_id == 1) {
            return m_display_data;
        } else {
            return nullptr;
        }
    }

    virtual void *GetDisplayDataForPoint(int x, int y) const override {
        (void)x, (void)y;

        return m_display_data;
    }

  protected:
  private:
    std::chrono::microseconds m_interval;
    Handler *m_handler = nullptr;
    void *m_display_data = nullptr;
    std::atomic<bool> m_stop_thread{false};
    std::thread m_thread;

    void ThreadFunc() {
        while (!m_stop_thread) {
            std::this_thread::sleep_for(m_interval);

            m_handler->ThreadVBlank(1, m_display_data);
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<VBlankMonitor> CreateVBlankMonitorDefault(std::chrono::microseconds interval) {
    return std::make_unique<VBlankMonitorDefault>(interval);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
