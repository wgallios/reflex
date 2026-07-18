#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace reflex::assets {

enum class AssetType { Level, Model, Texture, Sound, Music, WeaponDefinitions,
                       EnemyDefinitions, Navigation };

struct AssetEntry {
    std::string id;
    AssetType type{AssetType::Model};
    std::filesystem::path path;
};

class AssetManifest {
public:
    [[nodiscard]] bool load(const std::filesystem::path& path, std::string& error);
    [[nodiscard]] const AssetEntry* find(std::string_view id) const noexcept;
    [[nodiscard]] const std::vector<AssetEntry>& entries() const noexcept { return entries_; }
private:
    std::vector<AssetEntry> entries_;
    std::unordered_map<std::string, std::size_t> lookup_;
};

[[nodiscard]] const char* assetTypeName(AssetType type) noexcept;

} // namespace reflex::assets
