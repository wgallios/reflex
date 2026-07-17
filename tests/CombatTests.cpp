#include "combat/CombatSystem.hpp"
#include "gameplay/GameplayWorld.hpp"
#include "persistence/SaveGame.hpp"
#include "scene/Scene.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
int failures = 0;
void check(const bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

GameplayEntityDefinition enemySpawn() {
    GameplayEntityDefinition result;
    result.type = GameplayEntityType::EnemySpawn;
    result.name = "enemy_test";
    result.id = stableEntityId(result.name);
    result.enemyType = "grunt";
    result.authoredWorldTransform[3] = glm::vec4{0.0F, 0.9F, -8.0F, 1.0F};
    result.boxSize = {0.8F, 1.8F, 0.8F};
    return result;
}
} // namespace

int main() {
    const std::filesystem::path definitionsPath =
        std::filesystem::path{REFLEX_SOURCE_DIR} / "assets/combat/combat.json";
    CombatDefinitions definitions;
    std::string error;
    check(loadCombatDefinitions(definitionsPath, definitions, error),
          "valid combat definitions parse");
    check(definitions.weapons.size() == 3 && definitions.enemies.size() == 1,
          "required weapon and enemy definitions are present");
    const std::filesystem::path invalidDefinitionPath =
        std::filesystem::temp_directory_path() / "reflex_invalid_combat.json";
    {
        std::ofstream invalid{invalidDefinitionPath};
        invalid << R"({"maximum_ammo":{"bullets":20},"weapons":[{"id":"bad","ammo_type":"bullets","damage":1,"shots_per_second":0,"magazine_size":1,"reload_time":1}],"enemies":[]})";
    }
    CombatDefinitions rejected;
    check(!loadCombatDefinitions(invalidDefinitionPath, rejected, error),
          "invalid fire rate rejects weapon definition");
    std::filesystem::remove(invalidDefinitionPath);

    CombatRng first{1234}, second{1234};
    bool deterministic = true;
    for (int i = 0; i < 16; ++i) deterministic = deterministic && first.next() == second.next();
    check(deterministic, "combat RNG sequence is deterministic");
    const glm::vec3 spread = first.spreadDirection({0.0F, 0.0F, -1.0F}, 6.0F);
    check(std::abs(glm::length(spread) - 1.0F) < 0.0001F, "spread direction is normalized");
    check(spread.z < 0.0F, "spread remains inside the forward cone");
    check(std::abs(splashDamageAtDistance(100.0F, 10.0F, 5.0F) - 50.0F) < 0.001F,
          "splash damage uses linear falloff");
    check(splashDamageAtDistance(100.0F, 10.0F, 10.0F) == 0.0F,
          "splash damage ends at radius");
    check(insidePerceptionCone({0,0,-1}, {0,0,-5}, 90.0F), "perception accepts target in cone");
    check(!insidePerceptionCone({0,0,-1}, {5,0,0}, 90.0F), "perception rejects target outside cone");

    AmmoInventory ammo;
    ammo.configure({{"bullets", 20}});
    check(ammo.add("bullets", 30) == 20 && ammo.get("bullets") == 20,
          "ammunition addition clamps to carry maximum");
    check(ammo.consume("bullets", 7) && ammo.get("bullets") == 13,
          "ammunition consumption is exact");
    check(!ammo.consume("bullets", 14), "ammunition cannot become negative");

    Scene scene;
    DynamicCollisionWorld dynamic;
    GameplayWorld gameplay;
    const GameplayEntityDefinition spawn = enemySpawn();
    check(gameplay.initialize({spawn}, scene, dynamic, {}), "combat gameplay fixture initializes");
    CombatSystem combat;
    check(combat.initialize(definitionsPath, scene, gameplay, scene.collisionWorld, dynamic),
          "combat system initializes");
    check(combat.equippedDefinition() != nullptr &&
          combat.equippedDefinition()->id == "pistol", "pistol is the initial weapon");
    check(combat.equippedWeapon()->magazine == 12 && combat.reserveAmmo() == 36,
          "initial pistol magazine and reserve are configured");

    const Capsule player{{0.0F, 0.0F, 0.0F}, 1.8F, 0.35F};
    CombatInput fire; fire.firePressed = true;
    combat.fixedUpdate(1.0F / 120.0F, fire, {0,1.6F,0}, {0,0,-1}, player, player.position);
    check(combat.equippedWeapon()->magazine == 11, "valid shot consumes one magazine round");
    combat.fixedUpdate(1.0F / 120.0F, fire, {0,1.6F,0}, {0,0,-1}, player, player.position);
    check(combat.equippedWeapon()->magazine == 11, "fire rate blocks an immediate second shot");

    for (int i = 0; i < 40; ++i) combat.fixedUpdate(1.0F / 120.0F, {},
        {0,1.6F,0}, {0,0,-1}, player, player.position);
    CombatInput reload; reload.reloadPressed = true;
    combat.fixedUpdate(1.0F / 120.0F, reload, {0,1.6F,0}, {0,0,-1}, player, player.position);
    check(combat.equippedWeapon()->state == WeaponState::Reloading,
          "reload enters explicit reloading state");
    for (int i = 0; i < 180; ++i) combat.fixedUpdate(1.0F / 120.0F, {},
        {0,1.6F,0}, {0,0,-1}, player, player.position);
    check(combat.equippedWeapon()->magazine == 12 && combat.reserveAmmo() == 35,
          "reload transfers reserve ammunition and clamps magazine");

    check(combat.grantWeapon("shotgun", 8), "weapon acquisition grants new ownership");
    check(combat.selectSlot(1), "direct weapon slot selection starts equip");
    check(combat.equippedWeapon()->state == WeaponState::Equipping,
          "weapon switch uses explicit equipping state");
    const CombatSaveState saved = combat.captureState();
    check(combat.validateState(saved, error), "combat save state validates");
    CombatSaveState invalid = saved;
    invalid.weapons.front().magazine = 999;
    check(!combat.validateState(invalid, error), "invalid saved magazine is rejected");
    invalid = saved; invalid.weapons.front().id = "removed_weapon";
    check(!combat.validateState(invalid, error), "unknown saved weapon is rejected");

    const std::filesystem::path savePath =
        std::filesystem::temp_directory_path() / "reflex_combat_save_test.json";
    const SaveGameData save = SaveGame::capture("combat.glb", gameplay,
        player.position, 12.0F, -3.0F, &combat);
    check(SaveGame::write(savePath, save, error), "combat save serializes");
    const auto loaded = SaveGame::read(savePath, error);
    check(loaded.has_value() && loaded->combat.weapons.size() == saved.weapons.size(),
          "combat save deserializes owned weapons");
    check(loaded.has_value() && SaveGame::apply(*loaded, gameplay, error, &combat),
          "validated combat save applies");
    std::filesystem::remove(savePath);

    EnemyActor const& actor = combat.enemies().front();
    combat.queueDamage({1,1,actor.id,DamageType::Bullet,1000.0F});
    combat.queueDamage({1,1,actor.id,DamageType::Bullet,1000.0F});
    combat.fixedUpdate(1.0F / 120.0F, {}, {0,1.6F,0}, {0,0,-1}, player, player.position);
    check(combat.enemies().front().state == EnemyState::Dead &&
          combat.enemies().front().health == 0, "enemy death is clamped and idempotent");

    return failures == 0 ? 0 : 1;
}
