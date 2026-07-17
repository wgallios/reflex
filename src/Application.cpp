#include "Application.hpp"
#include "persistence/SaveGame.hpp"

#include <glad/gl.h>
#include <glm/vec4.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <string>

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

    if (!window_.initialize("Reflex Engine - Phase 4", initialWindowWidth,
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
    if (!hudRenderer_.initialize(shaderDirectory)) {
        std::cerr << "Failed to initialize the gameplay HUD.\n";
        return false;
    }

    const std::filesystem::path resolvedScenePath = resolveAssetPath(scenePath);
    if (!gltfLoader_.loadGlb(resolvedScenePath, scene_)) {
        std::cerr << "Scene initialization failed; Reflex Engine will exit.\n";
        return false;
    }
    scenePath_ = resolvedScenePath;
    const RespawnPoint fallback{0, scene_.playerSpawnPosition,
                                scene_.playerSpawnYawDegrees, 100, 0};
    if (!gameplay_.initialize(scene_.gameplayEntities, scene_, dynamicCollision_, fallback)) {
        std::cerr << "Failed to initialize the gameplay world.\n";
        return false;
    }
    camera_.setYawDegrees(scene_.playerSpawnYawDegrees);
    if (!player_.initialize(scene_.collisionWorld, scene_.playerSpawnPosition, {},
                            &dynamicCollision_)) {
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
    hudRenderer_ = HudRenderer{};
    gameplay_ = GameplayWorld{};
    dynamicCollision_.clear();
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
    if (input_.wasPressed(SDL_SCANCODE_F4)) {
        gameplayDiagnosticsVisible_ = !gameplayDiagnosticsVisible_;
        std::cout << "Gameplay diagnostics: "
                  << (gameplayDiagnosticsVisible_ ? "on" : "off") << '\n';
    }
    if (input_.wasPressed(SDL_SCANCODE_F5)) quickSave();
    if (input_.wasPressed(SDL_SCANCODE_F9)) quickLoad();
    if (input_.wasPressed(SDL_SCANCODE_F6)) resetLevel();
    pendingJump_ = pendingJump_ ||
        (gameplay_.vitals().alive && input_.wasPressed(SDL_SCANCODE_SPACE));
    player_.beginDiagnosticsFrame();
    simulationAccumulator_ = std::min(simulationAccumulator_ + deltaTimeSeconds,
                                      maximumAccumulatedTimeSeconds);

    bool consumedJump = false;
    while (simulationAccumulator_ >= fixedDeltaTimeSeconds) {
        PlayerInput playerInput;
        if (gameplay_.vitals().alive) {
            playerInput.movement.x = (input_.isDown(SDL_SCANCODE_D) ? 1.0F : 0.0F) -
                                     (input_.isDown(SDL_SCANCODE_A) ? 1.0F : 0.0F);
            playerInput.movement.y = (input_.isDown(SDL_SCANCODE_W) ? 1.0F : 0.0F) -
                                     (input_.isDown(SDL_SCANCODE_S) ? 1.0F : 0.0F);
        }
        playerInput.verticalMovement = (input_.isDown(SDL_SCANCODE_SPACE) ? 1.0F : 0.0F) -
                                       (input_.isDown(SDL_SCANCODE_LCTRL) ? 1.0F : 0.0F);
        playerInput.sprint = input_.isDown(SDL_SCANCODE_LSHIFT);
        playerInput.jumpPressed = pendingJump_ && !consumedJump;
        if (gameplay_.vitals().alive) {
            player_.simulate(static_cast<float>(fixedDeltaTimeSeconds), playerInput,
                             camera_.forward(), camera_.right());
        }
        gameplay_.fixedUpdate(static_cast<float>(fixedDeltaTimeSeconds), player_.capsule(),
                              player_.position(), camera_.yawDegrees());
        if (!gameplay_.vitals().alive) {
            deathTimer_ += static_cast<float>(fixedDeltaTimeSeconds);
            if (deathTimer_ >= 2.0F) respawnPlayer();
        }
        consumedJump = consumedJump || playerInput.jumpPressed;
        simulationAccumulator_ -= fixedDeltaTimeSeconds;
    }
    if (consumedJump) {
        pendingJump_ = false;
    }
    camera_.setPosition(player_.cameraPosition());
    gameplay_.updatePresentation(deltaTimeSeconds);
    gameplay_.updateInteraction(camera_.position(), camera_.forward(), scene_.collisionWorld);
    if (input_.wasPressed(SDL_SCANCODE_E) && gameplay_.vitals().alive) gameplay_.interact();

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
        framebufferWidth_ = framebufferWidth;
        framebufferHeight_ = framebufferHeight;
        if (framebufferHeight > 0) {
            camera_.setAspectRatio(static_cast<float>(framebufferWidth) /
                                   static_cast<float>(framebufferHeight));
        }
    }

    constexpr glm::vec4 clearColor{0.055F, 0.075F, 0.11F, 1.0F};
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer_.render(scene_, camera_);
    if (collisionDiagnosticsVisible_ || gameplayDiagnosticsVisible_) debugDraw_.clear();
    if (collisionDiagnosticsVisible_) {
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
    }
    if (gameplayDiagnosticsVisible_) {
        for (const GameplayEntity& entity : gameplay_.entities()) {
            const glm::vec3 center{entity.runtimeWorldTransform *
                glm::vec4{entity.authored.boxOffset, 1.0F}};
            const glm::vec3 half = entity.authored.boxSize * 0.5F;
            debugDraw_.box(AABB{center - half, center + half},
                           entity.authored.id == gameplay_.interactionTarget()
                               ? glm::vec3{1.0F, 1.0F, 0.1F}
                               : glm::vec3{0.2F, 0.8F, 1.0F});
        }
    }
    if (collisionDiagnosticsVisible_ || gameplayDiagnosticsVisible_) debugDraw_.render(camera_);

    hudRenderer_.begin(framebufferWidth_, framebufferHeight_);
    hudRenderer_.crosshair();
    const PlayerVitals& vitals = gameplay_.vitals();
    hudRenderer_.text(18.0F, static_cast<float>(framebufferHeight_) - 48.0F, 3.0F,
        "HEALTH " + std::to_string(vitals.health) + "  ARMOR " + std::to_string(vitals.armor));
    const std::vector<std::string> keys = gameplay_.inventory().sortedKeys();
    if (!keys.empty()) {
        std::string keyText = "KEYS";
        for (const std::string& key : keys) keyText += " " + key;
        hudRenderer_.text(18.0F, static_cast<float>(framebufferHeight_) - 75.0F,
                          2.0F, keyText, glm::vec3{0.4F, 0.75F, 1.0F});
    }
    const std::string prompt = gameplay_.interactionPrompt();
    if (!prompt.empty()) hudRenderer_.centeredText(
        static_cast<float>(framebufferHeight_) * 0.62F, 3.0F, prompt,
        glm::vec3{1.0F, 0.9F, 0.35F});
    if (!gameplay_.message().text.empty()) hudRenderer_.centeredText(
        36.0F, 3.0F, gameplay_.message().text, glm::vec3{1.0F, 0.85F, 0.3F});
    if (gameplayDiagnosticsVisible_) {
        hudRenderer_.text(18.0F, 18.0F, 2.0F,
            "F4 GAMEPLAY  ENTITIES " + std::to_string(gameplay_.entities().size()) +
            "  EVENTS " + std::to_string(gameplay_.pendingEventCount()) +
            "  MODE " + (player_.isNoclip() ? std::string{"NOCLIP"} : std::string{"FPS"}));
        const std::string description = gameplay_.debugDescription(gameplay_.interactionTarget());
        if (!description.empty()) hudRenderer_.text(18.0F, 38.0F, 2.0F, description);
    }
    hudRenderer_.render();
}

void Application::quickSave() {
    std::string error;
    const SaveGameData data = SaveGame::capture(scenePath_.string(), gameplay_,
        player_.position(), camera_.yawDegrees(), camera_.pitchDegrees());
    if (SaveGame::write("saves/quicksave.json", data, error)) {
        std::cout << "Quick save written to saves/quicksave.json\n";
        gameplay_.showMessage("Game saved", 2.0F, 3);
    } else {
        std::cerr << "Quick save failed: " << error << '\n';
        gameplay_.showMessage("Save failed", 3.0F, 5);
    }
}

void Application::quickLoad() {
    std::string error;
    const std::optional<SaveGameData> data = SaveGame::read("saves/quicksave.json", error);
    if (!data) {
        std::cerr << "Quick load failed: " << error << '\n';
        gameplay_.showMessage("Load failed", 3.0F, 5);
        return;
    }
    if (std::filesystem::path{data->levelPath} != scenePath_) {
        std::cerr << "Quick load failed: save belongs to a different level.\n";
        gameplay_.showMessage("Save is for another level", 3.0F, 5);
        return;
    }
    const SaveGameData backup = SaveGame::capture(scenePath_.string(), gameplay_,
        player_.position(), camera_.yawDegrees(), camera_.pitchDegrees());
    if (!SaveGame::apply(*data, gameplay_, error) ||
        !player_.setPosition(data->playerPosition, true)) {
        std::string restoreError;
        static_cast<void>(SaveGame::apply(backup, gameplay_, restoreError));
        static_cast<void>(player_.setPosition(backup.playerPosition, true));
        std::cerr << "Quick load failed: "
                  << (error.empty() ? "invalid player position" : error) << '\n';
        gameplay_.showMessage("Load failed", 3.0F, 5);
        return;
    }
    camera_.setYawDegrees(data->playerYaw);
    camera_.setPitchDegrees(data->playerPitch);
    deathTimer_ = 0.0F;
    gameplay_.showMessage("Game loaded", 2.0F, 3);
    std::cout << "Quick save loaded from saves/quicksave.json\n";
}

void Application::resetLevel() {
    gameplay_.reset();
    static_cast<void>(player_.setPosition(scene_.playerSpawnPosition, true));
    camera_.setYawDegrees(scene_.playerSpawnYawDegrees);
    camera_.setPitchDegrees(0.0F);
    deathTimer_ = 0.0F;
}

void Application::respawnPlayer() {
    const RespawnPoint checkpoint = gameplay_.checkpoint();
    gameplay_.vitals().reset(checkpoint.health, checkpoint.armor);
    if (!player_.setPosition(checkpoint.position, true)) {
        static_cast<void>(player_.setPosition(scene_.playerSpawnPosition, true));
    }
    camera_.setYawDegrees(checkpoint.yawDegrees);
    camera_.setPitchDegrees(0.0F);
    deathTimer_ = 0.0F;
    gameplay_.showMessage("Respawned", 2.0F, 6);
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
