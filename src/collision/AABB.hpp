#pragma once

#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include <limits>

struct AABB {
    glm::vec3 minimum{std::numeric_limits<float>::max()};
    glm::vec3 maximum{std::numeric_limits<float>::lowest()};

    void expand(const glm::vec3& point) noexcept {
        minimum = glm::min(minimum, point);
        maximum = glm::max(maximum, point);
    }

    void expand(const AABB& other) noexcept {
        expand(other.minimum);
        expand(other.maximum);
    }

    void inflate(float amount) noexcept {
        minimum -= glm::vec3{amount};
        maximum += glm::vec3{amount};
    }

    [[nodiscard]] glm::vec3 center() const noexcept { return (minimum + maximum) * 0.5F; }

    [[nodiscard]] bool overlaps(const AABB& other) const noexcept {
        return minimum.x <= other.maximum.x && maximum.x >= other.minimum.x &&
               minimum.y <= other.maximum.y && maximum.y >= other.minimum.y &&
               minimum.z <= other.maximum.z && maximum.z >= other.minimum.z;
    }
};
