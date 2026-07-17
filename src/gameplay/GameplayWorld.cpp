#include "gameplay/GameplayWorld.hpp"

#include "scene/Scene.hpp"

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>

namespace {
constexpr std::size_t maximumEventsPerTick = 256;

bool rayBox(const glm::vec3& origin, const glm::vec3& direction, const AABB& box,
            const float maximumDistance, float& distance) noexcept {
    float nearTime = 0.0F;
    float farTime = maximumDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 0.000001F) {
            if (origin[axis] < box.minimum[axis] || origin[axis] > box.maximum[axis]) return false;
            continue;
        }
        float first = (box.minimum[axis] - origin[axis]) / direction[axis];
        float second = (box.maximum[axis] - origin[axis]) / direction[axis];
        if (first > second) std::swap(first, second);
        nearTime = std::max(nearTime, first);
        farTime = std::min(farTime, second);
        if (nearTime > farTime) return false;
    }
    distance = nearTime;
    return farTime >= 0.0F && nearTime <= maximumDistance;
}

glm::vec3 transformedDirection(const glm::mat4& transform, const glm::vec3& direction) {
    glm::vec3 result{transform * glm::vec4{direction, 0.0F}};
    const float length = glm::length(result);
    return length > 0.000001F ? result / length : glm::vec3{0.0F, 1.0F, 0.0F};
}
} // namespace

bool GameplayWorld::initialize(const std::vector<GameplayEntityDefinition>& definitions,
                               Scene& scene, DynamicCollisionWorld& dynamicCollision,
                               const RespawnPoint& fallbackCheckpoint) {
    scene_ = &scene;
    dynamicCollision_ = &dynamicCollision;
    fallbackCheckpoint_ = fallbackCheckpoint;
    entities_.clear();
    byId_.clear();
    byName_.clear();
    events_.clear();
    delayedEvents_.clear();
    dynamicCollision.clear();
    entities_.reserve(definitions.size());
    byId_.reserve(definitions.size());
    byName_.reserve(definitions.size());

    for (const GameplayEntityDefinition& definition : definitions) {
        if (definition.name.empty() || definition.id == 0) {
            std::cerr << "Warning: gameplay entity has no stable identity and was disabled.\n";
            continue;
        }
        if (byName_.contains(definition.name) || byId_.contains(definition.id)) {
            std::cerr << "Warning: duplicate gameplay entity name or ID '"
                      << definition.name << "'; duplicate disabled.\n";
            continue;
        }
        GameplayEntity entity;
        entity.authored = definition;
        resetEntity(entity);
        const std::size_t index = entities_.size();
        entities_.push_back(std::move(entity));
        byId_.emplace(definition.id, index);
        byName_.emplace(definition.name, definition.id);
    }

    std::size_t resolved = 0;
    std::size_t unresolved = 0;
    for (GameplayEntity& entity : entities_) {
        if (!entity.authored.targetName.empty()) {
            const auto target = byName_.find(entity.authored.targetName);
            if (target == byName_.end()) {
                entity.valid = false;
                entity.enabled = false;
                ++unresolved;
                std::cerr << "Warning: entity '" << entity.authored.name
                          << "' targets unknown entity '" << entity.authored.targetName
                          << "' and was disabled.\n";
            } else {
                entity.target = target->second;
                ++resolved;
            }
        }
    }
    checkpoint_ = fallbackCheckpoint_;
    for (GameplayEntity& entity : entities_) {
        if (entity.authored.type == GameplayEntityType::Door) updateDoorTransform(entity);
    }
    std::size_t doors = 0, switches = 0, triggers = 0, pickups = 0, damage = 0, checkpoints = 0;
    for (const GameplayEntity& entity : entities_) {
        switch (entity.authored.type) {
        case GameplayEntityType::Door: ++doors; break;
        case GameplayEntityType::Switch: ++switches; break;
        case GameplayEntityType::Trigger: ++triggers; break;
        case GameplayEntityType::Pickup: ++pickups; break;
        case GameplayEntityType::DamageVolume: ++damage; break;
        case GameplayEntityType::Checkpoint: ++checkpoints; break;
        }
    }
    std::cout << "Gameplay summary:\n"
              << "  entities:          " << entities_.size() << '\n'
              << "  doors:             " << doors << '\n'
              << "  switches:          " << switches << '\n'
              << "  triggers:          " << triggers << '\n'
              << "  pickups:           " << pickups << '\n'
              << "  damage volumes:    " << damage << '\n'
              << "  checkpoints:       " << checkpoints << '\n'
              << "  resolved targets:  " << resolved << '\n'
              << "  unresolved targets:" << unresolved << '\n'
              << "  dynamic colliders: " << dynamicCollision.colliders().size() << '\n';
    return true;
}

