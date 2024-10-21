#ifndef HEADER_5E5A9B9A9EC2441C8E8970CB6886FD32
#define HEADER_5E5A9B9A9EC2441C8E8970CB6886FD32

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BBCMicro;
class DiscInterface;
struct DiscInterfaceDef;

#include "roms.h"
#include <string>
#include <functional>

#include <shared/enum_decl.h>
#include "DiscInterface.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Normalized control bits.
struct DiscInterfaceControl {
    // Drive to use, or <0 if indeterminate.
    int8_t drive = -1;

    // Density select - 1=double, 0=single.
    uint8_t dden = 0;

    // Side select.
    uint8_t side = 0;

    // Reset.
    uint8_t reset = 0;
};
typedef struct DiscInterfaceControl DiscInterfaceControl;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DiscInterface {
  public:
    uint16_t fdc_addr = 0;
    uint16_t control_addr = 0;
    uint32_t flags = 0;

    DiscInterface(uint16_t fdc_addr, uint16_t control_addr, uint32_t flags);
    virtual ~DiscInterface() = 0;

    virtual DiscInterface *Clone() const = 0;

    virtual DiscInterfaceControl GetControlFromByte(uint8_t value) const = 0;
    virtual uint8_t GetByteFromControl(DiscInterfaceControl control) const = 0;

    // Default impl does nothing.
    virtual void InstallExtraHardware(BBCMicro *m);

  protected:
    DiscInterface(DiscInterface &) = default;
    DiscInterface &operator=(const DiscInterface &) = default;

    DiscInterface(DiscInterface &&) = default;
    DiscInterface &operator=(DiscInterface &&) = default;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct DiscInterfaceDef {
    const std::string name;
    StandardROM fs_rom;
    std::function<DiscInterface *()> create_fun;

    // Quick bodge to indicate that the Challenger is known to use
    // page &FC, making it incompatible with the ExtRam. (Some better
    // mechanism for all of this is plausible... one day...)
    bool uses_1MHz_bus;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This disc interface is used for the B+ and B+128.
extern const DiscInterfaceDef DISC_INTERFACE_ACORN_1770;
#ifdef B2_LIBRETRO_CORE
// Disc interface definitions are used directly by the libretro core
extern const DiscInterfaceDef DISC_INTERFACE_WATFORD_DDB2;
extern const DiscInterfaceDef DISC_INTERFACE_WATFORD_DDB3;
extern const DiscInterfaceDef DISC_INTERFACE_OPUS;
extern const DiscInterfaceDef DISC_INTERFACE_CHALLENGER_256K;
extern const DiscInterfaceDef DISC_INTERFACE_CHALLENGER_512K;
#endif
// This disc interface is used for the Master 128.
extern const DiscInterfaceDef DISC_INTERFACE_MASTER128;

// The list of disc interfaces that can be used with a model B. Array
// ends with NULL.
extern const DiscInterfaceDef *const ALL_DISC_INTERFACES[];

const DiscInterfaceDef *FindDiscInterfaceByName(const char *name);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
