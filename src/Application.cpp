#include "Application.hpp"

#include <glad/gl.h>
#include <glm/vec4.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>

namespace {
constexpr int initialWindowWidth = 1280;
constexpr int initialWindowHeight = 720;
constexpr float maximumDeltaTimeSeconds = 0.25F;
constexpr double fixedDeltaTimeSeconds = 1.0 / 120.0;
constexpr double maximumAccumulatedTimeSeconds = 0.25;

const char* glString(const GLenum name) {
    const auto* value = glGetString(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : "unavailable";
}
} // namespace

Application::~Application() {
    shutdown();
}

bool Application::initialize(const std::filesystem::path& scenePath) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialize SDL video: " << SDL_GetError() << '\n';
        return false;
    }
    sdlInitialized_ = true;
    std::cout << "SDL video initialized successfully.\n";

    if (!window_.initialize("Reflex Engine - Phase 3", initialWindowWidth,
                            initialWindowHeight)) {
        return false;
    }

    const int loadedVersion = gladLoadGL([](const char* functionName) -> GLADapiproc {
        return reinterpret_cast<GLADapiproc>(SDL_GL_GetProcAddress(functionName));
    });
    if (loadedVersion == 0) {
        std::cerr << "Failed to load OpenGL functions through GLAD.\n";
        return false;
    }

    if (!GLAD_GL_VERSION_3_3) {
        std::cerr << "OpenGL 3.3 Core is not supported by this system.\n";
        return false;
    }

    std::cout << "OpenGL vendor:   " << glString(GL_VENDOR) << '\n'
              << "OpenGL renderer: " << glString(GL_RENDERER) << '\n'
              << "OpenGL version:  " << glString(GL_VERSION) << '\n';

    const std::filesystem::path shaderDirectory = resolveAssetPath("assets/shaders");
    if (!renderer_.initialize(shaderDirectory)) {
        std::cerr << "Failed to initialize the static scene renderer.\n";
        return false;
    }
    if (!debugDraw_.initialize(shaderDirectory)) {
        std::cerr << "Failed to initialize collision debug lines.\n";
        return false;
    }

    const std::filesystem::path resolvedScenePath = resolveAssetPath(scenePath);
    if (!gltfLoader_.loadGlb(resolvedScenePath, scene_)) {
        std::cerr << "Scene initialization failed; Reflex Engine will exit.\n";
        return false;
    }
    camera_.setYawDegrees(scene_.playerSpawnYawDegrees);
    if (!player_.initialize(scene_.collisionWorld, scene_.playerSpawnPosition)) {
        std::cerr << "Failed to initialize the player controller.\n";
        return false;
    }
    camera_.setPosition(player_.cameraPosition());

    initialized_ = true;
    return true;
}

void Application::run() {
    if (!initialized_) {
        return;
    }

    using Clock = std::chrono::steady_clock;
    auto previousFrameTime = Clock::now();

    while (true) {
        if (!window_.processEvents(input_)) {
            break;
        }

        const auto currentFrameTime = Clock::now();
        const float elapsedSeconds =
            std::chrono::duration<float>(currentFrameTime - previousFrameTime).count();
        previousFrameTime = currentFrameTime;
        const float deltaTimeSeconds =
            std::clamp(elapsedSeconds, 0.0F, maximumDeltaTimeSeconds);

        update(deltaTimeSeconds);
        render();
        window_.swapBuffers();
    }
}

void Application::shutdown() noexcept {
    initialized_ = false;
    renderer_ = Renderer{};
    debugDraw_ = DebugDraw{};
    scene_ = Scene{};
    window_.shutdown();

    if (sdlInitialized_) {
        SDL_Quit();
        sdlInitialized_ = false;
    }
}

