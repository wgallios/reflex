#include "debug/DebugDraw.hpp"

#include "collision/CollisionWorld.hpp"
#include "scene/Camera.hpp"

#include <glad/gl.h>
#include <glm/trigonometric.hpp>
#include <glm/gtc/constants.hpp>

#include <cstddef>
#include <utility>

DebugDraw::~DebugDraw() { release(); }

DebugDraw::DebugDraw(DebugDraw&& other) noexcept
    : shader_(std::move(other.shader_)), vertexArray_(std::exchange(other.vertexArray_, 0)),
      vertexBuffer_(std::exchange(other.vertexBuffer_, 0)), vertices_(std::move(other.vertices_)) {}

DebugDraw& DebugDraw::operator=(DebugDraw&& other) noexcept {
    if (this != &other) {
        release();
        shader_ = std::move(other.shader_);
        vertexArray_ = std::exchange(other.vertexArray_, 0);
        vertexBuffer_ = std::exchange(other.vertexBuffer_, 0);
        vertices_ = std::move(other.vertices_);
    }
    return *this;
}

bool DebugDraw::initialize(const std::filesystem::path& shaderDirectory) {
    if (!shader_.load(shaderDirectory / "debug_line.vert", shaderDirectory / "debug_line.frag")) {
        return false;
    }
    glGenVertexArrays(1, &vertexArray_);
    glGenBuffers(1, &vertexBuffer_);
    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, color)));
    glBindVertexArray(0);
    vertices_.reserve(512);
    return vertexArray_ != 0 && vertexBuffer_ != 0;
}

void DebugDraw::clear() noexcept { vertices_.clear(); }

void DebugDraw::line(const glm::vec3& from, const glm::vec3& to,
                     const glm::vec3& color) {
    vertices_.push_back({from, color});
    vertices_.push_back({to, color});
}

void DebugDraw::circle(const glm::vec3& center, const float radius, const int axis,
                       const glm::vec3& color, const int segments) {
    for (int i = 0; i < segments; ++i) {
        const float angleA = glm::two_pi<float>() * static_cast<float>(i) /
                             static_cast<float>(segments);
        const float angleB = glm::two_pi<float>() * static_cast<float>(i + 1) /
                             static_cast<float>(segments);
        glm::vec3 a{};
        glm::vec3 b{};
        const int first = (axis + 1) % 3;
        const int second = (axis + 2) % 3;
        a[first] = glm::cos(angleA) * radius;
        a[second] = glm::sin(angleA) * radius;
        b[first] = glm::cos(angleB) * radius;
        b[second] = glm::sin(angleB) * radius;
        line(center + a, center + b, color);
    }
}

void DebugDraw::capsule(const Capsule& shape, const glm::vec3& color) {
    const glm::vec3 bottom = shape.bottomCenter();
    const glm::vec3 top = shape.topCenter();
    circle(bottom, shape.radius, 1, color);
    circle(top, shape.radius, 1, color);
    circle(bottom, shape.radius, 0, color);
    circle(top, shape.radius, 0, color);
    circle(bottom, shape.radius, 2, color);
    circle(top, shape.radius, 2, color);
    line(bottom + glm::vec3{shape.radius, 0.0F, 0.0F},
         top + glm::vec3{shape.radius, 0.0F, 0.0F}, color);
    line(bottom - glm::vec3{shape.radius, 0.0F, 0.0F},
         top - glm::vec3{shape.radius, 0.0F, 0.0F}, color);
    line(bottom + glm::vec3{0.0F, 0.0F, shape.radius},
         top + glm::vec3{0.0F, 0.0F, shape.radius}, color);
    line(bottom - glm::vec3{0.0F, 0.0F, shape.radius},
         top - glm::vec3{0.0F, 0.0F, shape.radius}, color);
}

void DebugDraw::box(const AABB& bounds, const glm::vec3& color) {
    const glm::vec3 a{bounds.minimum.x, bounds.minimum.y, bounds.minimum.z};
    const glm::vec3 b{bounds.maximum.x, bounds.minimum.y, bounds.minimum.z};
    const glm::vec3 c{bounds.maximum.x, bounds.minimum.y, bounds.maximum.z};
    const glm::vec3 d{bounds.minimum.x, bounds.minimum.y, bounds.maximum.z};
    const glm::vec3 e{bounds.minimum.x, bounds.maximum.y, bounds.minimum.z};
    const glm::vec3 f{bounds.maximum.x, bounds.maximum.y, bounds.minimum.z};
    const glm::vec3 g{bounds.maximum.x, bounds.maximum.y, bounds.maximum.z};
    const glm::vec3 h{bounds.minimum.x, bounds.maximum.y, bounds.maximum.z};
    line(a,b,color); line(b,c,color); line(c,d,color); line(d,a,color);
    line(e,f,color); line(f,g,color); line(g,h,color); line(h,e,color);
    line(a,e,color); line(b,f,color); line(c,g,color); line(d,h,color);
}

void DebugDraw::render(const Camera& camera) {
    if (vertices_.empty() || vertexArray_ == 0) {
        return;
    }
    shader_.use();
    shader_.setMat4("uView", camera.viewMatrix());
    shader_.setMat4("uProjection", camera.projectionMatrix());
    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
                 vertices_.data(), GL_STREAM_DRAW);
    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices_.size()));
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}

void DebugDraw::release() noexcept {
    if (vertexBuffer_ != 0) {
        glDeleteBuffers(1, &vertexBuffer_);
        vertexBuffer_ = 0;
    }
    if (vertexArray_ != 0) {
        glDeleteVertexArrays(1, &vertexArray_);
        vertexArray_ = 0;
    }
}
