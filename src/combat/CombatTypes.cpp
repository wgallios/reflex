#include "combat/CombatTypes.hpp"

#include <nlohmann/json.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <unordered_set>

namespace {
using json = nlohmann::json;

bool finitePositive(const float value) { return std::isfinite(value) && value > 0.0F; }

std::optional<WeaponDefinition> parseWeapon(const json& value, std::string& error) {
    if (!value.is_object()) { error = "weapon entry is not an object"; return std::nullopt; }
    WeaponDefinition result;
    result.id = value.value("id", ""); result.displayName = value.value("display_name", result.id);
    const std::string attack = value.value("attack_type", "hitscan");
    result.attackType = attack == "projectile" ? AttackType::Projectile : AttackType::Hitscan;
    result.damageType = result.attackType == AttackType::Projectile ? DamageType::Projectile :
        (value.value("pellets", 1) > 1 ? DamageType::Pellet : DamageType::Bullet);
    result.damage = value.value("damage", 0.0F); result.range = value.value("range", 100.0F);
    result.shotsPerSecond = value.value("shots_per_second", 0.0F);
    result.ammoType = value.value("ammo_type", "");
    result.magazineSize = value.value("magazine_size", 0);
    result.reloadTime = value.value("reload_time", 1.0F);
    result.equipTime = value.value("equip_time", 0.25F);
    result.spreadDegrees = value.value("spread_degrees", 0.0F);
    result.pellets = value.value("pellets", 1);
    result.automatic = value.value("automatic", false);
    result.projectileSpeed = value.value("projectile_speed", 20.0F);
    result.projectileRadius = value.value("projectile_radius", 0.12F);
    result.projectileLifetime = value.value("projectile_lifetime", 5.0F);
    result.splashDamage = value.value("splash_damage", 0.0F);
    result.splashRadius = value.value("splash_radius", 0.0F);
    result.selfDamage = value.value("self_damage", false);
    result.tracerFrequency = std::max(1, value.value("tracer_frequency", 1));
    if (result.id.empty() || result.ammoType.empty() || !finitePositive(result.damage) ||
        !finitePositive(result.shotsPerSecond) || result.magazineSize <= 0 ||
        !finitePositive(result.reloadTime) || result.pellets <= 0 ||
        result.spreadDegrees < 0.0F || !std::isfinite(result.spreadDegrees) ||
        (result.attackType == AttackType::Projectile &&
         (!finitePositive(result.projectileSpeed) || !finitePositive(result.projectileLifetime) ||
          !finitePositive(result.projectileRadius)))) {
        error = "weapon '" + result.id + "' has invalid required values";
        return std::nullopt;
    }
    return result;
}

std::optional<EnemyDefinition> parseEnemy(const json& value, std::string& error) {
    if (!value.is_object()) { error = "enemy entry is not an object"; return std::nullopt; }
    EnemyDefinition result;
    result.id = value.value("id", ""); result.displayName = value.value("display_name", result.id);
    result.maximumHealth = value.value("maximum_health", 75);
    result.movementSpeed = value.value("movement_speed", 2.5F);
    result.sightDistance = value.value("sight_distance", 25.0F);
    result.fieldOfViewDegrees = value.value("field_of_view_degrees", 100.0F);
    result.reactionTime = value.value("reaction_time", 0.25F);
    result.lostSightDuration = value.value("lost_sight_duration", 2.0F);
    result.attackDamage = value.value("attack_damage", 8.0F);
    result.attackRange = value.value("attack_range", 18.0F);
    result.shotsPerSecond = value.value("shots_per_second", 1.5F);
    result.attackSpreadDegrees = value.value("attack_spread_degrees", 4.0F);
    result.painDuration = value.value("pain_duration", 0.2F);
    result.radius = value.value("radius", 0.4F); result.height = value.value("height", 1.8F);
    if (result.id.empty() || result.maximumHealth <= 0 || !finitePositive(result.movementSpeed) ||
        !finitePositive(result.sightDistance) || result.fieldOfViewDegrees <= 0.0F ||
        result.fieldOfViewDegrees >= 180.0F || !finitePositive(result.attackRange) ||
        !finitePositive(result.shotsPerSecond) || result.height <= result.radius * 2.0F) {
        error = "enemy '" + result.id + "' has invalid required values";
        return std::nullopt;
    }
    return result;
}
} // namespace

bool loadCombatDefinitions(const std::filesystem::path& path, CombatDefinitions& definitions,
                           std::string& error) {
    std::ifstream stream{path};
    if (!stream) { error = "combat definition file not found: " + path.string(); return false; }
    json root;
    try { stream >> root; } catch (const json::exception& exception) {
        error = std::string{"invalid combat definition JSON: "} + exception.what(); return false;
    }
    CombatDefinitions parsed;
    try {
        if (!root.contains("weapons") || !root["weapons"].is_array() ||
            !root.contains("enemies") || !root["enemies"].is_array() ||
            !root.contains("maximum_ammo") || !root["maximum_ammo"].is_object()) {
            error = "combat definitions require weapons, enemies, and maximum_ammo"; return false;
        }
        std::unordered_set<std::string> ids;
        for (const json& entry : root["weapons"]) {
            auto weapon = parseWeapon(entry, error); if (!weapon) return false;
            if (!ids.insert(weapon->id).second) { error = "duplicate weapon id: " + weapon->id; return false; }
            parsed.weapons.push_back(std::move(*weapon));
        }
        ids.clear();
        for (const json& entry : root["enemies"]) {
            auto enemy = parseEnemy(entry, error); if (!enemy) return false;
            if (!ids.insert(enemy->id).second) { error = "duplicate enemy id: " + enemy->id; return false; }
            parsed.enemies.push_back(std::move(*enemy));
        }
        for (const auto& [name, value] : root["maximum_ammo"].items()) {
            if (!value.is_number_integer() || value.get<int>() <= 0) {
                error = "maximum ammo for '" + name + "' must be positive"; return false;
            }
            parsed.maximumAmmo.emplace(name, value.get<int>());
        }
        for (const WeaponDefinition& weapon : parsed.weapons) {
            if (!parsed.maximumAmmo.contains(weapon.ammoType)) {
                error = "weapon '" + weapon.id + "' references unknown ammo '" + weapon.ammoType + "'";
                return false;
            }
        }
    } catch (const json::exception& exception) {
        error = std::string{"invalid combat definition value: "} + exception.what(); return false;
    }
    definitions = std::move(parsed); return true;
}

