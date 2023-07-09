#include <shared/system.h>
#include <shared/debug.h>
#include <SDL.h>
#include <memory>
#include "joysticks.h"
#include "misc.h"
#include "Messages.h"
#include "BeebWindows.h"
#include "dear_imgui.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct PCJoystick {
    SDL_JoystickID id = -1;
    std::unique_ptr<SDL_GameController, SDL_Deleter> sdl_controller;
    int16_t controller_axis_values[SDL_CONTROLLER_AXIS_MAX] = {};
    uint32_t controller_button_states = 0;
    std::string device_name;
    std::string display_name;
};
static_assert(sizeof(PCJoystick::controller_button_states) * CHAR_BIT >= SDL_CONTROLLER_BUTTON_MAX, "PCJoystick::controller_button_states needs to be a wider type");

struct BeebJoystick {
    std::string device_name;
    PCJoystick *pc_joystick = nullptr; //if connected
};

static const std::string NULL_JOYSTICK_NAME("(none)");
static const std::string NOT_CONNECTED("(not connected) ");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// indexed by joystick index.
static std::vector<std::unique_ptr<PCJoystick>> g_pc_joysticks;

static BeebJoystick g_beeb_joysticks[NUM_BEEB_JOYSTICKS];
static PCJoystick *g_last_used_pc_joystick;
static bool g_swap_shared_joysticks = false;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static PCJoystick *FindPCJoystick(SDL_JoystickID id) {
    for (std::unique_ptr<PCJoystick> &pc_joystick : g_pc_joysticks) {
        if (pc_joystick->id == id) {
            return pc_joystick.get();
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SetPCJoystick(int beeb_index, const std::unique_ptr<PCJoystick> &pc_joystick, Messages *msg) {
    BeebJoystick *beeb_joystick = &g_beeb_joysticks[beeb_index];

    if (msg) {
        if (beeb_joystick->device_name.empty()) {
            ASSERT(!pc_joystick);
            msg->i.f("Joystick %d: %s\n", beeb_index, NULL_JOYSTICK_NAME.c_str());
        } else {
            if (!pc_joystick) {
                msg->i.f("Joystick %d: %s%s\n", beeb_index, NOT_CONNECTED.c_str(), beeb_joystick->device_name.c_str());
            } else {
                msg->i.f("Joystick %d: %s\n", beeb_index, pc_joystick->display_name.c_str());
            }
        }
    }

    beeb_joystick->pc_joystick = pc_joystick.get();

    if (!!pc_joystick) {
        for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis) {
            int16_t value = SDL_GameControllerGetAxis(pc_joystick->sdl_controller.get(),
                                                      (SDL_GameControllerAxis)axis);
            pc_joystick->controller_axis_values[axis] = value;
        }

        beeb_joystick->pc_joystick->controller_button_states = 0;
        for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; ++button) {
            if (SDL_GameControllerGetButton(pc_joystick->sdl_controller.get(),
                                            (SDL_GameControllerButton)button)) {
                pc_joystick->controller_button_states |= 1u << button;
            }
        }

        // Re-send events so that the new joystick state is updated.
        uint32_t timestamp = SDL_GetTicks();

        for (Uint8 axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis) {
            SDL_Event event = {};

            event.caxis.type = SDL_CONTROLLERAXISMOTION;
            event.caxis.timestamp = timestamp;
            event.caxis.which = pc_joystick->id;
            event.caxis.axis = axis;
            event.caxis.value = pc_joystick->controller_axis_values[axis];

            SDL_PushEvent(&event);
        }

        for (Uint8 button = 0; button < SDL_CONTROLLER_BUTTON_MAX; ++button) {
            bool state = !!(pc_joystick->controller_button_states & 1u << button);
            SDL_Event event = {};

            event.cbutton.type = state ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
            event.cbutton.timestamp = timestamp;
            event.cbutton.which = pc_joystick->id;
            event.cbutton.button = button;
            event.cbutton.state = state ? SDL_PRESSED : SDL_RELEASED;

            SDL_PushEvent(&event);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void OpenJoystick(int sdl_joystick_device_index, Messages *msg) {
    SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(sdl_joystick_device_index);
    ASSERT(id >= 0);

    if (FindPCJoystick(id)) {
        // already open.
        return;
    }

    {
        if (!SDL_IsGameController(sdl_joystick_device_index)) {
            if (const char *name = SDL_JoystickNameForIndex(sdl_joystick_device_index)) {
                msg->w.f("Not a supported game controller: %s\n", name);
            } else {
                msg->w.f("Not a supported game controller: joystick index %d\n", sdl_joystick_device_index);
            }

            return;
        }

        auto pc_joystick = std::make_unique<PCJoystick>();

        pc_joystick->sdl_controller.reset(SDL_GameControllerOpen(sdl_joystick_device_index));
        if (!pc_joystick->sdl_controller) {
            msg->w.f("Failed to open game controller: %s\n", SDL_GetError());
            return;
        }

        pc_joystick->id = id;

        if (const char *name = SDL_GameControllerName(pc_joystick->sdl_controller.get())) {
            pc_joystick->device_name = name;
        } else {
            SDL_Joystick *sdl_joystick = SDL_GameControllerGetJoystick(pc_joystick->sdl_controller.get());
            char guid_str[33];
            SDL_JoystickGUID guid = SDL_JoystickGetGUID(sdl_joystick);
            SDL_JoystickGetGUIDString(guid, guid_str, sizeof guid_str);
            pc_joystick->device_name = guid_str;
        }

        pc_joystick->display_name = pc_joystick->device_name + " {#" + std::to_string(pc_joystick->id) + "}";

        for (int i = 0; i < 2; ++i) {
            BeebJoystick *beeb_joystick = &g_beeb_joysticks[i];

            if (beeb_joystick->device_name == pc_joystick->device_name) {
                SetPCJoystick(i, pc_joystick, msg);
            }
        }

        g_pc_joysticks.push_back(std::move(pc_joystick));
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void CloseJoystick(SDL_JoystickID id, Messages *msg) {
    for (auto &&it = g_pc_joysticks.begin(); it != g_pc_joysticks.end(); ++it) {
        if ((*it)->id == id) {
            //msg->w.f("PC joystick disconnected; %s\n", (*it)->name.c_str());

            if (g_last_used_pc_joystick == &**it) {
                g_last_used_pc_joystick = nullptr;
            }

            for (int i = 0; i < 2; ++i) {
                BeebJoystick *beeb_joystick = &g_beeb_joysticks[i];

                if (beeb_joystick->pc_joystick == &**it) {
                    SetPCJoystick(i, nullptr, msg);
                }
            }

            g_pc_joysticks.erase(it);
            return;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void JoystickDeviceAdded(int device_index, Messages *msg) {
    OpenJoystick(device_index, msg);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void JoystickDeviceRemoved(int device_instance, Messages *msg) {
    CloseJoystick(device_instance, msg);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Returns true if a single gamepad is driving both Beeb joysticks.
static bool AreJoysticksShared() {
    if (g_beeb_joysticks[0].pc_joystick) {
        if (g_beeb_joysticks[0].pc_joystick == g_beeb_joysticks[1].pc_joystick) {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint16_t GetAnalogueChannelValueFromJoystickAxisValue(int16_t joystick_axis_value) {
    uint16_t value = (uint16_t) ~((int32_t)joystick_axis_value + 32768);
    value >>= 6;
    return value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

JoystickResult ControllerAxisMotion(int device_instance, int axis, int16_t value) {
    if (axis >= 0 && axis < SDL_CONTROLLER_AXIS_MAX) {
        if (PCJoystick *pc_joystick = FindPCJoystick(device_instance)) {
            pc_joystick->controller_axis_values[axis] = value;

            uint16_t uvalue = GetAnalogueChannelValueFromJoystickAxisValue(value);
            if (AreJoysticksShared()) {
                JoystickResult result = {};
                switch (axis) {
                case SDL_CONTROLLER_AXIS_LEFTX:
                    result = {0, uvalue};
                    break;

                case SDL_CONTROLLER_AXIS_LEFTY:
                    result = {1, uvalue};
                    break;

                case SDL_CONTROLLER_AXIS_RIGHTX:
                    result = {2, uvalue};
                    break;

                case SDL_CONTROLLER_AXIS_RIGHTY:
                    result = {3, uvalue};
                    break;
                }

                if (g_swap_shared_joysticks) {
                    if (result.channel >= 0) {
                        result.channel ^= 2;
                    }
                }

                return result;
            } else if (g_beeb_joysticks[0].pc_joystick == pc_joystick) {
                switch (axis) {
                case SDL_CONTROLLER_AXIS_LEFTX:
                    return {0, uvalue};

                case SDL_CONTROLLER_AXIS_LEFTY:
                    return {1, uvalue};
                }
            } else if (g_beeb_joysticks[1].pc_joystick == pc_joystick) {
                switch (axis) {
                case SDL_CONTROLLER_AXIS_LEFTX:
                    return {2, uvalue};

                case SDL_CONTROLLER_AXIS_LEFTY:
                    return {3, uvalue};
                }
            }
        }
    }

    return {};
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static JoystickResult GetJoystickResultForButton(const PCJoystick *pc_joystick, int button, int8_t beeb_index) {
    JoystickResult jr;

    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        jr.channel = beeb_index * 2 + 0;
        if (pc_joystick->controller_button_states & 1 << SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
            jr.channel_value = GetAnalogueChannelValueFromJoystickAxisValue(-32768);
        } else if (pc_joystick->controller_button_states & 1 << SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
            jr.channel_value = GetAnalogueChannelValueFromJoystickAxisValue(32767);
        } else {
            jr.channel_value = GetAnalogueChannelValueFromJoystickAxisValue(pc_joystick->controller_axis_values[SDL_CONTROLLER_AXIS_LEFTX]);
        }
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_UP:
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        jr.channel = beeb_index * 2 + 1;
        if (pc_joystick->controller_button_states & 1 << SDL_CONTROLLER_BUTTON_DPAD_UP) {
            jr.channel_value = GetAnalogueChannelValueFromJoystickAxisValue(-32768);
        } else if (pc_joystick->controller_button_states & 1 << SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
            jr.channel_value = GetAnalogueChannelValueFromJoystickAxisValue(32767);
        } else {
            jr.channel_value = GetAnalogueChannelValueFromJoystickAxisValue(pc_joystick->controller_axis_values[SDL_CONTROLLER_AXIS_LEFTY]);
        }
        break;

    case SDL_CONTROLLER_BUTTON_A:
    case SDL_CONTROLLER_BUTTON_B:
    case SDL_CONTROLLER_BUTTON_X:
    case SDL_CONTROLLER_BUTTON_Y:
        jr.button_joystick_index = beeb_index;
        jr.button_state = (pc_joystick->controller_button_states & (1 << SDL_CONTROLLER_BUTTON_A | 1 << SDL_CONTROLLER_BUTTON_B | 1 << SDL_CONTROLLER_BUTTON_X | 1 << SDL_CONTROLLER_BUTTON_Y)) != 0;
        break;
    }

    return jr;
}

JoystickResult ControllerButton(int device_instance, int button, bool state) {
    if (button >= 0 && button < SDL_CONTROLLER_BUTTON_MAX) {
        if (PCJoystick *pc_joystick = FindPCJoystick(device_instance)) {
            uint32_t mask = 1u << button;
            if (state) {
                pc_joystick->controller_button_states |= mask;
                g_last_used_pc_joystick = pc_joystick;
            } else {
                pc_joystick->controller_button_states &= ~mask;
            }

            if (AreJoysticksShared()) {
                JoystickResult jr;

                if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                    jr.button_joystick_index = 0;
                    jr.button_state = state;
                } else if (button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                    jr.button_joystick_index = 1;
                    jr.button_state = state;
                }

                if (g_swap_shared_joysticks) {
                    if (jr.button_joystick_index >= 0) {
                        jr.button_joystick_index ^= 1;
                    }
                }

                return jr;
            } else if (g_beeb_joysticks[0].pc_joystick == pc_joystick) {
                return GetJoystickResultForButton(pc_joystick, button, 0);
            } else if (g_beeb_joysticks[1].pc_joystick == pc_joystick) {
                return GetJoystickResultForButton(pc_joystick, button, 1);
            }
        }
    }

    return {};
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DoJoysticksMenuImGui(Messages *msg) {
    for (int i = 0; i < NUM_BEEB_JOYSTICKS; ++i) {
        BeebJoystick *beeb_joystick = &g_beeb_joysticks[i];

        std::string label = std::to_string(i) + ": ";

        if (beeb_joystick->device_name.empty()) {
            label += NULL_JOYSTICK_NAME;
        } else {
            if (beeb_joystick->pc_joystick) {
                label += beeb_joystick->pc_joystick->display_name;
            } else {
                label += NOT_CONNECTED + beeb_joystick->device_name;
            }
        }

        if (ImGui::BeginMenu(label.c_str())) {
            bool tick;

            tick = beeb_joystick->device_name.empty();
            if (ImGui::MenuItem(NULL_JOYSTICK_NAME.c_str(), nullptr, &tick)) {
                beeb_joystick->device_name.clear();
                SetPCJoystick(i, nullptr, msg);
            }

            for (const std::unique_ptr<PCJoystick> &pc_joystick : g_pc_joysticks) {
                tick = pc_joystick.get() == beeb_joystick->pc_joystick;
                if (ImGui::MenuItem(pc_joystick->display_name.c_str(), nullptr, &tick)) {
                    beeb_joystick->device_name = pc_joystick->device_name;
                    SetPCJoystick(i, pc_joystick, msg);
                }
            }

            ImGui::EndMenu();
        }
    }

    if (AreJoysticksShared()) {
        ImGui::Separator();

        ImGui::Checkbox("Swap shared joysticks", &g_swap_shared_joysticks);
    }

    ImGui::Separator();

    std::string label = "Last used joystick: ";

    if (g_last_used_pc_joystick) {
        label += g_last_used_pc_joystick->display_name;
    } else {
        label += NULL_JOYSTICK_NAME;
    }

    ImGui::MenuItem(label.c_str());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

JoysticksConfig GetJoysticksConfig() {
    JoysticksConfig config;

    for (uint8_t beeb_index = 0; beeb_index < NUM_BEEB_JOYSTICKS; ++beeb_index) {
        config.device_names[beeb_index] = g_beeb_joysticks[beeb_index].device_name;
    }

    config.swap_joysticks_when_shared = g_swap_shared_joysticks;

    return config;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetJoysticksConfig(const JoysticksConfig &config) {
    for (uint8_t beeb_index = 0; beeb_index < NUM_BEEB_JOYSTICKS; ++beeb_index) {
        g_beeb_joysticks[beeb_index].device_name = config.device_names[beeb_index];
    }

    g_swap_shared_joysticks = config.swap_joysticks_when_shared;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CloseJoysticks() {
    g_pc_joysticks.clear();

    for (uint8_t i = 0; i < NUM_BEEB_JOYSTICKS; ++i) {
        g_beeb_joysticks[i].pc_joystick = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
