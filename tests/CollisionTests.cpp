#include "collision/CollisionQueries.hpp"
#include "collision/CollisionWorld.hpp"
#include "gameplay/PlayerController.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr float tolerance = 0.002F;

Triangle makeTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    Triangle triangle{a, b, c};
    const glm::vec3 cross = glm::cross(b - a, c - a);
    triangle.normal = glm::normalize(cross);
    triangle.bounds.expand(a);
    triangle.bounds.expand(b);
    triangle.bounds.expand(c);
    return triangle;
}

bool near(const glm::vec3& left, const glm::vec3& right, const float epsilon = tolerance) {
    return glm::length(left - right) <= epsilon;
}

void addBox(std::vector<Triangle>& triangles, const glm::vec3& minimum,
            const glm::vec3& maximum) {
    const glm::vec3 vertices[] = {
        {minimum.x, minimum.y, minimum.z}, {maximum.x, minimum.y, minimum.z},
        {maximum.x, maximum.y, minimum.z}, {minimum.x, maximum.y, minimum.z},
        {minimum.x, minimum.y, maximum.z}, {maximum.x, minimum.y, maximum.z},
        {maximum.x, maximum.y, maximum.z}, {minimum.x, maximum.y, maximum.z},
    };
    constexpr unsigned indices[] = {
        0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
        0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5,
        3, 7, 6, 3, 6, 2, 0, 1, 5, 0, 5, 4,
    };
    for (std::size_t i = 0; i < std::size(indices); i += 3) {
        triangles.push_back(makeTriangle(vertices[indices[i]], vertices[indices[i + 1]],
                                         vertices[indices[i + 2]]));
    }
}

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}
} // namespace

