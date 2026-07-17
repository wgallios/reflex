#include "collision/DynamicCollisionWorld.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr float epsilon = 0.000001F;

bool rayAabb(const glm::vec3& origin, const glm::vec3& direction, const AABB& box,
             const float maximumDistance, float& distance, glm::vec3& normal) noexcept {
    float nearTime = 0.0F;
    float farTime = maximumDistance;
    glm::vec3 nearNormal{};
    if (origin.x >= box.minimum.x && origin.x <= box.maximum.x &&
        origin.y >= box.minimum.y && origin.y <= box.maximum.y &&
        origin.z >= box.minimum.z && origin.z <= box.maximum.z) {
        const std::array<float, 6> faces{
            origin.x - box.minimum.x, box.maximum.x - origin.x,
            origin.y - box.minimum.y, box.maximum.y - origin.y,
            origin.z - box.minimum.z, box.maximum.z - origin.z};
        const auto nearest = std::min_element(faces.begin(), faces.end());
        const int face = static_cast<int>(std::distance(faces.begin(), nearest));
        nearNormal[face / 2] = face % 2 == 0 ? -1.0F : 1.0F;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < epsilon) {
            if (origin[axis] < box.minimum[axis] || origin[axis] > box.maximum[axis]) {
                return false;
            }
            continue;
        }
        float first = (box.minimum[axis] - origin[axis]) / direction[axis];
        float second = (box.maximum[axis] - origin[axis]) / direction[axis];
        glm::vec3 axisNormal{};
        axisNormal[axis] = direction[axis] > 0.0F ? -1.0F : 1.0F;
        if (first > second) {
            std::swap(first, second);
            axisNormal = -axisNormal;
        }
        if (first > nearTime) {
            nearTime = first;
            nearNormal = axisNormal;
        }
        farTime = std::min(farTime, second);
        if (nearTime > farTime) {
            return false;
        }
    }
    distance = nearTime;
    normal = nearNormal;
    return nearTime <= maximumDistance && farTime >= 0.0F;
}
} // namespace

bool capsuleOverlapsAabb(const Capsule& capsule, const AABB& box, glm::vec3* normal,
                         float* depth) noexcept {
    const glm::vec3 bottom = capsule.bottomCenter();
    const glm::vec3 top = capsule.topCenter();
    const float segmentY = std::clamp(box.center().y, bottom.y, top.y);
    const glm::vec3 segmentPoint{capsule.position.x, segmentY, capsule.position.z};
    const glm::vec3 closest = glm::clamp(segmentPoint, box.minimum, box.maximum);
    const glm::vec3 delta = segmentPoint - closest;
    const float distanceSquared = glm::dot(delta, delta);
    if (distanceSquared > capsule.radius * capsule.radius) {
        return false;
    }
    const float distance = std::sqrt(std::max(distanceSquared, 0.0F));
    if (distance > epsilon) {
        if (normal != nullptr) *normal = delta / distance;
        if (depth != nullptr) *depth = capsule.radius - distance;
        return true;
    }
    const std::array<float, 6> distances{
        segmentPoint.x - box.minimum.x, box.maximum.x - segmentPoint.x,
        segmentPoint.y - box.minimum.y, box.maximum.y - segmentPoint.y,
        segmentPoint.z - box.minimum.z, box.maximum.z - segmentPoint.z};
    const auto nearest = std::min_element(distances.begin(), distances.end());
    const int face = static_cast<int>(std::distance(distances.begin(), nearest));
    glm::vec3 recovery{};
    recovery[face / 2] = face % 2 == 0 ? -1.0F : 1.0F;
    if (normal != nullptr) *normal = recovery;
    if (depth != nullptr) *depth = capsule.radius + *nearest;
    return true;
}

void DynamicCollisionWorld::clear() noexcept { colliders_.clear(); }

void DynamicCollisionWorld::upsert(const EntityId owner, const AABB& bounds,
                                   const bool enabled) {
    const auto found = std::find_if(colliders_.begin(), colliders_.end(),
        [owner](const DynamicCollider& collider) { return collider.owner == owner; });
    if (found != colliders_.end()) {
        found->bounds = bounds;
        found->enabled = enabled;
    } else {
        colliders_.push_back({owner, bounds, enabled});
    }
}

bool DynamicCollisionWorld::sweepCapsule(const Capsule& capsule,
                                         const glm::vec3& displacement,
                                         CollisionSweepHit& hit) const noexcept {
    const float length = glm::length(displacement);
    if (length < epsilon) return false;
    const glm::vec3 direction = displacement / length;
    bool found = false;
    float best = length;
    for (const DynamicCollider& collider : colliders_) {
        if (!collider.enabled) continue;
        AABB expanded = collider.bounds;
        expanded.minimum -= glm::vec3{capsule.radius};
        expanded.maximum += glm::vec3{capsule.radius};
        constexpr int samples = 5;
        for (int sample = 0; sample < samples; ++sample) {
            const float t = static_cast<float>(sample) / static_cast<float>(samples - 1);
            const glm::vec3 center = capsule.bottomCenter() +
                (capsule.topCenter() - capsule.bottomCenter()) * t;
            float distance = 0.0F;
            glm::vec3 normal{};
            if (rayAabb(center, direction, expanded, best, distance, normal) &&
                glm::dot(direction, normal) < 0.0F) {
                found = true;
                best = distance;
                hit.fraction = std::clamp(distance / length, 0.0F, 1.0F);
                hit.normal = normal;
                hit.point = center + direction * distance - normal * capsule.radius;
            }
        }
    }
    return found;
}

bool DynamicCollisionWorld::overlapCapsule(const Capsule& capsule, glm::vec3* normal,
                                           float* depth,
                                           const EntityId ignoredOwner) const noexcept {
    bool found = false;
    float deepest = 0.0F;
    glm::vec3 deepestNormal{};
    for (const DynamicCollider& collider : colliders_) {
        if (!collider.enabled || collider.owner == ignoredOwner) continue;
        glm::vec3 candidateNormal{};
        float candidateDepth = 0.0F;
        if (capsuleOverlapsAabb(capsule, collider.bounds, &candidateNormal, &candidateDepth) &&
            candidateDepth > deepest) {
            found = true;
            deepest = candidateDepth;
            deepestNormal = candidateNormal;
        }
    }
    if (found && normal != nullptr) *normal = deepestNormal;
    if (found && depth != nullptr) *depth = deepest;
    return found;
}

bool DynamicCollisionWorld::raycast(const glm::vec3& origin, const glm::vec3& direction,
                                    const float maximumDistance, RayHit& hit,
                                    const EntityId ignoredOwner) const noexcept {
    bool found = false;
    float closest = maximumDistance;
    for (const DynamicCollider& collider : colliders_) {
        if (!collider.enabled || collider.owner == ignoredOwner) continue;
        float distance = 0.0F;
        glm::vec3 normal{};
        if (rayAabb(origin, direction, collider.bounds, closest, distance, normal)) {
            found = true;
            closest = distance;
            hit = {distance, origin + direction * distance, normal};
        }
    }
    return found;
}

const std::vector<DynamicCollider>& DynamicCollisionWorld::colliders() const noexcept {
    return colliders_;
}
