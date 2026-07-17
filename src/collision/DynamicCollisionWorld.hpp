#pragma once

#include "collision/CollisionWorld.hpp"
#include "gameplay/GameplayTypes.hpp"

#include <vector>

struct DynamicCollider {
    EntityId owner{0};
    AABB bounds{};
    bool enabled{true};
};

class DynamicCollisionWorld {
public:
    void clear() noexcept;
    void upsert(EntityId owner, const AABB& bounds, bool enabled);
    [[nodiscard]] bool sweepCapsule(const Capsule& capsule, const glm::vec3& displacement,
                                    CollisionSweepHit& hit) const noexcept;
    [[nodiscard]] bool overlapCapsule(const Capsule& capsule, glm::vec3* normal = nullptr,
                                      float* depth = nullptr,
                                      EntityId ignoredOwner = 0) const noexcept;
    [[nodiscard]] bool raycast(const glm::vec3& origin, const glm::vec3& direction,
                               float maximumDistance, RayHit& hit,
                               EntityId ignoredOwner = 0) const noexcept;
    [[nodiscard]] const std::vector<DynamicCollider>& colliders() const noexcept;

private:
    std::vector<DynamicCollider> colliders_;
};

[[nodiscard]] bool capsuleOverlapsAabb(const Capsule& capsule, const AABB& box,
                                       glm::vec3* normal = nullptr,
                                       float* depth = nullptr) noexcept;

