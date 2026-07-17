#pragma once

#include <filesystem>

class Scene;

class GltfLoader {
public:
    [[nodiscard]] bool loadGlb(const std::filesystem::path& path, Scene& scene) const;
};