void GameplayWorld::resetEntity(GameplayEntity& entity) {
    entity.enabled = entity.valid;
    entity.active = true;
    entity.completed = false;
    entity.overlapping = false;
    entity.collected = false;
    entity.activated = false;
    entity.locked = entity.authored.startsLocked;
    entity.doorProgress = entity.authored.startsOpen ? 1.0F : 0.0F;
    entity.doorState = entity.authored.startsOpen ? DoorState::Open : DoorState::Closed;
    entity.stateTimer = 0.0F;
    entity.damageAccumulator = 0.0F;
    entity.runtimeWorldTransform = entity.authored.authoredWorldTransform;
}

void GameplayWorld::reset() {
    events_.clear();
    delayedEvents_.clear();
    nextEventSequence_ = 1;
    simulationTime_ = 0.0;
    inventory_.clear();
    vitals_.reset();
    checkpoint_ = fallbackCheckpoint_;
    interactionTarget_ = 0;
    for (GameplayEntity& entity : entities_) {
        const EntityId target = entity.target;
        resetEntity(entity);
        entity.target = target;
        if (entity.authored.type == GameplayEntityType::Door) updateDoorTransform(entity);
    }
    synchronizePersistentState();
    showMessage("Level state reset", 2.0F, 1);
}

void GameplayWorld::fixedUpdate(const float deltaTime, const Capsule& playerCapsule,
                                const glm::vec3& playerPosition,
                                const float playerYawDegrees) {
    simulationTime_ += deltaTime;
    for (auto iterator = delayedEvents_.begin(); iterator != delayedEvents_.end();) {
        if (iterator->deliveryTime <= simulationTime_) {
            events_.push_back(std::move(iterator->event));
            iterator = delayedEvents_.erase(iterator);
        } else {
            ++iterator;
        }
    }
    for (GameplayEntity& entity : entities_) {
        if (!entity.enabled || !entity.active) continue;
        if (entity.authored.type == GameplayEntityType::Door) {
            updateDoor(entity, deltaTime, playerCapsule);
        }
    }
    for (GameplayEntity& entity : entities_) {
        if (!entity.enabled || !entity.active) continue;
        if (entity.authored.type != GameplayEntityType::Door &&
            entity.authored.type != GameplayEntityType::Switch) {
            updateOverlapEntity(entity, deltaTime, playerCapsule,
                                playerPosition, playerYawDegrees);
        }
    }
    processEvents();
}

void GameplayWorld::updatePresentation(const float deltaTime) noexcept {
    message_.remainingSeconds = std::max(0.0F, message_.remainingSeconds - deltaTime);
    if (message_.remainingSeconds == 0.0F) {
        message_.text.clear();
        message_.priority = 0;
    }
}

