#include <doctest/doctest.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "fireteam/Combat.hpp"
#include "fireteam/Grenade.hpp"
#include "fireteam/Projectile.hpp"

using namespace fireteam;

// All distances are in metres. Gravity is 9.8 m/s^2, a throw goes up at
// 7 m/s, and the fuse is 3 seconds.

// Entity is abstract, so we need a simple subclass for the tests.
// The unnamed namespace keeps these helpers private to this file.
namespace {
class Trooper : public Entity {
 public:
  Trooper(Vector2 position, int maxHealth) : Entity(position, maxHealth) {}
  void update(float) override {}
};

Trooper* addTrooper(std::vector<std::unique_ptr<Entity>>& list,
                    Vector2 position, int health = 100) {
  auto trooper = std::make_unique<Trooper>(position, health);
  Trooper* ptr = trooper.get();
  list.push_back(std::move(trooper));
  return ptr;
}

// A grenade shot thrown along +x, starting at (0, startHeight, 0).
Shot makeGrenadeShot(float range = 15.0f, int damage = 100,
                     float blastRadius = 5.0f, float startHeight = 0.0f) {
  Shot shot;
  shot.kind = ShotKind::Grenade;
  shot.origin = Vector3(0, startHeight, 0);
  shot.direction = Vector3(1, 0, 0);
  shot.range = range;
  shot.damage = damage;
  shot.blastRadius = blastRadius;
  return shot;
}

// Runs update() until the first bounce (or gives up after 1000 frames).
void flyUntilFirstLanding(Projectile& p, float dt) {
  int frames = 0;
  while (p.bounceCount() == 0 && frames < 1000) {
    p.update(dt);
    frames++;
  }
}
}  // namespace

// 1/64 of a second (about 60 fps). It's exact in floating point, so the
// fuse adds up to exactly 3.0 after 192 frames.
const float DT = 1.0f / 64.0f;
const int FUSE_FRAMES = 192;

// ---------- Grenade weapon ----------

TEST_CASE("Grenade makes a grenade Shot with its blast radius") {
  Grenade grenade(100, 15.0f, 1.0f, 5.0f);
  Shot shot;

  REQUIRE(grenade.fire(Vector3(5, 1.5f, 5), Vector3(0, 0, 1), shot));
  CHECK(shot.kind == ShotKind::Grenade);
  CHECK(shot.origin == Vector3(5, 1.5f, 5));
  CHECK(shot.direction == Vector3(0, 0, 1));
  CHECK(shot.damage == 100);
  CHECK(shot.range == 15.0f);
  CHECK(shot.blastRadius == 5.0f);
}