void Application::update(const float deltaTimeSeconds) {
    camera_.updateLook(input_, window_.isMouseCaptured());
    if (input_.wasPressed(SDL_SCANCODE_N)) {
        static_cast<void>(player_.toggleNoclip());
    }
    if (input_.wasPressed(SDL_SCANCODE_F3)) {
        collisionDiagnosticsVisible_ = !collisionDiagnosticsVisible_;
        std::cout << "Collision diagnostics: "
                  << (collisionDiagnosticsVisible_ ? "on" : "off") << '\n';
    }
    pendingJump_ = pendingJump_ || input_.wasPressed(SDL_SCANCODE_SPACE);
    player_.beginDiagnosticsFrame();
    simulationAccumulator_ = std::min(simulationAccumulator_ + deltaTimeSeconds,
                                      maximumAccumulatedTimeSeconds);

    bool consumedJump = false;
    while (simulationAccumulator_ >= fixedDeltaTimeSeconds) {
        PlayerInput playerInput;
        playerInput.movement.x = (input_.isDown(SDL_SCANCODE_D) ? 1.0F : 0.0F) -
                                 (input_.isDown(SDL_SCANCODE_A) ? 1.0F : 0.0F);
        playerInput.movement.y = (input_.isDown(SDL_SCANCODE_W) ? 1.0F : 0.0F) -
                                 (input_.isDown(SDL_SCANCODE_S) ? 1.0F : 0.0F);
        playerInput.verticalMovement = (input_.isDown(SDL_SCANCODE_SPACE) ? 1.0F : 0.0F) -
                                       (input_.isDown(SDL_SCANCODE_LCTRL) ? 1.0F : 0.0F);
        playerInput.sprint = input_.isDown(SDL_SCANCODE_LSHIFT);
        playerInput.jumpPressed = pendingJump_ && !consumedJump;
        player_.simulate(static_cast<float>(fixedDeltaTimeSeconds), playerInput,
                         camera_.forward(), camera_.right());
        consumedJump = consumedJump || playerInput.jumpPressed;
        simulationAccumulator_ -= fixedDeltaTimeSeconds;
    }
    if (consumedJump) {
        pendingJump_ = false;
    }
    camera_.setPosition(player_.cameraPosition());

    if (collisionDiagnosticsVisible_) {
        diagnosticLogAccumulator_ += deltaTimeSeconds;
        if (diagnosticLogAccumulator_ >= 0.5F) {
            diagnosticLogAccumulator_ = 0.0F;
            const PlayerDiagnostics& diagnostics = player_.diagnostics();
            const glm::vec3& position = player_.position();
            const glm::vec3& velocity = player_.velocity();
            std::cout << "[collision] mode=" << (player_.isNoclip() ? "noclip" : "fps")
                      << " grounded=" << (player_.isGrounded() ? "yes" : "no")
                      << " pos=(" << position.x << ',' << position.y << ',' << position.z << ')'
                      << " vel=(" << velocity.x << ',' << velocity.y << ',' << velocity.z << ')'
                      << " candidates=" << diagnostics.collision.candidateTriangles
                      << " tests=" << diagnostics.collision.narrowPhaseTests
                      << " contacts=" << diagnostics.collision.contacts
                      << " slides=" << diagnostics.slideIterations
                      << " steps=" << diagnostics.stepSuccesses << '/'
                      << diagnostics.stepAttempts << '\n';
        }
    }
}

void Application::render() {
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    if (window_.takeFramebufferResize(framebufferWidth, framebufferHeight)) {
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        if (framebufferHeight > 0) {
            camera_.setAspectRatio(static_cast<float>(framebufferWidth) /
                                   static_cast<float>(framebufferHeight));
        }
    }

    constexpr glm::vec4 clearColor{0.055F, 0.075F, 0.11F, 1.0F};
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer_.render(scene_, camera_);
    if (collisionDiagnosticsVisible_) {
        debugDraw_.clear();
        debugDraw_.capsule(player_.capsule(), player_.isGrounded()
            ? glm::vec3{0.2F, 1.0F, 0.25F} : glm::vec3{1.0F, 0.75F, 0.15F});
        const Capsule shape = player_.capsule();
        debugDraw_.line(shape.position,
                        shape.position - glm::vec3{0.0F,
                            player_.settings().groundSnapDistance, 0.0F},
                        glm::vec3{0.2F, 0.7F, 1.0F});
        const PlayerDiagnostics& diagnostics = player_.diagnostics();
        for (std::size_t i = 0; i < diagnostics.contactCount; ++i) {
            debugDraw_.line(diagnostics.contactPoints[i],
                            diagnostics.contactPoints[i] + diagnostics.contactNormals[i] * 0.3F,
                            glm::vec3{1.0F, 0.15F, 0.15F});
        }
        debugDraw_.render(camera_);
    }
}

std::filesystem::path Application::resolveAssetPath(
    const std::filesystem::path& path) const {
    if (std::filesystem::exists(path)) {
        return path;
    }

    const char* basePath = SDL_GetBasePath();
    if (basePath != nullptr) {
        const std::filesystem::path besideExecutable =
            std::filesystem::path{basePath} / path;
        if (std::filesystem::exists(besideExecutable)) {
            return besideExecutable;
        }
    }
    return path;
}
