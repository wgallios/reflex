#pragma once

#include "Window.hpp"
#include "debug/DebugDraw.hpp"
#include "gameplay/PlayerController.hpp"
#include "input/InputState.hpp"
#include "rendering/Renderer.hpp"
#include "scene/Camera.hpp"
#include "scene/GltfLoader.hpp"
#include "scene/Scene.hpp"

#include <filesystem>

class Application {
public:
    Application() = default;
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] bool initialize(const std::filesystem::path& scenePath);
    void run();
    void shutdown() noexcept;

private:
    void update(float deltaTimeSeconds);
    void render();
    [[nodiscard]] std::filesystem::path resolveAssetPath(
        const std::filesystem::path& path) const;

    Window window_;
    InputState input_;
    Camera camera_;
    GltfLoader gltfLoader_;
    Scene scene_;
    Renderer renderer_;
    DebugDraw debugDraw_;
    PlayerController player_;
    double simulationAccumulator_{0.0};
    float diagnosticLogAccumulator_{0.0F};
    bool pendingJump_{false};
    bool collisionDiagnosticsVisible_{false};
    bool sdlInitialized_{false};
    bool initialized_{false};
};
