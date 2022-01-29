#include <shared/system.h>
#include "VBlankMonitorOSX.h"
#include <shared/debug.h>
#include <QuartzCore/QuartzCore.h>
#include <shared/log.h>
#include "VBlankMonitor.h"
#include <vector>
#include "Messages.h"
#include <inttypes.h>

#include <shared/enum_def.h>
#include "VBlankMonitorOSX_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(VBLANK, "vblank", "VBLANK", &log_printer_stderr_and_debugger);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VBlankMonitorOSX : public VBlankMonitor {
  public:
    ~VBlankMonitorOSX() {
        CGDisplayRemoveReconfigurationCallback(&ReconfigurationCallback, this);
        this->ResetDisplaysList();
    }

    bool Init(Handler *handler, Messages *messages) override {
        CGError cge;

        ASSERT(!m_handler);
        m_handler = handler;

        if (!this->InitDisplaysList(messages)) {
            return false;
        }

        cge = CGDisplayRegisterReconfigurationCallback(&ReconfigurationCallback, this);
        if (cge != kCGErrorSuccess) {
            return false;
        }

        return true;
    }

    void *GetDisplayDataForDisplayID(uint32_t display_id) const override {
        for (auto &&display : m_displays) {
            if (display->id == display_id) {
                return display->data;
            }
        }

        return nullptr;
    }

    void *GetDisplayDataForPoint(int x, int y) const override {
        CGError cge;

        CGDirectDisplayID display_id;
        uint32_t num_ids;
        cge = CGGetDisplaysWithPoint(CGPointMake(x, y), 1, &display_id, &num_ids);
        if (cge != kCGErrorSuccess) {
            return nullptr;
        }

        if (num_ids == 0) {
            display_id = CGMainDisplayID();
        }

        for (auto &&display : m_displays) {
            if (display->id == display_id) {
                return display->data;
            }
        }

        return nullptr;
    }

  protected:
  private:
    struct Display {
        CGDirectDisplayID const id;
        VBlankMonitorOSX *const vbm;

        CVDisplayLinkRef link = nullptr;
        void *data = nullptr;

        Display(CGDirectDisplayID id_, VBlankMonitorOSX *vbm_)
            : id(id_)
            , vbm(vbm_) {
        }

        ~Display() {
            CVReturn cvr = CVDisplayLinkStop(this->link);
            ASSERTF(cvr == kCVReturnSuccess, "%s", GetCVReturnEnumName(cvr));
            this->link = nullptr;

            CVDisplayLinkRelease(this->link);
            this->link = nullptr;

            if (this->data) {
                this->vbm->m_handler->FreeDisplayData(this->id, this->data);
            }
        }
    };
    Handler *m_handler = nullptr;
    std::vector<std::unique_ptr<Display>> m_displays;

    bool InitDisplaysList(Messages *messages) {
        CGError cge;

        this->ResetDisplaysList();

        uint32_t num_cgdisplays;
        cge = CGGetActiveDisplayList(UINT32_MAX, NULL, &num_cgdisplays);
        if (cge != kCGErrorSuccess) {
            messages->e.f("CGGetActiveDisplayList failed: %s\n", GetCGErrorEnumName(cge));
            return false;
        }

        std::vector<CGDirectDisplayID> cgdisplays;
        cgdisplays.resize(num_cgdisplays);
        cge = CGGetActiveDisplayList(num_cgdisplays, cgdisplays.data(), &num_cgdisplays);
        if (cge != kCGErrorSuccess) {
            messages->e.f("CGGetActiveDisplayList (2) failed: %s\n", GetCGErrorEnumName(cge));
            return false;
        }

        for (uint32_t i = 0; i < num_cgdisplays; ++i) {
            if (!this->AddDisplay(cgdisplays[i])) {
                // I have no idea how much help this error will be, but at least you'll know.
                messages->e.f("Failed to initialise display 0x%" PRIx32 "\n", cgdisplays[i]);
            }
        }

        return true;
    }

    void ResetDisplaysList() {
        while (!m_displays.empty()) {
            this->RemoveDisplay(m_displays[0]->id);
        }
    }

    bool AddDisplay(CGDirectDisplayID id) {
        CVReturn cvr;

        for (auto &&display : m_displays) {
            if (display->id == id) {
                LOGF(VBLANK, "Add Display %" PRIx32 ": already added.\n", id);
                return true;
            }
        }

        auto &&display = std::make_unique<Display>(id, this);

        CGRect bounds = CGDisplayBounds(id);
        (void)bounds;
        LOGF(VBLANK, "Add Dispay %" PRIx32 ": ", id);
        LOGI(VBLANK);

        LOGF(VBLANK, "(%.1f,%.1f), %.1f x %.1f\n", bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height);

        display->data = m_handler->AllocateDisplayData(display->id);
        if (!display->data) {
            LOGF(VBLANK, "AllocateDisplayData failed\n");
            return false;
        }

        cvr = CVDisplayLinkCreateWithCGDisplay(display->id,
                                               &display->link);
        if (cvr != kCVReturnSuccess) {
            LOGF(VBLANK, "CVDisplayLinkCreateWithCGDisplay failed: %s\n", GetCVReturnEnumName(cvr));
            return false;
        }

        cvr = CVDisplayLinkSetOutputCallback(display->link,
                                             &OutputCallback,
                                             display.get());
        if (cvr != kCVReturnSuccess) {
            LOGF(VBLANK, "CVDisplayLinkSetOutputCallback failed: %s\n", GetCVReturnEnumName(cvr));
            return false;
        }

        cvr = CVDisplayLinkStart(display->link);
        if (cvr != kCVReturnSuccess) {
            LOGF(VBLANK, "CVDisplayLinkStart failed: %s\n", GetCVReturnEnumName(cvr));
            return false;
        }

        m_displays.push_back(std::move(display));
        return true;
    }

    void RemoveDisplay(CGDirectDisplayID id) {
        for (auto &&display_it = m_displays.begin(); display_it != m_displays.end(); ++display_it) {
            if ((*display_it)->id == id) {
                LOGF(VBLANK, "Remove Display %" PRIx32 "\n", id);
                m_displays.erase(display_it);
                return;
            }
        }
    }

    static CVReturn OutputCallback(CVDisplayLinkRef displayLink,
                                   const CVTimeStamp *inNow,
                                   const CVTimeStamp *inOutputTime,
                                   CVOptionFlags flagsIn,
                                   CVOptionFlags *flagsOut,
                                   void *displayLinkContext) {
        (void)displayLink;
        (void)inNow, (void)inOutputTime;
        (void)flagsIn, (void)flagsOut;

        auto d = (Display *)displayLinkContext;

        d->vbm->m_handler->ThreadVBlank(d->id, d->data);

        return kCVReturnSuccess;
    }

    static void ReconfigurationCallback(CGDirectDisplayID display,
                                        CGDisplayChangeSummaryFlags flags,
                                        void *userInfo) {
        auto vbm = (VBlankMonitorOSX *)userInfo;

        if (flags & kCGDisplayAddFlag) {
            vbm->AddDisplay(display);
        } else if (flags & kCGDisplayRemoveFlag) {
            vbm->RemoveDisplay(display);
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<VBlankMonitor> CreateVBlankMonitorOSX() {
    return std::make_unique<VBlankMonitorOSX>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
