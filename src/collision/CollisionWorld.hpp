#pragma once

#include "collision/CollisionQueries.hpp"

#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

struct Capsule {
    // Position is the center of the capsule's bottom-most point on the ground plane.
    glm::vec3 position{};
    float height{1.8F};
    float radius{0.35F};

    [[nodiscard]] glm::vec3 bottomCenter() const noexcept;
    [[nodiscard]] glm::vec3 topCenter() const noexcept;
    [[nodiscard]] AABB bounds() const noexcept;
};

struct CollisionSweepHit {
    float fraction{1.0F};
    glm::vec3 point{};
    glm::vec3 normal{};
    std::uint32_t triangleIndex{0};
};

struct CollisionQueryStatistics {
    std::size_t candidateTriangles{0};
    std::size_t narrowPhaseTests{0};
    std::size_t contacts{0};
};

class CollisionWorld {
public:
    [[nodiscard]] bool build(std::vector<Triangle> triangles);

    void query(const AABB& bounds, std::vector<std::uint32_t>& results) const;
    [[nodiscard]] bool sweepCapsule(const Capsule& capsule, const glm::vec3& displacement,
                                    CollisionSweepHit& hit,
                                    CollisionQueryStatistics* statistics = nullptr,
                                    float minimumUpDot = -2.0F) const;
    [[nodiscard]] bool overlapCapsule(const Capsule& capsule, glm::vec3& correctionNormal,
                                      float& penetrationDepth,
                                      CollisionQueryStatistics* statistics = nullptr) const;
    [[nodiscard]] bool raycast(const glm::vec3& origin, const glm::vec3& direction,
                               float maximumDistance, RayHit& hit,
                               CollisionQueryStatistics* statistics = nullptr) const;

    [[nodiscard]] const std::vector<Triangle>& triangles() const noexcept;
    [[nodiscard]] std::size_t nodeCount() const noexcept;
    [[nodiscard]] const AABB& bounds() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    struct Node {
        AABB bounds{};
        std::uint32_t first{0};
        std::uint32_t count{0};
        std::uint32_t left{0};
        std::uint32_t right{0};
        [[nodiscard]] bool isLeaf() const noexcept { return count != 0; }
    };

    std::uint32_t buildNode(std::uint32_t first, std::uint32_t count);
    void queryNode(std::uint32_t nodeIndex, const AABB& bounds,
                   std::vector<std::uint32_t>& results) const;

    std::vector<Triangle> triangles_;
    std::vector<std::uint32_t> triangleIndices_;
    std::vector<Node> nodes_;
    mutable std::vector<std::uint32_t> scratchCandidates_;
    AABB bounds_{};
};
