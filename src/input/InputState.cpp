#include "input/InputState.hpp"

#include <cstddef>

void InputState::beginFrame() noexcept {
    mouseDeltaX_ = 0.0F;
    mouseDeltaY_ = 0.0F;
}

void InputState::handleEvent(const SDL_Event& event) noexcept {
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        const auto index = static_cast<std::size_t>(event.key.scancode);
        if (index < keys_.size()) {
            keys_[index] = event.type == SDL_EVENT_KEY_DOWN;
        }
    } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
        mouseDeltaX_ += event.motion.xrel;
        mouseDeltaY_ += event.motion.yrel;
    }
}

void InputState::clear() noexcept {
    keys_.fill(false);
    beginFrame();
}

bool InputState::isDown(const SDL_Scancode key) const noexcept {
    const auto index = static_cast<std::size_t>(key);
    return index < keys_.size() && keys_[index];
}

float InputState::mouseDeltaX() const noexcept {
    return mouseDeltaX_;
}

float InputState::mouseDeltaY() const noexcept {
    return mouseDeltaY_;
}
