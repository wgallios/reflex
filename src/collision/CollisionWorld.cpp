#include "collision/CollisionWorld.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>

namespace {
constexpr std::uint32_t leafTriangleCount = 8;
constexpr int capsuleSphereSamples = 5;

AABB sweptBounds(const Capsule& capsule, const glm::vec3& displacement) {
    AABB result = capsule.bounds();
    Capsule moved = capsule;
    moved.position += displacement;
    result.expand(moved.bounds());
    return result;
}
} // namespace

glm::vec3 Capsule::bottomCenter() const noexcept {
    return position + glm::vec3{0.0F, radius, 0.0F};
}

glm::vec3 Capsule::topCenter() const noexcept {
    return position + glm::vec3{0.0F, height - radius, 0.0F};
}

AABB Capsule::bounds() const noexcept {
    AABB result;
    result.expand(position + glm::vec3{-radius, 0.0F, -radius});
    result.expand(position + glm::vec3{radius, height, radius});
    return result;
}

bool CollisionWorld::build(std::vector<Triangle> triangles) {
    const auto start = std::chrono::steady_clock::now();
    triangles_ = std::move(triangles);
    triangleIndices_.resize(triangles_.size());
    std::iota(triangleIndices_.begin(), triangleIndices_.end(), 0U);
    nodes_.clear();
    scratchCandidates_.clear();
    scratchCandidates_.reserve(triangles_.size());
    bounds_ = AABB{};

    if (!triangles_.empty()) {
        nodes_.reserve(triangles_.size() * 2);
        buildNode(0, static_cast<std::uint32_t>(triangles_.size()));
        bounds_ = nodes_.front().bounds;
    }

    const float milliseconds = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "Collision world:\n"
              << "  triangles:        " << triangles_.size() << '\n'
              << "  BVH nodes:        " << nodes_.size() << '\n'
              << "  build time:       " << milliseconds << " ms\n";
    if (!triangles_.empty()) {
        std::cout << "  world bounds:     (" << bounds_.minimum.x << ", "
                  << bounds_.minimum.y << ", " << bounds_.minimum.z << ") to ("
                  << bounds_.maximum.x << ", " << bounds_.maximum.y << ", "
                  << bounds_.maximum.z << ")\n";
    }
    return true;
}

std::uint32_t CollisionWorld::buildNode(const std::uint32_t first,
                                        const std::uint32_t count) {
    const std::uint32_t nodeIndex = static_cast<std::uint32_t>(nodes_.size());
    nodes_.push_back(Node{});
    AABB nodeBounds;
    AABB centroidBounds;
    for (std::uint32_t i = first; i < first + count; ++i) {
        const Triangle& triangle = triangles_[triangleIndices_[i]];
        nodeBounds.expand(triangle.bounds);
        centroidBounds.expand(triangle.bounds.center());
    }
    nodes_[nodeIndex].bounds = nodeBounds;
    if (count <= leafTriangleCount) {
        nodes_[nodeIndex].first = first;
        nodes_[nodeIndex].count = count;
        return nodeIndex;
    }

    const glm::vec3 extent = centroidBounds.maximum - centroidBounds.minimum;
    int axis = 0;
    if (extent.y > extent.x) {
        axis = 1;
    }
    if (extent.z > extent[axis]) {
        axis = 2;
    }
    const std::uint32_t middle = first + count / 2;
    std::nth_element(triangleIndices_.begin() + first, triangleIndices_.begin() + middle,
                     triangleIndices_.begin() + first + count,
                     [this, axis](const std::uint32_t left, const std::uint32_t right) {
                         return triangles_[left].bounds.center()[axis] <
                                triangles_[right].bounds.center()[axis];
                     });
    const std::uint32_t left = buildNode(first, middle - first);
    const std::uint32_t right = buildNode(middle, first + count - middle);
    nodes_[nodeIndex].left = left;
    nodes_[nodeIndex].right = right;
    return nodeIndex;
}

void CollisionWorld::query(const AABB& bounds, std::vector<std::uint32_t>& results) const {
    results.clear();
    if (!nodes_.empty()) {
        queryNode(0, bounds, results);
    }
}

void CollisionWorld::queryNode(const std::uint32_t nodeIndex, const AABB& bounds,
                               std::vector<std::uint32_t>& results) const {
    const Node& node = nodes_[nodeIndex];
    if (!node.bounds.overlaps(bounds)) {
        return;
    }
    if (node.isLeaf()) {
        for (std::uint32_t i = node.first; i < node.first + node.count; ++i) {
            const std::uint32_t triangleIndex = triangleIndices_[i];
            if (triangles_[triangleIndex].bounds.overlaps(bounds)) {
                results.push_back(triangleIndex);
            }
        }
        return;
    }
    queryNode(node.left, bounds, results);
    queryNode(node.right, bounds, results);
}

