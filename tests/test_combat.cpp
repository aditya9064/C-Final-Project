#include <doctest/doctest.h>

#include <memory>
#include <vector>

#include "fireteam/Combat.hpp"
#include "fireteam/Rifle.hpp"

using namespace fireteam;

// All distances are in metres. Bodies are BODY_RADIUS (0.25 m) wide and
// BODY_HEIGHT (1.80 m) tall, and shots start at eye height (1.63 m).

// Entity is abstract, so we need a simple subclass for the tests.
// The unnamed namespace keeps these helpers private to this file.
namespace {
class Soldier : public Entity {
 public:
  Soldier(Vector2 position, int maxHealth) : Entity(position, maxHealth) {}
  void update(float) override {}
};

// Adds a soldier to the list and returns a plain pointer to it, so the
// test can check its health later. The list still owns it.
Soldier* addSoldier(std::vector<std::unique_ptr<Entity>>& list,
                    Vector2 position, int health = 100) {
  auto soldier = std::make_unique<Soldier>(position, health);
  Soldier* ptr = soldier.get();
  list.push_back(std::move(soldier));
  return ptr;
}

// Builds a shot by hand, so these tests don't depend on a Weapon.
// The shot starts at `from` (on the ground) at eye height and aims at
// the point above `toward` at `aimHeight`.
Shot makeTestShot(Vector2 from, Vector2 toward, float range, int damage,
                  float aimHeight = STAND_EYE_HEIGHT) {
  Shot shot;
  shot.origin = groundToWorld(from, STAND_EYE_HEIGHT);
  Vector3 target = groundToWorld(toward, aimHeight);
  shot.direction = (target - shot.origin).normalized();
  shot.range = range;
  shot.damage = damage;
  return shot;
}

// The same low barrier as the arena: 0.65 m thick (along x), 3 m wide
// (along z), 1.28 m tall.
CoverBox lowCoverAt(Vector2 center) {
  return makeCover(center, 0.65f, 3.0f, 1.28f);
}

CoverBox tallCoverAt(Vector2 center) {
  return makeCover(center, 0.85f, 2.8f, 2.10f);
}
}  // namespace

// ---------- Normal ----------

TEST_CASE("A target the shot goes through takes the shot's damage") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(30, 40));  // 50 m away

  Entity* hit = resolveShot(
      makeTestShot(Vector2(0, 0), Vector2(30, 40), 100.0f, 25), *shooter,
      world);

  CHECK(hit == target);
  CHECK(target->health() == 75);
  CHECK(shooter->health() == 100);
}

TEST_CASE("The shooter never hits themselves") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));

  // The shot starts inside the shooter's own body, but they're skipped.
  Entity* hit = resolveShot(
      makeTestShot(Vector2(0, 0), Vector2(10, 0), 100.0f, 25), *shooter,
      world);

  CHECK(hit == nullptr);
  CHECK(shooter->health() == 100);
}

TEST_CASE("Only the closest target on the line is hit") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* far = addSoldier(world, Vector2(80, 0));
  Soldier* near = addSoldier(world, Vector2(20, 0));

  Entity* hit = resolveShot(
      makeTestShot(Vector2(0, 0), Vector2(1, 0), 100.0f, 25), *shooter,
      world);

  CHECK(hit == near);
  CHECK(near->health() == 75);
  CHECK(far->health() == 100);
}

TEST_CASE("Enough hits kill a target and its health stops at 0") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(10, 0));
  Shot shot = makeTestShot(Vector2(0, 0), Vector2(10, 0), 100.0f, 40);

  resolveShot(shot, *shooter, world);
  resolveShot(shot, *shooter, world);
  resolveShot(shot, *shooter, world);

  CHECK(target->health() == 0);
  CHECK_FALSE(target->isAlive());
}

TEST_CASE("Rifle and resolveShot work together") {
  // The full flow a Player will use: fire from eye height, then resolve.
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(20, 0));
  Rifle rifle;
  Shot shot;
  Vector3 eye = groundToWorld(shooter->position(), STAND_EYE_HEIGHT);

  REQUIRE(rifle.fire(eye, Vector3(1, 0, 0), shot));
  CHECK(resolveShot(shot, *shooter, world) == target);
  CHECK(target->health() == 100 - RIFLE_DAMAGE);

  // Firing again right away is blocked by the cooldown, so no new shot.
  CHECK_FALSE(rifle.fire(eye, Vector3(1, 0, 0), shot));
  CHECK(target->health() == 100 - RIFLE_DAMAGE);
}

// ---------- Aim matters now ----------

