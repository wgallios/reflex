#pragma once

#include "Window.hpp"

class Application {
public:
    Application() = default;
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] bool initialize();
    void run();
    void shutdown() noexcept;

private:
    void update(float deltaTimeSeconds);
    void render();

    Window window_;
    bool sdlInitialized_{false};
    bool initialized_{false};
};
