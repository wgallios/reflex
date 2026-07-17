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

    if (!window_.initialize("Reflex Engine - Phase 5", initialWindowWidth,
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
    if (!combat_.initialize(resolveAssetPath("assets/combat/combat.json"), scene_, gameplay_,
                            scene_.collisionWorld, dynamicCollision_)) {
        std::cerr << "Failed to initialize combat systems.\n";
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
    combat_ = CombatSystem{};
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
    if (input_.wasPressed(SDL_SCANCODE_F7)) {
        combatDiagnosticsVisible_ = !combatDiagnosticsVisible_;
        std::cout << "Combat diagnostics: " << (combatDiagnosticsVisible_ ? "on" : "off") << '\n';
    }
    if (input_.wasPressed(SDL_SCANCODE_F5)) quickSave();
    if (input_.wasPressed(SDL_SCANCODE_F9)) quickLoad();
    if (input_.wasPressed(SDL_SCANCODE_F6)) resetLevel();
    pendingJump_ = pendingJump_ ||
        (gameplay_.vitals().alive && input_.wasPressed(SDL_SCANCODE_SPACE));
    pendingFire_ = pendingFire_ ||
        (window_.isMouseCaptured() && input_.mouseButtonPressed(SDL_BUTTON_LEFT));
    pendingReload_ = pendingReload_ || input_.wasPressed(SDL_SCANCODE_R);
    if (input_.wasPressed(SDL_SCANCODE_1)) pendingWeaponSlot_ = 0;
    if (input_.wasPressed(SDL_SCANCODE_2)) pendingWeaponSlot_ = 1;
    if (input_.wasPressed(SDL_SCANCODE_3)) pendingWeaponSlot_ = 2;
    if (input_.mouseWheelY() != 0.0F) pendingWeaponCycle_ = input_.mouseWheelY() > 0.0F ? -1 : 1;
    player_.beginDiagnosticsFrame();
    simulationAccumulator_ = std::min(simulationAccumulator_ + deltaTimeSeconds,
                                      maximumAccumulatedTimeSeconds);

    bool consumedJump = false;
    bool consumedCombatEdges = false;
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
        CombatInput combatInput;
        combatInput.firePressed = pendingFire_ && !consumedCombatEdges;
        combatInput.fireHeld = window_.isMouseCaptured() &&
            input_.mouseButtonDown(SDL_BUTTON_LEFT);
        combatInput.reloadPressed = pendingReload_ && !consumedCombatEdges;
        combatInput.selectSlot = !consumedCombatEdges ? pendingWeaponSlot_ : -1;
        combatInput.cycleDirection = !consumedCombatEdges ? pendingWeaponCycle_ : 0;
        combat_.fixedUpdate(static_cast<float>(fixedDeltaTimeSeconds), combatInput,
                            camera_.position(), camera_.forward(), player_.capsule(),
                            player_.position());
        consumedCombatEdges = true;
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
    if (consumedCombatEdges) {
        pendingFire_ = false;
        pendingReload_ = false;
        pendingWeaponSlot_ = -1;
        pendingWeaponCycle_ = 0;
    }
    camera_.setPosition(player_.cameraPosition());
    gameplay_.updatePresentation(deltaTimeSeconds);
    combat_.updatePresentation(deltaTimeSeconds);
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
    if (collisionDiagnosticsVisible_ || gameplayDiagnosticsVisible_ || combatDiagnosticsVisible_ ||
        !combat_.effects().empty() || !combat_.projectiles().empty()) debugDraw_.clear();
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
    for (const CombatLineEffect& effect : combat_.effects()) {
        debugDraw_.line(effect.from, effect.to, effect.color);
    }
    for (const Projectile& projectile : combat_.projectiles()) {
        debugDraw_.circle(projectile.position, projectile.radius, 0, {0.2F, 0.8F, 1.0F}, 12);
        debugDraw_.circle(projectile.position, projectile.radius, 1, {0.2F, 0.8F, 1.0F}, 12);
    }
    if (combatDiagnosticsVisible_) {
        for (const EnemyActor& enemy : combat_.enemies()) {
            const EnemyDefinition* definition = nullptr;
            static_cast<void>(definition);
            const glm::vec3 half{0.4F, 0.9F, 0.4F};
            debugDraw_.box({enemy.position - glm::vec3{half.x, 0.0F, half.z},
                            enemy.position + glm::vec3{half.x, half.y * 2.0F, half.z}},
                           enemy.state == EnemyState::Dead ? glm::vec3{0.3F} : glm::vec3{1.0F,0.1F,0.1F});
            debugDraw_.line(enemy.position + glm::vec3{0,1.5F,0},
                            enemy.position + glm::vec3{0,1.5F,0} + enemy.forward * 2.0F,
                            {1.0F,0.5F,0.1F});
        }
    }
    if (collisionDiagnosticsVisible_ || gameplayDiagnosticsVisible_ || combatDiagnosticsVisible_ ||
        !combat_.effects().empty() || !combat_.projectiles().empty()) debugDraw_.render(camera_);

    hudRenderer_.begin(framebufferWidth_, framebufferHeight_);
    const glm::vec3 crosshairColor = combat_.killMarkerVisible() ? glm::vec3{1.0F,0.2F,0.1F} :
        (combat_.hitMarkerVisible() ? glm::vec3{1.0F,0.85F,0.1F} : glm::vec3{1.0F});
    if (gameplay_.vitals().alive) hudRenderer_.crosshair(crosshairColor);
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
    if (const WeaponInstance* weapon = combat_.equippedWeapon()) {
        const WeaponDefinition* definition = combat_.equippedDefinition();
        const std::string label = (definition ? definition->displayName : weapon->definitionId) +
            "  " + std::to_string(weapon->magazine) + " / " + std::to_string(combat_.reserveAmmo());
        hudRenderer_.text(static_cast<float>(framebufferWidth_) -
            static_cast<float>(label.size()) * 18.0F - 18.0F,
            static_cast<float>(framebufferHeight_) - 48.0F, 3.0F, label,
            weapon->state == WeaponState::Reloading ? glm::vec3{1.0F,0.75F,0.2F} : glm::vec3{1.0F});
        if (gameplay_.vitals().alive) {
            hudRenderer_.weaponPlaceholder(combat_.muzzleFlashRemaining() > 0.0F ? 1.0F : 0.0F,
                                            weapon->state == WeaponState::Reloading,
                                            combat_.muzzleFlashRemaining() > 0.0F);
        }
    }
    if (combat_.damageIndicatorRemaining() > 0.0F) {
        const glm::vec3 sourceDirection = combat_.lastDamageDirection() - player_.position();
        const float side = glm::dot(sourceDirection, camera_.right());
        const float front = glm::dot(sourceDirection, camera_.forward());
        const std::string direction = std::abs(side) > std::abs(front)
            ? (side > 0.0F ? "DAMAGE RIGHT" : "DAMAGE LEFT")
            : (front > 0.0F ? "DAMAGE FRONT" : "DAMAGE REAR");
        hudRenderer_.centeredText(static_cast<float>(framebufferHeight_) * 0.18F, 3.0F,
                                  direction, {1.0F,0.15F,0.1F});
    }
    if (combatDiagnosticsVisible_) {
        hudRenderer_.text(18.0F, 58.0F, 2.0F, combat_.debugSummary(), {1.0F,0.45F,0.2F});
        float y = 78.0F;
        for (const EnemyActor& enemy : combat_.enemies()) {
            hudRenderer_.text(18.0F, y, 2.0F, enemy.name + " " + enemyStateName(enemy.state) +
                              " HP " + std::to_string(enemy.health));
            y += 18.0F;
        }
    }
    hudRenderer_.render();
}

void Application::quickSave() {
    std::string error;
    const SaveGameData data = SaveGame::capture(scenePath_.string(), gameplay_,
        player_.position(), camera_.yawDegrees(), camera_.pitchDegrees(), &combat_);
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
        player_.position(), camera_.yawDegrees(), camera_.pitchDegrees(), &combat_);
    if (!SaveGame::apply(*data, gameplay_, error, &combat_) ||
        !player_.setPosition(data->playerPosition, true)) {
        std::string restoreError;
        static_cast<void>(SaveGame::apply(backup, gameplay_, restoreError, &combat_));
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
    combat_.reset();
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
