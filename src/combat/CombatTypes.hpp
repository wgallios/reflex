#pragma once

#include "gameplay/GameplayTypes.hpp"
#include "gameplay/PlayerState.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

enum class AttackType { Hitscan, Projectile };
enum class WeaponState { Holstered, Equipping, Ready, Firing, Cooldown, Reloading, Empty };
enum class EnemyState { Idle, Alert, Chasing, Attacking, Pain, Dead };
enum class SurfaceType { Default, Metal, Stone, Wood, Glass, Flesh };

struct WeaponDefinition {
    std::string id;
    std::string displayName;
    AttackType attackType{AttackType::Hitscan};
    DamageType damageType{DamageType::Bullet};
    float damage{10.0F};
    float range{100.0F};
    float shotsPerSecond{2.0F};
    std::string ammoType;
    int magazineSize{1};
    float reloadTime{1.0F};
    float equipTime{0.25F};
    float spreadDegrees{0.0F};
    int pellets{1};
    bool automatic{false};
    float projectileSpeed{20.0F};
    float projectileRadius{0.12F};
    float projectileLifetime{5.0F};
    float splashDamage{0.0F};
    float splashRadius{0.0F};
    bool selfDamage{false};
    int tracerFrequency{1};
};

struct EnemyDefinition {
    std::string id;
    std::string displayName;
    int maximumHealth{75};
    float movementSpeed{2.5F};
    float sightDistance{25.0F};
    float fieldOfViewDegrees{100.0F};
    float reactionTime{0.25F};
    float lostSightDuration{2.0F};
    float attackDamage{8.0F};
    float attackRange{18.0F};
    float shotsPerSecond{1.5F};
    float attackSpreadDegrees{4.0F};
    float painDuration{0.2F};
    float radius{0.4F};
    float height{1.8F};
};

struct WeaponInstance {
    std::string definitionId;
    WeaponState state{WeaponState::Holstered};
    int magazine{0};
    float timer{0.0F};
    std::uint64_t shotsFired{0};
};

struct DamageEvent {
    EntityId source{0};
    EntityId instigator{0};
    EntityId target{0};
    DamageType type{DamageType::Generic};
    float amount{0.0F};
    glm::vec3 hitPoint{};
    glm::vec3 hitNormal{};
    glm::vec3 impulse{};
    std::uint64_t sequence{0};
};

struct HitResult {
    bool hit{false};
    EntityId entity{0};
    float distance{0.0F};
    glm::vec3 point{};
    glm::vec3 normal{};
    SurfaceType surface{SurfaceType::Default};
};

struct Projectile {
    EntityId id{0};
    EntityId owner{0};
    EntityId instigator{0};
    glm::vec3 position{};
    glm::vec3 previousPosition{};
    glm::vec3 velocity{};
    float radius{0.1F};
    float remainingLifetime{0.0F};
    float directDamage{0.0F};
    float splashDamage{0.0F};
    float splashRadius{0.0F};
    DamageType damageType{DamageType::Projectile};
    bool selfDamage{false};
    bool active{true};
};

struct EnemyActor {
    EntityId id{0};
    std::string name;
    std::string definitionId;
    std::vector<std::size_t> primitiveIndices;
    glm::vec3 spawnPosition{};
    glm::vec3 position{};
    glm::vec3 forward{0.0F, 0.0F, -1.0F};
    glm::vec3 lastSeenPlayer{};
    int health{1};
    EnemyState state{EnemyState::Idle};
    bool startsActive{true};
    bool active{true};
    float stateTimer{0.0F};
    float attackCooldown{0.0F};
    float lostSightTimer{0.0F};
    bool sawPlayerLastTick{false};
};

struct CombatInput {
    bool firePressed{false};
    bool fireHeld{false};
    bool reloadPressed{false};
    int selectSlot{-1};
    int cycleDirection{0};
};

struct CombatDefinitions {
    std::vector<WeaponDefinition> weapons;
    std::vector<EnemyDefinition> enemies;
    std::unordered_map<std::string, int> maximumAmmo;
};

[[nodiscard]] bool loadCombatDefinitions(const std::filesystem::path& path,
                                         CombatDefinitions& definitions,
                                         std::string& error);
[[nodiscard]] float splashDamageAtDistance(float maximumDamage, float radius,
                                           float distance) noexcept;
[[nodiscard]] bool insidePerceptionCone(const glm::vec3& observerForward,
                                        const glm::vec3& toTarget,
                                        float fieldOfViewDegrees) noexcept;
[[nodiscard]] const char* weaponStateName(WeaponState state) noexcept;
[[nodiscard]] const char* enemyStateName(EnemyState state) noexcept;

class CombatRng {
public:
    explicit CombatRng(std::uint64_t seed = 0xC0FFEE1234ULL) noexcept;
    [[nodiscard]] std::uint32_t next() noexcept;
    [[nodiscard]] float unitFloat() noexcept;
    [[nodiscard]] glm::vec3 spreadDirection(const glm::vec3& forward,
                                            float coneDegrees) noexcept;
    void reset(std::uint64_t seed) noexcept;
    [[nodiscard]] std::uint64_t seed() const noexcept;
    [[nodiscard]] std::uint64_t sequence() const noexcept;
private:
    std::uint64_t state_{0};
    std::uint64_t seed_{0};
    std::uint64_t sequence_{0};
};

class AmmoInventory {
public:
    void configure(const std::unordered_map<std::string, int>& maximum);
    [[nodiscard]] int get(std::string_view type) const;
    [[nodiscard]] int maximum(std::string_view type) const;
    [[nodiscard]] int add(std::string_view type, int amount);
    [[nodiscard]] bool consume(std::string_view type, int amount);
    void clear() noexcept;
    [[nodiscard]] const std::unordered_map<std::string, int>& values() const noexcept;
    [[nodiscard]] bool restore(const std::unordered_map<std::string, int>& values);
private:
    std::unordered_map<std::string, int> maximum_;
    std::unordered_map<std::string, int> values_;
};
