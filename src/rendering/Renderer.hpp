#pragma once

#include "rendering/Shader.hpp"

#include <filesystem>

class Camera;
class Scene;

class Renderer {
public:
    [[nodiscard]] bool initialize(const std::filesystem::path& shaderDirectory);
    void render(const Scene& scene, const Camera& camera);

private:
    Shader staticMeshShader_;
    Shader skinnedMeshShader_;
};
