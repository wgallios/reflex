#include "rendering/SkinnedMesh.hpp"

#include <cstddef>
#include <iostream>
#include <limits>
#include <utility>

SkinnedMesh::~SkinnedMesh() { reset(); }
SkinnedMesh::SkinnedMesh(SkinnedMesh&& other) noexcept
    : vertexArray_(std::exchange(other.vertexArray_, 0)),
      vertexBuffer_(std::exchange(other.vertexBuffer_, 0)),
      indexBuffer_(std::exchange(other.indexBuffer_, 0)),
      indexCount_(std::exchange(other.indexCount_, 0)) {}
SkinnedMesh& SkinnedMesh::operator=(SkinnedMesh&& other) noexcept {
    if (this != &other) {
        reset(); vertexArray_ = std::exchange(other.vertexArray_, 0);
        vertexBuffer_ = std::exchange(other.vertexBuffer_, 0);
        indexBuffer_ = std::exchange(other.indexBuffer_, 0);
        indexCount_ = std::exchange(other.indexCount_, 0);
    }
    return *this;
}

bool SkinnedMesh::upload(const std::span<const SkinnedVertex> vertices,
                         const std::span<const std::uint32_t> indices) {
    if (vertices.empty() || indices.empty() ||
        indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) return false;
    GLuint vao = 0, vbo = 0, ebo = 0;
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ebo);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size_bytes()), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size_bytes()), indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, position)));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, normal)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, texCoord)));
    glEnableVertexAttribArray(3); glVertexAttribIPointer(3, 4, GL_UNSIGNED_INT, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, joints)));
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, weights)));
    glBindVertexArray(0);
    if (glGetError() != GL_NO_ERROR) {
        std::cerr << "OpenGL skinned-mesh upload failed.\n";
        glDeleteBuffers(1, &ebo); glDeleteBuffers(1, &vbo); glDeleteVertexArrays(1, &vao); return false;
    }
    reset(); vertexArray_ = vao; vertexBuffer_ = vbo; indexBuffer_ = ebo;
    indexCount_ = static_cast<GLsizei>(indices.size()); return true;
}
void SkinnedMesh::draw() const noexcept { glBindVertexArray(vertexArray_); glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr); }
void SkinnedMesh::reset() noexcept {
    if (indexBuffer_) glDeleteBuffers(1, &indexBuffer_);
    if (vertexBuffer_) glDeleteBuffers(1, &vertexBuffer_);
    if (vertexArray_) glDeleteVertexArrays(1, &vertexArray_);
    vertexArray_ = vertexBuffer_ = indexBuffer_ = 0; indexCount_ = 0;
}
