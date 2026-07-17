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

    if (!window_.initialize("Reflex Engine - Phase 2", initialWindowWidth,
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

    const std::filesystem::path resolvedScenePath = resolveAssetPath(scenePath);
    if (!gltfLoader_.loadGlb(resolvedScenePath, scene_)) {
        std::cerr << "Scene initialization failed; Reflex Engine will exit.\n";
        return false;
    }

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
    scene_ = Scene{};
    window_.shutdown();

    if (sdlInitialized_) {
        SDL_Quit();
        sdlInitialized_ = false;
    }
}

void Application::update(const float deltaTimeSeconds) {
    camera_.update(input_, deltaTimeSeconds, window_.isMouseCaptured());
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