bool CollisionWorld::sweepCapsule(const Capsule& capsule, const glm::vec3& displacement,
                                  CollisionSweepHit& hit,
                                  CollisionQueryStatistics* statistics,
                                  const float minimumUpDot) const {
    query(sweptBounds(capsule, displacement), scratchCandidates_);
    if (statistics != nullptr) {
        statistics->candidateTriangles += scratchCandidates_.size();
    }

    bool found = false;
    hit.fraction = 1.0F;
    for (const std::uint32_t triangleIndex : scratchCandidates_) {
        const Triangle& triangle = triangles_[triangleIndex];
        for (int sample = 0; sample < capsuleSphereSamples; ++sample) {
            const float t = static_cast<float>(sample) /
                            static_cast<float>(capsuleSphereSamples - 1);
            const glm::vec3 center = capsule.bottomCenter() +
                (capsule.topCenter() - capsule.bottomCenter()) * t;
            SweepHit sphereHit;
            if (statistics != nullptr) {
                ++statistics->narrowPhaseTests;
            }
            if (!sweepSphereTriangle(center, capsule.radius, displacement, triangle,
                                     sphereHit) || sphereHit.fraction >= hit.fraction) {
                continue;
            }
            if (minimumUpDot > -1.0F) {
                glm::vec3 surfaceNormal = triangle.normal;
                if (glm::dot(surfaceNormal, sphereHit.normal) < 0.0F) {
                    surfaceNormal = -surfaceNormal;
                }
                if (surfaceNormal.y < minimumUpDot) {
                    continue;
                }
                sphereHit.normal = surfaceNormal;
            }

            // A triangle soup has artificial edges where coplanar triangles meet.
            // Suppress an edge/vertex hit when a coplanar neighbor supports the same
            // sphere at impact without opposing its motion.
            bool internalSeam = false;
            const glm::vec3 impactCenter = center + displacement * sphereHit.fraction;
            if (std::abs(glm::dot(sphereHit.normal, triangle.normal)) < 0.98F) {
                for (const std::uint32_t neighborIndex : scratchCandidates_) {
                    if (neighborIndex == triangleIndex ||
                        std::abs(glm::dot(triangle.normal,
                                         triangles_[neighborIndex].normal)) < 0.999F) {
                        continue;
                    }
                    glm::vec3 neighborNormal{};
                    if (sphereOverlapsTriangle(impactCenter, capsule.radius + 0.00001F,
                                               triangles_[neighborIndex], &neighborNormal) &&
                        glm::dot(displacement, neighborNormal) >= -0.00001F) {
                        internalSeam = true;
                        break;
                    }
                }
            }
            if (!internalSeam) {
                found = true;
                hit = {sphereHit.fraction, sphereHit.position, sphereHit.normal,
                       triangleIndex};
            }
        }
    }
    if (found && statistics != nullptr) {
        ++statistics->contacts;
    }
    return found;
}

bool CollisionWorld::overlapCapsule(const Capsule& capsule, glm::vec3& correctionNormal,
                                    float& penetrationDepth,
                                    CollisionQueryStatistics* statistics) const {
    query(capsule.bounds(), scratchCandidates_);
    if (statistics != nullptr) {
        statistics->candidateTriangles += scratchCandidates_.size();
    }
    bool found = false;
    penetrationDepth = 0.0F;
    for (const std::uint32_t triangleIndex : scratchCandidates_) {
        glm::vec3 normal{};
        float depth = 0.0F;
        if (statistics != nullptr) {
            ++statistics->narrowPhaseTests;
        }
        if (capsuleOverlapsTriangle(capsule.bottomCenter(), capsule.topCenter(),
                                    capsule.radius, triangles_[triangleIndex], &normal, &depth) &&
            depth > penetrationDepth) {
            found = true;
            penetrationDepth = depth;
            correctionNormal = normal;
        }
    }
    if (found && statistics != nullptr) {
        ++statistics->contacts;
    }
    return found;
}

bool CollisionWorld::raycast(const glm::vec3& origin, const glm::vec3& direction,
                             const float maximumDistance, RayHit& hit,
                             CollisionQueryStatistics* statistics) const {
    AABB rayBounds;
    rayBounds.expand(origin);
    rayBounds.expand(origin + direction * maximumDistance);
    query(rayBounds, scratchCandidates_);
    if (statistics != nullptr) {
        statistics->candidateTriangles += scratchCandidates_.size();
    }
    bool found = false;
    float nearest = maximumDistance;
    for (const std::uint32_t index : scratchCandidates_) {
        RayHit candidate;
        if (statistics != nullptr) {
            ++statistics->narrowPhaseTests;
        }
        if (rayTriangle(origin, direction, nearest, triangles_[index], candidate)) {
            nearest = candidate.distance;
            hit = candidate;
            found = true;
        }
    }
    if (found && statistics != nullptr) {
        ++statistics->contacts;
    }
    return found;
}

const std::vector<Triangle>& CollisionWorld::triangles() const noexcept { return triangles_; }
std::size_t CollisionWorld::nodeCount() const noexcept { return nodes_.size(); }
const AABB& CollisionWorld::bounds() const noexcept { return bounds_; }
bool CollisionWorld::empty() const noexcept { return triangles_.empty(); }
