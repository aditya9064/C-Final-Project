#include <doctest/doctest.h>

#include <cmath>

#include "fireteam/Player.hpp"

using fireteam::Player;
using fireteam::PlayerSnapshot;
using fireteam::Team;
using fireteam::Vector2;

namespace {
// Floating-point compare helper (doctest's doctest::Approx with a small eps).
bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }
}  // namespace

TEST_CASE("Player starts alive at full health on its team spawn") {
  Player p{Team::Teal, Vector2{-18.0f, 0.0f}};
  CHECK(p.isAlive());
  CHECK(p.health() == Player::kMaxHealth);
  CHECK(p.team() == Team::Teal);
  CHECK(p.position() == Vector2{-18.0f, 0.0f});
  CHECK(p.ammo() == Player::kMagazineSize);
}

TEST_CASE("Move intent becomes planar velocity at walk/run speed") {
  Player p{Team::Orange, Vector2{18.0f, 0.0f}};

  p.setMoveIntent(Vector2{1.0f, 0.0f});
  p.update(0.1f);
  CHECK(near(p.velocity().x, Player::kWalkSpeed));
  CHECK(near(p.velocity().y, 0.0f));

  p.setRunning(true);
  p.update(0.1f);
  CHECK(near(p.velocity().x, Player::kRunSpeed));
}

TEST_CASE("Diagonal intent is normalized (no faster-on-the-diagonal bug)") {
  Player p{Team::Teal, Vector2{}};
  p.setMoveIntent(Vector2{3.0f, 3.0f});  // magnitude 4.24, not 1
  p.update(0.1f);
  CHECK(near(p.velocity().length(), Player::kWalkSpeed));
}

TEST_CASE("update() integrates position frame-rate independently") {
  Player p{Team::Teal, Vector2{0.0f, 0.0f}};
  p.setMoveIntent(Vector2{1.0f, 0.0f});
  p.update(0.5f);
  CHECK(near(p.position().x, Player::kWalkSpeed * 0.5f));
}

TEST_CASE("No intent means zero velocity, position unchanged") {
  Player p{Team::Teal, Vector2{5.0f, 5.0f}};
  p.update(1.0f);
  CHECK(near(p.velocity().length(), 0.0f));
  CHECK(p.position() == Vector2{5.0f, 5.0f});
}

TEST_CASE("Firing consumes ammo and respects the fire rules") {
  Player p{Team::Orange, Vector2{}};
  CHECK(p.canFire());
  CHECK(p.fire());
  CHECK(p.ammo() == Player::kMagazineSize - 1);

  // Empty the magazine.
  while (p.ammo() > 0) CHECK(p.fire());
  CHECK_FALSE(p.canFire());
  CHECK_FALSE(p.fire());  // empty: no shot leaves the muzzle
  CHECK(p.ammo() == 0);
}

TEST_CASE("Reload refills the magazine after kReloadSeconds and blocks firing") {
  Player p{Team::Teal, Vector2{}};
  while (p.ammo() > 0) p.fire();
  p.reload();
  CHECK(p.isReloading());
  CHECK_FALSE(p.canFire());  // cannot fire mid-reload

  p.update(Player::kReloadSeconds * 0.5f);
  CHECK(p.isReloading());
  CHECK(near(p.reloadProgress(), 0.5f, 0.05f));

  p.update(Player::kReloadSeconds * 0.5f + 0.01f);
  CHECK_FALSE(p.isReloading());
  CHECK(p.ammo() == Player::kMagazineSize);
  CHECK(near(p.reloadProgress(), 1.0f));
}

TEST_CASE("A killed player freezes, runs a respawn countdown, then is ready") {
  Player p{Team::Orange, Vector2{18.0f, 0.0f}};
  // Combat only ever calls applyDamage(); Player must detect its own death on
  // the next update() -- no manual onKilled() call from the combat path.
  p.applyDamage(Player::kMaxHealth);  // combat lands the killing blow (#8/#9)
  CHECK_FALSE(p.isAlive());
  CHECK_FALSE(p.awaitingRespawn());  // not yet -- detected on the next tick
  p.update(0.016f);
  CHECK(p.awaitingRespawn());
  CHECK_FALSE(p.readyToRespawn());

  // Dead players do not move even with intent.
  p.setMoveIntent(Vector2{1.0f, 0.0f});
  p.update(0.1f);
  CHECK(near(p.velocity().length(), 0.0f));
  CHECK_FALSE(p.canFire());

  p.update(Player::kRespawnSeconds + 0.01f);
  CHECK_FALSE(p.awaitingRespawn());
  CHECK(p.readyToRespawn());  // owner now recreates a full-health player
}

TEST_CASE("Snapshot/applyRemoteState round-trips the proxy state (network seam)") {
  Player local{Team::Teal, Vector2{-18.0f, 3.0f}};
  local.setYaw(1.25f);
  local.fire();  // ammo 30 -> 29

  PlayerSnapshot s = local.snapshot();
  CHECK(near(s.x, -18.0f));
  CHECK(near(s.z, 3.0f));
  CHECK(near(s.yaw, 1.25f));
  CHECK(s.ammo == Player::kMagazineSize - 1);
  CHECK(s.team == Team::Teal);

  Player proxy{Team::Orange, Vector2{}};
  proxy.applyRemoteState(s);
  CHECK(near(proxy.position().x, -18.0f));
  CHECK(near(proxy.position().y, 3.0f));
  CHECK(near(proxy.yaw(), 1.25f));
  CHECK(proxy.ammo() == Player::kMagazineSize - 1);
  CHECK(proxy.team() == Team::Teal);
}

TEST_CASE("update() ignores non-positive and non-finite timesteps") {
  Player p{Team::Teal, Vector2{2.0f, 0.0f}};
  p.setMoveIntent(Vector2{1.0f, 0.0f});
  p.update(0.0f);
  CHECK(p.position() == Vector2{2.0f, 0.0f});
  p.update(-1.0f);
  CHECK(p.position() == Vector2{2.0f, 0.0f});
  p.update(std::nan(""));
  CHECK(p.position() == Vector2{2.0f, 0.0f});
}
