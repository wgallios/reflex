#pragma once

#include <glm/vec3.hpp>

struct Material {
    glm::vec3 baseColorFactor{1.0F};
    int baseColorTexture{-1};
    bool hasBaseColorTexture{false};
    bool doubleSided{false};
};
