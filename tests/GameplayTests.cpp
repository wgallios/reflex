#include "collision/DynamicCollisionWorld.hpp"
#include "gameplay/GameplayWorld.hpp"
#include "persistence/SaveGame.hpp"
#include "scene/Scene.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
int failures = 0;
void check(const bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

GameplayEntityDefinition door(const std::string& name) {
    GameplayEntityDefinition result;
    result.type = GameplayEntityType::Door;
    result.name = name;
    result.id = stableEntityId(name);
    result.boxSize = {1.0F, 2.0F, 0.25F};
    result.moveAxis = {0.0F, 1.0F, 0.0F};
    result.moveDistance = 2.0F;
    result.openSpeed = 1.0F;
    result.closeSpeed = 1.0F;
    return result;
}
} // namespace

int main() {
    check(stableEntityId("door") == stableEntityId("door"), "entity IDs are deterministic");
    check(stableEntityId("door") != stableEntityId("other"), "distinct names differ");

    PlayerInventory inventory;
    check(inventory.addKey("blue_key"), "key insertion succeeds");
    check(!inventory.addKey("blue_key"), "duplicate key insertion is rejected");
    check(inventory.hasKey("blue_key"), "key lookup succeeds");

    PlayerVitals vitals;
    vitals.armor = 10;
    check(vitals.applyDamage(10, DamageType::Generic, 0) == 5,
          "armor absorbs half of eligible damage");
    check(vitals.health == 95 && vitals.armor == 5, "vitals clamp and armor accounting");
    check(vitals.addHealth(100) && vitals.health == 100, "health pickup clamps to maximum");
    check(!vitals.addHealth(1), "health pickup is rejected at maximum");

    Scene scene;
    DynamicCollisionWorld dynamic;
    GameplayWorld world;
    GameplayEntityDefinition locked = door("door_locked");
    locked.startsLocked = true;
    locked.requiredKey = "blue_key";
    check(world.initialize({locked, locked}, scene, dynamic, {}), "world tolerates invalid duplicate");
    check(world.entities().size() == 1, "duplicate entity name is disabled");
    GameplayEntity* entity = world.findByName("door_locked");
    check(entity != nullptr && entity->locked, "door starts locked");
    world.queueEvent({GameplayEventType::Open, 0, locked.id});
    world.processEvents();
    check(entity->doorState == DoorState::Closed, "locked door rejects open event");
    world.queueEvent({GameplayEventType::Unlock, 0, locked.id});
    world.queueEvent({GameplayEventType::Open, 0, locked.id});
    world.processEvents();
    check(entity->doorState == DoorState::Opening, "ordered events unlock then open door");
    const Capsule farAway{{100.0F, 0.0F, 100.0F}, 1.8F, 0.35F};
    world.fixedUpdate(5.0F, farAway, farAway.position, 0.0F);
    check(entity->doorState == DoorState::Open && std::abs(entity->doorProgress - 1.0F) < 0.0001F,
          "door motion clamps at open endpoint");
    world.queueDelayedEvent({GameplayEventType::Close, 0, locked.id}, 1.0F);
    world.fixedUpdate(0.5F, farAway, farAway.position, 0.0F);
    check(entity->doorState == DoorState::Open, "delayed event waits for simulation time");
    world.fixedUpdate(0.5F, farAway, farAway.position, 0.0F);
    check(entity->doorState == DoorState::Closing, "delayed event fires at deterministic due time");

    Scene triggerScene;
    DynamicCollisionWorld triggerDynamic;
    GameplayWorld triggerWorld;
    GameplayEntityDefinition triggeredDoor = door("triggered_door");
    triggeredDoor.authoredWorldTransform[3] = glm::vec4{5.0F, 0.0F, 0.0F, 1.0F};
    GameplayEntityDefinition trigger;
    trigger.type = GameplayEntityType::Trigger;
    trigger.name = "trigger_once";
    trigger.id = stableEntityId(trigger.name);
    trigger.targetName = triggeredDoor.name;
    trigger.boxSize = {2.0F, 2.0F, 2.0F};
    trigger.enterEvent = GameplayEventType::Open;
    trigger.exitEvent = GameplayEventType::Close;
    trigger.oneShot = true;
    check(triggerWorld.initialize({triggeredDoor, trigger}, triggerScene, triggerDynamic, {}),
          "trigger fixture initializes");
    const Capsule inside{{0.0F, 0.0F, 0.0F}, 1.8F, 0.35F};
    triggerWorld.fixedUpdate(0.1F, inside, inside.position, 0.0F);
    check(triggerWorld.findByName("triggered_door")->doorState == DoorState::Opening,
          "trigger enter fires once");
    const Capsule outside{{20.0F, 0.0F, 0.0F}, 1.8F, 0.35F};
    triggerWorld.fixedUpdate(0.1F, outside, outside.position, 0.0F);
    triggerWorld.queueEvent({GameplayEventType::Close, 0, triggeredDoor.id});
    triggerWorld.processEvents();
    triggerWorld.fixedUpdate(0.1F, inside, inside.position, 0.0F);
    check(triggerWorld.findByName("triggered_door")->doorState != DoorState::Opening,
          "one-shot trigger does not reactivate after re-entry");

    Scene authScene;
    DynamicCollisionWorld authDynamic;
    GameplayWorld authWorld;
    GameplayEntityDefinition authDoor = door("auth_door");
    authDoor.startsLocked = true;
    authDoor.requiredKey = "blue_key";
    check(authWorld.initialize({authDoor}, authScene, authDynamic, {}),
          "authorization fixture initializes");
    authWorld.updateInteraction({0.0F, 1.0F, 2.0F}, {0.0F, 0.0F, -1.0F},
                                authScene.collisionWorld);
    authWorld.interact();
    check(authWorld.findByName("auth_door")->locked, "locked interaction rejects missing key");
    static_cast<void>(authWorld.inventory().addKey("blue_key"));
    authWorld.interact();
    check(!authWorld.findByName("auth_door")->locked, "matching key authorizes locked door");

    const std::filesystem::path savePath =
        std::filesystem::temp_directory_path() / "reflex_engine_gameplay_test.json";
    std::string error;
    reflex::campaign::ObjectiveSystem objectives;
    check(objectives.initialize({{"campaign_goal", reflex::campaign::ObjectiveType::ReachLocation,
        "Reach the goal", "goal", 1, true, {}}}, error), "save objective fixture initializes");
    reflex::campaign::EncounterDefinition encounterDefinition;
    encounterDefinition.id = "saved_encounter";
    encounterDefinition.waves = {{0.0F, {"saved_wave"}}};
    reflex::campaign::EncounterSystem encounters;
    check(encounters.initialize({encounterDefinition}, error) && encounters.start("saved_encounter"),
          "save encounter fixture initializes");
    SaveGameData captured = SaveGame::capture("test.glb", world, {1.0F,2.0F,3.0F}, 20.0F, -5.0F,
        nullptr, &objectives, &encounters, "facility_test");
    check(SaveGame::write(savePath, captured, error), "save serialization succeeds");
    const auto loaded = SaveGame::read(savePath, error);
    check(loaded.has_value() && loaded->levelPath == "test.glb" &&
          loaded->campaignLevelId == "facility_test" &&
          loaded->objectives.contains("campaign_goal") &&
          loaded->encounters.contains("saved_encounter"),
          "campaign save deserialization succeeds");
    SaveGameData future = captured;
    future.formatVersion = 999;
    check(SaveGame::write(savePath, future, error), "future-version fixture writes");
    check(!SaveGame::read(savePath, error), "future save version is rejected");
    SaveGameData unknownState = captured;
    unknownState.entities.emplace("removed_entity", SavedEntityState{});
    check(SaveGame::apply(unknownState, world, error), "unknown saved entity state is ignored safely");
    std::filesystem::remove(savePath);

    return failures == 0 ? 0 : 1;
}
