#pragma once

#include "Window.hpp"
#include "collision/DynamicCollisionWorld.hpp"
#include "debug/DebugDraw.hpp"
#include "gameplay/GameplayWorld.hpp"
#include "gameplay/PlayerController.hpp"
#include "input/InputState.hpp"
#include "rendering/Renderer.hpp"
#include "rendering/HudRenderer.hpp"
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
    void quickSave();
    void quickLoad();
    void resetLevel();
    void respawnPlayer();
    [[nodiscard]] std::filesystem::path resolveAssetPath(
        const std::filesystem::path& path) const;

    Window window_;
    InputState input_;
    Camera camera_;
    GltfLoader gltfLoader_;
    Scene scene_;
    Renderer renderer_;
    DebugDraw debugDraw_;
    HudRenderer hudRenderer_;
    PlayerController player_;
    DynamicCollisionWorld dynamicCollision_;
    GameplayWorld gameplay_;
    std::filesystem::path scenePath_;
    double simulationAccumulator_{0.0};
    float diagnosticLogAccumulator_{0.0F};
    bool pendingJump_{false};
    bool collisionDiagnosticsVisible_{false};
    bool gameplayDiagnosticsVisible_{false};
    float deathTimer_{0.0F};
    int framebufferWidth_{1280};
    int framebufferHeight_{720};
    bool sdlInitialized_{false};
    bool initialized_{false};
};
