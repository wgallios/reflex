#pragma once

#include <SDL3/SDL.h>

#include <array>

class InputState {
public:
    void beginFrame() noexcept;
    void handleEvent(const SDL_Event& event) noexcept;
    void clear() noexcept;

    [[nodiscard]] bool isDown(SDL_Scancode key) const noexcept;
    [[nodiscard]] bool wasPressed(SDL_Scancode key) const noexcept;
    [[nodiscard]] float mouseDeltaX() const noexcept;
    [[nodiscard]] float mouseDeltaY() const noexcept;

private:
    std::array<bool, SDL_SCANCODE_COUNT> keys_{};
    std::array<bool, SDL_SCANCODE_COUNT> pressed_{};
    float mouseDeltaX_{0.0F};
    float mouseDeltaY_{0.0F};
};
