#pragma once

#include "rendering/Shader.hpp"

#include <glm/vec3.hpp>

#include <filesystem>
#include <vector>

class Camera;
struct Capsule;

class DebugDraw {
public:
    DebugDraw() = default;
    ~DebugDraw();
    DebugDraw(const DebugDraw&) = delete;
    DebugDraw& operator=(const DebugDraw&) = delete;
    DebugDraw(DebugDraw&& other) noexcept;
    DebugDraw& operator=(DebugDraw&& other) noexcept;

    [[nodiscard]] bool initialize(const std::filesystem::path& shaderDirectory);
    void clear() noexcept;
    void line(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color);
    void circle(const glm::vec3& center, float radius, int axis,
                const glm::vec3& color, int segments = 20);
    void capsule(const Capsule& capsule, const glm::vec3& color);
    void render(const Camera& camera);

private:
    struct Vertex {
        glm::vec3 position{};
        glm::vec3 color{};
    };
    void release() noexcept;

    Shader shader_;
    unsigned int vertexArray_{0};
    unsigned int vertexBuffer_{0};
    std::vector<Vertex> vertices_;
};

