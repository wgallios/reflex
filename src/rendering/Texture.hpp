#pragma once

#include <glad/gl.h>

#include <cstdint>
#include <span>

struct TextureSampler {
    GLint wrapS{GL_REPEAT};
    GLint wrapT{GL_REPEAT};
    GLint minFilter{GL_LINEAR_MIPMAP_LINEAR};
    GLint magFilter{GL_LINEAR};
};

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    [[nodiscard]] bool upload(int width, int height, int channels,
                              std::span<const std::uint8_t> pixels,
                              const TextureSampler& sampler = {});
    void bind(unsigned int unit = 0) const noexcept;

private:
    void reset() noexcept;
    GLuint texture_{0};
};
