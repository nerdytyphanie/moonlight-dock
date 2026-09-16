// Moonlight Dock additions, 2026. GPL-3.0-or-later; see LICENSE.
#define SDL_MAIN_HANDLED
#include "../app/streaming/input/dockexithold.h"
#include <cassert>
#include <cstdio>

int main()
{
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    assert(SDL_Init(SDL_INIT_GAMECONTROLLER) == 0);
    int device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
        SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    assert(device >= 0 && SDL_IsGameController(device));
    auto controller = SDL_GameControllerOpen(device);
    assert(controller);
    auto joystick = SDL_GameControllerGetJoystick(controller);
    auto reset = [&]() {
        for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) assert(SDL_JoystickSetVirtualButton(joystick, i, 0) == 0);
        for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) assert(SDL_JoystickSetVirtualAxis(joystick, i, i >= SDL_CONTROLLER_AXIS_TRIGGERLEFT ? -32768 : 0) == 0);
        SDL_GameControllerUpdate();
    };
    reset();
    assert(findDockExitButton("select") == &dockExitButtons[0]);
    assert(!findDockExitButton("invalid"));
    for (const auto& button : dockExitButtons) {
        reset();
        DockExitHold hold;
        assert(!hold.poll(controller, button, 100));
        if (button.axis == SDL_CONTROLLER_AXIS_INVALID) assert(SDL_JoystickSetVirtualButton(joystick, button.button, 1) == 0);
        else assert(SDL_JoystickSetVirtualAxis(joystick, button.axis, 32767) == 0);
        SDL_GameControllerUpdate();
        assert(!hold.poll(controller, button, 200));
        assert(!hold.poll(controller, button, 3199));
        assert(hold.poll(controller, button, 3200));
        reset();
        assert(!hold.poll(controller, button, 3300));
        std::printf("PASS SDL virtual controller: %s\n", button.name);
    }
    const auto& select = *findDockExitButton("select");
    DockExitHold hold;
    assert(SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_BACK, 1) == 0);
    SDL_GameControllerUpdate();
    assert(!hold.poll(controller, select, 0xFFFFFF00u));
    assert(hold.poll(controller, select, 0xFFFFFF00u + 3000u));
    assert(!hold.poll(nullptr, select, 3000));
    assert(!hold.poll(controller, select, 3100));
    assert(!hold.poll(controller, select, 6099));
    assert(hold.poll(controller, select, 6100));
    assert(SDL_JoystickDetachVirtual(device) == 0);
    assert(!hold.poll(controller, select, 10000));
    SDL_GameControllerClose(controller);
    SDL_Quit();
    std::puts("All Dock SDL hold checks passed, including timeout, release, disconnect and tick wraparound.");
}
