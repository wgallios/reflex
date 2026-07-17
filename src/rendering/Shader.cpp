#include "rendering/Shader.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
bool readTextFile(const std::filesystem::path& path, std::string& contents) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Shader file not found: " << path << '\n';
        return false;
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    contents = stream.str();
    return true;
}

GLuint compileShader(const GLenum type, const std::string& source,
                     const std::filesystem::path& path) {
    const GLuint shader = glCreateShader(type);
    const char* sourcePointer = source.c_str();
    glShaderSource(shader, 1, &sourcePointer, nullptr);
    glCompileShader(shader);

    GLint succeeded = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &succeeded);
    if (succeeded == GL_TRUE) {
        return shader;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(static_cast<std::size_t>(std::max(logLength, 1)));
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    std::cerr << "Failed to compile shader " << path << ":\n" << log.data() << '\n';
    glDeleteShader(shader);
    return 0;
}
} // namespace

Shader::~Shader() {
    reset();
}

Shader::Shader(Shader&& other) noexcept : program_(std::exchange(other.program_, 0)) {
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        reset();
        program_ = std::exchange(other.program_, 0);
    }
    return *this;
}

bool Shader::load(const std::filesystem::path& vertexPath,
                  const std::filesystem::path& fragmentPath) {
    std::string vertexSource;
    std::string fragmentSource;
    if (!readTextFile(vertexPath, vertexSource) ||
        !readTextFile(fragmentPath, fragmentSource)) {
        return false;
    }

    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, vertexPath);
    if (vertexShader == 0) {
        return false;
    }

    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    const GLuint newProgram = glCreateProgram();
    glAttachShader(newProgram, vertexShader);
    glAttachShader(newProgram, fragmentShader);
    glLinkProgram(newProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint succeeded = GL_FALSE;
    glGetProgramiv(newProgram, GL_LINK_STATUS, &succeeded);
    if (succeeded != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramiv(newProgram, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<std::size_t>(std::max(logLength, 1)));
        glGetProgramInfoLog(newProgram, logLength, nullptr, log.data());
        std::cerr << "Failed to link shader program (" << vertexPath << ", "
                  << fragmentPath << "):\n" << log.data() << '\n';
        glDeleteProgram(newProgram);
        return false;
    }

    reset();
    program_ = newProgram;
    return true;
}

void Shader::use() const noexcept {
    glUseProgram(program_);
}

void Shader::setMat4(const std::string_view name, const glm::mat4& value) const {
    glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec3(const std::string_view name, const glm::vec3& value) const {
    glUniform3fv(uniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setInt(const std::string_view name, const int value) const {
    glUniform1i(uniformLocation(name), value);
}

void Shader::setFloat(const std::string_view name, const float value) const {
    glUniform1f(uniformLocation(name), value);
}

void Shader::reset() noexcept {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

GLint Shader::uniformLocation(const std::string_view name) const {
    const std::string nullTerminatedName{name};
    return glGetUniformLocation(program_, nullTerminatedName.c_str());
}
