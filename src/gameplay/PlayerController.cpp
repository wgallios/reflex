#include "gameplay/PlayerController.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
constexpr glm::vec3 worldUp{0.0F, 1.0F, 0.0F};

glm::vec3 moveToward(const glm::vec3& current, const glm::vec3& target,
                     const float maximumDelta) {
    const glm::vec3 delta = target - current;
    const float length = glm::length(delta);
    return length <= maximumDelta || length < 0.000001F
        ? target : current + delta * (maximumDelta / length);
}
} // namespace

bool PlayerController::initialize(const CollisionWorld& collisionWorld,
                                  const glm::vec3& spawnPosition,
                                  const PlayerSettings& settings) {
    if (!settings.validate()) {
        return false;
    }
    collisionWorld_ = &collisionWorld;
    settings_ = settings;
    position_ = spawnPosition;
    lastValidPosition_ = position_;
    velocity_ = {};
    noclip_ = collisionWorld.empty();
    if (!noclip_ && !recoverPenetration()) {
        std::cerr << "Warning: spawn penetration could not be fully resolved; using noclip mode.\n";
        noclip_ = true;
    }
    updateGround(true);
    std::cout << "Player initialized at (" << position_.x << ", " << position_.y << ", "
              << position_.z << ") in " << (noclip_ ? "noclip" : "collision") << " mode.\n";
    return true;
}

void PlayerController::simulate(const float deltaTime, const PlayerInput& input,
                                const glm::vec3& viewForward, const glm::vec3& viewRight) {
    jumpedThisStep_ = false;
    if (noclip_) {
        simulateNoclip(deltaTime, input, viewForward, viewRight);
    } else {
        simulateCollision(deltaTime, input, viewForward, viewRight);
        lastValidPosition_ = position_;
    }
}

void PlayerController::beginDiagnosticsFrame() noexcept { diagnostics_ = {}; }

void PlayerController::simulateNoclip(const float deltaTime, const PlayerInput& input,
                                      const glm::vec3& viewForward,
                                      const glm::vec3& viewRight) {
    glm::vec3 movement = viewForward * input.movement.y + viewRight * input.movement.x;
    movement += worldUp * input.verticalMovement;
    const float length = glm::length(movement);
    if (length > 1.0F) {
        movement /= length;
    }
    const float speed = settings_.walkSpeed *
        (input.sprint ? settings_.sprintMultiplier : 1.0F);
    position_ += movement * speed * deltaTime;
    velocity_ = movement * speed;
    grounded_ = false;
}

void PlayerController::simulateCollision(const float deltaTime, const PlayerInput& input,
                                         const glm::vec3& viewForward,
                                         const glm::vec3& viewRight) {
    glm::vec3 forward{viewForward.x, 0.0F, viewForward.z};
    glm::vec3 right{viewRight.x, 0.0F, viewRight.z};
    if (glm::dot(forward, forward) > 0.000001F) {
        forward = glm::normalize(forward);
    }
    if (glm::dot(right, right) > 0.000001F) {
        right = glm::normalize(right);
    }
    glm::vec3 wishDirection = forward * input.movement.y + right * input.movement.x;
    const float wishLength = glm::length(wishDirection);
    if (wishLength > 1.0F) {
        wishDirection /= wishLength;
    }

    const float speed = settings_.walkSpeed *
        (input.sprint ? settings_.sprintMultiplier : 1.0F);
    if (grounded_) {
        wishDirection -= groundNormal_ * glm::dot(wishDirection, groundNormal_);
        if (glm::dot(wishDirection, wishDirection) > 0.000001F) {
            wishDirection = glm::normalize(wishDirection);
        }
        const glm::vec3 targetGroundVelocity = wishDirection * speed;
        glm::vec3 currentGroundVelocity =
            velocity_ - groundNormal_ * glm::dot(velocity_, groundNormal_);
        velocity_ = moveToward(currentGroundVelocity, targetGroundVelocity,
                               settings_.groundAcceleration * deltaTime);
        if (input.jumpPressed) {
            velocity_.y = settings_.jumpSpeed;
            grounded_ = false;
            jumpedThisStep_ = true;
        }
    } else {
        glm::vec3 horizontal{velocity_.x, 0.0F, velocity_.z};
        const glm::vec3 target = wishDirection * settings_.maximumAirSpeed;
        horizontal = moveToward(horizontal, target, settings_.airAcceleration * deltaTime);
        velocity_.x = horizontal.x;
        velocity_.z = horizontal.z;
    }

    velocity_.y = std::max(velocity_.y + settings_.gravity * deltaTime,
                           settings_.terminalVelocity);
    moveAndSlide(velocity_ * deltaTime, grounded_ && !jumpedThisStep_);
    updateGround(!jumpedThisStep_);
}