int main() {
    const Triangle triangle = makeTriangle({0.0F, 0.0F, 0.0F},
                                           {2.0F, 0.0F, 0.0F},
                                           {0.0F, 0.0F, 2.0F});
    if (!near(closestPointOnTriangle({0.5F, 1.0F, 0.5F}, triangle),
              {0.5F, 0.0F, 0.5F})) {
        return fail("Closest point on triangle is incorrect.");
    }
    if (!sphereOverlapsTriangle({0.5F, 0.2F, 0.5F}, 0.25F, triangle) ||
        sphereOverlapsTriangle({0.5F, 1.0F, 0.5F}, 0.25F, triangle)) {
        return fail("Sphere-triangle overlap is incorrect.");
    }
    if (!capsuleOverlapsTriangle({0.5F, 0.2F, 0.5F}, {0.5F, 1.2F, 0.5F},
                                 0.25F, triangle)) {
        return fail("Capsule-triangle overlap is incorrect.");
    }
    SweepHit sweep;
    if (!sweepSphereTriangle({0.5F, 1.0F, 0.5F}, 0.25F, {0.0F, -2.0F, 0.0F},
                             triangle, sweep) || std::abs(sweep.fraction - 0.375F) > 0.002F) {
        return fail("Swept sphere time of impact is incorrect.");
    }
    RayHit ray;
    if (!rayTriangle({0.5F, 1.0F, 0.5F}, {0.0F, -1.0F, 0.0F}, 2.0F,
                     triangle, ray) || std::abs(ray.distance - 1.0F) > tolerance) {
        return fail("Ray-triangle intersection is incorrect.");
    }
    if (!isWalkableNormal({0.0F, 1.0F, 0.0F}, 46.0F) ||
        isWalkableNormal({1.0F, 0.0F, 0.0F}, 46.0F)) {
        return fail("Walkable-normal classification is incorrect.");
    }
    if (!near(projectMovementOntoPlane({1.0F, -1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}),
              {1.0F, 0.0F, 0.0F})) {
        return fail("Collision-plane movement projection is incorrect.");
    }

    AABB first;
    first.expand({-1.0F, -1.0F, -1.0F});
    first.expand({1.0F, 1.0F, 1.0F});
    AABB second;
    second.expand({0.5F, 0.5F, 0.5F});
    second.expand({2.0F, 2.0F, 2.0F});
    if (!first.overlaps(second)) {
        return fail("AABB overlap is incorrect.");
    }

    std::vector<Triangle> level;
    level.push_back(makeTriangle({-10.0F, 0.0F, -10.0F},
                                 {10.0F, 0.0F, 10.0F},
                                 {10.0F, 0.0F, -10.0F}));
    level.push_back(makeTriangle({-10.0F, 0.0F, -10.0F},
                                 {-10.0F, 0.0F, 10.0F},
                                 {10.0F, 0.0F, 10.0F}));
    level.push_back(makeTriangle({-5.0F, 0.0F, -2.0F},
                                 {5.0F, 3.0F, -2.0F},
                                 {5.0F, 0.0F, -2.0F}));
    level.push_back(makeTriangle({-5.0F, 0.0F, -2.0F},
                                 {-5.0F, 3.0F, -2.0F},
                                 {5.0F, 3.0F, -2.0F}));
    CollisionWorld world;
    if (!world.build(std::move(level)) || world.nodeCount() == 0) {
        return fail("Collision BVH construction failed.");
    }
    std::vector<std::uint32_t> candidates;
    world.query(first, candidates);
    if (candidates.empty()) {
        return fail("BVH broad-phase query returned no floor candidates.");
    }

    PlayerController player;
    if (!player.initialize(world, {0.0F, 2.0F, 0.0F})) {
        return fail("Player initialization failed.");
    }
    const glm::vec3 viewForward{0.0F, 0.0F, -1.0F};
    const glm::vec3 viewRight{1.0F, 0.0F, 0.0F};
    for (int i = 0; i < 240; ++i) {
        player.simulate(1.0F / 120.0F, {}, viewForward, viewRight);
    }
    if (!player.isGrounded() || player.position().y < -tolerance ||
        player.position().y > 0.02F) {
        return fail("Player did not land stably on the floor.");
    }

    PlayerInput jump;
    jump.jumpPressed = true;
    player.simulate(1.0F / 120.0F, jump, viewForward, viewRight);
    if (player.isGrounded() || player.velocity().y <= 0.0F) {
        return fail("Grounded jump impulse was not applied.");
    }

    PlayerController diagonal;
    if (!diagonal.initialize(world, {0.0F, 0.01F, 1.0F})) {
        return fail("Diagonal-movement player initialization failed.");
    }
    PlayerInput diagonalInput;
    diagonalInput.movement = {1.0F, 1.0F};
    for (int i = 0; i < 120; ++i) {
        diagonal.simulate(1.0F / 120.0F, diagonalInput, viewForward, viewRight);
    }
    const glm::vec3 horizontalDelta = glm::vec3{diagonal.position().x, 0.0F,
                                                diagonal.position().z - 1.0F};
    if (glm::length(horizontalDelta) > 5.05F) {
        return fail("Diagonal movement exceeds configured walk speed.");
    }
    if (diagonal.position().x < 2.0F || diagonal.position().z < -1.7F) {
        std::cerr << "Wall-slide final position: " << diagonal.position().x << ", "
                  << diagonal.position().y << ", " << diagonal.position().z
                  << " velocity " << diagonal.velocity().x << ", "
                  << diagonal.velocity().y << ", " << diagonal.velocity().z
                  << " contacts " << diagonal.diagnostics().contactCount << '\n';
        for (std::size_t i = 0; i < diagonal.diagnostics().contactCount; ++i) {
            const glm::vec3 normal = diagonal.diagnostics().contactNormals[i];
            std::cerr << "  normal " << normal.x << ", " << normal.y << ", "
                      << normal.z << '\n';
        }
        return fail("Player did not slide along and stop at the wall.");
    }

    std::vector<Triangle> unevenFloor;
    constexpr int unevenFloorHalfExtent = 12;
    const auto unevenFloorPoint = [](const int x, const int z) {
        const float y = 0.04F * std::sin(static_cast<float>(x) * 1.7F) *
                        std::cos(static_cast<float>(z) * 1.3F);
        return glm::vec3{static_cast<float>(x), y, static_cast<float>(z)};
    };
    for (int z = -unevenFloorHalfExtent; z < unevenFloorHalfExtent; ++z) {
        for (int x = -unevenFloorHalfExtent; x < unevenFloorHalfExtent; ++x) {
            const glm::vec3 a = unevenFloorPoint(x, z);
            const glm::vec3 b = unevenFloorPoint(x + 1, z);
            const glm::vec3 c = unevenFloorPoint(x + 1, z + 1);
            const glm::vec3 d = unevenFloorPoint(x, z + 1);
            unevenFloor.push_back(makeTriangle(a, c, b));
            unevenFloor.push_back(makeTriangle(a, d, c));
        }
    }
    CollisionWorld unevenFloorWorld;
    if (!unevenFloorWorld.build(std::move(unevenFloor))) {
        return fail("Uneven-floor BVH construction failed.");
    }
    PlayerController unevenFloorPlayer;
    if (!unevenFloorPlayer.initialize(unevenFloorWorld, {-10.5F, 0.1F, 0.23F})) {
        return fail("Uneven-floor player initialization failed.");
    }
    PlayerInput unevenFloorInput;
    unevenFloorInput.movement.y = 1.0F;
    const glm::vec3 unevenFloorForward{1.0F, 0.0F, 0.0F};
    const glm::vec3 unevenFloorRight{0.0F, 0.0F, 1.0F};
    int consecutiveStalledTicks = 0;
    int maximumStalledTicks = 0;
    float previousUnevenFloorX = unevenFloorPlayer.position().x;
    for (int tick = 0; tick < 420; ++tick) {
        unevenFloorPlayer.simulate(1.0F / 120.0F, unevenFloorInput,
                                   unevenFloorForward, unevenFloorRight);
        const float advance = unevenFloorPlayer.position().x - previousUnevenFloorX;
        previousUnevenFloorX = unevenFloorPlayer.position().x;
        if (unevenFloorPlayer.velocity().x > 1.0F && advance < 0.0001F) {
            ++consecutiveStalledTicks;
            maximumStalledTicks = std::max(maximumStalledTicks, consecutiveStalledTicks);
        } else {
            consecutiveStalledTicks = 0;
        }
    }
    if (unevenFloorPlayer.position().x < 5.0F || !unevenFloorPlayer.isGrounded() ||
        maximumStalledTicks > 12) {
        std::cerr << "Uneven-floor final position: " << unevenFloorPlayer.position().x << ", "
                  << unevenFloorPlayer.position().y << ", "
                  << unevenFloorPlayer.position().z << " grounded "
                  << unevenFloorPlayer.isGrounded() << " maximum stalled ticks "
                  << maximumStalledTicks << '\n';
        return fail("Player became stuck while crossing uneven triangle edges.");
    }

    std::vector<Triangle> stepLevel;
    stepLevel.push_back(makeTriangle({-5.0F, 0.0F, -5.0F}, {5.0F, 0.0F, 5.0F},
                                     {5.0F, 0.0F, -5.0F}));
    stepLevel.push_back(makeTriangle({-5.0F, 0.0F, -5.0F}, {-5.0F, 0.0F, 5.0F},
                                     {5.0F, 0.0F, 5.0F}));
    addBox(stepLevel, {-1.0F, 0.0F, -1.0F}, {1.0F, 0.3F, 0.0F});
    addBox(stepLevel, {2.0F, 0.0F, -1.0F}, {4.0F, 0.7F, 0.0F});
    CollisionWorld stepWorld;
    if (!stepWorld.build(std::move(stepLevel))) {
        return fail("Step-test BVH construction failed.");
    }
    PlayerController stepPlayer;
    if (!stepPlayer.initialize(stepWorld, {0.0F, 0.01F, 1.2F})) {
        return fail("Step-test player initialization failed.");
    }
    PlayerInput forwardInput;
    forwardInput.movement.y = 1.0F;
    for (int i = 0; i < 45; ++i) {
        stepPlayer.simulate(1.0F / 120.0F, forwardInput, viewForward, viewRight);
    }
    if (stepPlayer.position().y < 0.25F || stepPlayer.position().z > 0.1F) {
        std::cerr << "Low-step final position: " << stepPlayer.position().x << ", "
                  << stepPlayer.position().y << ", " << stepPlayer.position().z
                  << " grounded " << stepPlayer.isGrounded()
                  << " attempts " << stepPlayer.diagnostics().stepAttempts
                  << " successes " << stepPlayer.diagnostics().stepSuccesses << '\n';
        return fail("Player did not step onto a ledge within maximum step height.");
    }

    PlayerController tallObstaclePlayer;
    if (!tallObstaclePlayer.initialize(stepWorld, {3.0F, 0.01F, 1.2F})) {
        return fail("Tall-obstacle player initialization failed.");
    }
    for (int i = 0; i < 45; ++i) {
        tallObstaclePlayer.simulate(1.0F / 120.0F, forwardInput,
                                    viewForward, viewRight);
    }
    if (tallObstaclePlayer.position().z < 0.34F ||
        tallObstaclePlayer.position().y > 0.1F) {
        return fail("Player climbed an obstacle above maximum step height.");
    }

    CollisionWorld emptyWorld;
    if (!emptyWorld.build({})) {
        return fail("Empty collision world construction failed.");
    }
    PlayerController noclipFallback;
    if (!noclipFallback.initialize(emptyWorld, {0.0F, 1.0F, 0.0F}) ||
        !noclipFallback.isNoclip()) {
        return fail("A scene without collision did not fall back to noclip.");
    }

    PlayerSettings invalidSettings;
    invalidSettings.capsuleHeight = invalidSettings.capsuleRadius;
    PlayerController invalidPlayer;
    if (invalidPlayer.initialize(world, {}, invalidSettings)) {
        return fail("Invalid capsule dimensions were accepted.");
    }

    return 0;
}
