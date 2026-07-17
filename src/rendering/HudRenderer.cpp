#include "rendering/HudRenderer.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <utility>

namespace {
using Glyph = std::array<unsigned char, 7>;

Glyph glyph(const char input) {
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(input)));
    switch (c) {
    case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15}; case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14}; case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
    case '[': return {14,8,8,8,8,8,14}; case ']': return {14,2,2,2,2,2,14};
    case ':': return {0,4,4,0,4,4,0}; case '-': return {0,0,0,31,0,0,0};
    case '.': return {0,0,0,0,0,4,4}; case '/': return {1,2,2,4,8,8,16};
    case '_': return {0,0,0,0,0,0,31}; case '?': return {14,17,1,2,4,0,4};
    default: return {};
    }
}
} // namespace

HudRenderer::~HudRenderer() { release(); }
HudRenderer::HudRenderer(HudRenderer&& other) noexcept
    : shader_(std::move(other.shader_)), vertexArray_(std::exchange(other.vertexArray_, 0)),
      vertexBuffer_(std::exchange(other.vertexBuffer_, 0)), width_(other.width_),
      height_(other.height_), vertices_(std::move(other.vertices_)) {}
HudRenderer& HudRenderer::operator=(HudRenderer&& other) noexcept {
    if (this != &other) {
        release(); shader_ = std::move(other.shader_);
        vertexArray_ = std::exchange(other.vertexArray_, 0);
        vertexBuffer_ = std::exchange(other.vertexBuffer_, 0);
        width_ = other.width_; height_ = other.height_; vertices_ = std::move(other.vertices_);
    }
    return *this;
}

bool HudRenderer::initialize(const std::filesystem::path& shaderDirectory) {
    if (!shader_.load(shaderDirectory / "hud.vert", shaderDirectory / "hud.frag")) return false;
    glGenVertexArrays(1, &vertexArray_); glGenBuffers(1, &vertexBuffer_);
    glBindVertexArray(vertexArray_); glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, color)));
    glBindVertexArray(0); vertices_.reserve(4096);
    return vertexArray_ != 0 && vertexBuffer_ != 0;
}

void HudRenderer::begin(const int framebufferWidth, const int framebufferHeight) noexcept {
    width_ = std::max(1, framebufferWidth); height_ = std::max(1, framebufferHeight);
    vertices_.clear();
}

void HudRenderer::rectangle(const float x, const float y, const float width,
                            const float height, const glm::vec3& color) {
    const auto point = [this](const float px, const float py) {
        return glm::vec2{px / static_cast<float>(width_) * 2.0F - 1.0F,
                         1.0F - py / static_cast<float>(height_) * 2.0F};
    };
    const glm::vec2 a = point(x, y), b = point(x + width, y);
    const glm::vec2 c = point(x + width, y + height), d = point(x, y + height);
    vertices_.insert(vertices_.end(), {{a,color},{b,color},{c,color},{a,color},{c,color},{d,color}});
}

void HudRenderer::text(float x, const float y, const float pixelSize,
                       const std::string_view value, const glm::vec3& color) {
    for (const char character : value) {
        const Glyph rows = glyph(character);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[static_cast<std::size_t>(row)] & (1U << (4 - column))) != 0) {
                    rectangle(x + static_cast<float>(column) * pixelSize,
                              y + static_cast<float>(row) * pixelSize,
                              pixelSize, pixelSize, color);
                }
            }
        }
        x += pixelSize * 6.0F;
    }
}

void HudRenderer::centeredText(const float y, const float pixelSize,
                               const std::string_view value, const glm::vec3& color) {
    const float textWidth = static_cast<float>(value.size()) * pixelSize * 6.0F;
    text((static_cast<float>(width_) - textWidth) * 0.5F, y, pixelSize, value, color);
}

void HudRenderer::crosshair(const glm::vec3& color) {
    const float x = static_cast<float>(width_) * 0.5F;
    const float y = static_cast<float>(height_) * 0.5F;
    rectangle(x - 6.0F, y - 1.0F, 12.0F, 2.0F, color);
    rectangle(x - 1.0F, y - 6.0F, 2.0F, 12.0F, color);
}

void HudRenderer::render() {
    if (vertices_.empty()) return;
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); shader_.use();
    glBindVertexArray(vertexArray_); glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
                 vertices_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0); glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE);
}

void HudRenderer::release() noexcept {
    if (vertexBuffer_ != 0) glDeleteBuffers(1, &vertexBuffer_);
    if (vertexArray_ != 0) glDeleteVertexArrays(1, &vertexArray_);
    vertexBuffer_ = 0; vertexArray_ = 0;
}

