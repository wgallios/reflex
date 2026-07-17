#include "rendering/Texture.hpp"

#include <iostream>
#include <utility>

namespace {
void clearGlErrors() {
    while (glGetError() != GL_NO_ERROR) {
    }
}
} // namespace

Texture::~Texture() {
    reset();
}

Texture::Texture(Texture&& other) noexcept : texture_(std::exchange(other.texture_, 0)) {
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        reset();
        texture_ = std::exchange(other.texture_, 0);
    }
    return *this;
}

bool Texture::upload(const int width, const int height, const int channels,
                     const std::span<const std::uint8_t> pixels,
                     const TextureSampler& sampler) {
    if (width <= 0 || height <= 0 || (channels != 3 && channels != 4)) {
        std::cerr << "Invalid texture dimensions or channel count (expected RGB or RGBA).\n";
        return false;
    }

    const auto requiredBytes = static_cast<std::size_t>(width) *
                               static_cast<std::size_t>(height) *
                               static_cast<std::size_t>(channels);
    if (pixels.size() < requiredBytes) {
        std::cerr << "Texture pixel buffer is smaller than its declared dimensions.\n";
        return false;
    }

    GLuint newTexture = 0;
    clearGlErrors();
    glGenTextures(1, &newTexture);
    glBindTexture(GL_TEXTURE_2D, newTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler.minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler.magFilter);

    const GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
    const GLint internalFormat = channels == 4 ? GL_SRGB8_ALPHA8 : GL_SRGB8;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format,
                 GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL texture upload failed with error 0x" << std::hex << error
                  << std::dec << ".\n";
        glDeleteTextures(1, &newTexture);
        return false;
    }

    reset();
    texture_ = newTexture;
    return true;
}

void Texture::bind(const unsigned int unit) const noexcept {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture_);
}

void Texture::reset() noexcept {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
}
