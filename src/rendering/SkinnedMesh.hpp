#pragma once

#include <glad/gl.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <span>

struct SkinnedVertex {
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 texCoord{};
    glm::uvec4 joints{};
    glm::vec4 weights{1.0F, 0.0F, 0.0F, 0.0F};
};

class SkinnedMesh {
public:
    SkinnedMesh() = default;
    ~SkinnedMesh();
    SkinnedMesh(const SkinnedMesh&) = delete;
    SkinnedMesh& operator=(const SkinnedMesh&) = delete;
    SkinnedMesh(SkinnedMesh&& other) noexcept;
    SkinnedMesh& operator=(SkinnedMesh&& other) noexcept;
    [[nodiscard]] bool upload(std::span<const SkinnedVertex> vertices,
                              std::span<const std::uint32_t> indices);
    void draw() const noexcept;
private:
    void reset() noexcept;
    GLuint vertexArray_{0};
    GLuint vertexBuffer_{0};
    GLuint indexBuffer_{0};
    GLsizei indexCount_{0};
};
