#pragma once

#include "input/InputState.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Camera {
public:
    void update(const InputState& input, float deltaTimeSeconds, bool mouseCaptured);
    void updateLook(const InputState& input, bool mouseCaptured);
    void setAspectRatio(float aspectRatio) noexcept;
    void setPosition(const glm::vec3& position) noexcept;
    void setYawDegrees(float yawDegrees) noexcept;
    [[nodiscard]] float yawDegrees() const noexcept;

    [[nodiscard]] glm::mat4 viewMatrix() const;
    [[nodiscard]] glm::mat4 projectionMatrix() const;
    [[nodiscard]] const glm::vec3& position() const noexcept;
    [[nodiscard]] glm::vec3 forward() const;
    [[nodiscard]] glm::vec3 right() const;
    [[nodiscard]] glm::vec3 up() const;

private:
    glm::vec3 position_{0.0F, 1.6F, 5.0F};
    float yawDegrees_{-90.0F};
    float pitchDegrees_{0.0F};
    float verticalFovDegrees_{72.0F};
    float nearPlane_{0.05F};
    float farPlane_{1000.0F};
    float aspectRatio_{16.0F / 9.0F};
    float movementSpeed_{4.0F};
    float sprintMultiplier_{3.0F};
    float mouseSensitivity_{0.1F};
};
