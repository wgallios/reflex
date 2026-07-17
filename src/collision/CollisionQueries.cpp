#include "collision/CollisionQueries.hpp"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr float geometryEpsilon = 0.000001F;
constexpr int sweepIterations = 24;

glm::vec3 safeContactNormal(const glm::vec3& center, const glm::vec3& closest,
                            const Triangle& triangle) {
    const glm::vec3 delta = center - closest;
    const float lengthSquared = glm::dot(delta, delta);
    if (lengthSquared > geometryEpsilon * geometryEpsilon) {
        return delta / std::sqrt(lengthSquared);
    }
    return triangle.normal;
}
} // namespace

glm::vec3 closestPointOnTriangle(const glm::vec3& point,
                                 const Triangle& triangle) noexcept {
    const glm::vec3 ab = triangle.b - triangle.a;
    const glm::vec3 ac = triangle.c - triangle.a;
    const glm::vec3 ap = point - triangle.a;
    const float d1 = glm::dot(ab, ap);
    const float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0F && d2 <= 0.0F) {
        return triangle.a;
    }

    const glm::vec3 bp = point - triangle.b;
    const float d3 = glm::dot(ab, bp);
    const float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0F && d4 <= d3) {
        return triangle.b;
    }

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0F && d1 >= 0.0F && d3 <= 0.0F) {
        const float v = d1 / (d1 - d3);
        return triangle.a + v * ab;
    }

    const glm::vec3 cp = point - triangle.c;
    const float d5 = glm::dot(ab, cp);
    const float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0F && d5 <= d6) {
        return triangle.c;
    }

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0F && d2 >= 0.0F && d6 <= 0.0F) {
        const float w = d2 / (d2 - d6);
        return triangle.a + w * ac;
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0F && (d4 - d3) >= 0.0F && (d5 - d6) >= 0.0F) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return triangle.b + w * (triangle.c - triangle.b);
    }

    const float denominator = 1.0F / (va + vb + vc);
    const float v = vb * denominator;
    const float w = vc * denominator;
    return triangle.a + ab * v + ac * w;
}

bool sphereOverlapsTriangle(const glm::vec3& center, const float radius,
                            const Triangle& triangle, glm::vec3* normal,
                            float* depth) noexcept {
    const glm::vec3 closest = closestPointOnTriangle(center, triangle);
    const glm::vec3 delta = center - closest;
    const float distanceSquared = glm::dot(delta, delta);
    if (distanceSquared > radius * radius) {
        return false;
    }
    const float distance = std::sqrt(std::max(distanceSquared, 0.0F));
    if (normal != nullptr) {
        *normal = safeContactNormal(center, closest, triangle);
    }
    if (depth != nullptr) {
        *depth = radius - distance;
    }
    return true;
}

bool capsuleOverlapsTriangle(const glm::vec3& bottomCenter, const glm::vec3& topCenter,
                             const float radius, const Triangle& triangle,
                             glm::vec3* normal, float* depth) noexcept {
    constexpr int samples = 5;
    bool found = false;
    float deepest = -std::numeric_limits<float>::max();
    glm::vec3 deepestNormal{};
    for (int i = 0; i < samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples - 1);
        const glm::vec3 center = bottomCenter + (topCenter - bottomCenter) * t;
        glm::vec3 sampleNormal{};
        float sampleDepth = 0.0F;
        if (sphereOverlapsTriangle(center, radius, triangle, &sampleNormal, &sampleDepth) &&
            sampleDepth > deepest) {
            found = true;
            deepest = sampleDepth;
            deepestNormal = sampleNormal;
        }
    }
    if (found && normal != nullptr) {
        *normal = deepestNormal;
    }
    if (found && depth != nullptr) {
        *depth = deepest;
    }
    return found;
}

bool sweepSphereTriangle(const glm::vec3& center, const float radius,
                         const glm::vec3& displacement, const Triangle& triangle,
                         SweepHit& hit) noexcept {
    const float movementLength = glm::length(displacement);
    glm::vec3 closest = closestPointOnTriangle(center, triangle);
    float separation = glm::length(center - closest) - radius;
    if (separation <= geometryEpsilon) {
        const glm::vec3 normal = safeContactNormal(center, closest, triangle);
        if (glm::dot(displacement, normal) >= -geometryEpsilon) {
            return false;
        }
        hit = {0.0F, closest, normal};
        return true;
    }
    if (movementLength <= geometryEpsilon) {
        return false;
    }

    float fraction = 0.0F;
    for (int iteration = 0; iteration < sweepIterations; ++iteration) {
        // Distance to a closed set is 1-Lipschitz. Advancing by separation divided by
        // total path length cannot skip the first contact, even when the closest feature changes.
        fraction += separation / movementLength;
        if (fraction > 1.0F) {
            return false;
        }
        const glm::vec3 sampleCenter = center + displacement * fraction;
        closest = closestPointOnTriangle(sampleCenter, triangle);
        separation = glm::length(sampleCenter - closest) - radius;
        if (separation <= geometryEpsilon) {
            const glm::vec3 normal = safeContactNormal(sampleCenter, closest, triangle);
            if (glm::dot(displacement, normal) >= -geometryEpsilon) {
                return false;
            }
            hit = {fraction, closest, normal};
            return true;
        }
    }
    return false;
}

bool rayTriangle(const glm::vec3& origin, const glm::vec3& direction,
                 const float maximumDistance, const Triangle& triangle,
                 RayHit& hit) noexcept {
    const glm::vec3 edge1 = triangle.b - triangle.a;
    const glm::vec3 edge2 = triangle.c - triangle.a;
    const glm::vec3 p = glm::cross(direction, edge2);
    const float determinant = glm::dot(edge1, p);
    if (std::abs(determinant) <= geometryEpsilon) {
        return false;
    }
    const float inverseDeterminant = 1.0F / determinant;
    const glm::vec3 t = origin - triangle.a;
    const float u = glm::dot(t, p) * inverseDeterminant;
    if (u < 0.0F || u > 1.0F) {
        return false;
    }
    const glm::vec3 q = glm::cross(t, edge1);
    const float v = glm::dot(direction, q) * inverseDeterminant;
    if (v < 0.0F || u + v > 1.0F) {
        return false;
    }
    const float distance = glm::dot(edge2, q) * inverseDeterminant;
    if (distance < 0.0F || distance > maximumDistance) {
        return false;
    }
    hit = {distance, origin + direction * distance, triangle.normal};
    return true;
}

bool isWalkableNormal(const glm::vec3& normal,
                      const float maximumSlopeAngleDegrees) noexcept {
    return glm::dot(normal, glm::vec3{0.0F, 1.0F, 0.0F}) >=
           std::cos(glm::radians(maximumSlopeAngleDegrees));
}

glm::vec3 projectMovementOntoPlane(const glm::vec3& movement,
                                   const glm::vec3& normal) noexcept {
    const float intoSurface = glm::dot(movement, normal);
    return intoSurface < 0.0F ? movement - normal * intoSurface : movement;
}
