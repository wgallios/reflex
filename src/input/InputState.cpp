#include "input/InputState.hpp"

#include <cstddef>

void InputState::beginFrame() noexcept {
    pressed_.fill(false);
    mouseDeltaX_ = 0.0F;
    mouseDeltaY_ = 0.0F;
    mouseButtonsPressed_.fill(false);
    mouseWheelY_ = 0.0F;
}

void InputState::handleEvent(const SDL_Event& event) noexcept {
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        const auto index = static_cast<std::size_t>(event.key.scancode);
        if (index < keys_.size()) {
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && !keys_[index]) {
                pressed_[index] = true;
            }
            keys_[index] = event.type == SDL_EVENT_KEY_DOWN;
        }
    } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
        mouseDeltaX_ += event.motion.xrel;
        mouseDeltaY_ += event.motion.yrel;
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
               event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        const auto index = static_cast<std::size_t>(event.button.button);
        if (index < mouseButtons_.size()) {
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !mouseButtons_[index]) {
                mouseButtonsPressed_[index] = true;
            }
            mouseButtons_[index] = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        }
    } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        mouseWheelY_ += event.wheel.y;
    }
}

void InputState::clear() noexcept {
    keys_.fill(false);
    pressed_.fill(false);
    mouseButtons_.fill(false);
    mouseButtonsPressed_.fill(false);
    beginFrame();
}

bool InputState::wasPressed(const SDL_Scancode key) const noexcept {
    const auto index = static_cast<std::size_t>(key);
    return index < pressed_.size() && pressed_[index];
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

bool InputState::mouseButtonDown(const unsigned char button) const noexcept {
    return button < mouseButtons_.size() && mouseButtons_[button];
}

bool InputState::mouseButtonPressed(const unsigned char button) const noexcept {
    return button < mouseButtonsPressed_.size() && mouseButtonsPressed_[button];
}

float InputState::mouseWheelY() const noexcept { return mouseWheelY_; }
