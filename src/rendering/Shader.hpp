#pragma once

#include <glad/gl.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <filesystem>
#include <string_view>
#include <span>

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    [[nodiscard]] bool load(const std::filesystem::path& vertexPath,
                            const std::filesystem::path& fragmentPath);
    void use() const noexcept;

    void setMat4(std::string_view name, const glm::mat4& value) const;
    void setVec3(std::string_view name, const glm::vec3& value) const;
    void setInt(std::string_view name, int value) const;
    void setFloat(std::string_view name, float value) const;
    void setMat4Array(std::string_view name, std::span<const glm::mat4> values) const;

private:
    void reset() noexcept;
    [[nodiscard]] GLint uniformLocation(std::string_view name) const;

    GLuint program_{0};
};
