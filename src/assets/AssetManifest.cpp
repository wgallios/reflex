#include "assets/AssetManifest.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <optional>

namespace reflex::assets {
namespace {
std::optional<AssetType> parseType(const std::string& value) {
    if (value == "level") return AssetType::Level;
    if (value == "model") return AssetType::Model;
    if (value == "texture") return AssetType::Texture;
    if (value == "sound") return AssetType::Sound;
    if (value == "music") return AssetType::Music;
    if (value == "weapons") return AssetType::WeaponDefinitions;
    if (value == "enemies") return AssetType::EnemyDefinitions;
    if (value == "navigation") return AssetType::Navigation;
    return std::nullopt;
}
}

bool AssetManifest::load(const std::filesystem::path& path, std::string& error) {
    std::ifstream stream{path};
    if (!stream) { error = "asset manifest not found: " + path.string(); return false; }
    nlohmann::json root;
    try { stream >> root; } catch (const nlohmann::json::exception& exception) {
        error = std::string{"invalid asset manifest JSON: "} + exception.what(); return false;
    }
    if (!root.is_object() || root.value("format_version", 0) != 1 ||
        !root.contains("assets") || !root["assets"].is_array()) {
        error = "asset manifest requires format_version 1 and an assets array"; return false;
    }
    std::vector<AssetEntry> entries;
    std::unordered_map<std::string, std::size_t> lookup;
    try {
        for (const nlohmann::json& value : root["assets"]) {
            const std::string id = value.value("id", std::string{});
            const auto type = parseType(value.value("type", std::string{}));
            const std::filesystem::path assetPath = value.value("path", std::string{});
            if (id.empty() || !type || assetPath.empty() || !lookup.emplace(id, entries.size()).second) {
                error = "assets require unique IDs, known types, and paths"; return false;
            }
            entries.push_back({id, *type, assetPath});
        }
    } catch (const nlohmann::json::exception& exception) {
        error = std::string{"invalid asset entry: "} + exception.what(); return false;
    }
    entries_ = std::move(entries); lookup_ = std::move(lookup); return true;
}

const AssetEntry* AssetManifest::find(const std::string_view id) const noexcept {
    const auto found = lookup_.find(std::string{id}); return found == lookup_.end() ? nullptr : &entries_[found->second];
}

const char* assetTypeName(const AssetType type) noexcept {
    switch (type) {
    case AssetType::Level: return "level"; case AssetType::Model: return "model";
    case AssetType::Texture: return "texture"; case AssetType::Sound: return "sound";
    case AssetType::Music: return "music"; case AssetType::WeaponDefinitions: return "weapons";
    case AssetType::EnemyDefinitions: return "enemies"; case AssetType::Navigation: return "navigation";
    }
    return "unknown";
}

} // namespace reflex::assets