TEST_CASE("A target behind the shooter is not hit") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* behind = addSoldier(world, Vector2(-20, 0));

  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(1, 0), 100.0f, 25),
                    *shooter, world) == nullptr);
  CHECK(behind->health() == 100);
}

TEST_CASE("A shot that passes beside the target misses") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(10, 1));  // 1 m to the side

  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(1, 0), 100.0f, 25),
                    *shooter, world) == nullptr);
  CHECK(target->health() == 100);
}

TEST_CASE("A shot aimed over the target's head misses") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(10, 0));

  // Aiming at 3 m high, the bullet is about 3 m up when it gets there.
  CHECK(resolveShot(
            makeTestShot(Vector2(0, 0), Vector2(10, 0), 100.0f, 25, 3.0f),
            *shooter, world) == nullptr);
  CHECK(target->health() == 100);
}

TEST_CASE("A steep shot from above hits the top of the head") {
  // Like shooting down from a ledge onto someone right below.
  float hitDist = 0.0f;
  Vector3 origin(9.5f, 4.0f, 0.0f);
  Vector3 direction = (Vector3(10.0f, 0.0f, 0.0f) - origin).normalized();

  REQUIRE(rayHitsBody(origin, direction, 100.0f, Vector2(10, 0), BODY_HEIGHT,
                      hitDist));
  // The bullet is at head height (1.80 m) when it hits.
  CHECK(origin.y + direction.y * hitDist == doctest::Approx(BODY_HEIGHT));
}

// ---------- Crouching ----------

TEST_CASE("A crouching body is shorter, so a shot at standing eye height "
          "goes over it") {
  float hitDist = 0.0f;
  Vector3 origin(0.0f, STAND_EYE_HEIGHT, 0.0f);
  Vector3 flat(1.0f, 0.0f, 0.0f);

  CHECK(rayHitsBody(origin, flat, 100.0f, Vector2(10, 0), BODY_HEIGHT,
                    hitDist));
  CHECK_FALSE(rayHitsBody(origin, flat, 100.0f, Vector2(10, 0),
                          CROUCH_HEIGHT, hitDist));

  // Aiming lower still hits the crouching body.
  Vector3 low(0.0f, 1.0f, 0.0f);
  CHECK(rayHitsBody(low, flat, 100.0f, Vector2(10, 0), CROUCH_HEIGHT,
                    hitDist));
}

// ---------- Cover ----------
// These copy the cover rules from Koushik's arena tests:
//   standing behind low cover  -> head is still exposed
//   crouching behind low cover -> hidden
//   standing behind tall cover -> hidden

TEST_CASE("Low cover: a standing target's head can still be shot") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(10, 0));
  std::vector<CoverBox> cover = {lowCoverAt(Vector2(8, 0))};

  // Eye height to eye height: 1.63 m, over the 1.28 m cover.
  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(10, 0), 100.0f, 25),
                    *shooter, world, cover) == target);
  CHECK(target->health() == 75);
}

TEST_CASE("Low cover: a shot aimed low hits the cover instead") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(10, 0));
  std::vector<CoverBox> cover = {lowCoverAt(Vector2(8, 0))};

  // Aiming at the hips (0.6 m): the bullet is under 1.28 m at the cover.
  CHECK(resolveShot(
            makeTestShot(Vector2(0, 0), Vector2(10, 0), 100.0f, 25, 0.6f),
            *shooter, world, cover) == nullptr);
  CHECK(target->health() == 100);
}

TEST_CASE("Low cover: a crouching target is hidden") {
  std::vector<CoverBox> cover = {lowCoverAt(Vector2(8, 0))};
  Vector3 shooterEye = groundToWorld(Vector2(0, 0), STAND_EYE_HEIGHT);

  Vector3 standingEye = groundToWorld(Vector2(10, 0), STAND_EYE_HEIGHT);
  Vector3 crouchingEye = groundToWorld(Vector2(10, 0), CROUCH_EYE_HEIGHT);

  CHECK(lineOfSightClear(shooterEye, standingEye, cover));
  CHECK_FALSE(lineOfSightClear(shooterEye, crouchingEye, cover));
}

TEST_CASE("Tall cover: even a head shot is blocked") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(10, 0));
  std::vector<CoverBox> cover = {tallCoverAt(Vector2(8, 0))};

  CHECK(resolveShot(
            makeTestShot(Vector2(0, 0), Vector2(10, 0), 100.0f, 25,
                         BODY_HEIGHT),
            *shooter, world, cover) == nullptr);
  CHECK(target->health() == 100);
}

