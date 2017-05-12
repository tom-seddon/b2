#ifndef HEADER_5E5A9B9A9EC2441C8E8970CB6886FD32
#define HEADER_5E5A9B9A9EC2441C8E8970CB6886FD32

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BBCMicro;
class DiscInterface;
struct DiscInterfaceDef;

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
    int8_t drive=-1;

    // Density select - 1=double, 0=single.
    uint8_t dden=0;

    // Side select.
    uint8_t side=0;

    // Reset.
    uint8_t reset=0;
};
typedef struct DiscInterfaceControl DiscInterfaceControl;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DiscInterface {
public:
    uint16_t fdc_addr=0;
    uint16_t control_addr=0;
    uint32_t flags=0;

    DiscInterface(uint16_t fdc_addr,uint16_t control_addr,uint32_t flags);
    virtual ~DiscInterface()=0;

    virtual DiscInterface *Clone() const=0;

    virtual DiscInterfaceControl GetControlFromByte(uint8_t value) const=0;
    virtual uint8_t GetByteFromControl(DiscInterfaceControl control) const=0;

    // Default impl does nothing.
    virtual void InstallExtraHardware(BBCMicro *m);
protected:
    DiscInterface(DiscInterface &)=default;
    DiscInterface &operator=(const DiscInterface &)=default;

    DiscInterface(DiscInterface &&)=default;
    DiscInterface &operator=(DiscInterface &&)=default;
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct DiscInterfaceDef {
    const std::string name;
    const std::string default_fs_rom;
    std::function<DiscInterface *()> create_fun;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This disc interface is used for the B+ and B+128.
extern const DiscInterfaceDef DISC_INTERFACE_ACORN_1770;

// This disc interface is used for the Master 128.
extern const DiscInterfaceDef DISC_INTERFACE_MASTER128;

// The list of disc interfaces that can be used with a model B. Array
// ends with NULL.
extern const DiscInterfaceDef *const ALL_DISC_INTERFACES[];

// Finds an entry in the ALL_DISC_INTERFACES list by name, or NULL if
// not found.
const DiscInterfaceDef *FindDiscInterfaceByName(const char *name);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