TEST_CASE("Grenade uses the normal weapon cooldown") {
  Grenade grenade(100, 15.0f, 1.0f, 5.0f);
  Shot shot;

  REQUIRE(grenade.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
  CHECK_FALSE(grenade.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
  grenade.update(1.0f);
  CHECK(grenade.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
}

TEST_CASE("resolveShot ignores grenade shots") {
  std::vector<std::unique_ptr<Entity>> world;
  Trooper* thrower = addTrooper(world, Vector2(0, 0));
  Trooper* target = addTrooper(world, Vector2(10, 0));

  CHECK(resolveShot(makeGrenadeShot(), *thrower, world) == nullptr);
  CHECK(target->health() == 100);
}

TEST_CASE("A negative blast radius is fixed to 0") {
  Grenade grenade(100, 15.0f, 1.0f, -10.0f);
  CHECK(grenade.blastRadius() == 0.0f);
}

// ---------- Flight: the arc ----------

TEST_CASE("Projectile height follows a parabola, not a straight line") {
  Projectile p(makeGrenadeShot());
  float heights[21];
  heights[0] = p.height();  // starts on the ground
  for (int i = 1; i <= 20; i++) {
    p.update(DT);
    heights[i] = p.height();
  }

  // In a parabola the change in height shrinks by the same amount every
  // frame (constant "second difference" = -gravity * dt * dt).
  // A straight line would have a second difference of 0.
  float expected = -PROJECTILE_GRAVITY * DT * DT;
  for (int i = 1; i <= 19; i++) {
    float secondDiff = heights[i + 1] - 2 * heights[i] + heights[i - 1];
    CHECK(secondDiff == doctest::Approx(expected).epsilon(0.01));
  }
}

TEST_CASE("Projectile peaks at about up^2 / (2 * gravity)") {
  // 7 * 7 / (2 * 9.8) = 2.5 m from the ground.
  Projectile p(makeGrenadeShot());
  float highest = 0.0f;
  for (int i = 0; i < 100; i++) {
    p.update(DT);
    if (p.height() > highest) {
      highest = p.height();
    }
  }
  CHECK(highest == doctest::Approx(2.5f).epsilon(0.03));
}

TEST_CASE("Thrown from hand height, a grenade goes over tall cover") {
  // Starting at 1.5 m it peaks around 4 m, well over the 2.10 m walls.
  Projectile p(makeGrenadeShot(15.0f, 100, 5.0f, 1.5f));
  CHECK(p.height() == 1.5f);

  float highest = 0.0f;
  for (int i = 0; i < 100; i++) {
    p.update(DT);
    if (p.height() > highest) {
      highest = p.height();
    }
  }
  CHECK(highest > 2.10f);
}

TEST_CASE("Projectile moves in a straight line on the ground while flying") {
  // From the ground it's in the air for 2 * 7 / 9.8 seconds, so the
  // ground speed is 15 / that = 10.5 m/s.
  Projectile p(makeGrenadeShot());
  for (int i = 1; i <= 40; i++) {
    p.update(DT);
    CHECK(p.position().x == doctest::Approx(10.5f * DT * i));
    CHECK(p.position().y == 0.0f);  // ground z doesn't change
  }
}

TEST_CASE("First landing is about `range` away") {
  Projectile fromGround(makeGrenadeShot(15.0f));
  flyUntilFirstLanding(fromGround, DT);
  REQUIRE(fromGround.bounceCount() == 1);
  CHECK(fromGround.position().x == doctest::Approx(15.0f).epsilon(0.05));

  // Starting higher means more time in the air, and the constructor
  // takes that into account, so it still lands about 15 m away.
  Projectile fromHand(makeGrenadeShot(15.0f, 100, 5.0f, 1.5f));
  flyUntilFirstLanding(fromHand, DT);
  REQUIRE(fromHand.bounceCount() == 1);
  CHECK(fromHand.position().x == doctest::Approx(15.0f).epsilon(0.05));
}

TEST_CASE("Only the ground part of the aim decides the direction") {
  // Aiming up and along +z still throws the same lob along +z.
  Shot shot = makeGrenadeShot();
  shot.direction = Vector3(0, 0.8f, 0.6f);
  Projectile p(shot);

  CHECK(p.velocity().x == doctest::Approx(0.0f));
  CHECK(p.velocity().y == doctest::Approx(PROJECTILE_THROW_UP_SPEED));
  CHECK(p.velocity().z == doctest::Approx(10.5f));
}

TEST_CASE("Throwing with no aim direction goes straight up and down") {
  Shot shot = makeGrenadeShot();
  shot.direction = Vector3(0, 0, 0);
  Projectile p(shot);

  p.update(DT);
  p.update(DT);
  CHECK(p.position() == Vector2(0, 0));
  CHECK(p.height() > 0.0f);
}

// ---------- Bouncing ----------

TEST_CASE("Projectile bounces lower and slower each time, then stops") {
  Projectile p(makeGrenadeShot());
  flyUntilFirstLanding(p, DT);

  // It lands at about 7 m/s down and bounces up at half that.
  CHECK(p.bounceCount() == 1);
  CHECK(p.velocity().y == doctest::Approx(3.5f).epsilon(0.05));
  CHECK(p.velocity().x == doctest::Approx(5.25f));  // ground speed halved
  CHECK_FALSE(p.isStopped());

  // Bounces: 3.5, then 1.75, then 0.875 is under 1.5 m/s, so it stops.
  int frames = 0;
  while (!p.isStopped() && frames < 1000) {
    p.update(DT);
    CHECK(p.height() >= 0.0f);  // never under the floor
    frames++;
  }
  CHECK(p.isStopped());
  CHECK(p.bounceCount() == 3);  // no endless tiny bounces
  CHECK(p.velocity() == Vector3(0, 0, 0));
  CHECK(p.height() == 0.0f);
  CHECK_FALSE(p.readyToExplode());  // stopped before the 3 s fuse
}

TEST_CASE("A stopped projectile stays put") {
  Projectile p(makeGrenadeShot());
  for (int i = 0; i < 180; i++) {  // 2.8 s, under the fuse
    p.update(DT);
  }
  REQUIRE(p.isStopped());
  Vector2 restingSpot = p.position();

  p.update(DT);
  p.update(DT);
  CHECK(p.position() == restingSpot);
  CHECK(p.height() == 0.0f);
}

// ---------- Fuse ----------

TEST_CASE("Projectile is ready to explode exactly when the fuse runs out") {
  Projectile p(makeGrenadeShot());
  for (int i = 0; i < FUSE_FRAMES - 1; i++) {
    p.update(DT);
  }
  CHECK_FALSE(p.readyToExplode());

  p.update(DT);  // exactly 3.0 seconds
  CHECK(p.readyToExplode());
  CHECK(p.fuseLeft() == 0.0f);
  CHECK(p.isAlive());  // still alive until explode() is called
}

// ---------- Blast damage falloff ----------

TEST_CASE("Blast damage is full at the center and falls off with distance") {
  CHECK(blastDamageAt(0.0f, 5.0f, 100) == 100);
  CHECK(blastDamageAt(1.25f, 5.0f, 100) == 75);
  CHECK(blastDamageAt(2.5f, 5.0f, 100) == 50);
  CHECK(blastDamageAt(3.75f, 5.0f, 100) == 25);
}

TEST_CASE("A closer target takes more blast damage than a farther one") {
  std::vector<std::unique_ptr<Entity>> world;
  Trooper* close = addTrooper(world, Vector2(0.625f, 0));
  Trooper* far = addTrooper(world, Vector2(3.75f, 0));

  applyBlast(Vector3(0, 0, 0), 5.0f, 100, world);
  CHECK(close->health() < far->health());
  CHECK(close->health() == 100 - 88);  // 87.5 rounds to 88
  CHECK(far->health() == 100 - 25);
}

// ---------- Blast radius boundary ----------

TEST_CASE("Just inside the blast radius takes damage, just outside doesn't") {
  CHECK(blastDamageAt(4.99f, 5.0f, 100) >= 1);
  CHECK(blastDamageAt(5.0f, 5.0f, 100) >= 1);  // edge counts as inside
  CHECK(blastDamageAt(5.01f, 5.0f, 100) == 0);

  std::vector<std::unique_ptr<Entity>> world;
  Trooper* inside = addTrooper(world, Vector2(4.99f, 0));
  Trooper* outside = addTrooper(world, Vector2(5.01f, 0));

  CHECK(applyBlast(Vector3(0, 0, 0), 5.0f, 100, world) == 1);
  CHECK(inside->health() < 100);
  CHECK(outside->health() == 100);
}

// ---------- 3D blasts ----------

TEST_CASE("A blast right above someone's head measures from the head") {
  std::vector<std::unique_ptr<Entity>> world;
  Trooper* target = addTrooper(world, Vector2(0, 0));

  // 3 m up, the head is at 1.8 m, so it's 1.2 m away: 100 * (1 - 1.2/5).
  applyBlast(Vector3(0, 3, 0), 5.0f, 100, world);
  CHECK(target->health() == 100 - 76);
}

TEST_CASE("A blast high in the air is out of range of people below") {
  std::vector<std::unique_ptr<Entity>> world;
  Trooper* target = addTrooper(world, Vector2(0, 0));

  // 10 m up is 8.2 m from the top of the head, more than the 5 m radius.
  CHECK(applyBlast(Vector3(0, 10, 0), 5.0f, 100, world) == 0);
  CHECK(target->health() == 100);
}

// ---------- Cover ----------

TEST_CASE("Tall cover between the blast and you protects you") {
  std::vector<std::unique_ptr<Entity>> world;
  Trooper* hiding = addTrooper(world, Vector2(4, 0));
  std::vector<CoverBox> cover = {makeCover(Vector2(2, 0), 0.85f, 2.8f, 2.10f)};

  CHECK(applyBlast(Vector3(0, 0, 0), 5.0f, 100, world, cover) == 0);
  CHECK(hiding->health() == 100);
}

TEST_CASE("Cover that isn't between the blast and you doesn't help") {
  std::vector<std::unique_ptr<Entity>> world;
  Trooper* target = addTrooper(world, Vector2(4, 0));
  std::vector<CoverBox> cover = {makeCover(Vector2(2, 5), 0.85f, 2.8f, 2.10f)};

  CHECK(applyBlast(Vector3(0, 0, 0), 5.0f, 100, world, cover) == 1);
  CHECK(target->health() < 100);
}

TEST_CASE("Low cover blocks a grenade on the ground but not one above it") {
  std::vector<CoverBox> cover = {makeCover(Vector2(3, 0), 0.65f, 3.0f, 1.28f)};

  // On the ground: the cover blocks the line to the feet AND to the head.
  std::vector<std::unique_ptr<Entity>> world1;
  Trooper* a = addTrooper(world1, Vector2(4, 0));
  CHECK(applyBlast(Vector3(2, 0, 0), 5.0f, 100, world1, cover) == 0);
  CHECK(a->health() == 100);

  // Going off 2.5 m up: the line to the head goes over the cover.
  std::vector<std::unique_ptr<Entity>> world2;
  Trooper* b = addTrooper(world2, Vector2(4, 0));
  CHECK(applyBlast(Vector3(2, 2.5f, 0), 5.0f, 100, world2, cover) == 1);
  CHECK(b->health() < 100);
}

TEST_CASE("explodeReadyProjectiles passes the cover on to the blast") {
  std::vector<std::unique_ptr<Entity>> world;
  Trooper* hiding = addTrooper(world, Vector2(4, 0));
  std::vector<CoverBox> cover = {makeCover(Vector2(2, 0), 0.85f, 2.8f, 2.10f)};

  // No aim: it goes straight up, bounces at (0, 0), and stops there
  // before the fuse runs out.
  Shot shot = makeGrenadeShot();
  shot.direction = Vector3(0, 0, 0);
  auto grenade = std::make_unique<Projectile>(shot);
  Projectile* grenadePtr = grenade.get();
  for (int i = 0; i < FUSE_FRAMES; i++) {
    grenadePtr->update(DT);
  }
  REQUIRE(grenadePtr->readyToExplode());
  CHECK(grenadePtr->worldPosition() == Vector3(0, 0, 0));
  world.push_back(std::move(grenade));

  CHECK(explodeReadyProjectiles(world, cover) == 1);
  CHECK_FALSE(grenadePtr->isAlive());
  CHECK(hiding->health() == 100);  // the wall took the blast
}

// ---------- Explosions ----------

TEST_CASE("Exploding damages nearby entities, including the thrower") {
  std::vector<std::unique_ptr<Entity>> world;
  Trooper* thrower = addTrooper(world, Vector2(1, 0));
  Trooper* target = addTrooper(world, Vector2(3, 0));
  Projectile p(makeGrenadeShot());  // sitting at (0, 0, 0)

  CHECK(p.explode(world) == 2);
  CHECK(thrower->health() < 100);  // grenades don't care who threw them
  CHECK(target->health() < 100);
}

TEST_CASE("A projectile only explodes once") {
  std::vector<std::unique_ptr<Entity>> world;
  Trooper* target = addTrooper(world, Vector2(0, 0));
  Projectile p(makeGrenadeShot());

  p.explode(world);
  CHECK_FALSE(p.isAlive());  // dead, so GameWorld will remove it
  CHECK(target->health() == 0);

  // Calling explode() again does nothing.
  CHECK(p.explode(world) == 0);
}

TEST_CASE("A blast skips dead entities and other projectiles") {
  std::vector<std::unique_ptr<Entity>> world;
  Trooper* dead = addTrooper(world, Vector2(1, 0), 10);
  dead->applyDamage(10);
  auto other = std::make_unique<Projectile>(makeGrenadeShot());
  Projectile* otherPtr = other.get();
  world.push_back(std::move(other));

  CHECK(applyBlast(Vector3(0, 0, 0), 5.0f, 100, world) == 0);
  CHECK(dead->health() == 0);
  CHECK(otherPtr->isAlive());  // one grenade doesn't destroy another
}

TEST_CASE("explodeReadyProjectiles only explodes projectiles whose fuse is done") {
  std::vector<std::unique_ptr<Entity>> world;
  auto ready = std::make_unique<Projectile>(makeGrenadeShot());
  auto notReady = std::make_unique<Projectile>(makeGrenadeShot());
  Projectile* readyPtr = ready.get();
  Projectile* notReadyPtr = notReady.get();
  for (int i = 0; i < FUSE_FRAMES; i++) {
    readyPtr->update(DT);  // 3 seconds, fuse done
  }
  notReadyPtr->update(DT);
  world.push_back(std::move(ready));
  world.push_back(std::move(notReady));

  CHECK(explodeReadyProjectiles(world) == 1);
  CHECK_FALSE(readyPtr->isAlive());
  CHECK(notReadyPtr->isAlive());
}

TEST_CASE("Full flow: throw from the hand, fly, bounce, explode") {
  std::vector<std::unique_ptr<Entity>> world;
  Trooper* thrower = addTrooper(world, Vector2(0, 0));
  Trooper* near = addTrooper(world, Vector2(21, 0));
  Trooper* far = addTrooper(world, Vector2(30, 0));
  Grenade grenade;  // default: 15 m range, 5 m blast
  Shot shot;

  Vector3 hand = groundToWorld(thrower->position(), 1.5f);
  REQUIRE(grenade.fire(hand, Vector3(1, 0, 0), shot));
  auto projectile = std::make_unique<Projectile>(shot);
  Projectile* grenadePtr = projectile.get();
  world.push_back(std::move(projectile));

  // Run the game for 3 seconds: update everything, then explode grenades.
  for (int frame = 0; frame < FUSE_FRAMES; frame++) {
    for (const auto& entity : world) {
      entity->update(DT);
    }
    explodeReadyProjectiles(world);
  }

  // It lands at about 15 m, then bounces and rolls on to about 20 m.
  CHECK(grenadePtr->position().x > 18.0f);
  CHECK(grenadePtr->position().x < 22.0f);
  CHECK_FALSE(grenadePtr->isAlive());  // it went off

  CHECK(near->health() < 100);      // about 1 m from the blast
  CHECK(far->health() == 100);      // about 10 m away, outside the radius
  CHECK(thrower->health() == 100);  // about 20 m away, safe
}

// ---------- Invalid input ----------

TEST_CASE("Projectile update throws on negative, NaN, or infinite dt") {
  Projectile p(makeGrenadeShot());
  float nan = std::numeric_limits<float>::quiet_NaN();
  float inf = std::numeric_limits<float>::infinity();

  CHECK_THROWS_AS(p.update(-1.0f), std::invalid_argument);
  CHECK_THROWS_AS(p.update(nan), std::invalid_argument);
  CHECK_THROWS_AS(p.update(inf), std::invalid_argument);

  // Nothing moved.
  CHECK(p.height() == 0.0f);
  CHECK(p.position() == Vector2(0, 0));
}

TEST_CASE("A grenade thrown from under the floor starts on the floor") {
  Projectile p(makeGrenadeShot(15.0f, 100, 5.0f, -2.0f));
  CHECK(p.height() == 0.0f);
}

TEST_CASE("Zero or negative blast damage does nothing") {
  CHECK(blastDamageAt(0.0f, 5.0f, 0) == 0);
  CHECK(blastDamageAt(0.0f, 5.0f, -50) == 0);
}
