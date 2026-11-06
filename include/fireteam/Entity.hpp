// Entity: abstract base class for everything that lives in the GameWorld
// (players, bots, projectiles).
//
// This is a `class` (not a `struct`) because it protects an invariant:
// health is never negative. The only way to change health from outside is
// `applyDamage()`, which enforces that rule in one place.
#pragma once

#include "fireteam/Vector2.hpp"

namespace fireteam {

class Entity {
 public:
  // `explicit` stops the compiler from silently converting an int into an
  // Entity (e.g. passing `100` where an Entity was expected).
  explicit Entity(Vector2 position, int maxHealth);

  // RULE: a class with any virtual method needs a virtual destructor.
  // Without it, `delete`-ing a Player through an `Entity*` (which is exactly
  // what `std::unique_ptr<Entity>` does) would only run ~Entity() and skip
  // ~Player(), leaking whatever Player owns. `= default` says "generate the
  // normal destructor, just make it virtual."
  virtual ~Entity() = default;

  // Copying a polymorphic object through a base reference "slices" off the
  // derived part, which is almost always a bug. Deleting these makes that
  // mistake a compile error instead of a silent runtime bug. Entities are
  // owned by unique_ptr and never need copying anyway.
  Entity(const Entity&) = delete;
  Entity& operator=(const Entity&) = delete;

  // Pure virtual (`= 0`): Entity provides no implementation, so Entity
  // itself can never be instantiated -- only subclasses that override this.
  // `deltaSeconds` is the time since the last frame, so movement is
  // frame-rate independent (`position += velocity * deltaSeconds`).
  virtual void update(float deltaSeconds) = 0;

  // Reduces health by `amount`, clamping at 0. Non-positive `amount` is
  // ignored (you can't "damage" something by a negative number to heal it).
  // Not virtual: every entity takes damage the same way. Subclasses that
  // need a reaction to damage can check `isAlive()` in their update().
  void applyDamage(int amount);

  // Accessors are `const` because they don't modify the object; this lets
  // them be called on `const Entity&` (e.g. when another entity is only
  // inspecting this one for a range check).
  const Vector2& position() const { return position_; }
  int health() const { return health_; }
  int maxHealth() const { return maxHealth_; }
  bool isAlive() const { return health_ > 0; }

  void setPosition(Vector2 position) { position_ = position; }

 protected:
  // `protected` lets subclasses (Player, Bot) read/write position directly
  // in their update() without exposing it to unrelated code.
  Vector2 position_;

 private:
  // Trailing underscore marks private data members, distinguishing them from
  // the accessor methods of the same name (`health_` vs `health()`).
  int health_;
  int maxHealth_;
};

}  // namespace fireteam