void PlayerController::moveAndSlide(glm::vec3 displacement, const bool allowStep) {
    std::array<glm::vec3, 3> planes{};
    int planeCount = 0;
    for (int iteration = 0; iteration < settings_.maximumSlideIterations; ++iteration) {
        diagnostics_.slideIterations = iteration + 1;
        const float distance = glm::length(displacement);
        if (distance < settings_.minimumMovementDistance) {
            break;
        }
        CollisionSweepHit hit;
        if (!collisionWorld_->sweepCapsule(capsule(), displacement, hit,
                                           &diagnostics_.collision)) {
            position_ += displacement;
            break;
        }

        const float safeFraction = std::max(0.0F, hit.fraction -
            settings_.contactOffset / distance);
        position_ += displacement * safeFraction;
        addContact(hit.point, hit.normal);

        displacement *= 1.0F - safeFraction;
        const glm::vec3 horizontal{displacement.x, 0.0F, displacement.z};
        if (allowStep && !isWalkable(hit.normal) && glm::dot(horizontal, horizontal) > 0.000001F &&
            attemptStep(horizontal)) {
            return;
        }
        if (glm::dot(displacement, hit.normal) < -0.000001F && planeCount < 3) {
            bool duplicatePlane = false;
            for (int plane = 0; plane < planeCount; ++plane) {
                duplicatePlane = duplicatePlane ||
                    std::abs(glm::dot(planes[plane], hit.normal)) > 0.999F;
            }
            if (!duplicatePlane) {
                planes[planeCount++] = hit.normal;
            }
        }
        if (planeCount == 1) {
            displacement -= planes[0] * glm::dot(displacement, planes[0]);
            velocity_ -= planes[0] * std::min(glm::dot(velocity_, planes[0]), 0.0F);
        } else if (planeCount == 2) {
            glm::vec3 crease = glm::cross(planes[0], planes[1]);
            const float creaseLength = glm::length(crease);
            if (creaseLength < 0.0001F) {
                displacement = {};
                break;
            }
            crease /= creaseLength;
            displacement = crease * glm::dot(displacement, crease);
            velocity_ = crease * glm::dot(velocity_, crease);
        } else {
            displacement = {};
            velocity_ = {};
            break;
        }
    }
}

bool PlayerController::attemptStep(const glm::vec3& horizontalDisplacement) {
    ++diagnostics_.stepAttempts;
    const glm::vec3 original = position_;
    CollisionSweepHit hit;
    const glm::vec3 upward{0.0F, settings_.maximumStepHeight, 0.0F};
    if (collisionWorld_->sweepCapsule(capsule(), upward, hit, &diagnostics_.collision)) {
        return false;
    }
    position_ += upward;
    glm::vec3 stepHorizontal = horizontalDisplacement;
    const float horizontalLength = glm::length(stepHorizontal);
    if (horizontalLength > settings_.minimumMovementDistance) {
        stepHorizontal += stepHorizontal / horizontalLength * settings_.stepSearchDistance;
    }
    if (collisionWorld_->sweepCapsule(capsule(), stepHorizontal, hit,
                                      &diagnostics_.collision)) {
        position_ = original;
        return false;
    }
    position_ += stepHorizontal;
    const glm::vec3 downward{0.0F, -(settings_.maximumStepHeight + settings_.groundSnapDistance),
                             0.0F};
    if (!collisionWorld_->sweepCapsule(capsule(), downward, hit, &diagnostics_.collision,
                                      std::cos(glm::radians(settings_.maximumSlopeAngleDegrees))) ||
        !isWalkable(hit.normal)) {
        position_ = original;
        return false;
    }
    position_ += downward * std::max(0.0F, hit.fraction -
        settings_.contactOffset / glm::length(downward));
    if (position_.y <= original.y + settings_.skinWidth) {
        position_ = original;
        return false;
    }
    glm::vec3 overlapNormal{};
    float overlapDepth = 0.0F;
    if (collisionWorld_->overlapCapsule(capsule(), overlapNormal, overlapDepth,
                                        &diagnostics_.collision) &&
        overlapDepth > settings_.skinWidth) {
        position_ = original;
        return false;
    }
    grounded_ = true;
    groundNormal_ = hit.normal;
    velocity_.y = 0.0F;
    addContact(hit.point, hit.normal);
    ++diagnostics_.stepSuccesses;
    return true;
}

