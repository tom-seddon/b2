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

    g_beeb_joysticks[beeb_index].pc_joystick = pc_joystick.get();
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

JoystickResult ControllerAxisMotion(int timestamp, int device_instance, int axis, int16_t value) {
    if (axis >= 0 && axis < SDL_CONTROLLER_AXIS_MAX) {
        if (PCJoystick *pc_joystick = FindPCJoystick(device_instance)) {
            pc_joystick->controller_axis_values[axis] = value;

            uint16_t uvalue = (uint16_t) ~((int32_t)value + 32768);
            if (g_beeb_joysticks[0].pc_joystick && g_beeb_joysticks[0].pc_joystick == g_beeb_joysticks[1].pc_joystick) {
                // two Beeb joysticks from one PC controller
                switch (axis) {
                case SDL_CONTROLLER_AXIS_LEFTX:
                    return {0, uvalue};
                    break;
                case SDL_CONTROLLER_AXIS_LEFTY:
                    return {1, uvalue};
                    break;
                case SDL_CONTROLLER_AXIS_RIGHTX:
                    return {2, uvalue};
                    break;
                case SDL_CONTROLLER_AXIS_RIGHTY:
                    return {3, uvalue};
                    break;
                }
            } else if (g_beeb_joysticks[0].pc_joystick == pc_joystick) {
                switch (axis) {
                case SDL_CONTROLLER_AXIS_LEFTX:
                case SDL_CONTROLLER_AXIS_RIGHTX:
                    return {0, uvalue};

                case SDL_CONTROLLER_AXIS_LEFTY:
                case SDL_CONTROLLER_AXIS_RIGHTY:
                    return {1, uvalue};
                }
            } else if (g_beeb_joysticks[1].pc_joystick == pc_joystick) {
                switch (axis) {
                case SDL_CONTROLLER_AXIS_LEFTX:
                case SDL_CONTROLLER_AXIS_RIGHTX:
                    return {2, uvalue};

                case SDL_CONTROLLER_AXIS_LEFTY:
                case SDL_CONTROLLER_AXIS_RIGHTY:
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
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        jr.channel = beeb_index * 2 + 1;
        if (pc_joystick->controller_button_states & 1 << SDL_CONTROLLER_BUTTON_DPAD_UP) {
            jr.channel_value = 0;
        } else if (pc_joystick->controller_button_states & 1 << SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
            jr.channel_value = 0xffff;
        }
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        jr.channel = beeb_index * 2 + 0;
        if (pc_joystick->controller_button_states & 1 << SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
            jr.channel_value = 0;
        } else if (pc_joystick->controller_button_states & 1 << SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
            jr.channel_value = 0xffff;
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

JoystickResult ControllerButton(int timestamp, int device_instance, int button, bool state) {
    if (button >= 0 && button < SDL_CONTROLLER_BUTTON_MAX) {
        if (PCJoystick *pc_joystick = FindPCJoystick(device_instance)) {
            uint32_t mask = 1u << button;
            if (state) {
                pc_joystick->controller_button_states |= mask;
            } else {
                pc_joystick->controller_button_states &= ~mask;
            }

            g_last_used_pc_joystick = pc_joystick;

            if (g_beeb_joysticks[0].pc_joystick && g_beeb_joysticks[0].pc_joystick == g_beeb_joysticks[1].pc_joystick) {
                // two Beeb joysticks from one PC controller. Only the shoulder buttons count.
                if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                    JoystickResult jr;
                    jr.button_joystick_index = 0;
                    jr.button_state = state;
                    return jr;
                } else if (button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                    JoystickResult jr;
                    jr.button_joystick_index = 1;
                    jr.button_state = state;
                    return jr;
                }
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

            ImGui::Separator();

            label = "Last used joystick: ";

            if (g_last_used_pc_joystick) {
                label += g_last_used_pc_joystick->display_name;
            } else {
                label += NULL_JOYSTICK_NAME;
            }

            ImGui::MenuItem(label.c_str());

            ImGui::EndMenu();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetPCJoystickDeviceNameByBeebIndex(uint8_t beeb_index) {
    ASSERT(beeb_index < NUM_BEEB_JOYSTICKS);

    return g_beeb_joysticks[beeb_index].device_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetPCJoystickDeviceNameByBeebIndex(uint8_t beeb_index, std::string device_name) {
    ASSERT(g_pc_joysticks.empty());
    ASSERT(beeb_index < NUM_BEEB_JOYSTICKS);

    g_beeb_joysticks[beeb_index].device_name = std::move(device_name);
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
