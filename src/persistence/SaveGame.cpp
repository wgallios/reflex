#include "persistence/SaveGame.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <utility>

namespace {
using json = nlohmann::json;

const char* stateName(const DoorState state) {
    switch (state) {
    case DoorState::Closed: return "closed";
    case DoorState::Opening: return "opening";
    case DoorState::Open: return "open";
    case DoorState::Closing: return "closing";
    case DoorState::Blocked: return "blocked";
    }
    return "closed";
}

std::optional<DoorState> parseState(const std::string& state) {
    if (state == "closed") return DoorState::Closed;
    if (state == "opening") return DoorState::Opening;
    if (state == "open") return DoorState::Open;
    if (state == "closing") return DoorState::Closing;
    if (state == "blocked") return DoorState::Blocked;
    return std::nullopt;
}

json vectorJson(const glm::vec3& value) { return json::array({value.x, value.y, value.z}); }

bool parseVector(const json& value, glm::vec3& output) {
    if (!value.is_array() || value.size() != 3) return false;
    for (std::size_t i = 0; i < 3; ++i) {
        if (!value[i].is_number()) return false;
        output[static_cast<int>(i)] = value[i].get<float>();
        if (!std::isfinite(output[static_cast<int>(i)])) return false;
    }
    return true;
}
} // namespace

SaveGameData SaveGame::capture(const std::string& levelPath, const GameplayWorld& gameplay,
                               const glm::vec3& playerPosition, const float yaw,
                               const float pitch, const CombatSystem* combat,
                               const reflex::campaign::ObjectiveSystem* objectives,
                               const reflex::campaign::EncounterSystem* encounters,
                               std::string campaignLevelId) {
    SaveGameData data;
    data.levelPath = levelPath;
    data.playerPosition = playerPosition;
    data.playerYaw = yaw;
    data.playerPitch = pitch;
    data.health = gameplay.vitals().health;
    data.armor = gameplay.vitals().armor;
    data.keys = gameplay.inventory().sortedKeys();
    data.checkpoint = gameplay.checkpoint();
    for (const GameplayEntity& entity : gameplay.entities()) {
        data.entities.emplace(entity.authored.name, SavedEntityState{
            entity.active, entity.completed, entity.collected, entity.activated,
            entity.locked, entity.doorState, entity.doorProgress});
    }
    if (combat != nullptr) data.combat = combat->captureState();
    data.campaignLevelId = std::move(campaignLevelId);
    if (objectives != nullptr) data.objectives = objectives->states();
    if (encounters != nullptr) data.encounters = encounters->states();
    return data;
}

bool SaveGame::write(const std::filesystem::path& path, const SaveGameData& data,
                     std::string& error) {
    json root;
    root["format_version"] = data.formatVersion;
    root["level"] = data.levelPath;
    root["player"] = {{"position", vectorJson(data.playerPosition)},
                      {"yaw", data.playerYaw}, {"pitch", data.playerPitch},
                      {"health", data.health}, {"armor", data.armor}, {"keys", data.keys}};
    root["checkpoint"] = {{"id", data.checkpoint.checkpoint},
                           {"position", vectorJson(data.checkpoint.position)},
                           {"yaw", data.checkpoint.yawDegrees},
                           {"health", data.checkpoint.health},
                           {"armor", data.checkpoint.armor}};
    root["entities"] = json::object();
    for (const auto& [name, state] : data.entities) {
        root["entities"][name] = {{"active", state.active}, {"completed", state.completed},
            {"collected", state.collected}, {"activated", state.activated},
            {"locked", state.locked}, {"door_state", stateName(state.doorState)},
            {"door_progress", state.doorProgress}};
    }
    root["combat"] = json::object();
    root["campaign_level_id"] = data.campaignLevelId;
    root["objectives"] = json::object();
    for (const auto& [id, progress] : data.objectives) {
        root["objectives"][id] = {{"state", static_cast<int>(progress.state)}, {"count", progress.count}};
    }
    root["encounters"] = json::object();
    for (const auto& [id, runtime] : data.encounters) {
        root["encounters"][id] = {{"state", static_cast<int>(runtime.state)},
            {"wave", runtime.wave}, {"timer", runtime.timer},
            {"completion_delivered", runtime.completionDelivered}};
    }
    root["combat"]["equipped_weapon"] = data.combat.equippedWeapon;
    root["combat"]["weapons"] = json::array();
    for (const SavedWeaponState& weapon : data.combat.weapons) {
        root["combat"]["weapons"].push_back({{"id", weapon.id}, {"magazine", weapon.magazine}});
    }
    root["combat"]["ammunition"] = data.combat.ammunition;
    root["combat"]["enemies"] = json::array();
    for (const SavedEnemyState& enemy : data.combat.enemies) {
        root["combat"]["enemies"].push_back({{"name", enemy.name}, {"health", enemy.health},
            {"position", vectorJson(enemy.position)}, {"active", enemy.active}});
    }
    std::error_code directoryError;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, directoryError);
    if (directoryError) {
        error = "could not create save directory: " + directoryError.message();
        return false;
    }
    std::ofstream stream{path};
    if (!stream) { error = "could not open save file for writing"; return false; }
    stream << root.dump(2) << '\n';
    if (!stream) { error = "failed while writing save file"; return false; }
    return true;
}

