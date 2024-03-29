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

    virtual void *GetDisplayDataForDisplayID(uint32_t display_id) const = 0;
    virtual void *GetDisplayDataForPoint(int x, int y) const = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// calls Init() as part of the process. If it fails, returns null.
std::unique_ptr<VBlankMonitor> CreateVBlankMonitor(VBlankMonitor::Handler *handler,
                                                   bool force_default,
                                                   Messages *messages);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
