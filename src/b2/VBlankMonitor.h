#ifndef HEADER_0C29552443F1455C8BFDA39B14B6E819 // -*- mode:c++ -*-
#define HEADER_0C29552443F1455C8BFDA39B14B6E819

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <memory>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Messages;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VBlankMonitor {
  public:
    class Handler {
      public:
        Handler();
        virtual ~Handler() = 0;

        virtual void *AllocateDisplayData(uint32_t display_id) = 0;
        virtual void FreeDisplayData(uint32_t display_id, void *data) = 0;
        virtual void ThreadVBlank(uint32_t display_id, void *data) = 0;

      protected:
      private:
    };

    VBlankMonitor();
    virtual ~VBlankMonitor() = 0;

    virtual bool Init(Handler *handler, Messages *messages) = 0;

    // For now, non-abstract. Default impl does nothing and returns true.
    //
    // The return value is for informational purposes only, and may be ignored.
    // The vblank monitor should do something sensible if used after
    // RefreshDisplayList returns false, in the hope that a future
    // RefreshDisplayList might return true.
    virtual bool RefreshDisplayList(Messages *messages);

    // For now, non-abstract. Default NeedsRefreshDisplayList impl returns
    // false.
    //
    // Called regularly, same sort of frequency as the window title update
    // messages. If it ever returns true, RefreshDisplayList will get called.
    virtual bool NeedsRefreshDisplayList() const;

    virtual void *GetDisplayDataForDisplayID(uint32_t display_id) const = 0;
    virtual void *GetDisplayDataForPoint(int x, int y) const = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// calls Init() and RefreshDisplayList() as part of the process. If either
// fails, returns null.
std::unique_ptr<VBlankMonitor> CreateVBlankMonitor(VBlankMonitor::Handler *handler,
                                                   bool force_default,
                                                   Messages *messages);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
