#pragma once

#include "rendering/Shader.hpp"

#include <glm/vec3.hpp>

#include <filesystem>
#include <string_view>
#include <vector>

class HudRenderer {
public:
    HudRenderer() = default;
    ~HudRenderer();
    HudRenderer(const HudRenderer&) = delete;
    HudRenderer& operator=(const HudRenderer&) = delete;
    HudRenderer(HudRenderer&& other) noexcept;
    HudRenderer& operator=(HudRenderer&& other) noexcept;

    [[nodiscard]] bool initialize(const std::filesystem::path& shaderDirectory);
    void begin(int framebufferWidth, int framebufferHeight) noexcept;
    void text(float x, float y, float pixelSize, std::string_view value,
              const glm::vec3& color = glm::vec3{1.0F});
    void centeredText(float y, float pixelSize, std::string_view value,
                      const glm::vec3& color = glm::vec3{1.0F});
    void crosshair(const glm::vec3& color = glm::vec3{1.0F});
    void weaponPlaceholder(float recoil, bool reloading, bool muzzleFlash);
    void render();

private:
    struct Vertex { glm::vec2 position{}; glm::vec3 color{}; };
    void rectangle(float x, float y, float width, float height, const glm::vec3& color);
    void release() noexcept;

    Shader shader_;
    unsigned int vertexArray_{0};
    unsigned int vertexBuffer_{0};
    int width_{1};
    int height_{1};
    std::vector<Vertex> vertices_;
};
