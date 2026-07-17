#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using EntityId = std::uint64_t;

[[nodiscard]] constexpr EntityId stableEntityId(const std::string_view name) noexcept {
    EntityId hash = 14695981039346656037ULL;
    for (const char character : name) {
        hash ^= static_cast<unsigned char>(character);
        hash *= 1099511628211ULL;
    }
    return hash == 0 ? 1 : hash;
}

enum class GameplayEntityType { Door, Switch, Trigger, Pickup, DamageVolume, Checkpoint,
                                EnemySpawn };
enum class GameplayEventType {
    Activate, Deactivate, Toggle, Open, Close, Lock, Unlock,
    TriggerEnter, TriggerStay, TriggerExit, PickupCollected,
    PlayerDamaged, CheckpointActivated, Reset
};

struct GameplayEvent {
    GameplayEvent() = default;
    GameplayEvent(GameplayEventType eventType, EntityId eventSender, EntityId eventTarget,
                  float value = 0.0F, std::string eventParameter = {})
        : type(eventType), sender(eventSender), target(eventTarget), numericValue(value),
          parameter(std::move(eventParameter)) {}
    GameplayEventType type{GameplayEventType::Activate};
    EntityId sender{0};
    EntityId target{0};
    float numericValue{0.0F};
    std::string parameter;
    std::uint64_t sequence{0};
};

struct GameplayEntityDefinition {
    GameplayEntityType type{GameplayEntityType::Trigger};
    EntityId id{0};
    std::string name;
    glm::mat4 authoredWorldTransform{1.0F};
    std::vector<std::size_t> primitiveIndices;
    std::string targetName;
    GameplayEventType enterEvent{GameplayEventType::Activate};
    GameplayEventType exitEvent{GameplayEventType::Deactivate};
    glm::vec3 boxSize{1.0F}; // Full extents.
    glm::vec3 boxOffset{0.0F};
    glm::vec3 moveAxis{0.0F, 1.0F, 0.0F};
    float moveDistance{0.0F};
    float openSpeed{1.5F};
    float closeSpeed{1.5F};
    float autoCloseDelay{0.0F};
    bool startsOpen{false};
    bool startsLocked{false};
    bool toggleMode{true};
    bool oneShot{false};
    std::string requiredKey;
    std::string pickupType;
    std::string itemId;
    std::string displayName;
    int amount{0};
    float damagePerSecond{0.0F};
    bool bypassArmor{false};
    int restoreHealth{100};
    int restoreArmor{0};
    std::string enemyType;
    bool startsActive{true};
};
