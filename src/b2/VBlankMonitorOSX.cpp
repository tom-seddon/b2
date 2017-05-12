#include <shared/system.h>
#include "VBlankMonitorOSX.h"
#include <shared/debug.h>
#include <QuartzCore/QuartzCore.h>
#include <shared/log.h>
#include "VBlankMonitor.h"
#include <vector>
#include "Messages.h"

#include <shared/enum_def.h>
#include "VBlankMonitorOSX_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VBlankMonitorOSX:
    public VBlankMonitor
{
public:
    ~VBlankMonitorOSX() {
        this->ResetDisplaysList();
    }
    
    bool Init(Handler *handler,Messages *messages) override {
        CVReturn cvr;

        ASSERT(!m_handler);
        m_handler=handler;

        if(!this->InitDisplaysList(messages)) {
            return false;
        }
        
        for(auto &&display:m_displays) {
            cvr=CVDisplayLinkCreateWithCGDisplay(display->id,
                                                 &display->link);
            if(cvr!=kCVReturnSuccess) {
                messages->e.f("CVDisplayLinkCreateWithCGDisplay failed: %s\n",GetCVReturnEnumName(cvr));
                return false;
            }

            cvr=CVDisplayLinkSetOutputCallback(display->link,
                                               &OutputCallback,
                                               display.get());
            if(cvr!=kCVReturnSuccess) {
                messages->e.f("CVDisplayLinkSetOutputCallback failed: %s\n",GetCVReturnEnumName(cvr));
                return false;
            }

            cvr=CVDisplayLinkStart(display->link);
            if(cvr!=kCVReturnSuccess) {
                messages->e.f("CVDisplayLinkStart failed: %s\n",GetCVReturnEnumName(cvr));
                return false;
            }
        }

        return true;
    }

    void *GetDisplayDataForDisplayID(uint32_t display_id) const override {
        for(auto &&display:m_displays) {
            if(display->id==display_id) {
                return display->data;
            }
        }

        return nullptr;
    }

    void *GetDisplayDataForPoint(int x,int y) const override {
        CGError cge;

        CGDirectDisplayID display_id;
        uint32_t num_ids;
        cge=CGGetDisplaysWithPoint(CGPointMake(x,y),1,&display_id,&num_ids);
        if(cge!=kCGErrorSuccess) {
            return nullptr;
        }

        if(num_ids==0) {
            return nullptr;
        }

        for(auto &&display:m_displays) {
            if(display->id==display_id) {
                return display->data;
            }
        }

        return nullptr;
    }
protected:
private:
    struct Display {
        // CGDirectDisplayID=uint32_t
        CGDirectDisplayID id=0;
        CVDisplayLinkRef link=nullptr;
        void *data=nullptr;
        VBlankMonitorOSX *vbm=nullptr;
        std::atomic<bool> stop_thread{false};
    };
    Handler *m_handler=nullptr;
    std::vector<std::unique_ptr<Display>> m_displays;

    bool InitDisplaysList(Messages *messages) {
        CGError cge;
        
        this->ResetDisplaysList();

        uint32_t num_cgdisplays;
        cge=CGGetActiveDisplayList(UINT32_MAX,NULL,&num_cgdisplays);
        if(cge!=kCGErrorSuccess) {
            messages->e.f("CGGetActiveDisplayList failed: %s\n",GetCGErrorEnumName(cge));
            return false;
        }

        std::vector<CGDirectDisplayID> cgdisplays;
        cgdisplays.resize(num_cgdisplays);
        cge=CGGetActiveDisplayList(num_cgdisplays,cgdisplays.data(),&num_cgdisplays);
        if(cge!=kCGErrorSuccess) {
            messages->e.f("CGGetActiveDisplayList (2) failed: %s\n",GetCGErrorEnumName(cge));
            return false;
        }

        for(uint32_t i=0;i<num_cgdisplays;++i) {
            auto &&display=std::make_unique<Display>();

            display->id=cgdisplays[i];
            display->vbm=this;

            display->data=m_handler->AllocateDisplayData(display->id);
            if(!display->data) {
                messages->e.f("AllocateDisplayData failed\n");
                return false;
            }

            m_displays.push_back(std::move(display));
        }

        return true;
    }

    void ResetDisplaysList() {
        for(auto &&display:m_displays) {
            if(display->link) {
                CVReturn cvr;

                cvr=CVDisplayLinkStop(display->link);
                ASSERTF(cvr==kCVReturnSuccess,"%s",GetCVReturnEnumName(cvr));

                CVDisplayLinkRelease(display->link);
                display->link=nullptr;
            }

            m_handler->FreeDisplayData(display->id,display->data);
            display->data=nullptr;
        }

        m_displays.clear();
    }

    static CVReturn OutputCallback(CVDisplayLinkRef displayLink,
                                   const CVTimeStamp *inNow,
                                   const CVTimeStamp *inOutputTime,
                                   CVOptionFlags flagsIn,
                                   CVOptionFlags *flagsOut,
                                   void *displayLinkContext)
    {
        (void)displayLink;
        (void)inNow,(void)inOutputTime;
        (void)flagsIn,(void)flagsOut;

        auto d=(Display *)displayLinkContext;

        d->vbm->m_handler->ThreadVBlank(d->id,d->data);

        return kCVReturnSuccess;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<VBlankMonitor> CreateVBlankMonitorOSX() {
    return std::make_unique<VBlankMonitorOSX>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
