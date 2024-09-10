#ifndef HEADER_5E5A9B9A9EC2441C8E8970CB6886FD32
#define HEADER_5E5A9B9A9EC2441C8E8970CB6886FD32

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BBCMicro;
class DiscInterface;

#include "roms.h"
#include <string>
#include <memory>

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

// This exists just to be a base class that's suitable for deletion and
// dynamic_cast in ASSERTs.

class DiscInterfaceExtraHardwareState {
  public:
    DiscInterfaceExtraHardwareState() = default;
    virtual ~DiscInterfaceExtraHardwareState() = 0;

  protected:
    DiscInterfaceExtraHardwareState(const DiscInterfaceExtraHardwareState &) = default;
    DiscInterfaceExtraHardwareState &operator=(const DiscInterfaceExtraHardwareState &) = default;
    DiscInterfaceExtraHardwareState(DiscInterfaceExtraHardwareState &&) = default;
    DiscInterfaceExtraHardwareState &operator=(DiscInterfaceExtraHardwareState &&) = default;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DiscInterface {
  public:
    const std::string config_name;
    const std::string display_name;
    const StandardROM fs_rom;
    const uint16_t fdc_addr = 0;
    const uint16_t control_addr = 0;
    const uint32_t flags = 0;

    DiscInterface(std::string config_name, std::string display_name, StandardROM fs_rom, uint16_t fdc_addr, uint16_t control_addr, uint32_t flags);
    virtual ~DiscInterface() = 0;

    //virtual DiscInterface *Clone() const = 0;

    virtual DiscInterfaceControl GetControlFromByte(uint8_t value) const = 0;
    virtual uint8_t GetByteFromControl(DiscInterfaceControl control) const = 0;

    // create accompanying ExtraHardwareState.
    [[nodiscard]] virtual std::shared_ptr<DiscInterfaceExtraHardwareState> CreateExtraHardwareState() const;

    // close accompanying ExtraHardwareState.
    [[nodiscard]] virtual std::shared_ptr<DiscInterfaceExtraHardwareState> CloneExtraHardwareState(const std::shared_ptr<DiscInterfaceExtraHardwareState> &src) const;

    virtual void InstallExtraHardware(BBCMicro *m, const std::shared_ptr<DiscInterfaceExtraHardwareState> &state) const;

  protected:
    DiscInterface(DiscInterface &) = delete;
    DiscInterface &operator=(const DiscInterface &) = delete;

    DiscInterface(DiscInterface &&) = delete;
    DiscInterface &operator=(DiscInterface &&) = delete;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This disc interface is used for the B+ and B+128.
extern const DiscInterface *const DISC_INTERFACE_ACORN_1770;

// This disc interface is used for the Master 128.
extern const DiscInterface *const DISC_INTERFACE_MASTER128;

// The list of disc interfaces that can be used with a model B. Array
// ends with NULL.
extern const DiscInterface *const MODEL_B_DISC_INTERFACES[];

const DiscInterface *FindDiscInterfaceByConfigName(const char *config_name);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
