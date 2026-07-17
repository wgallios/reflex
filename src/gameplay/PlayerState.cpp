#include "gameplay/PlayerState.hpp"

bool PlayerInventory::hasKey(const std::string_view keyId) const {
    return keys_.contains(std::string{keyId});
}

bool PlayerInventory::addKey(std::string keyId) {
    if (keyId.empty()) return false;
    return keys_.insert(std::move(keyId)).second;
}

bool PlayerInventory::removeKey(const std::string_view keyId) {
    return keys_.erase(std::string{keyId}) != 0;
}

void PlayerInventory::clear() noexcept { keys_.clear(); }

std::vector<std::string> PlayerInventory::sortedKeys() const {
    std::vector<std::string> result{keys_.begin(), keys_.end()};
    std::sort(result.begin(), result.end());
    return result;
}

void PlayerVitals::reset(const int newHealth, const int newArmor) noexcept {
    maximumHealth = std::max(maximumHealth, 1);
    maximumArmor = std::max(maximumArmor, 0);
    health = std::clamp(newHealth, 1, maximumHealth);
    armor = std::clamp(newArmor, 0, maximumArmor);
    alive = true;
}

int PlayerVitals::applyDamage(const int amount, const DamageType, const EntityId,
                              const bool bypassArmor) noexcept {
    if (!alive || amount <= 0) return 0;
    int remaining = amount;
    if (!bypassArmor && armor > 0) {
        const int absorbed = std::min(armor, (amount + 1) / 2);
        armor -= absorbed;
        remaining -= absorbed;
    }
    health = std::max(0, health - remaining);
    alive = health > 0;
    return remaining;
}

bool PlayerVitals::addHealth(const int amount) noexcept {
    if (amount <= 0 || health >= maximumHealth || !alive) return false;
    health = std::min(maximumHealth, health + amount);
    return true;
}

bool PlayerVitals::addArmor(const int amount) noexcept {
    if (amount <= 0 || armor >= maximumArmor || !alive) return false;
    armor = std::min(maximumArmor, armor + amount);
    return true;
}