void GameplayWorld::updateDoor(GameplayEntity& entity, const float deltaTime,
                               const Capsule& playerCapsule) {
    if (entity.doorState == DoorState::Open && entity.authored.autoCloseDelay > 0.0F) {
        entity.stateTimer += deltaTime;
        if (entity.stateTimer >= entity.authored.autoCloseDelay) {
            entity.doorState = DoorState::Closing;
            entity.stateTimer = 0.0F;
        }
    }
    if (entity.doorState == DoorState::Blocked) {
        entity.stateTimer += deltaTime;
        if (entity.stateTimer >= 0.35F) {
            entity.doorState = DoorState::Opening;
            entity.stateTimer = 0.0F;
        }
    }
    const float previous = entity.doorProgress;
    if (entity.doorState == DoorState::Opening) {
        entity.doorProgress = std::min(1.0F, entity.doorProgress +
            entity.authored.openSpeed * deltaTime /
            std::max(entity.authored.moveDistance, 0.0001F));
    } else if (entity.doorState == DoorState::Closing) {
        entity.doorProgress = std::max(0.0F, entity.doorProgress -
            entity.authored.closeSpeed * deltaTime /
            std::max(entity.authored.moveDistance, 0.0001F));
    }
    if (entity.doorProgress != previous) {
        updateDoorTransform(entity);
        const AABB candidate = entityBounds(entity);
        if (capsuleOverlapsAabb(playerCapsule, candidate)) {
            entity.doorProgress = previous;
            entity.doorState = DoorState::Blocked;
            entity.stateTimer = 0.0F;
            updateDoorTransform(entity);
        }
    }
    if (entity.doorProgress >= 1.0F) {
        entity.doorProgress = 1.0F;
        entity.doorState = DoorState::Open;
    } else if (entity.doorProgress <= 0.0F) {
        entity.doorProgress = 0.0F;
        entity.doorState = DoorState::Closed;
    }
}

void GameplayWorld::updateDoorTransform(GameplayEntity& entity) {
    if (scene_ == nullptr || dynamicCollision_ == nullptr ||
        entity.authored.type != GameplayEntityType::Door) return;
    const glm::vec3 axis = transformedDirection(entity.authored.authoredWorldTransform,
                                                entity.authored.moveAxis);
    const glm::vec3 offset = axis * entity.authored.moveDistance * entity.doorProgress;
    entity.runtimeWorldTransform = glm::translate(glm::mat4{1.0F}, offset) *
                                   entity.authored.authoredWorldTransform;
    for (const std::size_t primitiveIndex : entity.authored.primitiveIndices) {
        if (primitiveIndex < scene_->primitives.size()) {
            scene_->primitives[primitiveIndex].worldTransform = entity.runtimeWorldTransform;
        }
    }
    dynamicCollision_->upsert(entity.authored.id, entityBounds(entity), entity.enabled);
}

void GameplayWorld::updateOverlapEntity(GameplayEntity& entity, const float deltaTime,
                                        const Capsule& playerCapsule,
                                        const glm::vec3& playerPosition,
                                        const float playerYawDegrees) {
    const bool inside = capsuleOverlapsAabb(playerCapsule, entityBounds(entity));
    const bool entered = inside && !entity.overlapping;
    const bool exited = !inside && entity.overlapping;
    entity.overlapping = inside;
    if (entity.authored.type == GameplayEntityType::Trigger) {
        if (entered && !(entity.authored.oneShot && entity.completed)) {
            queueEvent({entity.authored.enterEvent, entity.authored.id, entity.target});
            entity.completed = entity.authored.oneShot;
        }
        if (exited && !entity.authored.oneShot) {
            queueEvent({entity.authored.exitEvent, entity.authored.id, entity.target});
        }
        if (inside) queueEvent({GameplayEventType::TriggerStay, entity.authored.id, entity.target});
    } else if (entity.authored.type == GameplayEntityType::Pickup && entered && !entity.collected) {
        collectPickup(entity);
    } else if (entity.authored.type == GameplayEntityType::DamageVolume && inside) {
        entity.damageAccumulator += entity.authored.damagePerSecond * deltaTime;
        const int damage = static_cast<int>(std::floor(entity.damageAccumulator));
        if (damage > 0) {
            const bool wasAlive = vitals_.alive;
            entity.damageAccumulator -= static_cast<float>(damage);
            static_cast<void>(vitals_.applyDamage(damage, DamageType::Environmental,
                                                  entity.authored.id,
                                                  entity.authored.bypassArmor));
            if (wasAlive && !vitals_.alive) showMessage("You died", 4.0F, 10);
        }
    } else if (entity.authored.type == GameplayEntityType::Checkpoint && entered &&
               checkpoint_.checkpoint != entity.authored.id) {
        checkpoint_ = {entity.authored.id, playerPosition, playerYawDegrees,
                       entity.authored.restoreHealth, entity.authored.restoreArmor};
        entity.activated = true;
        showMessage("Checkpoint reached", 3.0F, 3);
    }
}

