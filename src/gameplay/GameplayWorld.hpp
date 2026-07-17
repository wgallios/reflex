#pragma once

#include "collision/DynamicCollisionWorld.hpp"
#include "gameplay/GameplayTypes.hpp"
#include "gameplay/PlayerState.hpp"

#include <glm/mat4x4.hpp>

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class CollisionWorld;
class Scene;

enum class DoorState { Closed, Opening, Open, Closing, Blocked };

struct GameplayEntity {
    GameplayEntityDefinition authored;
    EntityId target{0};
    bool valid{true};
    bool enabled{true};
    bool active{true};
    bool completed{false};
    bool overlapping{false};
    bool collected{false};
    bool activated{false};
    bool locked{false};
    DoorState doorState{DoorState::Closed};
    float doorProgress{0.0F};
    float stateTimer{0.0F};
    float damageAccumulator{0.0F};
    glm::mat4 runtimeWorldTransform{1.0F};
};

struct GameplayMessage {
    std::string text;
    float remainingSeconds{0.0F};
    int priority{0};
};

struct RespawnPoint {
    EntityId checkpoint{0};
    glm::vec3 position{};
    float yawDegrees{-90.0F};
    int health{100};
    int armor{0};
};

class GameplayWorld {
public:
    [[nodiscard]] bool initialize(const std::vector<GameplayEntityDefinition>& definitions,
                                  Scene& scene, DynamicCollisionWorld& dynamicCollision,
                                  const RespawnPoint& fallbackCheckpoint);
    void reset();
    void fixedUpdate(float deltaTime, const Capsule& playerCapsule,
                     const glm::vec3& playerPosition, float playerYawDegrees);
    void updatePresentation(float deltaTime) noexcept;

    void updateInteraction(const glm::vec3& origin, const glm::vec3& direction,
                           const CollisionWorld& staticCollision);
    void interact();
    void queueEvent(GameplayEvent event);
    void queueDelayedEvent(GameplayEvent event, float delaySeconds);
    void processEvents();

    [[nodiscard]] GameplayEntity* find(EntityId id) noexcept;
    [[nodiscard]] const GameplayEntity* find(EntityId id) const noexcept;
    [[nodiscard]] GameplayEntity* findByName(const std::string& name) noexcept;
    [[nodiscard]] const std::vector<GameplayEntity>& entities() const noexcept;
    [[nodiscard]] std::vector<GameplayEntity>& mutableEntities() noexcept;
    [[nodiscard]] PlayerInventory& inventory() noexcept;
    [[nodiscard]] const PlayerInventory& inventory() const noexcept;
    [[nodiscard]] PlayerVitals& vitals() noexcept;
    [[nodiscard]] const PlayerVitals& vitals() const noexcept;
    [[nodiscard]] const RespawnPoint& checkpoint() const noexcept;
    [[nodiscard]] EntityId interactionTarget() const noexcept;
    [[nodiscard]] std::string interactionPrompt() const;
    [[nodiscard]] const GameplayMessage& message() const noexcept;
    [[nodiscard]] std::size_t pendingEventCount() const noexcept;
    [[nodiscard]] std::size_t delayedEventCount() const noexcept;
    void restoreCheckpoint(const RespawnPoint& checkpoint) noexcept;
    void synchronizePersistentState();
    void clearPendingEvents() noexcept;
    [[nodiscard]] std::string debugDescription(EntityId id) const;
    void showMessage(std::string text, float duration = 2.5F, int priority = 0);

private:
    void resetEntity(GameplayEntity& entity);
    void updateDoor(GameplayEntity& entity, float deltaTime, const Capsule& playerCapsule);
    void updateDoorTransform(GameplayEntity& entity);
    void updateOverlapEntity(GameplayEntity& entity, float deltaTime,
                             const Capsule& playerCapsule, const glm::vec3& playerPosition,
                             float playerYawDegrees);
    void dispatch(const GameplayEvent& event);
    void collectPickup(GameplayEntity& entity);
    [[nodiscard]] AABB entityBounds(const GameplayEntity& entity) const noexcept;
    [[nodiscard]] bool rayEntity(const GameplayEntity& entity, const glm::vec3& origin,
                                 const glm::vec3& direction, float maximumDistance,
                                 float& distance) const noexcept;
    [[nodiscard]] static const char* doorStateName(DoorState state) noexcept;

    Scene* scene_{nullptr};
    DynamicCollisionWorld* dynamicCollision_{nullptr};
    std::vector<GameplayEntity> entities_;
    std::unordered_map<EntityId, std::size_t> byId_;
    std::unordered_map<std::string, EntityId> byName_;
    std::deque<GameplayEvent> events_;
    struct DelayedEvent { double deliveryTime{0.0}; GameplayEvent event; };
    std::vector<DelayedEvent> delayedEvents_;
    PlayerInventory inventory_;
    PlayerVitals vitals_;
    RespawnPoint fallbackCheckpoint_{};
    RespawnPoint checkpoint_{};
    GameplayMessage message_{};
    EntityId interactionTarget_{0};
    std::uint64_t nextEventSequence_{1};
    double simulationTime_{0.0};
};
