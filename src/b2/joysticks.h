#ifndef HEADER_FCB6535989674B6CA35DD3E7E2C1405F // -*- mode:c++ -*-
#define HEADER_FCB6535989674B6CA35DD3E7E2C1405F

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Need to fix the terminology - SDL has "joysticks" (device with uncategorised
// bundle of axes, buttons, POV hats, etc.) and "game controllers" (device with
// some subset of standard game controller widgets: two analogue sticks, a
// d-pad, 4 face buttons, 2 triggers, 2 trigger buttons, etc.).
//
// This code actually deals with game controllers.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <beeb/BBCMicro.h>

class Messages;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void JoystickDeviceAdded(int device_index, Messages *msg);
void JoystickDeviceRemoved(int device_instance, Messages *msg);

struct JoystickResult {
    // if >=0, this event resulted in ADC channel data on this channel.
    int8_t channel = -1;
    uint16_t channel_value = 0;

    // if >=0, this event resulted in digital state info for this joystick.
    int8_t digital_joystick_index = -1;
    BBCMicroState::DigitalJoystickInput digital_state = {};
};

JoystickResult ControllerAxisMotion(int device_instance, int axis, int16_t value);
JoystickResult ControllerButton(int device_instance, int button, bool state);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This is not much like the other menus, but the state all ended up pretty
// self-contained, so why not.
//
// In the long run, this will probably become more flexible/fiddly.
void DoJoysticksMenuImGui(Messages *msg);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// 0 = ADC 0
// 1 = ADC 1
// 2 = digital
//
// (this arrangement will be improved...)
static constexpr uint8_t NUM_BEEB_JOYSTICKS = 3;

struct JoysticksConfig {
    std::string device_names[NUM_BEEB_JOYSTICKS];
    bool swap_joysticks_when_shared = false;
};

JoysticksConfig GetJoysticksConfig();
void SetJoysticksConfig(const JoysticksConfig &config);

void CloseJoysticks();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
