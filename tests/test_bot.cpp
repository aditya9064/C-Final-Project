#include <doctest/doctest.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "fireteam/Bot.hpp"
#include "fireteam/Projectile.hpp"
#include "fireteam/Rifle.hpp"

using namespace fireteam;

// All distances below are in metres. Default bot: speed 4 m/s,
// sight range 40 m.

// Stand-in for a human player until Player (#6) exists.
// The unnamed namespace keeps these helpers private to this file.
namespace {
class FakePlayer : public Entity {
 public:
  FakePlayer(Vector2 position, int maxHealth) : Entity(position, maxHealth) {}
  void update(float) override {}
};

FakePlayer* addPlayer(std::vector<std::unique_ptr<Entity>>& list,
                      Vector2 position, int health = 100) {
  auto player = std::make_unique<FakePlayer>(position, health);
  FakePlayer* ptr = player.get();
  list.push_back(std::move(player));
  return ptr;
}

Bot* addBot(std::vector<std::unique_ptr<Entity>>& list, Vector2 position,
            std::vector<Vector2> waypoints = {}) {
  auto bot = std::make_unique<Bot>(position, waypoints);
  Bot* ptr = bot.get();
  list.push_back(std::move(bot));
  return ptr;
}
}  // namespace

TEST_CASE("A new bot starts patrolling with the default settings") {
  Bot bot(Vector2(0, 0), {});
  CHECK(bot.state() == BotState::Patrol);
  CHECK(bot.health() == BOT_HEALTH);
  CHECK(bot.sightRange() == BOT_SIGHT_RANGE);
  CHECK(bot.speed() == BOT_SPEED);
  CHECK(bot.weapon().damage() == RIFLE_DAMAGE);
  CHECK(bot.weapon().cooldownSeconds() == BOT_FIRE_COOLDOWN);
}

TEST_CASE("Bots fire slower than a player's rifle") {
  // So a player has time to shoot back.
  CHECK(BOT_FIRE_COOLDOWN > RIFLE_COOLDOWN);
}

TEST_CASE("Bot sight range is shorter than its rifle range") {
  // Otherwise a bot could see someone, start shooting, and always miss.
  Bot bot(Vector2(0, 0), {});
  CHECK(bot.sightRange() <= bot.weapon().range());
}

// ---------- Patrol (wandering) ----------

TEST_CASE("Patrolling bot walks toward its waypoint at its speed") {
  Bot bot(Vector2(0, 0), {Vector2(4, 0)});

  bot.update(0.5f);  // 4 m/s * 0.5 s = 2 m
  CHECK(bot.position() == Vector2(2, 0));
}

TEST_CASE("Patrolling bot goes around its waypoints and loops back") {
  Bot bot(Vector2(0, 0), {Vector2(4, 0), Vector2(4, 4)});

  bot.update(0.5f);
  bot.update(0.5f);  // reaches (4, 0)
  CHECK(bot.position() == Vector2(4, 0));
  CHECK(bot.currentWaypoint() == 1);

  bot.update(0.5f);
  bot.update(0.5f);  // reaches (4, 4), last one, so loop to the first
  CHECK(bot.position() == Vector2(4, 4));
  CHECK(bot.currentWaypoint() == 0);

  bot.update(0.5f);  // heading back toward (4, 0)
  CHECK(bot.position() == Vector2(4, 2));
}

TEST_CASE("A bot with no waypoints stands still") {
  Bot bot(Vector2(5, 5), {});
  bot.update(1.0f);
  CHECK(bot.position() == Vector2(5, 5));
}

TEST_CASE("Bot keeps patrolling when no player is in sight") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0), {Vector2(4, 0)});
  FakePlayer* player = addPlayer(world, Vector2(100, 0));  // way out of sight

  bot->think(world);
  bot->update(0.5f);

  CHECK(bot->state() == BotState::Patrol);
  CHECK(bot->position() == Vector2(2, 0));  // still walking
  CHECK(player->health() == 100);
}

// ---------- Engage ----------

TEST_CASE("Bot engages and shoots when a player comes into sight") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  FakePlayer* player = addPlayer(world, Vector2(20, 0));

  bot->think(world);

  CHECK(bot->state() == BotState::Engage);
  CHECK(player->health() == 100 - RIFLE_DAMAGE);
}