TEST_CASE("Cover behind the target or off to the side doesn't block") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(10, 0));
  std::vector<CoverBox> cover = {tallCoverAt(Vector2(15, 0)),   // behind
                                 tallCoverAt(Vector2(5, 6))};  // to the side

  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(10, 0), 100.0f, 25),
                    *shooter, world, cover) == target);
}

TEST_CASE("rayHitsBox basics") {
  CoverBox box = makeCover(Vector2(5, 0), 1.0f, 1.0f, 2.0f);
  Vector3 flat(1.0f, 0.0f, 0.0f);
  float hitDist = 0.0f;

  // Straight into the front face at x = 4.5.
  REQUIRE(rayHitsBox(Vector3(0, 1, 0), flat, 100.0f, box, hitDist));
  CHECK(hitDist == doctest::Approx(4.5f));

  // Too short to reach it.
  CHECK_FALSE(rayHitsBox(Vector3(0, 1, 0), flat, 4.0f, box, hitDist));

  // Box is behind the start.
  CHECK_FALSE(rayHitsBox(Vector3(10, 1, 0), flat, 100.0f, box, hitDist));

  // Over the top of the box.
  CHECK_FALSE(rayHitsBox(Vector3(0, 2.5f, 0), flat, 100.0f, box, hitDist));

  // Starting inside the box is a hit right away.
  REQUIRE(rayHitsBox(Vector3(5, 1, 0), flat, 100.0f, box, hitDist));
  CHECK(hitDist == doctest::Approx(0.0f));
}

// ---------- Boundary ----------
// The target at x = 50.25 has its front surface at exactly 50 m
// (50.25 - 0.25 radius). These numbers are exact in floating point.

TEST_CASE("A target whose body is exactly at max range is hit") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(50.25f, 0));

  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(1, 0), 50.0f, 25),
                    *shooter, world) == target);
  CHECK(target->health() == 75);
}

TEST_CASE("A target just past max range is not hit") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(50.5f, 0));

  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(1, 0), 50.0f, 25),
                    *shooter, world) == nullptr);
  CHECK(target->health() == 100);
}

TEST_CASE("An empty world means nothing is hit") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier shooter(Vector2(0, 0), 100);  // not in the list at all

  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(1, 0), 100.0f, 25),
                    shooter, world) == nullptr);
}

// ---------- Invalid input ----------

TEST_CASE("Shooting an already dead target does nothing") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* dead = addSoldier(world, Vector2(10, 0), 10);
  dead->applyDamage(10);
  REQUIRE_FALSE(dead->isAlive());

  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(1, 0), 100.0f, 25),
                    *shooter, world) == nullptr);
  CHECK(dead->health() == 0);  // not negative
}

TEST_CASE("A dead target in front doesn't block a live one behind it") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* dead = addSoldier(world, Vector2(10, 0), 10);
  Soldier* alive = addSoldier(world, Vector2(60, 0));
  dead->applyDamage(10);

  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(1, 0), 100.0f, 25),
                    *shooter, world) == alive);
  CHECK(alive->health() == 75);
}

TEST_CASE("A dead shooter can't hit anything") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0), 10);
  Soldier* target = addSoldier(world, Vector2(10, 0));
  shooter->applyDamage(10);

  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(1, 0), 100.0f, 25),
                    *shooter, world) == nullptr);
  CHECK(target->health() == 100);
}

TEST_CASE("A null entry in the list is skipped, not a crash") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  world.push_back(nullptr);
  Soldier* target = addSoldier(world, Vector2(10, 0));

  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(1, 0), 100.0f, 25),
                    *shooter, world) == target);
}

TEST_CASE("A zero aim direction hits nothing") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(10, 0));

  Shot shot = makeTestShot(Vector2(0, 0), Vector2(1, 0), 100.0f, 25);
  shot.direction = Vector3(0, 0, 0);  // what fire() gives for a zero aim

  CHECK(resolveShot(shot, *shooter, world) == nullptr);
  CHECK(target->health() == 100);
}

TEST_CASE("A hand-made shot with bad range or damage does no harm") {
  std::vector<std::unique_ptr<Entity>> world;
  Soldier* shooter = addSoldier(world, Vector2(0, 0));
  Soldier* target = addSoldier(world, Vector2(10, 0));

  // Negative range: nothing can be that close.
  CHECK(resolveShot(makeTestShot(Vector2(0, 0), Vector2(1, 0), -5.0f, 25),
                    *shooter, world) == nullptr);

  // Negative damage: the target is "hit" but Entity::applyDamage
  // ignores it, so it isn't healed.
  resolveShot(makeTestShot(Vector2(0, 0), Vector2(1, 0), 100.0f, -25),
              *shooter, world);
  CHECK(target->health() == 100);
}
