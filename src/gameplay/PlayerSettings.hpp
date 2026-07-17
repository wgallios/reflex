#pragma once

#include <iostream>

struct PlayerSettings {
    float capsuleHeight{1.8F};
    float capsuleRadius{0.35F};
    float eyeHeight{1.65F};
    float walkSpeed{5.0F};
    float sprintMultiplier{1.6F};
    float groundAcceleration{35.0F};
    float airAcceleration{8.0F};
    float maximumAirSpeed{5.0F};
    float jumpSpeed{6.0F};
    float gravity{-15.0F};
    float terminalVelocity{-45.0F};
    float maximumSlopeAngleDegrees{46.0F};
    float maximumStepHeight{0.35F};
    float stepSearchDistance{0.2F};
    float groundProbeDistance{0.08F};
    float groundSnapDistance{0.18F};
    float skinWidth{0.002F};
    float contactOffset{0.001F};
    float minimumMovementDistance{0.00001F};
    float maximumPenetrationCorrection{0.2F};
    int maximumSlideIterations{6};
    int maximumPenetrationIterations{6};

    [[nodiscard]] bool validate() const {
        const bool valid = capsuleRadius > 0.0F && capsuleHeight > capsuleRadius * 2.0F &&
            eyeHeight > 0.0F && eyeHeight < capsuleHeight && walkSpeed > 0.0F &&
            sprintMultiplier >= 1.0F && gravity < 0.0F && terminalVelocity < 0.0F &&
            maximumSlopeAngleDegrees > 0.0F && maximumSlopeAngleDegrees < 90.0F &&
            maximumStepHeight >= 0.0F && stepSearchDistance >= 0.0F && skinWidth >= 0.0F &&
            maximumSlideIterations > 0 && maximumPenetrationIterations > 0;
        if (!valid) {
            std::cerr << "Invalid player settings: check capsule, eye, movement, slope, and iteration values.\n";
        }
        return valid;
    }
};
