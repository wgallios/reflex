#pragma once

#include "gameplay/GameplayTypes.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace reflex::campaign {

enum class ObjectiveType { ReachLocation, Interact, CollectItem, KillEntity, ClearEncounter, ExitLevel };
enum class ObjectiveState { Inactive, Active, Completed, Failed };

struct ObjectiveDefinition {
    std::string id;
    ObjectiveType type{ObjectiveType::ReachLocation};
    std::string text;
    std::string target;
    int count{1};
    bool startsActive{false};
    std::string nextObjective;
};

struct ObjectiveProgress {
    ObjectiveState state{ObjectiveState::Inactive};
    int count{0};
};

class ObjectiveSystem {
public:
    [[nodiscard]] bool initialize(std::vector<ObjectiveDefinition> definitions, std::string& error);
    [[nodiscard]] bool activate(std::string_view id);
    [[nodiscard]] bool progress(std::string_view id, int amount = 1);
    [[nodiscard]] bool complete(std::string_view id);
    [[nodiscard]] const ObjectiveDefinition* current() const noexcept;
    [[nodiscard]] const ObjectiveProgress* state(std::string_view id) const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, ObjectiveProgress>& states() const noexcept { return states_; }
    [[nodiscard]] bool restore(const std::unordered_map<std::string, ObjectiveProgress>& states,
                               std::string& error);
private:
    std::vector<ObjectiveDefinition> definitions_;
    std::unordered_map<std::string, std::size_t> lookup_;
    std::unordered_map<std::string, ObjectiveProgress> states_;
};

enum class EncounterState { Inactive, Starting, Active, WaitingForNextWave, Completed, Failed };

struct EncounterWave {
    float delay{0.0F};
    std::vector<std::string> activateGroups;
};

struct EncounterDefinition {
    std::string id;
    std::string startTrigger;
    std::vector<std::string> lockDoors;
    std::vector<EncounterWave> waves;
    std::vector<std::string> openDoorsOnComplete;
    std::string completeObjective;
    std::string activateCheckpoint;
};

struct EncounterRuntime {
    EncounterState state{EncounterState::Inactive};
    std::size_t wave{0};
    float timer{0.0F};
    bool completionDelivered{false};
};

class EncounterSystem {
public:
    [[nodiscard]] bool initialize(std::vector<EncounterDefinition> definitions, std::string& error);
    [[nodiscard]] bool start(std::string_view id);
    [[nodiscard]] bool startForTrigger(std::string_view trigger);
    void update(float deltaTime, const std::unordered_set<std::string>& livingGroups,
                std::vector<std::string>& groupsToActivate,
                std::vector<std::string>& completedEncounters);
    [[nodiscard]] const EncounterRuntime* state(std::string_view id) const noexcept;
    [[nodiscard]] const EncounterDefinition* definition(std::string_view id) const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, EncounterRuntime>& states() const noexcept { return states_; }
    [[nodiscard]] bool restore(const std::unordered_map<std::string, EncounterRuntime>& states,
                               std::string& error);
private:
    std::vector<EncounterDefinition> definitions_;
    std::unordered_map<std::string, std::size_t> lookup_;
    std::unordered_map<std::string, EncounterRuntime> states_;
};

struct LevelDefinition {
    int formatVersion{1};
    std::string id;
    std::string displayName;
    std::filesystem::path scene;
    std::filesystem::path navigation;
    std::filesystem::path audioManifest;
    std::string music;
    std::string nextLevel;
    std::filesystem::path nextLevelDefinition;
    std::vector<ObjectiveDefinition> objectives;
    std::vector<EncounterDefinition> encounters;
};

[[nodiscard]] bool loadLevelDefinition(const std::filesystem::path& path,
                                       LevelDefinition& level, std::string& error);

struct PersistentPlayerState {
    int health{100};
    int armor{0};
    std::vector<std::string> weapons;
    std::unordered_map<std::string, int> magazines;
    std::unordered_map<std::string, int> ammunition;
    std::unordered_set<std::string> persistentKeys;
    std::string selectedWeapon;
    std::unordered_set<std::string> completedObjectives;
    std::unordered_set<std::string> completedEncounters;
};

} // namespace reflex::campaign
