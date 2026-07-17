#include "scene/Camera.hpp"

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

void Camera::update(const InputState& input, const float deltaTimeSeconds,
                    const bool mouseCaptured) {
    updateLook(input, mouseCaptured);

    // Retained for the Phase 2 camera test and standalone noclip use. Phase 3
    // movement is driven by PlayerController and calls updateLook directly.
    glm::vec3 movement{};
    if (input.isDown(SDL_SCANCODE_W)) {
        movement += forward();
    }
    if (input.isDown(SDL_SCANCODE_S)) {
        movement -= forward();
    }
    if (input.isDown(SDL_SCANCODE_D)) {
        movement += right();
    }
    if (input.isDown(SDL_SCANCODE_A)) {
        movement -= right();
    }
    if (input.isDown(SDL_SCANCODE_SPACE)) {
        movement += glm::vec3{0.0F, 1.0F, 0.0F};
    }
    if (input.isDown(SDL_SCANCODE_LCTRL)) {
        movement -= glm::vec3{0.0F, 1.0F, 0.0F};
    }
    const float length = glm::length(movement);
    if (length > 0.0F) {
        const float sprint = input.isDown(SDL_SCANCODE_LSHIFT) ? sprintMultiplier_ : 1.0F;
        position_ += (movement / length) * movementSpeed_ * sprint * deltaTimeSeconds;
    }
}

void Camera::updateLook(const InputState& input, const bool mouseCaptured) {
    if (mouseCaptured) {
        yawDegrees_ += input.mouseDeltaX() * mouseSensitivity_;
        pitchDegrees_ -= input.mouseDeltaY() * mouseSensitivity_;
        pitchDegrees_ = glm::clamp(pitchDegrees_, -89.0F, 89.0F);
    }

}

void Camera::setAspectRatio(const float aspectRatio) noexcept {
    if (aspectRatio > 0.0F) {
        aspectRatio_ = aspectRatio;
    }
}

void Camera::setPosition(const glm::vec3& position) noexcept { position_ = position; }
void Camera::setYawDegrees(const float yawDegrees) noexcept { yawDegrees_ = yawDegrees; }
void Camera::setPitchDegrees(const float pitchDegrees) noexcept {
    pitchDegrees_ = glm::clamp(pitchDegrees, -89.0F, 89.0F);
}
float Camera::yawDegrees() const noexcept { return yawDegrees_; }
float Camera::pitchDegrees() const noexcept { return pitchDegrees_; }

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position_, position_ + forward(), up());
}

glm::mat4 Camera::projectionMatrix() const {
    return glm::perspective(glm::radians(verticalFovDegrees_), aspectRatio_,
                            nearPlane_, farPlane_);
}

const glm::vec3& Camera::position() const noexcept {
    return position_;
}

glm::vec3 Camera::forward() const {
    const float yaw = glm::radians(yawDegrees_);
    const float pitch = glm::radians(pitchDegrees_);
    return glm::normalize(glm::vec3{
        glm::cos(yaw) * glm::cos(pitch),
        glm::sin(pitch),
        glm::sin(yaw) * glm::cos(pitch),
    });
}

glm::vec3 Camera::right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3{0.0F, 1.0F, 0.0F}));
}

glm::vec3 Camera::up() const {
    return glm::normalize(glm::cross(right(), forward()));
}