void GameplayWorld::collectPickup(GameplayEntity& entity) {
    bool collected = false;
    if (entity.authored.pickupType == "key") {
        collected = inventory_.addKey(entity.authored.itemId);
    } else if (entity.authored.pickupType == "health") {
        collected = vitals_.addHealth(entity.authored.amount);
    } else if (entity.authored.pickupType == "armor") {
        collected = vitals_.addArmor(entity.authored.amount);
    }
    if (!collected) return;
    entity.collected = true;
    entity.active = false;
    for (const std::size_t primitiveIndex : entity.authored.primitiveIndices) {
        if (scene_ != nullptr && primitiveIndex < scene_->primitives.size()) {
            scene_->primitives[primitiveIndex].visible = false;
        }
    }
    showMessage(entity.authored.displayName.empty() ? "Pickup collected" :
                "Picked up " + entity.authored.displayName, 2.5F, 2);
}

void GameplayWorld::updateInteraction(const glm::vec3& origin, const glm::vec3& direction,
                                      const CollisionWorld& staticCollision) {
    constexpr float maximumDistance = 2.75F;
    interactionTarget_ = 0;
    float nearest = maximumDistance;
    for (const GameplayEntity& entity : entities_) {
        if (!entity.enabled || !entity.active ||
            (entity.authored.type != GameplayEntityType::Door &&
             entity.authored.type != GameplayEntityType::Switch)) continue;
        float distance = 0.0F;
        if (rayEntity(entity, origin, direction, nearest, distance)) {
            nearest = distance;
            interactionTarget_ = entity.authored.id;
        }
    }
    if (interactionTarget_ == 0) return;
    RayHit worldHit;
    if (staticCollision.raycast(origin, direction, maximumDistance, worldHit) &&
        worldHit.distance + 0.01F < nearest) {
        interactionTarget_ = 0;
        return;
    }
    RayHit dynamicHit;
    if (dynamicCollision_ != nullptr &&
        dynamicCollision_->raycast(origin, direction, nearest, dynamicHit, interactionTarget_) &&
        dynamicHit.distance + 0.01F < nearest) {
        interactionTarget_ = 0;
    }
}

void GameplayWorld::interact() {
    GameplayEntity* entity = find(interactionTarget_);
    if (entity == nullptr) return;
    if (entity->authored.type == GameplayEntityType::Door) {
        if (entity->locked) {
            if (entity->authored.requiredKey.empty() ||
                !inventory_.hasKey(entity->authored.requiredKey)) {
                showMessage(entity->authored.requiredKey.empty() ? "Door is locked" :
                            "Requires " + entity->authored.requiredKey, 2.5F, 4);
                return;
            }
            entity->locked = false;
            showMessage("Door unlocked", 2.0F, 3);
        }
        queueEvent({entity->authored.toggleMode ? GameplayEventType::Toggle :
                    GameplayEventType::Open, 0, entity->authored.id});
    } else if (entity->authored.type == GameplayEntityType::Switch) {
        if (entity->authored.oneShot && entity->completed) return;
        if (!entity->authored.requiredKey.empty() &&
            !inventory_.hasKey(entity->authored.requiredKey)) {
            showMessage("Requires " + entity->authored.requiredKey, 2.5F, 4);
            return;
        }
        entity->activated = !entity->activated;
        entity->completed = entity->authored.oneShot;
        queueEvent({entity->authored.enterEvent, entity->authored.id, entity->target});
        showMessage("Switch activated", 1.5F, 1);
    }
    processEvents();
}

