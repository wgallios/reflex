#pragma once

#include "combat/CombatTypes.hpp"
#include "collision/DynamicCollisionWorld.hpp"

#include <glm/mat4x4.hpp>

#include <deque>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class CollisionWorld;
class GameplayWorld;
class Scene;
struct Capsule;

struct CombatLineEffect {
    glm::vec3 from{};
    glm::vec3 to{};
    glm::vec3 color{1.0F};
    float lifetime{0.1F};
};

struct SavedWeaponState { std::string id; int magazine{0}; };
struct SavedEnemyState { std::string name; int health{0}; glm::vec3 position{}; bool active{true}; };
struct CombatSaveState {
    std::vector<SavedWeaponState> weapons;
    std::string equippedWeapon;
    std::unordered_map<std::string, int> ammunition;
    std::vector<SavedEnemyState> enemies;
};

class CombatSystem {
public:
    [[nodiscard]] bool initialize(const std::filesystem::path& definitionPath,
                                  Scene& scene, GameplayWorld& gameplay,
                                  const CollisionWorld& collision,
                                  DynamicCollisionWorld& dynamicCollision);
    void reset();
    void fixedUpdate(float deltaTime, const CombatInput& input,
                     const glm::vec3& cameraPosition, const glm::vec3& cameraForward,
                     const Capsule& playerCapsule, const glm::vec3& playerPosition);
    void updatePresentation(float deltaTime) noexcept;

    [[nodiscard]] bool grantWeapon(std::string_view id, int reserveAmmo = 0);
    [[nodiscard]] int addAmmo(std::string_view type, int amount);
    [[nodiscard]] bool selectSlot(int slot);
    [[nodiscard]] bool cycleWeapon(int direction);
    void queueDamage(DamageEvent event);

    [[nodiscard]] const WeaponDefinition* equippedDefinition() const noexcept;
    [[nodiscard]] const WeaponInstance* equippedWeapon() const noexcept;
    [[nodiscard]] int reserveAmmo() const;
    [[nodiscard]] const AmmoInventory& ammunition() const noexcept;
    [[nodiscard]] const std::vector<EnemyActor>& enemies() const noexcept;
    [[nodiscard]] const std::vector<Projectile>& projectiles() const noexcept;
    [[nodiscard]] const std::vector<CombatLineEffect>& effects() const noexcept;
    [[nodiscard]] const HitResult& lastTrace() const noexcept;
    [[nodiscard]] bool hitMarkerVisible() const noexcept;
    [[nodiscard]] bool killMarkerVisible() const noexcept;
    [[nodiscard]] float muzzleFlashRemaining() const noexcept;
    [[nodiscard]] float damageIndicatorRemaining() const noexcept;
    [[nodiscard]] glm::vec3 lastDamageDirection() const noexcept;
    [[nodiscard]] std::string debugSummary() const;
    [[nodiscard]] std::uint64_t rngSeed() const noexcept;
    [[nodiscard]] std::uint64_t rngSequence() const noexcept;

    [[nodiscard]] CombatSaveState captureState() const;
    [[nodiscard]] bool validateState(const CombatSaveState& state, std::string& error) const;
    [[nodiscard]] bool restoreState(const CombatSaveState& state, std::string& error);

private:
    [[nodiscard]] const WeaponDefinition* weaponDefinition(std::string_view id) const;
    [[nodiscard]] const EnemyDefinition* enemyDefinition(std::string_view id) const;
    [[nodiscard]] EnemyActor* enemy(EntityId id) noexcept;
    [[nodiscard]] const EnemyActor* enemy(EntityId id) const noexcept;
    void updateWeapon(float deltaTime, const CombatInput& input,
                      const glm::vec3& origin, const glm::vec3& forward);
    void fireWeapon(WeaponInstance& instance, const WeaponDefinition& definition,
                    const glm::vec3& origin, const glm::vec3& forward);
    [[nodiscard]] HitResult trace(const glm::vec3& origin, const glm::vec3& direction,
                                  float maximumDistance, EntityId ignored = 0) const;
    void updateProjectiles(float deltaTime, const glm::vec3& playerPosition);
    void explode(const Projectile& projectile, const glm::vec3& point,
                 const glm::vec3& playerPosition);
    void updateEnemies(float deltaTime, const glm::vec3& playerPosition,
                       const Capsule& playerCapsule);
    void moveEnemy(EnemyActor& actor, const EnemyDefinition& definition,
                   float deltaTime, const glm::vec3& target);
    void processDamage();
    void addImpact(const HitResult& hit);
    void updateCombatPickups(const Capsule& playerCapsule);
    void synchronizeEnemy(EnemyActor& actor);
    [[nodiscard]] AABB enemyBounds(const EnemyActor& actor) const;

    Scene* scene_{nullptr};
    GameplayWorld* gameplay_{nullptr};
    const CollisionWorld* collision_{nullptr};
    DynamicCollisionWorld* dynamicCollision_{nullptr};
    CombatDefinitions definitions_;
    std::unordered_map<std::string, std::size_t> weaponDefinitions_;
    std::unordered_map<std::string, std::size_t> enemyDefinitions_;
    std::vector<WeaponInstance> weapons_;
    AmmoInventory ammunition_;
    int equippedIndex_{-1};
    std::vector<EnemyActor> enemies_;
    std::unordered_map<EntityId, std::size_t> enemiesById_;
    std::vector<Projectile> projectiles_;
    std::deque<DamageEvent> damageQueue_;
    std::vector<CombatLineEffect> effects_;
    CombatRng rng_;
    HitResult lastTrace_{};
    std::uint64_t nextProjectileId_{1};
    std::uint64_t nextDamageSequence_{1};
    float hitMarkerTimer_{0.0F};
    float killMarkerTimer_{0.0F};
    float muzzleFlashTimer_{0.0F};
    float damageIndicatorTimer_{0.0F};
    glm::vec3 lastDamageDirection_{};
};
