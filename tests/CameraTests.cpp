#include "input/InputState.hpp"
#include "scene/Camera.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <iostream>

namespace {
constexpr float tolerance = 0.0001F;

void setKey(InputState& input, const SDL_Scancode key, const bool down = true) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = key;
    input.handleEvent(event);
}

bool nearlyEqual(const glm::vec3& left, const glm::vec3& right) {
    return glm::length(left - right) < tolerance;
}
} // namespace

int main() {
    InputState forwardInput;
    setKey(forwardInput, SDL_SCANCODE_W);

    Camera singleStep;
    singleStep.update(forwardInput, 1.0F, false);
    Camera splitStep;
    splitStep.update(forwardInput, 0.5F, false);
    splitStep.update(forwardInput, 0.5F, false);
    if (!nearlyEqual(singleStep.position(), splitStep.position())) {
        std::cerr << "Camera movement depends on frame subdivision.\n";
        return 1;
    }

    Camera sprint;
    InputState sprintInput;
    setKey(sprintInput, SDL_SCANCODE_W);
    setKey(sprintInput, SDL_SCANCODE_LSHIFT);
    const glm::vec3 sprintStart = sprint.position();
    sprint.update(sprintInput, 1.0F, false);
    if (std::abs(glm::length(sprint.position() - sprintStart) - 12.0F) > tolerance) {
        std::cerr << "Camera sprint multiplier is incorrect.\n";
        return 1;
    }

    Camera vertical;
    InputState verticalInput;
    setKey(verticalInput, SDL_SCANCODE_SPACE);
    const float startingHeight = vertical.position().y;
    vertical.update(verticalInput, 0.5F, false);
    if (std::abs(vertical.position().y - startingHeight - 2.0F) > tolerance) {
        std::cerr << "Vertical noclip movement is incorrect.\n";
        return 1;
    }

    Camera mouseLook;
    InputState mouseInput;
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.xrel = 100.0F;
    motion.motion.yrel = -50.0F;
    mouseInput.handleEvent(motion);
    const glm::vec3 oldForward = mouseLook.forward();
    mouseLook.update(mouseInput, 0.0F, true);
    if (nearlyEqual(oldForward, mouseLook.forward())) {
        std::cerr << "Captured relative mouse motion did not rotate the camera.\n";
        return 1;
    }

    return 0;
}
