#include <shared/system.h>
#include <beeb/BBCMicroState.h>
#include <beeb/DiscImage.h>
#include <array>
#include <type_traits>

#include <shared/enum_def.h>
#include <beeb/BBCMicroState.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static_assert(!std::is_convertible<BBCMicroState, BBCMicroUniqueState>::value);
static_assert(!std::is_convertible<BBCMicroState, BBCMicroReadOnlyState>::value);
static_assert(!std::is_convertible<BBCMicroReadOnlyState, BBCMicroUniqueState>::value);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicroState::BBCMicroState(const BBCMicroType *type_,
                             const DiscInterface *disc_interface_,
                             BBCMicroParasiteType parasite_type_,
                             const std::vector<uint8_t> &nvram_contents,
                             uint32_t init_flags_,
                             const tm *rtc_time,
                             CycleCount initial_cycle_count)
    : type(type_)
    , init_flags(init_flags_)
    , parasite_type(parasite_type_)
    , cycle_count(initial_cycle_count)
    , disc_interface(disc_interface_) {
    M6502_Init(&this->cpu, this->type->m6502_config);
    this->ram_buffer = std::make_shared<std::vector<uint8_t>>(this->type->ram_buffer_size);

    if (this->disc_interface) {
        this->disc_interface_extra_hardware = this->disc_interface->CreateExtraHardwareState();
    }

    switch (this->type->type_id) {
    default:
        break;

    case BBCMicroTypeID_Master:
        this->rtc.SetRAMContents(nvram_contents);

        if (rtc_time) {
            this->rtc.SetTime(rtc_time);
        }
        break;

    case BBCMicroTypeID_MasterCompact:
        for (size_t i = 0; i < sizeof this->eeprom.ram; ++i) {
            this->eeprom.ram[i] = i < nvram_contents.size() ? nvram_contents[i] : 0;
        }
        break;
    }

    if (this->parasite_type != BBCMicroParasiteType_None) {
        this->parasite_ram_buffer = std::make_shared<std::vector<uint8_t>>(65536);
        this->parasite_boot_mode = true;
        M6502_Init(&this->parasite_cpu, &M6502_rockwell65c02_config);
        ResetTube(&this->parasite_tube);

        // Whether disabled or not, the parasite starts out inaccessible, as the
        // relevant I/O functions start out as the defaults. InitPaging will
        // sort this out, if it needs to change.
    }

    this->sn76489.Reset(!!(this->init_flags & BBCMicroInitFlag_PowerOnTone));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const M6502 *BBCMicroState::DebugGetM6502(uint32_t dso) const {
    if (dso & BBCMicroDebugStateOverride_Parasite) {
        if (this->parasite_type != BBCMicroParasiteType_None) {
            return &this->parasite_cpu;
        }
    }

    return &this->cpu;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const ExtMem *BBCMicroState::DebugGetExtMem() const {
    if (this->init_flags & BBCMicroInitFlag_ExtMem) {
        return &this->ext_mem;
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const MC146818 *BBCMicroState::DebugGetRTC() const {
    if (this->type->type_id == BBCMicroTypeID_Master) {
        return &this->rtc;
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const Tube *BBCMicroState::DebugGetTube() const {
    if (this->parasite_type != BBCMicroParasiteType_None) {
        return &this->parasite_tube;
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const ADC *BBCMicroState::DebugGetADC() const {
    if (this->type->adc_addr != 0) {
        return &this->adc;
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const PCD8572 *BBCMicroState::DebugGetEEPROM() const {
    if (this->type->type_id == BBCMicroTypeID_MasterCompact) {
        return &this->eeprom;
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
int BBCMicroState::DebugGetADJIDIPSwitches() const {
    if (this->init_flags & BBCMicroInitFlag_ADJI) {
        return this->init_flags >> BBCMicroInitFlag_ADJIDIPSwitchesShift & 3;
    } else {
        return -1;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DiscImage> BBCMicroState::GetDiscImage(int drive) const {
    if (drive >= 0 && drive < NUM_DRIVES) {
        return this->drives[drive].disc_image;
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicroUniqueState::BBCMicroUniqueState(const BBCMicroUniqueState &src)
    : BBCMicroState(src) {
    for (BBCMicroState::DiscDrive &dd : this->drives) {
        dd.disc_image = DiscImage::Clone(dd.disc_image);
    }

    if (this->ram_buffer) {
        this->ram_buffer = std::make_shared<std::vector<uint8_t>>(*this->ram_buffer);
    }

    for (int i = 0; i < 16; ++i) {
        if (this->sideways_ram_buffers[i]) {
            this->sideways_ram_buffers[i] = std::make_shared<std::array<uint8_t, 16384>>(*this->sideways_ram_buffers[i]);
        }
    }

    if (this->parasite_ram_buffer) {
        this->parasite_ram_buffer = std::make_shared<std::vector<uint8_t>>(*this->parasite_ram_buffer);
    }

    if (this->disc_interface) {
        this->disc_interface_extra_hardware = this->disc_interface->CloneExtraHardwareState(this->disc_interface_extra_hardware);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicroReadOnlyState::BBCMicroReadOnlyState(const BBCMicroUniqueState &src)
    : BBCMicroState(src) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////