float splashDamageAtDistance(const float maximumDamage, const float radius,
                             const float distance) noexcept {
    if (maximumDamage <= 0.0F || radius <= 0.0F || distance >= radius) return 0.0F;
    return maximumDamage * glm::clamp(1.0F - std::max(distance, 0.0F) / radius, 0.0F, 1.0F);
}

bool insidePerceptionCone(const glm::vec3& observerForward, const glm::vec3& toTarget,
                          const float fieldOfViewDegrees) noexcept {
    const glm::vec2 forward{observerForward.x, observerForward.z};
    const glm::vec2 target{toTarget.x, toTarget.z};
    if (glm::dot(forward, forward) < 0.000001F || glm::dot(target, target) < 0.000001F) return true;
    return glm::dot(glm::normalize(forward), glm::normalize(target)) >=
           std::cos(glm::radians(fieldOfViewDegrees * 0.5F));
}

const char* weaponStateName(const WeaponState state) noexcept {
    switch (state) { case WeaponState::Holstered:return "HOLSTERED"; case WeaponState::Equipping:return "EQUIPPING";
    case WeaponState::Ready:return "READY"; case WeaponState::Firing:return "FIRING";
    case WeaponState::Cooldown:return "COOLDOWN"; case WeaponState::Reloading:return "RELOADING";
    case WeaponState::Empty:return "EMPTY"; } return "UNKNOWN";
}
const char* enemyStateName(const EnemyState state) noexcept {
    switch (state) { case EnemyState::Idle:return "IDLE"; case EnemyState::Alert:return "ALERT";
    case EnemyState::Chasing:return "CHASING"; case EnemyState::Attacking:return "ATTACKING";
    case EnemyState::Pain:return "PAIN"; case EnemyState::Dead:return "DEAD"; } return "UNKNOWN";
}

CombatRng::CombatRng(const std::uint64_t seed) noexcept { reset(seed); }
void CombatRng::reset(const std::uint64_t seed) noexcept { seed_ = seed; state_ = seed ? seed : 1; sequence_ = 0; }
std::uint32_t CombatRng::next() noexcept {
    state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL; ++sequence_;
    std::uint64_t value = state_; value ^= value >> 18U; value ^= value << 27U; value ^= value >> 22U;
    return static_cast<std::uint32_t>(value >> 32U);
}
float CombatRng::unitFloat() noexcept { return static_cast<float>(next()) /
    static_cast<float>(std::numeric_limits<std::uint32_t>::max()); }
glm::vec3 CombatRng::spreadDirection(const glm::vec3& forward, const float coneDegrees) noexcept {
    const glm::vec3 f = glm::normalize(forward);
    const glm::vec3 helper = std::abs(f.y) < 0.99F ? glm::vec3{0,1,0} : glm::vec3{1,0,0};
    const glm::vec3 right = glm::normalize(glm::cross(f, helper));
    const glm::vec3 up = glm::normalize(glm::cross(right, f));
    const float radius = std::tan(glm::radians(coneDegrees)) * std::sqrt(unitFloat());
    constexpr float twoPi = 6.28318530717958647692F;
    const float angle = unitFloat() * twoPi;
    return glm::normalize(f + right * (radius * std::cos(angle)) + up * (radius * std::sin(angle)));
}
std::uint64_t CombatRng::seed() const noexcept { return seed_; }
std::uint64_t CombatRng::sequence() const noexcept { return sequence_; }

void AmmoInventory::configure(const std::unordered_map<std::string, int>& maximum) {
    maximum_ = maximum; values_.clear(); for (const auto& [name, value] : maximum_) values_[name] = 0;
}
int AmmoInventory::get(const std::string_view type) const { const auto it = values_.find(std::string{type}); return it == values_.end() ? 0 : it->second; }
int AmmoInventory::maximum(const std::string_view type) const { const auto it = maximum_.find(std::string{type}); return it == maximum_.end() ? 0 : it->second; }
int AmmoInventory::add(const std::string_view type, const int amount) { auto it = values_.find(std::string{type}); if (it == values_.end() || amount <= 0) return 0; const int before=it->second; it->second=std::min(maximum(type),before+amount); return it->second-before; }
bool AmmoInventory::consume(const std::string_view type, const int amount) { auto it=values_.find(std::string{type}); if(it==values_.end()||amount<0||it->second<amount)return false; it->second-=amount; return true; }
void AmmoInventory::clear() noexcept { for (auto& [name,value] : values_) { static_cast<void>(name); value=0; } }
const std::unordered_map<std::string,int>& AmmoInventory::values() const noexcept { return values_; }
bool AmmoInventory::restore(const std::unordered_map<std::string,int>& values) { for(const auto&[name,value]:values) if(!maximum_.contains(name)||value<0||value>maximum(name))return false; clear(); for(const auto&[name,value]:values) values_[name]=value; return true; }