void GameplayWorld::queueEvent(GameplayEvent event) {
    event.sequence = nextEventSequence_++;
    events_.push_back(std::move(event));
}

void GameplayWorld::queueDelayedEvent(GameplayEvent event, const float delaySeconds) {
    event.sequence = nextEventSequence_++;
    DelayedEvent delayed{simulationTime_ + std::max(0.0F, delaySeconds), std::move(event)};
    const auto position = std::upper_bound(delayedEvents_.begin(), delayedEvents_.end(), delayed,
        [](const DelayedEvent& a, const DelayedEvent& b) {
            if (a.deliveryTime != b.deliveryTime) return a.deliveryTime < b.deliveryTime;
            return a.event.sequence < b.event.sequence;
        });
    delayedEvents_.insert(position, std::move(delayed));
}

void GameplayWorld::processEvents() {
    std::size_t processed = 0;
    while (!events_.empty() && processed < maximumEventsPerTick) {
        GameplayEvent event = std::move(events_.front());
        events_.pop_front();
        dispatch(event);
        ++processed;
    }
    if (!events_.empty()) {
        std::cerr << "Warning: gameplay event limit reached; dropping "
                  << events_.size() << " events.\n";
        events_.clear();
    }
}

void GameplayWorld::dispatch(const GameplayEvent& event) {
    GameplayEntity* entity = find(event.target);
    if (entity == nullptr || !entity->enabled) return;
    if (entity->authored.type == GameplayEntityType::Door) {
        switch (event.type) {
        case GameplayEventType::Open:
        case GameplayEventType::Activate:
            if (!entity->locked && entity->doorState != DoorState::Open) entity->doorState = DoorState::Opening;
            break;
        case GameplayEventType::Close:
        case GameplayEventType::Deactivate:
            if (entity->doorState != DoorState::Closed) entity->doorState = DoorState::Closing;
            break;
        case GameplayEventType::Toggle:
            if (!entity->locked) entity->doorState =
                (entity->doorState == DoorState::Open || entity->doorState == DoorState::Opening)
                ? DoorState::Closing : DoorState::Opening;
            break;
        case GameplayEventType::Lock: entity->locked = true; break;
        case GameplayEventType::Unlock: entity->locked = false; break;
        case GameplayEventType::Reset: resetEntity(*entity); updateDoorTransform(*entity); break;
        default: break;
        }
    } else if (event.type == GameplayEventType::Activate) {
        entity->active = true;
    } else if (event.type == GameplayEventType::Deactivate) {
        entity->active = false;
    } else if (event.type == GameplayEventType::Toggle) {
        entity->active = !entity->active;
    }
}

AABB GameplayWorld::entityBounds(const GameplayEntity& entity) const noexcept {
    const glm::mat4& transform = entity.authored.type == GameplayEntityType::Door
        ? entity.runtimeWorldTransform : entity.authored.authoredWorldTransform;
    const glm::vec3 center{transform * glm::vec4{entity.authored.boxOffset, 1.0F}};
    const glm::vec3 half = entity.authored.boxSize * 0.5F;
    return AABB{center - half, center + half};
}

bool GameplayWorld::rayEntity(const GameplayEntity& entity, const glm::vec3& origin,
                              const glm::vec3& direction, const float maximumDistance,
                              float& distance) const noexcept {
    return rayBox(origin, direction, entityBounds(entity), maximumDistance, distance);
}

