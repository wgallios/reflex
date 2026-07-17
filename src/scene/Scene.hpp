#pragma once

#include "rendering/Material.hpp"
#include "rendering/Mesh.hpp"
#include "rendering/Texture.hpp"
#include "collision/CollisionWorld.hpp"
#include "gameplay/GameplayTypes.hpp"

#include <glm/mat4x4.hpp>

#include <cstddef>
#include <vector>

struct ScenePrimitive {
    std::size_t mesh{0};
    int material{-1};
    glm::mat4 worldTransform{1.0F};
    bool visible{true};
};

class Scene {
public:
    std::vector<Mesh> meshes;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<ScenePrimitive> primitives;
    std::vector<GameplayEntityDefinition> gameplayEntities;
    CollisionWorld collisionWorld;
    glm::vec3 playerSpawnPosition{0.0F, 1.0F, 5.0F};
    float playerSpawnYawDegrees{-90.0F};
    bool hasPlayerSpawn{false};
};
