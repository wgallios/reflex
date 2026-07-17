#pragma once

#include "rendering/Material.hpp"
#include "rendering/Mesh.hpp"
#include "rendering/Texture.hpp"

#include <glm/mat4x4.hpp>

#include <cstddef>
#include <vector>

struct ScenePrimitive {
    std::size_t mesh{0};
    int material{-1};
    glm::mat4 worldTransform{1.0F};
};

class Scene {
public:
    std::vector<Mesh> meshes;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<ScenePrimitive> primitives;
};