std::optional<SaveGameData> SaveGame::read(const std::filesystem::path& path,
                                           std::string& error) {
    std::ifstream stream{path};
    if (!stream) { error = "save file not found: " + path.string(); return std::nullopt; }
    json root;
    try { stream >> root; } catch (const json::exception& exception) {
        error = std::string{"invalid JSON: "} + exception.what(); return std::nullopt;
    }
    if (!root.is_object() || !root.contains("format_version") ||
        !root["format_version"].is_number_integer()) {
        error = "save has no valid format_version"; return std::nullopt;
    }
    SaveGameData data;
    data.formatVersion = root["format_version"].get<int>();
    if (data.formatVersion != SaveGameData::currentFormatVersion) {
        error = "unsupported save format version " + std::to_string(data.formatVersion);
        return std::nullopt;
    }
    try {
        if (!root.contains("level") || !root["level"].is_string() ||
            !root.contains("player") || !root["player"].is_object()) {
            error = "save is missing required level or player data"; return std::nullopt;
        }
        data.levelPath = root["level"].get<std::string>();
        data.campaignLevelId = root.value("campaign_level_id", std::string{});
        const json& player = root["player"];
        if (!player.contains("position") || !parseVector(player["position"], data.playerPosition)) {
            error = "save has an invalid player position"; return std::nullopt;
        }
        data.playerYaw = player.value("yaw", 0.0F);
        data.playerPitch = player.value("pitch", 0.0F);
        data.health = std::clamp(player.value("health", 100), 0, 100);
        data.armor = std::clamp(player.value("armor", 0), 0, 100);
        data.keys = player.value("keys", std::vector<std::string>{});
        for (const std::string& key : data.keys) {
            if (key.empty() || key.size() > 128) { error = "save contains an invalid key ID"; return std::nullopt; }
        }
        if (root.contains("checkpoint") && root["checkpoint"].is_object()) {
            const json& checkpoint = root["checkpoint"];
            data.checkpoint.checkpoint = checkpoint.value("id", EntityId{0});
            if (checkpoint.contains("position") &&
                !parseVector(checkpoint["position"], data.checkpoint.position)) {
                error = "save has an invalid checkpoint position"; return std::nullopt;
            }
            data.checkpoint.yawDegrees = checkpoint.value("yaw", -90.0F);
            data.checkpoint.health = std::clamp(checkpoint.value("health", 100), 1, 100);
            data.checkpoint.armor = std::clamp(checkpoint.value("armor", 0), 0, 100);
        }
        if (root.contains("entities") && root["entities"].is_object()) {
            for (const auto& [name, value] : root["entities"].items()) {
                if (!value.is_object()) continue;
                SavedEntityState state;
                state.active = value.value("active", true);
                state.completed = value.value("completed", false);
                state.collected = value.value("collected", false);
                state.activated = value.value("activated", false);
                state.locked = value.value("locked", false);
                state.doorProgress = std::clamp(value.value("door_progress", 0.0F), 0.0F, 1.0F);
                const auto parsed = parseState(value.value("door_state", std::string{"closed"}));
                if (!parsed) { error = "entity '" + name + "' has an invalid door state"; return std::nullopt; }
                state.doorState = *parsed;
                data.entities.emplace(name, state);
            }
        }
        if (root.contains("combat")) {
            const json& combat = root["combat"];
            if (!combat.is_object()) { error = "save combat section is not an object"; return std::nullopt; }
            data.combat.equippedWeapon = combat.value("equipped_weapon", std::string{});
            if (combat.contains("weapons")) {
                if (!combat["weapons"].is_array()) { error = "save weapons are invalid"; return std::nullopt; }
                for (const json& weapon : combat["weapons"]) {
                    if (!weapon.is_object() || !weapon.contains("id") || !weapon["id"].is_string() ||
                        !weapon.contains("magazine") || !weapon["magazine"].is_number_integer()) {
                        error = "save contains an invalid weapon"; return std::nullopt;
                    }
                    data.combat.weapons.push_back({weapon["id"].get<std::string>(),
                                                   weapon["magazine"].get<int>()});
                }
            }
            if (combat.contains("ammunition")) {
                if (!combat["ammunition"].is_object()) { error = "save ammunition is invalid"; return std::nullopt; }
                for (const auto& [name, value] : combat["ammunition"].items()) {
                    if (!value.is_number_integer()) { error = "save ammunition count is invalid"; return std::nullopt; }
                    data.combat.ammunition.emplace(name, value.get<int>());
                }
            }
            if (combat.contains("enemies")) {
                if (!combat["enemies"].is_array()) { error = "save enemies are invalid"; return std::nullopt; }
                for (const json& enemy : combat["enemies"]) {
                    SavedEnemyState saved;
                    if (!enemy.is_object() || !enemy.contains("name") || !enemy["name"].is_string() ||
                        !enemy.contains("health") || !enemy["health"].is_number_integer() ||
                        !enemy.contains("position") || !parseVector(enemy["position"], saved.position)) {
                        error = "save contains an invalid enemy"; return std::nullopt;
                    }
                    saved.name = enemy["name"].get<std::string>();
                    saved.health = enemy["health"].get<int>();
                    saved.active = enemy.value("active", true);
                    data.combat.enemies.push_back(std::move(saved));
                }
            }
        }
        if (root.contains("objectives")) {
            if (!root["objectives"].is_object()) { error = "save objectives are invalid"; return std::nullopt; }
            for (const auto& [id, value] : root["objectives"].items()) {
                const int state = value.value("state", 0);
                const int count = value.value("count", 0);
                if (!value.is_object() || state < 0 || state > 3 || count < 0) {
                    error = "save contains invalid objective state"; return std::nullopt;
                }
                data.objectives.emplace(id, reflex::campaign::ObjectiveProgress{
                    static_cast<reflex::campaign::ObjectiveState>(state), count});
            }
        }
        if (root.contains("encounters")) {
            if (!root["encounters"].is_object()) { error = "save encounters are invalid"; return std::nullopt; }
            for (const auto& [id, value] : root["encounters"].items()) {
                const int state = value.value("state", 0);
                if (!value.is_object() || state < 0 || state > 5) {
                    error = "save contains invalid encounter state"; return std::nullopt;
                }
                reflex::campaign::EncounterRuntime runtime;
                runtime.state = static_cast<reflex::campaign::EncounterState>(state);
                runtime.wave = value.value("wave", std::size_t{0});
                runtime.timer = std::max(0.0F, value.value("timer", 0.0F));
                runtime.completionDelivered = value.value("completion_delivered", false);
                data.encounters.emplace(id, runtime);
            }
        }
    } catch (const json::exception& exception) {
        error = std::string{"invalid save value: "} + exception.what(); return std::nullopt;
    }
    return data;
}