TEST_CASE("An engaging bot stands still") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0), {Vector2(4, 0)});
  addPlayer(world, Vector2(0, 20));

  bot->think(world);
  bot->update(0.5f);

  CHECK(bot->state() == BotState::Engage);
  CHECK(bot->position() == Vector2(0, 0));
}

TEST_CASE("An engaging bot can only shoot as fast as its cooldown allows") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  FakePlayer* player = addPlayer(world, Vector2(20, 0));

  bot->think(world);
  bot->think(world);  // same frame, rifle still cooling down
  CHECK(player->health() == 75);

  // A player's rifle would be ready again by now, but a bot's isn't.
  bot->update(RIFLE_COOLDOWN);
  bot->think(world);
  CHECK(player->health() == 75);

  bot->update(BOT_FIRE_COOLDOWN - RIFLE_COOLDOWN);
  bot->think(world);
  CHECK(player->health() == 50);
}

TEST_CASE("Bot shoots the closest player in sight") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  FakePlayer* far = addPlayer(world, Vector2(30, 0));
  FakePlayer* near = addPlayer(world, Vector2(0, 10));

  bot->think(world);
  CHECK(near->health() == 75);
  CHECK(far->health() == 100);
}

TEST_CASE("Bot goes back to patrolling when the player leaves sight") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  FakePlayer* player = addPlayer(world, Vector2(20, 0));

  bot->think(world);
  REQUIRE(bot->state() == BotState::Engage);

  player->setPosition(Vector2(100, 0));
  bot->think(world);
  CHECK(bot->state() == BotState::Patrol);
}

TEST_CASE("Bot goes back to patrolling when its target dies") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  FakePlayer* player = addPlayer(world, Vector2(20, 0), 25);

  bot->think(world);  // one rifle hit (25) kills it
  REQUIRE_FALSE(player->isAlive());

  bot->think(world);
  CHECK(bot->state() == BotState::Patrol);
}

// ---------- Sight range boundary ----------

TEST_CASE("A player exactly at sight range is seen") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  FakePlayer* player = addPlayer(world, Vector2(24, 32));  // exactly 40

  bot->think(world);
  CHECK(bot->state() == BotState::Engage);
  CHECK(player->health() == 75);
}

TEST_CASE("A player just past sight range is not seen") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  FakePlayer* player = addPlayer(world, Vector2(40.5f, 0));

  bot->think(world);
  CHECK(bot->state() == BotState::Patrol);
  CHECK(player->health() == 100);
}

// ---------- Who bots ignore ----------

TEST_CASE("Bots don't target other bots or grenades") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  Bot* otherBot = addBot(world, Vector2(10, 0));

  Shot grenadeShot;
  grenadeShot.kind = ShotKind::Grenade;
  grenadeShot.origin = Vector3(0, 0, 10);
  world.push_back(std::make_unique<Projectile>(grenadeShot));

  bot->think(world);
  CHECK(bot->state() == BotState::Patrol);
  CHECK(otherBot->health() == BOT_HEALTH);
}

TEST_CASE("A dead bot does nothing") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  FakePlayer* player = addPlayer(world, Vector2(20, 0));
  bot->applyDamage(BOT_HEALTH);

  bot->think(world);
  CHECK(player->health() == 100);
}

TEST_CASE("runBotAI lets every living bot think") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot1 = addBot(world, Vector2(0, 0));
  Bot* bot2 = addBot(world, Vector2(40, 0));
  FakePlayer* player = addPlayer(world, Vector2(20, 0));

  runBotAI(world);

  CHECK(bot1->state() == BotState::Engage);
  CHECK(bot2->state() == BotState::Engage);
  CHECK(player->health() == 100 - 2 * RIFLE_DAMAGE);
}

TEST_CASE("Bot works as a normal Entity in a unique_ptr<Entity> list") {
  std::vector<std::unique_ptr<Entity>> world;
  world.push_back(std::make_unique<Bot>(Vector2(0, 0),
                                        std::vector<Vector2>{Vector2(4, 0)}));

  world[0]->update(0.5f);  // calls Bot::update through Entity
  CHECK(world[0]->position() == Vector2(2, 0));
}

