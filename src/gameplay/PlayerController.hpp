#pragma once

#include "collision/CollisionWorld.hpp"
#include "collision/DynamicCollisionWorld.hpp"
#include "gameplay/PlayerSettings.hpp"

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <array>
#include <cstddef>

struct PlayerInput {
    glm::vec2 movement{};
    float verticalMovement{0.0F};
    bool sprint{false};
    bool jumpPressed{false};
};

struct PlayerDiagnostics {
    CollisionQueryStatistics collision{};
    std::array<glm::vec3, 8> contactPoints{};
    std::array<glm::vec3, 8> contactNormals{};
    std::size_t contactCount{0};
    int slideIterations{0};
    int stepAttempts{0};
    int stepSuccesses{0};
    bool groundProbeHit{false};
};

class PlayerController {
public:
    [[nodiscard]] bool initialize(const CollisionWorld& collisionWorld,
                                  const glm::vec3& spawnPosition,
                                  const PlayerSettings& settings = {},
                                  const DynamicCollisionWorld* dynamicCollision = nullptr);
    void simulate(float deltaTime, const PlayerInput& input,
                  const glm::vec3& viewForward, const glm::vec3& viewRight);
    void beginDiagnosticsFrame() noexcept;
    [[nodiscard]] bool toggleNoclip();
    [[nodiscard]] bool recoverPenetration();
    [[nodiscard]] bool setPosition(const glm::vec3& position, bool recover = true);
    void setDynamicCollisionWorld(const DynamicCollisionWorld* world) noexcept;

    [[nodiscard]] const glm::vec3& position() const noexcept;
    [[nodiscard]] glm::vec3 cameraPosition() const noexcept;
    [[nodiscard]] const glm::vec3& velocity() const noexcept;
    [[nodiscard]] bool isGrounded() const noexcept;
    [[nodiscard]] const glm::vec3& groundNormal() const noexcept;
    [[nodiscard]] float groundDistance() const noexcept;
    [[nodiscard]] bool isNoclip() const noexcept;
    [[nodiscard]] const PlayerSettings& settings() const noexcept;
    [[nodiscard]] const PlayerDiagnostics& diagnostics() const noexcept;
    [[nodiscard]] Capsule capsule() const noexcept;

private:
    void simulateNoclip(float deltaTime, const PlayerInput& input,
                        const glm::vec3& viewForward, const glm::vec3& viewRight);
    void simulateCollision(float deltaTime, const PlayerInput& input,
                           const glm::vec3& viewForward, const glm::vec3& viewRight);
    void moveAndSlide(glm::vec3 displacement, bool allowStep);
    [[nodiscard]] bool attemptStep(const glm::vec3& horizontalDisplacement);
    void updateGround(bool allowSnap);
    [[nodiscard]] bool isWalkable(const glm::vec3& normal) const noexcept;
    void addContact(const glm::vec3& point, const glm::vec3& normal) noexcept;
    [[nodiscard]] bool sweep(const Capsule& capsule, const glm::vec3& displacement,
                             CollisionSweepHit& hit, float minimumUpDot = -2.0F);
    [[nodiscard]] bool overlap(const Capsule& capsule, glm::vec3& normal, float& depth);

    const CollisionWorld* collisionWorld_{nullptr};
    const DynamicCollisionWorld* dynamicCollisionWorld_{nullptr};
    PlayerSettings settings_{};
    glm::vec3 position_{};
    glm::vec3 lastValidPosition_{};
    glm::vec3 velocity_{};
    glm::vec3 groundNormal_{0.0F, 1.0F, 0.0F};
    float groundDistance_{0.0F};
    bool grounded_{false};
    bool noclip_{false};
    bool jumpedThisStep_{false};
    PlayerDiagnostics diagnostics_{};
};