bool SaveGame::apply(const SaveGameData& data, GameplayWorld& gameplay, std::string& error,
                     CombatSystem* combat, reflex::campaign::ObjectiveSystem* objectives,
                     reflex::campaign::EncounterSystem* encounters) {
    if (data.formatVersion != SaveGameData::currentFormatVersion) {
        error = "unsupported save format version"; return false;
    }
    if (combat != nullptr && !combat->validateState(data.combat, error)) return false;
    gameplay.reset();
    gameplay.inventory().clear();
    for (const std::string& key : data.keys) static_cast<void>(gameplay.inventory().addKey(key));
    gameplay.vitals().reset(std::max(1, data.health), data.armor);
    if (data.health <= 0) gameplay.vitals().health = 1;
    gameplay.restoreCheckpoint(data.checkpoint);
    for (const auto& [name, saved] : data.entities) {
        GameplayEntity* entity = gameplay.findByName(name);
        if (entity == nullptr) continue; // Forward-compatible with removed authored entities.
        entity->active = saved.active;
        entity->completed = saved.completed;
        entity->collected = saved.collected;
        entity->activated = saved.activated;
        entity->locked = saved.locked;
        entity->doorState = saved.doorState;
        entity->doorProgress = saved.doorProgress;
    }
    gameplay.clearPendingEvents();
    gameplay.synchronizePersistentState();
    if (combat != nullptr && !combat->restoreState(data.combat, error)) return false;
    if (objectives != nullptr && !objectives->restore(data.objectives, error)) return false;
    if (encounters != nullptr && !encounters->restore(data.encounters, error)) return false;
    return true;
}