// ---------- Invalid input ----------

TEST_CASE("Bot update throws on negative, NaN, or infinite dt") {
  Bot bot(Vector2(0, 0), {Vector2(4, 0)});
  float nan = std::numeric_limits<float>::quiet_NaN();
  float inf = std::numeric_limits<float>::infinity();

  CHECK_THROWS_AS(bot.update(-1.0f), std::invalid_argument);
  CHECK_THROWS_AS(bot.update(nan), std::invalid_argument);
  CHECK_THROWS_AS(bot.update(inf), std::invalid_argument);
  CHECK(bot.position() == Vector2(0, 0));
}

TEST_CASE("Negative sight range or speed is fixed to 0") {
  Bot bot(Vector2(0, 0), {Vector2(4, 0)}, 100, -10.0f, -5.0f);
  CHECK(bot.sightRange() == 0.0f);
  CHECK(bot.speed() == 0.0f);

  bot.update(1.0f);
  CHECK(bot.position() == Vector2(0, 0));  // speed 0, doesn't move
}

// ---------- Cover (3D line of sight) ----------
// The bot looks and shoots from its eyes (1.63 m) to the target's eyes.

TEST_CASE("Bot doesn't see or shoot a player behind tall cover") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0), {Vector2(4, 0)});
  FakePlayer* player = addPlayer(world, Vector2(20, 0));
  std::vector<CoverBox> cover = {makeCover(Vector2(18, 0), 0.85f, 2.8f, 2.10f)};

  bot->think(world, cover);
  bot->update(0.5f);

  CHECK(bot->state() == BotState::Patrol);
  CHECK(bot->position() == Vector2(2, 0));  // kept walking
  CHECK(player->health() == 100);
}

TEST_CASE("Bot can see and shoot a standing player's head over low cover") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  FakePlayer* player = addPlayer(world, Vector2(20, 0));
  std::vector<CoverBox> cover = {makeCover(Vector2(18, 0), 0.65f, 3.0f, 1.28f)};

  bot->think(world, cover);

  CHECK(bot->state() == BotState::Engage);
  CHECK(player->health() == 100 - RIFLE_DAMAGE);
}

TEST_CASE("Bot skips a closer hidden player and shoots the visible one") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  FakePlayer* hidden = addPlayer(world, Vector2(10, 0));
  FakePlayer* visible = addPlayer(world, Vector2(0, 30));
  std::vector<CoverBox> cover = {makeCover(Vector2(8, 0), 0.85f, 2.8f, 2.10f)};

  bot->think(world, cover);

  CHECK(bot->state() == BotState::Engage);
  CHECK(hidden->health() == 100);
  CHECK(visible->health() == 100 - RIFLE_DAMAGE);
}

TEST_CASE("Bot engages once the player steps out from behind cover") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot = addBot(world, Vector2(0, 0));
  FakePlayer* player = addPlayer(world, Vector2(20, 0));
  std::vector<CoverBox> cover = {makeCover(Vector2(18, 0), 0.85f, 2.8f, 2.10f)};

  bot->think(world, cover);
  REQUIRE(bot->state() == BotState::Patrol);

  player->setPosition(Vector2(20, 5));  // stepped out to the side
  bot->think(world, cover);
  CHECK(bot->state() == BotState::Engage);
  CHECK(player->health() == 100 - RIFLE_DAMAGE);
}

TEST_CASE("runBotAI passes the cover on to every bot") {
  std::vector<std::unique_ptr<Entity>> world;
  Bot* bot1 = addBot(world, Vector2(0, 0));
  Bot* bot2 = addBot(world, Vector2(40, 0));
  FakePlayer* player = addPlayer(world, Vector2(20, 0));
  // A wall right in front of the player, on bot1's side only.
  std::vector<CoverBox> cover = {makeCover(Vector2(18, 0), 0.85f, 2.8f, 2.10f)};

  runBotAI(world, cover);

  CHECK(bot1->state() == BotState::Patrol);  // wall in the way
  CHECK(bot2->state() == BotState::Engage);  // clear view
  CHECK(player->health() == 100 - RIFLE_DAMAGE);
}
