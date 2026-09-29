// Moonlight Dock additions, 2026. GPL-3.0-or-later; see LICENSE.
#pragma once
#include <SDL.h>
#include <cstring>

struct DockExitButton {
    const char* name;
    SDL_GameControllerButton button;
    SDL_GameControllerAxis axis;
};

static const DockExitButton dockExitButtons[] = {
    {"select", SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_AXIS_INVALID},
    {"start", SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_AXIS_INVALID},
    {"a", SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_AXIS_INVALID},
    {"b", SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_AXIS_INVALID},
    {"x", SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_AXIS_INVALID},
    {"y", SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_AXIS_INVALID},
    {"leftbumper", SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_AXIS_INVALID},
    {"rightbumper", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_AXIS_INVALID},
    {"leftstick", SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_AXIS_INVALID},
    {"rightstick", SDL_CONTROLLER_BUTTON_RIGHTSTICK, SDL_CONTROLLER_AXIS_INVALID},
    {"dpadup", SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_AXIS_INVALID},
    {"dpaddown", SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_AXIS_INVALID},
    {"dpadleft", SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_AXIS_INVALID},
    {"dpadright", SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_AXIS_INVALID},
    {"lefttrigger", SDL_CONTROLLER_BUTTON_INVALID, SDL_CONTROLLER_AXIS_TRIGGERLEFT},
    {"righttrigger", SDL_CONTROLLER_BUTTON_INVALID, SDL_CONTROLLER_AXIS_TRIGGERRIGHT},
};

inline const DockExitButton* findDockExitButton(const char* name)
{
    for (const auto& choice : dockExitButtons) {
        if (std::strcmp(choice.name, name) == 0) return &choice;
    }
    return nullptr;
}

class DockExitHold {
public:
    bool poll(SDL_GameController* controller, const DockExitButton& button, Uint32 now)
    {
        if (!controller || !SDL_GameControllerGetAttached(controller)) {
            m_Pressed = false;
            m_Id = -1;
            return false;
        }
        SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
        if (id != m_Id) { m_Pressed = false; m_Id = id; }
        bool pressed = button.axis == SDL_CONTROLLER_AXIS_INVALID
            ? SDL_GameControllerGetButton(controller, button.button) != 0
            : SDL_GameControllerGetAxis(controller, button.axis) > 30 * 128;
        if (!pressed) { m_Pressed = false; return false; }
        if (!m_Pressed) { m_Pressed = true; m_PressedAt = now; }
        // Unsigned subtraction also works across SDL's tick-counter wraparound.
        return static_cast<Uint32>(now - m_PressedAt) >= 3000;
    }
private:
    SDL_JoystickID m_Id = -1;
    Uint32 m_PressedAt = 0;
    bool m_Pressed = false;
};
