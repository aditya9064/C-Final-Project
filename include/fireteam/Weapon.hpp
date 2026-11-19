#pragma once

#include <cstdint>

namespace fireteam {

class Weapon {
 public:
  virtual ~Weapon() = default;
  Weapon(const Weapon&) = delete;
  Weapon& operator=(const Weapon&) = delete;

  bool fire();
  bool reload();
  void update(float deltaSeconds);

  int damage() const { return damage_; }
  float range() const { return range_; }
  int ammo() const { return ammo_; }
  int magazineSize() const { return magazineSize_; }
  bool reloading() const { return reloadRemaining_ > 0.0f; }
  std::uint32_t shotsFired() const { return shotsFired_; }

 protected:
  Weapon(int damage, float range, float cooldownSeconds, int magazineSize,
         float reloadSeconds);

 private:
  int damage_;
  float range_;
  float cooldownSeconds_;
  int magazineSize_;
  float reloadSeconds_;
  int ammo_;
  float cooldownRemaining_ = 0.0f;
  float reloadRemaining_ = 0.0f;
  std::uint32_t shotsFired_ = 0;
};

}  // namespace fireteam
