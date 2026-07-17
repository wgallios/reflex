#pragma once

#include "collision/Triangle.hpp"

#include <glm/vec3.hpp>

struct RayHit {
    float distance{0.0F};
    glm::vec3 position{};
    glm::vec3 normal{};
};

struct SweepHit {
    float fraction{1.0F};
    glm::vec3 position{};
    glm::vec3 normal{};
};

[[nodiscard]] glm::vec3 closestPointOnTriangle(const glm::vec3& point,
                                                const Triangle& triangle) noexcept;
[[nodiscard]] bool sphereOverlapsTriangle(const glm::vec3& center, float radius,
                                           const Triangle& triangle,
                                           glm::vec3* normal = nullptr,
                                           float* depth = nullptr) noexcept;
[[nodiscard]] bool capsuleOverlapsTriangle(const glm::vec3& bottomCenter,
                                            const glm::vec3& topCenter, float radius,
                                            const Triangle& triangle,
                                            glm::vec3* normal = nullptr,
                                            float* depth = nullptr) noexcept;
[[nodiscard]] bool sweepSphereTriangle(const glm::vec3& center, float radius,
                                       const glm::vec3& displacement,
                                       const Triangle& triangle, SweepHit& hit) noexcept;
[[nodiscard]] bool rayTriangle(const glm::vec3& origin, const glm::vec3& direction,
                               float maximumDistance, const Triangle& triangle,
                               RayHit& hit) noexcept;
[[nodiscard]] bool isWalkableNormal(const glm::vec3& normal,
                                    float maximumSlopeAngleDegrees) noexcept;
[[nodiscard]] glm::vec3 projectMovementOntoPlane(const glm::vec3& movement,
                                                  const glm::vec3& normal) noexcept;
