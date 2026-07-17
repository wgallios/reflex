#include "Window.hpp"

#include "input/InputState.hpp"

#include <iostream>

Window::~Window() {
    shutdown();
}

bool Window::initialize(const char* title, const int width, const int height) {
    if (!setGlAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
        !setGlAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) ||
        !setGlAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) ||
        !setGlAttribute(SDL_GL_DOUBLEBUFFER, 1) ||
        !setGlAttribute(SDL_GL_DEPTH_SIZE, 24)) {
        return false;
    }

    constexpr SDL_WindowFlags windowFlags =
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window_.reset(SDL_CreateWindow(title, width, height, windowFlags));
    if (!window_) {
        std::cerr << "Failed to create the SDL window: " << SDL_GetError() << '\n';
        return false;
    }

    glContext_.reset(SDL_GL_CreateContext(window_.get()));
    if (glContext_.get() == nullptr) {
        std::cerr << "Failed to create an OpenGL 3.3 Core context: " << SDL_GetError() << '\n';
        return false;
    }

    if (!SDL_GL_MakeCurrent(window_.get(), glContext_.get())) {
        std::cerr << "Failed to make the OpenGL context current: " << SDL_GetError() << '\n';
        return false;
    }

    if (!SDL_GL_SetSwapInterval(1)) {
        std::cerr << "Warning: unable to enable vertical synchronization: "
                  << SDL_GetError() << '\n';
    }

    refreshFramebufferSize();
    framebufferResized_ = true;
    return true;
}

void Window::shutdown() noexcept {
    glContext_.reset();
    window_.reset();
    framebufferWidth_ = 0;
    framebufferHeight_ = 0;
    framebufferResized_ = false;
    mouseCaptured_ = false;
}

bool Window::processEvents(InputState& input) {
    input.beginFrame();
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }

        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE &&
            !event.key.repeat) {
            if (mouseCaptured_) {
                setMouseCaptured(false);
                input.clear();
                continue;
            }
            return false;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT && !mouseCaptured_) {
            setMouseCaptured(true);
        }

        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            setMouseCaptured(false);
            input.clear();
        }

        if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
            event.type == SDL_EVENT_WINDOW_RESIZED) {
            refreshFramebufferSize();
            framebufferResized_ = true;
        }

        input.handleEvent(event);
    }

    return true;
}

bool Window::isMouseCaptured() const noexcept {
    return mouseCaptured_;
}

void Window::swapBuffers() const {
    if (window_) {
        SDL_GL_SwapWindow(window_.get());
    }
}

bool Window::takeFramebufferResize(int& width, int& height) noexcept {
    if (!framebufferResized_) {
        return false;
    }

    width = framebufferWidth_;
    height = framebufferHeight_;
    framebufferResized_ = false;
    return true;
}

bool Window::setGlAttribute(const SDL_GLAttr attribute, const int value) const {
    if (SDL_GL_SetAttribute(attribute, value)) {
        return true;
    }

    std::cerr << "Failed to set an SDL OpenGL context attribute: " << SDL_GetError() << '\n';
    return false;
}

void Window::setMouseCaptured(const bool captured) {
    if (!window_ || mouseCaptured_ == captured) {
        return;
    }
    if (!SDL_SetWindowRelativeMouseMode(window_.get(), captured)) {
        std::cerr << "Warning: unable to " << (captured ? "capture" : "release")
                  << " the mouse: " << SDL_GetError() << '\n';
        return;
    }
    mouseCaptured_ = captured;
}

void Window::refreshFramebufferSize() {
    if (!window_) {
        return;
    }

    if (!SDL_GetWindowSizeInPixels(window_.get(), &framebufferWidth_, &framebufferHeight_)) {
        std::cerr << "Warning: unable to query framebuffer size: " << SDL_GetError() << '\n';
    }
}

void Window::SdlWindowDeleter::operator()(SDL_Window* window) const noexcept {
    if (window != nullptr) {
        SDL_DestroyWindow(window);
    }
}

Window::GlContext::~GlContext() {
    reset();
}

void Window::GlContext::reset(SDL_GLContext context) noexcept {
    if (context_ != nullptr) {
        SDL_GL_DestroyContext(context_);
    }
    context_ = context;
}

SDL_GLContext Window::GlContext::get() const noexcept {
    return context_;
}
