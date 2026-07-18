#include "assets/AssetManifest.hpp"
#include "audio/AudioSystem.hpp"
#include "campaign/Campaign.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {
using json = nlohmann::json;

bool readJson(const std::filesystem::path& path, json& value, std::string& error) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) { error = "file not found: " + path.string(); return false; }
    try { stream >> value; } catch (const json::exception& exception) {
        error = std::string{"invalid JSON in "} + path.string() + ": " + exception.what(); return false;
    }
    return true;
}

std::uint32_t littleU32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

bool readGlbJson(const std::filesystem::path& path, json& document, std::string& error) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) { error = "model not found: " + path.string(); return false; }
    const std::streamsize size = stream.tellg();
    if (size < 20) { error = "GLB is too small"; return false; }
    stream.seekg(0);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), size)) { error = "could not read GLB"; return false; }
    if (littleU32(bytes.data()) != 0x46546C67U || littleU32(bytes.data() + 4) != 2U ||
        littleU32(bytes.data() + 8) != bytes.size()) {
        error = "invalid glTF 2.0 binary header or length"; return false;
    }
    const std::uint32_t jsonLength = littleU32(bytes.data() + 12);
    const std::uint32_t chunkType = littleU32(bytes.data() + 16);
    if (chunkType != 0x4E4F534AU || 20ULL + jsonLength > bytes.size()) {
        error = "GLB has no valid JSON chunk"; return false;
    }
    try {
        document = json::parse(bytes.begin() + 20, bytes.begin() + 20 + jsonLength);
    } catch (const json::exception& exception) {
        error = std::string{"invalid GLB JSON: "} + exception.what(); return false;
    }
    return true;
}

bool validateModel(const std::filesystem::path& path, std::string& error) {
    json model;
    if (!readGlbJson(path, model, error)) return false;
    if (!model.contains("asset") || model["asset"].value("version", std::string{}) != "2.0") {
        error = "model does not declare glTF 2.0"; return false;
    }
    const std::size_t nodeCount = model.value("nodes", json::array()).size();
    for (const json& skin : model.value("skins", json::array())) {
        if (!skin.contains("joints") || !skin["joints"].is_array() || skin["joints"].empty()) {
            error = "skin has no joints"; return false;
        }
        if (skin["joints"].size() > 128) { error = "skin exceeds the 128-joint GPU limit"; return false; }
        std::unordered_set<int> joints;
        for (const json& value : skin["joints"]) {
            if (!value.is_number_integer() || value.get<int>() < 0 ||
                static_cast<std::size_t>(value.get<int>()) >= nodeCount || !joints.insert(value.get<int>()).second) {
                error = "skin contains an invalid or duplicate joint node"; return false;
            }
        }
        if (!skin.contains("inverseBindMatrices")) {
            std::cerr << "warning: skin has no inverseBindMatrices; identity matrices will be used\n";
        }
    }
    for (const json& mesh : model.value("meshes", json::array())) {
        for (const json& primitive : mesh.value("primitives", json::array())) {
            const int mode = primitive.value("mode", 4);
            if (mode != 4) { error = "unsupported non-triangle mesh primitive"; return false; }
            if (!primitive.contains("attributes") || !primitive["attributes"].contains("POSITION")) {
                error = "mesh primitive has no POSITION attribute"; return false;
            }
            const json& attributes = primitive["attributes"];
            if (attributes.contains("JOINTS_0") != attributes.contains("WEIGHTS_0")) {
                error = "skinned primitive must provide both JOINTS_0 and WEIGHTS_0"; return false;
            }
            if (!attributes.contains("NORMAL")) std::cerr << "warning: primitive has no normals\n";
        }
    }
    for (const json& animation : model.value("animations", json::array())) {
        if (!animation.contains("samplers") || !animation.contains("channels")) {
            error = "animation has no samplers or channels"; return false;
        }
        for (const json& channel : animation["channels"]) {
            const int sampler = channel.value("sampler", -1);
            if (sampler < 0 || static_cast<std::size_t>(sampler) >= animation["samplers"].size() ||
                !channel.contains("target") || channel["target"].value("node", -1) < 0 ||
                static_cast<std::size_t>(channel["target"].value("node", -1)) >= nodeCount) {
                error = "animation channel has an invalid sampler or target node"; return false;
            }
            const std::string target = channel["target"].value("path", std::string{});
            if (target != "translation" && target != "rotation" && target != "scale") {
                error = "unsupported animation channel target '" + target + "'"; return false;
            }
        }
    }
    std::cout << "valid model: " << path << " (skins " << model.value("skins", json::array()).size()
              << ", animations " << model.value("animations", json::array()).size() << ")\n";
    return true;
}