GameplayEntity* GameplayWorld::find(const EntityId id) noexcept {
    const auto found = byId_.find(id);
    return found == byId_.end() ? nullptr : &entities_[found->second];
}
const GameplayEntity* GameplayWorld::find(const EntityId id) const noexcept {
    const auto found = byId_.find(id);
    return found == byId_.end() ? nullptr : &entities_[found->second];
}
GameplayEntity* GameplayWorld::findByName(const std::string& name) noexcept {
    const auto found = byName_.find(name);
    return found == byName_.end() ? nullptr : find(found->second);
}
const std::vector<GameplayEntity>& GameplayWorld::entities() const noexcept { return entities_; }
std::vector<GameplayEntity>& GameplayWorld::mutableEntities() noexcept { return entities_; }
PlayerInventory& GameplayWorld::inventory() noexcept { return inventory_; }
const PlayerInventory& GameplayWorld::inventory() const noexcept { return inventory_; }
PlayerVitals& GameplayWorld::vitals() noexcept { return vitals_; }
const PlayerVitals& GameplayWorld::vitals() const noexcept { return vitals_; }
const RespawnPoint& GameplayWorld::checkpoint() const noexcept { return checkpoint_; }
EntityId GameplayWorld::interactionTarget() const noexcept { return interactionTarget_; }
const GameplayMessage& GameplayWorld::message() const noexcept { return message_; }
std::size_t GameplayWorld::pendingEventCount() const noexcept { return events_.size(); }
std::size_t GameplayWorld::delayedEventCount() const noexcept { return delayedEvents_.size(); }
void GameplayWorld::restoreCheckpoint(const RespawnPoint& checkpoint) noexcept {
    checkpoint_ = checkpoint;
}
void GameplayWorld::clearPendingEvents() noexcept { events_.clear(); delayedEvents_.clear(); }
void GameplayWorld::synchronizePersistentState() {
    for (GameplayEntity& entity : entities_) {
        for (const std::size_t primitiveIndex : entity.authored.primitiveIndices) {
            if (scene_ != nullptr && primitiveIndex < scene_->primitives.size()) {
                scene_->primitives[primitiveIndex].visible = entity.active && !entity.collected;
            }
        }
        if (entity.authored.type == GameplayEntityType::Door) updateDoorTransform(entity);
    }
}

std::string GameplayWorld::interactionPrompt() const {
    const GameplayEntity* entity = find(interactionTarget_);
    if (entity == nullptr) return {};
    if (entity->authored.type == GameplayEntityType::Switch) return "[E] Press switch";
    if (entity->locked) {
        return entity->authored.requiredKey.empty() ? "Door is locked" :
            (inventory_.hasKey(entity->authored.requiredKey) ? "[E] Unlock door" :
             "Requires " + entity->authored.requiredKey);
    }
    return entity->doorState == DoorState::Open ? "[E] Close door" : "[E] Open door";
}

const char* GameplayWorld::doorStateName(const DoorState state) noexcept {
    switch (state) {
    case DoorState::Closed: return "Closed";
    case DoorState::Opening: return "Opening";
    case DoorState::Open: return "Open";
    case DoorState::Closing: return "Closing";
    case DoorState::Blocked: return "Blocked";
    }
    return "Unknown";
}

std::string GameplayWorld::debugDescription(const EntityId id) const {
    const GameplayEntity* entity = find(id);
    if (entity == nullptr) return {};
    std::ostringstream output;
    output << "Name: " << entity->authored.name << "  ID: " << entity->authored.id;
    if (entity->authored.type == GameplayEntityType::Door) {
        output << "  State: " << doorStateName(entity->doorState)
               << "  Locked: " << (entity->locked ? "yes" : "no");
    }
    return output.str();
}

void GameplayWorld::showMessage(std::string text, const float duration, const int priority) {
    if (message_.remainingSeconds > 0.0F && priority < message_.priority) return;
    message_ = {std::move(text), duration, priority};
}
