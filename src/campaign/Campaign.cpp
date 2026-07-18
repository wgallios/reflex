#include "campaign/Campaign.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_set>

namespace reflex::campaign {
namespace {
using json = nlohmann::json;

std::optional<ObjectiveType> objectiveType(const std::string& value) {
    if (value == "ReachLocation") return ObjectiveType::ReachLocation;
    if (value == "Interact") return ObjectiveType::Interact;
    if (value == "CollectItem") return ObjectiveType::CollectItem;
    if (value == "KillEntity") return ObjectiveType::KillEntity;
    if (value == "ClearEncounter") return ObjectiveType::ClearEncounter;
    if (value == "ExitLevel") return ObjectiveType::ExitLevel;
    return std::nullopt;
}

bool stringArray(const json& object, const char* name, std::vector<std::string>& output) {
    if (!object.contains(name)) return true;
    if (!object[name].is_array()) return false;
    for (const json& entry : object[name]) {
        if (!entry.is_string() || entry.get_ref<const std::string&>().empty()) return false;
        output.push_back(entry.get<std::string>());
    }
    return true;
}
} // namespace

bool ObjectiveSystem::initialize(std::vector<ObjectiveDefinition> definitions, std::string& error) {
    std::unordered_map<std::string, std::size_t> lookup;
    std::unordered_map<std::string, ObjectiveProgress> states;
    for (std::size_t i = 0; i < definitions.size(); ++i) {
        const ObjectiveDefinition& definition = definitions[i];
        if (definition.id.empty() || definition.text.empty() || definition.count <= 0 ||
            !lookup.emplace(definition.id, i).second) {
            error = "objectives require unique IDs, text, and a positive count"; return false;
        }
        states.emplace(definition.id, ObjectiveProgress{
            definition.startsActive ? ObjectiveState::Active : ObjectiveState::Inactive, 0});
    }
    for (const ObjectiveDefinition& definition : definitions) {
        if (!definition.nextObjective.empty() && !lookup.contains(definition.nextObjective)) {
            error = "objective '" + definition.id + "' references unknown next objective '" +
                    definition.nextObjective + "'"; return false;
        }
    }
    definitions_ = std::move(definitions); lookup_ = std::move(lookup); states_ = std::move(states);
    return true;
}

bool ObjectiveSystem::activate(const std::string_view id) {
    const auto found = states_.find(std::string{id});
    if (found == states_.end() || found->second.state == ObjectiveState::Completed) return false;
    found->second.state = ObjectiveState::Active; return true;
}

bool ObjectiveSystem::progress(const std::string_view id, const int amount) {
    const auto index = lookup_.find(std::string{id});
    if (index == lookup_.end() || amount <= 0) return false;
    ObjectiveProgress& progressValue = states_.at(index->first);
    if (progressValue.state != ObjectiveState::Active) return false;
    progressValue.count = std::min(definitions_[index->second].count, progressValue.count + amount);
    return progressValue.count >= definitions_[index->second].count ? complete(id) : true;
}

bool ObjectiveSystem::complete(const std::string_view id) {
    const auto index = lookup_.find(std::string{id});
    if (index == lookup_.end()) return false;
    ObjectiveProgress& progressValue = states_.at(index->first);
    if (progressValue.state == ObjectiveState::Completed) return false;
    progressValue.state = ObjectiveState::Completed;
    progressValue.count = definitions_[index->second].count;
    if (!definitions_[index->second].nextObjective.empty()) {
        static_cast<void>(activate(definitions_[index->second].nextObjective));
    }
    return true;
}

const ObjectiveDefinition* ObjectiveSystem::current() const noexcept {
    const auto found = std::find_if(definitions_.begin(), definitions_.end(), [&](const ObjectiveDefinition& definition) {
        const auto progressValue = states_.find(definition.id);
        return progressValue != states_.end() && progressValue->second.state == ObjectiveState::Active;
    });
    return found == definitions_.end() ? nullptr : &*found;
}

const ObjectiveProgress* ObjectiveSystem::state(const std::string_view id) const noexcept {
    const auto found = states_.find(std::string{id}); return found == states_.end() ? nullptr : &found->second;
}

bool ObjectiveSystem::restore(const std::unordered_map<std::string, ObjectiveProgress>& states,
                              std::string& error) {
    for (const auto& [id, progressValue] : states) {
        const auto found = lookup_.find(id);
        if (found == lookup_.end()) continue;
        if (progressValue.count < 0 || progressValue.count > definitions_[found->second].count) {
            error = "objective '" + id + "' has invalid saved progress"; return false;
        }
    }
    for (const auto& [id, progressValue] : states) if (states_.contains(id)) states_[id] = progressValue;
    return true;
}

bool EncounterSystem::initialize(std::vector<EncounterDefinition> definitions, std::string& error) {
    std::unordered_map<std::string, std::size_t> lookup;
    std::unordered_map<std::string, EncounterRuntime> states;
    for (std::size_t i = 0; i < definitions.size(); ++i) {
        if (definitions[i].id.empty() || definitions[i].waves.empty() ||
            !lookup.emplace(definitions[i].id, i).second) {
            error = "encounters require unique IDs and at least one wave"; return false;
        }
        for (const EncounterWave& wave : definitions[i].waves) {
            if (!std::isfinite(wave.delay) || wave.delay < 0.0F || wave.activateGroups.empty()) {
                error = "encounter waves require a nonnegative delay and enemy groups"; return false;
            }
        }
        states.emplace(definitions[i].id, EncounterRuntime{});
    }
    definitions_ = std::move(definitions); lookup_ = std::move(lookup); states_ = std::move(states);
    return true;
}

bool EncounterSystem::start(const std::string_view id) {
    const auto found = states_.find(std::string{id});
    if (found == states_.end() || found->second.state != EncounterState::Inactive) return false;
    found->second.state = EncounterState::Starting; found->second.wave = 0; found->second.timer = 0.0F;
    return true;
}

bool EncounterSystem::startForTrigger(const std::string_view trigger) {
    bool started = false;
    for (const EncounterDefinition& definition : definitions_)
        if (definition.startTrigger == trigger) started = start(definition.id) || started;
    return started;
}

void EncounterSystem::update(const float deltaTime,
                             const std::unordered_set<std::string>& livingGroups,
                             std::vector<std::string>& groupsToActivate,
                             std::vector<std::string>& completedEncounters) {
    for (const EncounterDefinition& definition : definitions_) {
        EncounterRuntime& runtime = states_.at(definition.id);
        if (runtime.state == EncounterState::Inactive || runtime.state == EncounterState::Completed ||
            runtime.state == EncounterState::Failed) continue;
        runtime.timer += std::max(0.0F, deltaTime);
        const EncounterWave& wave = definition.waves[runtime.wave];
        if ((runtime.state == EncounterState::Starting || runtime.state == EncounterState::WaitingForNextWave) &&
            runtime.timer >= wave.delay) {
            groupsToActivate.insert(groupsToActivate.end(), wave.activateGroups.begin(), wave.activateGroups.end());
            runtime.state = EncounterState::Active; runtime.timer = 0.0F;
        }
        if (runtime.state != EncounterState::Active) continue;
        const bool alive = std::any_of(wave.activateGroups.begin(), wave.activateGroups.end(),
            [&](const std::string& group) { return livingGroups.contains(group); });
        if (alive) continue;
        if (runtime.wave + 1 < definition.waves.size()) {
            ++runtime.wave; runtime.state = EncounterState::WaitingForNextWave; runtime.timer = 0.0F;
        } else {
            runtime.state = EncounterState::Completed;
            if (!runtime.completionDelivered) {
                completedEncounters.push_back(definition.id); runtime.completionDelivered = true;
            }
        }
    }
}

const EncounterRuntime* EncounterSystem::state(const std::string_view id) const noexcept {
    const auto found = states_.find(std::string{id}); return found == states_.end() ? nullptr : &found->second;
}
const EncounterDefinition* EncounterSystem::definition(const std::string_view id) const noexcept {
    const auto found = lookup_.find(std::string{id});
    return found == lookup_.end() ? nullptr : &definitions_[found->second];
}

bool EncounterSystem::restore(const std::unordered_map<std::string, EncounterRuntime>& states,
                              std::string& error) {
    for (const auto& [id, runtime] : states) {
        const auto definition = lookup_.find(id);
        if (definition == lookup_.end()) continue;
        if (runtime.wave >= definitions_[definition->second].waves.size() ||
            !std::isfinite(runtime.timer) || runtime.timer < 0.0F) {
            error = "encounter '" + id + "' has invalid saved wave or timer"; return false;
        }
    }
    for (const auto& [id, runtime] : states) if (states_.contains(id)) states_[id] = runtime;
    return true;
}

bool loadLevelDefinition(const std::filesystem::path& path, LevelDefinition& level,
                         std::string& error) {
    std::ifstream stream{path};
    if (!stream) { error = "level definition not found: " + path.string(); return false; }
    json root;
    try { stream >> root; } catch (const json::exception& exception) {
        error = std::string{"invalid level JSON: "} + exception.what(); return false;
    }
    if (!root.is_object() || root.value("format_version", 0) != 1) {
        error = "level definition has unsupported format_version"; return false;
    }
    LevelDefinition parsed;
    parsed.id = root.value("id", std::string{}); parsed.displayName = root.value("display_name", std::string{});
    parsed.scene = root.value("scene", std::string{}); parsed.navigation = root.value("navigation", std::string{});
    parsed.audioManifest = root.value("audio_manifest", std::string{}); parsed.music = root.value("music", std::string{});
    parsed.nextLevel = root.value("next_level", std::string{});
    parsed.nextLevelDefinition = root.value("next_level_definition", std::string{});
    if (parsed.id.empty() || parsed.displayName.empty() || parsed.scene.empty()) {
        error = "level definition requires id, display_name, and scene"; return false;
    }
    try {
        for (const json& value : root.value("objectives", json::array())) {
            const auto type = objectiveType(value.value("type", std::string{}));
            if (!type) { error = "unknown objective type"; return false; }
            parsed.objectives.push_back({value.value("id", std::string{}), *type,
                value.value("text", std::string{}), value.value("target", std::string{}),
                value.value("count", 1), value.value("starts_active", false),
                value.value("next_objective", std::string{})});
        }
        for (const json& value : root.value("encounters", json::array())) {
            EncounterDefinition encounter;
            encounter.id = value.value("id", std::string{});
            encounter.startTrigger = value.value("start_trigger", std::string{});
            if (!stringArray(value, "lock_doors", encounter.lockDoors)) { error = "invalid encounter lock_doors"; return false; }
            for (const json& waveValue : value.value("waves", json::array())) {
                EncounterWave wave; wave.delay = waveValue.value("delay", 0.0F);
                if (!stringArray(waveValue, "activate_groups", wave.activateGroups)) { error = "invalid encounter wave groups"; return false; }
                encounter.waves.push_back(std::move(wave));
            }
            if (value.contains("completion") && value["completion"].is_object()) {
                const json& completion = value["completion"];
                if (!stringArray(completion, "open_doors", encounter.openDoorsOnComplete)) { error = "invalid completion doors"; return false; }
                encounter.completeObjective = completion.value("complete_objective", std::string{});
                encounter.activateCheckpoint = completion.value("activate_checkpoint", std::string{});
            }
            parsed.encounters.push_back(std::move(encounter));
        }
    } catch (const json::exception& exception) {
        error = std::string{"invalid level field: "} + exception.what(); return false;
    }
    ObjectiveSystem objectiveValidation;
    EncounterSystem encounterValidation;
    if (!objectiveValidation.initialize(parsed.objectives, error) ||
        !encounterValidation.initialize(parsed.encounters, error)) return false;
    level = std::move(parsed); return true;
}

} // namespace reflex::campaign
