#pragma once

#include "gameplay/GameplayTypes.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

enum class DamageType { Generic, Bullet, Pellet, Explosive, Projectile, Melee,
                        Environmental, Crushing };

class PlayerInventory {
public:
    [[nodiscard]] bool hasKey(std::string_view keyId) const;
    [[nodiscard]] bool addKey(std::string keyId);
    [[nodiscard]] bool removeKey(std::string_view keyId);
    void clear() noexcept;
    [[nodiscard]] std::vector<std::string> sortedKeys() const;

private:
    std::unordered_set<std::string> keys_;
};

struct PlayerVitals {
    int health{100};
    int maximumHealth{100};
    int armor{0};
    int maximumArmor{100};
    bool alive{true};

    void reset(int newHealth = 100, int newArmor = 0) noexcept;
    [[nodiscard]] int applyDamage(int amount, DamageType type, EntityId source,
                                  bool bypassArmor = false) noexcept;
    [[nodiscard]] bool addHealth(int amount) noexcept;
    [[nodiscard]] bool addArmor(int amount) noexcept;
};
