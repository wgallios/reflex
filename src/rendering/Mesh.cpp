#include "rendering/Mesh.hpp"

#include <cstddef>
#include <iostream>
#include <limits>
#include <utility>

namespace {
void clearGlErrors() {
    while (glGetError() != GL_NO_ERROR) {
    }
}
} // namespace

Mesh::~Mesh() {
    reset();
}

Mesh::Mesh(Mesh&& other) noexcept
    : vertexArray_(std::exchange(other.vertexArray_, 0)),
      vertexBuffer_(std::exchange(other.vertexBuffer_, 0)),
      indexBuffer_(std::exchange(other.indexBuffer_, 0)),
      indexCount_(std::exchange(other.indexCount_, 0)) {
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        reset();
        vertexArray_ = std::exchange(other.vertexArray_, 0);
        vertexBuffer_ = std::exchange(other.vertexBuffer_, 0);
        indexBuffer_ = std::exchange(other.indexBuffer_, 0);
        indexCount_ = std::exchange(other.indexCount_, 0);
    }
    return *this;
}

bool Mesh::upload(const std::span<const Vertex> vertices,
                  const std::span<const std::uint32_t> indices) {
    if (vertices.empty() || indices.empty() ||
        indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        std::cerr << "Cannot upload an empty or excessively large mesh primitive.\n";
        return false;
    }

    GLuint vertexArray = 0;
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    clearGlErrors();
    glGenVertexArrays(1, &vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glGenBuffers(1, &indexBuffer);

    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size_bytes()),
                 vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size_bytes()),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, texCoord)));
    glBindVertexArray(0);

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL mesh upload failed with error 0x" << std::hex << error
                  << std::dec << ".\n";
        glDeleteBuffers(1, &indexBuffer);
        glDeleteBuffers(1, &vertexBuffer);
        glDeleteVertexArrays(1, &vertexArray);
        return false;
    }

    reset();
    vertexArray_ = vertexArray;
    vertexBuffer_ = vertexBuffer;
    indexBuffer_ = indexBuffer;
    indexCount_ = static_cast<GLsizei>(indices.size());
    return true;
}

void Mesh::draw() const noexcept {
    glBindVertexArray(vertexArray_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}

void Mesh::reset() noexcept {
    if (indexBuffer_ != 0) {
        glDeleteBuffers(1, &indexBuffer_);
    }
    if (vertexBuffer_ != 0) {
        glDeleteBuffers(1, &vertexBuffer_);
    }
    if (vertexArray_ != 0) {
        glDeleteVertexArrays(1, &vertexArray_);
    }
    vertexArray_ = 0;
    vertexBuffer_ = 0;
    indexBuffer_ = 0;
    indexCount_ = 0;
}