bool validateCombat(const std::filesystem::path& path, const char* collection, std::string& error) {
    json root;
    if (!readJson(path, root, error)) return false;
    if (!root.contains(collection) || !root[collection].is_array()) {
        error = std::string{"combat definition has no '"} + collection + "' array"; return false;
    }
    std::unordered_set<std::string> ids;
    for (const json& value : root[collection]) {
        const std::string id = value.value("id", std::string{});
        if (id.empty() || !ids.insert(id).second) { error = std::string{collection} + " contain a missing or duplicate ID"; return false; }
        const char* modelField = std::string_view{collection} == "weapons" ? "viewmodel" : "model";
        if (value.contains(modelField)) {
            if (!value[modelField].is_string()) { error = id + " has an invalid model path"; return false; }
            json model;
            if (!readGlbJson(value[modelField].get<std::string>(), model, error)) return false;
            std::unordered_set<std::string> clipNames;
            for (const json& animation : model.value("animations", json::array()))
                clipNames.insert(animation.value("name", std::string{}));
            if (value.contains("animations")) {
                if (!value["animations"].is_object()) { error = id + " animations must be an object"; return false; }
                for (const auto& [state, clip] : value["animations"].items()) {
                    if (!clip.is_string() || !clipNames.contains(clip.get<std::string>())) {
                        error = id + " state '" + state + "' references missing clip"; return false;
                    }
                }
            }
        }
    }
    std::cout << "valid " << collection << ": " << ids.size() << '\n'; return true;
}

bool validateLevel(const std::filesystem::path& path, std::string& error) {
    reflex::campaign::LevelDefinition level;
    if (!reflex::campaign::loadLevelDefinition(path, level, error)) return false;
    if (!std::filesystem::exists(level.scene)) { error = "level scene not found: " + level.scene.string(); return false; }
    if (!level.audioManifest.empty() && !std::filesystem::exists(level.audioManifest)) {
        error = "level audio manifest not found: " + level.audioManifest.string(); return false;
    }
    if (!level.nextLevel.empty() && (level.nextLevelDefinition.empty() ||
        !std::filesystem::exists(level.nextLevelDefinition))) {
        error = "next level definition is missing: " + level.nextLevelDefinition.string(); return false;
    }
    if (!level.navigation.empty()) {
        json navigation;
        if (!readJson(level.navigation, navigation, error)) return false;
        if (navigation.value("format_version", 0) != 1 ||
            navigation.value("level_id", std::string{}) != level.id ||
            navigation.value("source", std::string{}) != level.scene.string()) {
            error = "navigation cache is missing, stale, or belongs to another level"; return false;
        }
        const auto timestamp = std::filesystem::last_write_time(level.scene).time_since_epoch().count();
        if (navigation.value("source_timestamp", std::int64_t{}) != timestamp) {
            error = "navigation cache is stale; run build-navmesh again"; return false;
        }
    }
    std::cout << "valid level '" << level.id << "': " << level.objectives.size()
              << " objectives, " << level.encounters.size() << " encounters\n";
    return true;
}

bool validateAudio(const std::filesystem::path& path, std::string& error) {
    std::vector<reflex::audio::SoundDefinition> definitions;
    if (!reflex::audio::loadAudioManifest(path, definitions, error)) return false;
    for (const auto& definition : definitions) {
        if (!std::filesystem::exists(definition.file)) {
            error = "sound asset not found: " + definition.file.string(); return false;
        }
    }
    std::cout << "valid audio manifest: " << definitions.size() << " sounds\n"; return true;
}

bool buildNavMetadata(const std::filesystem::path& levelPath,
                      const std::filesystem::path& output, std::string& error) {
    reflex::campaign::LevelDefinition level;
    if (!reflex::campaign::loadLevelDefinition(levelPath, level, error)) return false;
    if (!std::filesystem::exists(level.scene)) { error = "level scene not found: " + level.scene.string(); return false; }
    const auto timestamp = std::filesystem::last_write_time(level.scene).time_since_epoch().count();
    json metadata{{"format_version", 1}, {"level_id", level.id}, {"source", level.scene.string()},
        {"source_timestamp", timestamp}, {"generation", "runtime_recast"}};
    std::ofstream stream{output};
    if (!stream) { error = "cannot write navigation metadata: " + output.string(); return false; }
    stream << metadata.dump(2) << '\n';
    std::cout << "wrote navigation cache metadata: " << output << '\n'; return true;
}

void usage() {
    std::cerr << "usage: reflex-asset-tool <command> <file> [--output <file>]\n"
              << "commands: validate-level, validate-model, build-navmesh, validate-weapons, "
                 "validate-enemies, validate-audio, validate-manifest\n";
}
} // namespace

int main(const int argc, char** argv) {
    if (argc < 3) { usage(); return 2; }
    const std::string command = argv[1];
    const std::filesystem::path input = argv[2];
    std::string error;
    bool valid = false;
    if (command == "validate-level") valid = validateLevel(input, error);
    else if (command == "validate-model") valid = validateModel(input, error);
    else if (command == "validate-weapons") valid = validateCombat(input, "weapons", error);
    else if (command == "validate-enemies") valid = validateCombat(input, "enemies", error);
    else if (command == "validate-audio") valid = validateAudio(input, error);
    else if (command == "validate-manifest") {
        reflex::assets::AssetManifest manifest; valid = manifest.load(input, error);
        if (valid) {
            for (const auto& entry : manifest.entries()) {
                if (!std::filesystem::exists(entry.path)) {
                    error = "manifest asset not found: " + entry.path.string(); valid = false; break;
                }
            }
        }
        if (valid) std::cout << "valid asset manifest: " << manifest.entries().size() << " assets\n";
    } else if (command == "build-navmesh") {
        std::filesystem::path output = input; output.replace_extension(".nav");
        if (argc == 5 && std::string{argv[3]} == "--output") output = argv[4];
        valid = buildNavMetadata(input, output, error);
    } else { usage(); return 2; }
    if (!valid) { std::cerr << "error: " << error << '\n'; return 1; }
    return 0;
}
