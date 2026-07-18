#pragma once

#include "rendering/Material.hpp"
#include "rendering/Mesh.hpp"
#include "rendering/Texture.hpp"
#include "rendering/SkinnedMesh.hpp"
#include "animation/Animation.hpp"
#include "collision/CollisionWorld.hpp"
#include "gameplay/GameplayTypes.hpp"

#include <glm/mat4x4.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct ScenePrimitive {
    std::size_t mesh{0};
    int material{-1};
    glm::mat4 worldTransform{1.0F};
    bool visible{true};
};

struct SkeletalAsset {
    reflex::animation::Skeleton skeleton;
    std::vector<reflex::animation::AnimationClip> clips;
};

struct SkinnedScenePrimitive {
    std::size_t mesh{0};
    int material{-1};
    std::size_t skeleton{0};
    glm::mat4 worldTransform{1.0F};
    std::size_t clip{0};
    float time{0.0F};
    bool looping{true};
    std::string animationState;
    std::vector<reflex::animation::Transform> pose;
    std::vector<glm::mat4> jointWorld;
    std::vector<glm::mat4> skinMatrices;
    bool visible{true};
};

class Scene {
public:
    std::vector<Mesh> meshes;
    std::vector<SkinnedMesh> skinnedMeshes;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<ScenePrimitive> primitives;
    std::vector<SkeletalAsset> skeletalAssets;
    std::vector<SkinnedScenePrimitive> skinnedPrimitives;
    std::vector<GameplayEntityDefinition> gameplayEntities;
    CollisionWorld collisionWorld;
    glm::vec3 playerSpawnPosition{0.0F, 1.0F, 5.0F};
    float playerSpawnYawDegrees{-90.0F};
    bool hasPlayerSpawn{false};

    void updateAnimations(float deltaTime);
    void setAnimation(std::span<const std::size_t> primitives, std::string_view clip,
                      bool looping, bool restart = false);
};
