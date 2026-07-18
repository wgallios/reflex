#pragma once

#include "gameplay/GameplayWorld.hpp"
#include "combat/CombatSystem.hpp"
#include "campaign/Campaign.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct SavedEntityState {
    bool active{true};
    bool completed{false};
    bool collected{false};
    bool activated{false};
    bool locked{false};
    DoorState doorState{DoorState::Closed};
    float doorProgress{0.0F};
};

struct SaveGameData {
    static constexpr int currentFormatVersion = 3;
    int formatVersion{currentFormatVersion};
    std::string levelPath;
    glm::vec3 playerPosition{};
    float playerYaw{0.0F};
    float playerPitch{0.0F};
    int health{100};
    int armor{0};
    std::vector<std::string> keys;
    RespawnPoint checkpoint{};
    std::unordered_map<std::string, SavedEntityState> entities;
    CombatSaveState combat;
    std::string campaignLevelId;
    std::unordered_map<std::string, reflex::campaign::ObjectiveProgress> objectives;
    std::unordered_map<std::string, reflex::campaign::EncounterRuntime> encounters;
};

class SaveGame {
public:
    [[nodiscard]] static SaveGameData capture(const std::string& levelPath,
                                              const GameplayWorld& gameplay,
                                              const glm::vec3& playerPosition,
                                              float yaw, float pitch,
                                              const CombatSystem* combat = nullptr,
                                              const reflex::campaign::ObjectiveSystem* objectives = nullptr,
                                              const reflex::campaign::EncounterSystem* encounters = nullptr,
                                              std::string campaignLevelId = {});
    [[nodiscard]] static bool write(const std::filesystem::path& path,
                                    const SaveGameData& data, std::string& error);
    [[nodiscard]] static std::optional<SaveGameData> read(
        const std::filesystem::path& path, std::string& error);
    [[nodiscard]] static bool apply(const SaveGameData& data, GameplayWorld& gameplay,
                                    std::string& error,
                                    CombatSystem* combat = nullptr,
                                    reflex::campaign::ObjectiveSystem* objectives = nullptr,
                                    reflex::campaign::EncounterSystem* encounters = nullptr);
};