void PlayerController::updateGround(const bool allowSnap) {
    const float probeDistance = grounded_ && allowSnap
        ? settings_.groundSnapDistance : settings_.groundProbeDistance;
    CollisionSweepHit hit;
    const glm::vec3 downward{0.0F, -probeDistance, 0.0F};
    const bool wasGrounded = grounded_;
    diagnostics_.groundProbeHit = collisionWorld_->sweepCapsule(
        capsule(), downward, hit, &diagnostics_.collision,
        std::cos(glm::radians(settings_.maximumSlopeAngleDegrees)));
    grounded_ = diagnostics_.groundProbeHit && isWalkable(hit.normal) && velocity_.y <= 0.0F;
    if (grounded_) {
        groundNormal_ = hit.normal;
        groundDistance_ = hit.fraction * probeDistance;
        if (allowSnap && wasGrounded && groundDistance_ > settings_.contactOffset) {
            position_ += downward * std::max(0.0F, hit.fraction -
                settings_.contactOffset / probeDistance);
        }
        velocity_.y = 0.0F;
    } else {
        groundDistance_ = probeDistance;
    }
}

bool PlayerController::toggleNoclip() {
    if (noclip_) {
        if (collisionWorld_ == nullptr || collisionWorld_->empty()) {
            std::cerr << "Cannot enable collision mode: the scene has no collision geometry.\n";
            return false;
        }
        if (!recoverPenetration()) {
            position_ = lastValidPosition_;
            if (!recoverPenetration()) {
                std::cerr << "Cannot enable collision mode at the current position.\n";
                return false;
            }
        }
        noclip_ = false;
        velocity_ = {};
        updateGround(true);
    } else {
        noclip_ = true;
        velocity_ = {};
    }
    std::cout << "Movement mode: " << (noclip_ ? "noclip" : "collision") << '\n';
    return true;
}

bool PlayerController::recoverPenetration() {
    float totalCorrection = 0.0F;
    for (int iteration = 0; iteration < settings_.maximumPenetrationIterations; ++iteration) {
        glm::vec3 normal{};
        float depth = 0.0F;
        if (!collisionWorld_->overlapCapsule(capsule(), normal, depth,
                                             &diagnostics_.collision)) {
            return true;
        }
        const float correctionRemaining =
            settings_.maximumPenetrationCorrection - totalCorrection;
        if (correctionRemaining <= 0.0F) {
            break;
        }
        const float correction = std::min(depth + settings_.skinWidth,
                                          correctionRemaining);
        position_ += normal * correction;
        totalCorrection += correction;
    }
    glm::vec3 normal{};
    float depth = 0.0F;
    const bool resolved = !collisionWorld_->overlapCapsule(capsule(), normal, depth);
    if (!resolved) {
        std::cerr << "Warning: player penetration remains after recovery limit.\n";
    }
    return resolved;
}

bool PlayerController::isWalkable(const glm::vec3& normal) const noexcept {
    return isWalkableNormal(normal, settings_.maximumSlopeAngleDegrees);
}

void PlayerController::addContact(const glm::vec3& point, const glm::vec3& normal) noexcept {
    if (diagnostics_.contactCount < diagnostics_.contactPoints.size()) {
        diagnostics_.contactPoints[diagnostics_.contactCount] = point;
        diagnostics_.contactNormals[diagnostics_.contactCount] = normal;
        ++diagnostics_.contactCount;
    }
}

const glm::vec3& PlayerController::position() const noexcept { return position_; }
glm::vec3 PlayerController::cameraPosition() const noexcept {
    return position_ + glm::vec3{0.0F, settings_.eyeHeight, 0.0F};
}
const glm::vec3& PlayerController::velocity() const noexcept { return velocity_; }
bool PlayerController::isGrounded() const noexcept { return grounded_; }
const glm::vec3& PlayerController::groundNormal() const noexcept { return groundNormal_; }
float PlayerController::groundDistance() const noexcept { return groundDistance_; }
bool PlayerController::isNoclip() const noexcept { return noclip_; }
const PlayerSettings& PlayerController::settings() const noexcept { return settings_; }
const PlayerDiagnostics& PlayerController::diagnostics() const noexcept { return diagnostics_; }
Capsule PlayerController::capsule() const noexcept {
    return Capsule{position_, settings_.capsuleHeight, settings_.capsuleRadius};
}
