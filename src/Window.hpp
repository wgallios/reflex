#pragma once

#include <SDL3/SDL.h>

#include <memory>

class InputState;

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] bool initialize(const char* title, int width, int height);
    void shutdown() noexcept;

    [[nodiscard]] bool processEvents(InputState& input);
    void swapBuffers() const;

    [[nodiscard]] bool takeFramebufferResize(int& width, int& height) noexcept;
    [[nodiscard]] bool isMouseCaptured() const noexcept;

private:
    struct SdlWindowDeleter {
        void operator()(SDL_Window* window) const noexcept;
    };

    class GlContext {
    public:
        GlContext() = default;
        ~GlContext();

        GlContext(const GlContext&) = delete;
        GlContext& operator=(const GlContext&) = delete;

        void reset(SDL_GLContext context = nullptr) noexcept;
        [[nodiscard]] SDL_GLContext get() const noexcept;

    private:
        SDL_GLContext context_{nullptr};
    };

    [[nodiscard]] bool setGlAttribute(SDL_GLAttr attribute, int value) const;
    void setMouseCaptured(bool captured);
    void refreshFramebufferSize();

    std::unique_ptr<SDL_Window, SdlWindowDeleter> window_;
    GlContext glContext_;
    int framebufferWidth_{0};
    int framebufferHeight_{0};
    bool framebufferResized_{false};
    bool mouseCaptured_{false};
};
