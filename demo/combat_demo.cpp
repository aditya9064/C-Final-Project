// Combat demo: a small first-person 3D window for trying out the combat code.
//
// This is NOT the real game. It is a throwaway test program so you can SEE
// Rifle, resolveShot, cover and the Bot AI working in 3D, before the real
// Player, input and renderer exist. All the combat uses the real
// fireteam_core code; only the movement, camera and drawing are made up
// for the demo.
//
// The map is Koushik's arena (godot/scripts/build_arena.gd, section 8):
// the same octagon, spawn bays, canopy posts, portal, barriers, loophole
// walls, sandbags, crates and barrels, using the COLLISION boxes from that
// file (not the detailed meshes). Things that are rotated in his arena
// (the curved sandbag arcs) are left out, because CoverBox can't rotate.
// Barrels are cylinders there; here they are boxes the same size.
//
// Drawing uses basic old-style OpenGL (glBegin / glVertex) through the
// SFML window, because SFML itself only draws 2D.
//
// Controls:
//   WASD        move
//   mouse       look around (the mouse is captured; Esc quits)
//   left click  fire (hold it down, the rifle cooldown limits the rate)
//   C           crouch (hold)
//   R           reset (bring everyone back to full health)
//   Esc         quit
//
// What to look for:
//   - Low cover (1.28 m): standing, your eyes (1.63 m) are above it, so you
//     can shoot over it. Crouch (eyes at 0.92 m) and your shots hit it.
//   - Tall cover (2.10 m): blocks shots both ways.
//   - Loophole walls: 2.10 m walls with a slot from 1.35 m to 1.77 m.
//     Standing, you can shoot through the slot.
//   - Bot colour: blue = Patrol, red = Engage (it can see you).
//   - The bot does NOT know you are crouching yet (there is no real Player
//     with a crouch flag), so crouching behind low cover won't hide you
//     from it. That is the "crouching" item waiting on Koushik's Player.

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "fireteam/Bot.hpp"
#include "fireteam/Combat.hpp"
#include "fireteam/GameWorld.hpp"
#include "fireteam/Rifle.hpp"
#include "fireteam/Vector2.hpp"
#include "fireteam/Vector3.hpp"

using namespace fireteam;

// ---------- Demo settings ----------

const int WINDOW_W = 1280;
const int WINDOW_H = 720;
const float PI = 3.14159265f;

// Koushik's arena: a 42 x 34 m octagon with 5 m corner cuts, centred on
// (0, 0). Inside means |x| <= 21, |z| <= 17 and |x| + |z| <= 33.
const float ARENA_HALF_X = 21.0f;
const float ARENA_HALF_Z = 17.0f;
const float ARENA_CORNER_SUM = 33.0f;
const float WALL_HEIGHT = 4.5f;

const float PLAYER_SPEED = 5.0f;        // metres per second
const float CROUCH_SPEED = 2.0f;        // slower while crouching
const float MOUSE_SENSITIVITY = 0.15f;  // degrees per pixel
// More health than normal, so you have time to watch the bot shoot you.
const int PLAYER_HEALTH = 400;
const float DEMO_BOT_SIGHT = 20.0f;  // metres (normal bots see 40 m)
const float SHOT_LINE_TIME = 0.08f;  // how long a shot line stays visible
const float HURT_FLASH_TIME = 0.15f;
const float HIT_MARKER_TIME = 0.25f;

// ---------- A very simple player, only for this demo ----------

// The real Player (Koushik's #6) will have input, crouching and so on.
// This one just holds a Rifle so the demo has something to shoot with.
class DemoPlayer : public Entity {
 public:
  explicit DemoPlayer(Vector2 position) : Entity(position, PLAYER_HEALTH) {}

  void update(float deltaSeconds) override { rifle.update(deltaSeconds); }

  Rifle rifle;
};

// A 3D line to draw for a short time after a shot.
struct ShotLine {
  Vector3 from;
  Vector3 to;
  sf::Color color;
  float timeLeft;
};

// One solid piece of the map: the CoverBox the combat code uses, plus a
// colour so we can draw it.
struct ArenaPiece {
  CoverBox box;
  float r, g, b;
};

// ---------- Colours (from build_arena.gd's palette) ----------

struct Colour {
  float r, g, b;
};
const Colour CONCRETE = {0.72f, 0.67f, 0.60f};
const Colour CONCRETE_DARK = {0.58f, 0.54f, 0.48f};
const Colour STEEL = {0.20f, 0.22f, 0.24f};
const Colour SAND = {0.68f, 0.60f, 0.45f};
const Colour OLIVE = {0.38f, 0.42f, 0.26f};
const Colour CRATE_DARK = {0.25f, 0.24f, 0.22f};
const Colour BARREL = {0.45f, 0.20f, 0.15f};
const Colour TEAL = {0.06f, 0.67f, 0.65f};
const Colour ORANGE = {1.00f, 0.55f, 0.16f};

// ---------- Building the map ----------

// Adds a box with its centre at (x, z) on the ground, sizeX by sizeZ,
// going from height `bottom` up to height `top`.
void addBox(std::vector<ArenaPiece>& pieces, float x, float z, float sizeX,
            float sizeZ, float bottom, float top, Colour c) {
  ArenaPiece piece;
  piece.box.minCorner = Vector3(x - sizeX / 2, bottom, z - sizeZ / 2);
  piece.box.maxCorner = Vector3(x + sizeX / 2, top, z + sizeZ / 2);
  piece.r = c.r;
  piece.g = c.g;
  piece.b = c.b;
  pieces.push_back(piece);
}

// One team's spawn bay (build_arena.gd _spawn). `facing` is +1 for teal
// (at x = -18, opening toward +x) and -1 for orange.
void addSpawnBay(std::vector<ArenaPiece>& pieces, float centerX, int facing,
                 Colour team) {
  const float width = 5.5f;   // opening width (z)
  const float height = 3.4f;  // opening height
  const float depth = 2.1f;
  const float t = 0.4f;  // wall thickness
  float backX = centerX - facing * depth;
  float midX = centerX - facing * depth * 0.5f;
  float frontX = centerX + facing * 0.1f;
  float sideZ = width * 0.5f + t * 0.5f;

  // side walls, back wall and ceiling
  addBox(pieces, midX, -sideZ, depth, t, 0, height, CONCRETE_DARK);
  addBox(pieces, midX, sideZ, depth, t, 0, height, CONCRETE_DARK);
  addBox(pieces, backX, 0, t, width + t * 2, 0, height, CONCRETE_DARK);
  addBox(pieces, midX, 0, depth + t, width + t * 2, height, height + t,
         CONCRETE);
  // team-coloured entrance posts
  addBox(pieces, frontX, -sideZ, t * 1.4f, t * 1.4f, 0, height + t, team);
  addBox(pieces, frontX, sideZ, t * 1.4f, t * 1.4f, 0, height + t, team);
  // two equipment cases inside
  float caseX = backX + facing * 0.9f;
  addBox(pieces, caseX, -1.6f, 1.1f, 0.6f, 0, 0.6f, OLIVE);
  addBox(pieces, caseX, 1.6f, 1.1f, 0.6f, 0, 0.6f, OLIVE);
}

// Everything solid in Koushik's arena that a bullet can hit.
std::vector<ArenaPiece> buildArena() {
  std::vector<ArenaPiece> pieces;

  // Spawn bays (teal on -x, orange on +x).
  addSpawnBay(pieces, -18.0f, 1, TEAL);
  addSpawnBay(pieces, 18.0f, -1, ORANGE);

  // Canopy: four steel posts (the fabric roof doesn't stop bullets).
  const float post = 4.65f;
  addBox(pieces, -post, -post, 0.46f, 0.46f, 0, 3.75f, STEEL);
  addBox(pieces, -post, post, 0.46f, 0.46f, 0, 3.75f, STEEL);
  addBox(pieces, post, -post, 0.46f, 0.46f, 0, 3.75f, STEEL);
  addBox(pieces, post, post, 0.46f, 0.46f, 0, 3.75f, STEEL);

  // Central portal: two pillars with a 2.4 m doorway, a lintel on top,
  // and small buttresses on the outside.
  addBox(pieces, -1.6f, 0, 0.8f, 1.1f, 0, 2.9f, CONCRETE);
  addBox(pieces, 1.6f, 0, 0.8f, 1.1f, 0, 2.9f, CONCRETE);
  addBox(pieces, 0, 0, 4.2f, 1.1f, 2.9f, 3.45f, CONCRETE);
  addBox(pieces, -2.15f, 0, 0.5f, 0.5f, 0, 1.05f, CONCRETE_DARK);
  addBox(pieces, 2.15f, 0, 0.5f, 0.5f, 0, 1.05f, CONCRETE_DARK);

  // Tall barriers, 2.8 x 0.85 x 2.10 m at (+-12, +-9). They're turned so
  // the long side runs along z.
  addBox(pieces, -12, -9, 0.85f, 2.8f, 0, 2.10f, TEAL);
  addBox(pieces, -12, 9, 0.85f, 2.8f, 0, 2.10f, TEAL);
  addBox(pieces, 12, -9, 0.85f, 2.8f, 0, 2.10f, ORANGE);
  addBox(pieces, 12, 9, 0.85f, 2.8f, 0, 2.10f, ORANGE);

  // Low barriers, 3.0 x 0.65 x 1.28 m. The four at (+-8, +-4) run along
  // z; the two at (0, +-10) run along x.
  addBox(pieces, -8, -4, 0.65f, 3.0f, 0, 1.28f, CONCRETE);
  addBox(pieces, -8, 4, 0.65f, 3.0f, 0, 1.28f, CONCRETE);
  addBox(pieces, 8, -4, 0.65f, 3.0f, 0, 1.28f, CONCRETE);
  addBox(pieces, 8, 4, 0.65f, 3.0f, 0, 1.28f, CONCRETE);
  addBox(pieces, 0, -10, 3.0f, 0.65f, 0, 1.28f, CONCRETE);
  addBox(pieces, 0, 10, 3.0f, 0.65f, 0, 1.28f, CONCRETE);

  // L-shaped barriers, 1.45 m high, two boxes each, at (+-15, +-8).
  const float lx[4] = {-15, -15, 15, 15};
  const float lz[4] = {-8, 8, -8, 8};
  for (int i = 0; i < 4; i++) {
    addBox(pieces, lx[i] + 0.95f, lz[i], 2.6f, 0.7f, 0, 1.45f, CONCRETE);
    addBox(pieces, lx[i] - 0.6f, lz[i] + 0.95f, 0.7f, 2.0f, 0, 1.45f,
           CONCRETE);
  }

  // Loophole walls at (-5, -9.5) and (5, 9.5): 3.0 x 0.7 x 2.10 m with a
  // real slot from 1.35 m to 1.77 m high and 1.3 m wide in the middle.
  const float hx[2] = {-5, 5};
  const float hz[2] = {-9.5f, 9.5f};
  for (int i = 0; i < 2; i++) {
    addBox(pieces, hx[i] - 1.075f, hz[i], 0.85f, 0.7f, 0, 2.10f, CONCRETE);
    addBox(pieces, hx[i] + 1.075f, hz[i], 0.85f, 0.7f, 0, 2.10f, CONCRETE);
    addBox(pieces, hx[i], hz[i], 1.32f, 0.7f, 0, 1.35f, CONCRETE);      // sill
    addBox(pieces, hx[i], hz[i], 1.32f, 0.7f, 1.77f, 2.10f, CONCRETE);  // top
  }

  // Sandbag piles, 2.3 x 0.9 x 1.26 m, at (+-6.7, +-5.5).
  addBox(pieces, -6.7f, -5.5f, 2.3f, 0.9f, 0, 1.26f, SAND);
  addBox(pieces, -6.7f, 5.5f, 2.3f, 0.9f, 0, 1.26f, SAND);
  addBox(pieces, 6.7f, -5.5f, 2.3f, 0.9f, 0, 1.26f, SAND);
  addBox(pieces, 6.7f, 5.5f, 2.3f, 0.9f, 0, 1.26f, SAND);

  // Crate clusters: two 0.95 m crates side by side and a smaller one
  // stacked on top.
  const float cx[6] = {-16.5f, 16.5f, -9, 9, -13.5f, 13.5f};
  const float cz[6] = {2.5f, -2.5f, 13, -13, -5.5f, 5.5f};
  for (int i = 0; i < 6; i++) {
    Colour crate = (i >= 4) ? CRATE_DARK : OLIVE;
    addBox(pieces, cx[i] - 0.55f, cz[i], 0.95f, 0.95f, 0, 0.95f, crate);
    addBox(pieces, cx[i] + 0.55f, cz[i] + 0.3f, 0.95f, 0.95f, 0, 0.95f, crate);
    addBox(pieces, cx[i], cz[i] + 0.1f, 0.59f, 0.59f, 0.95f, 1.54f, crate);
  }

  // Barrel clusters: three 0.9 m barrels each (boxes here, not cylinders).
  const float bx[4] = {-17, 17, -3, 3};
  const float bz[4] = {-8, 8, 14, -14};
  for (int i = 0; i < 4; i++) {
    addBox(pieces, bx[i], bz[i], 0.6f, 0.6f, 0, 0.9f, BARREL);
    addBox(pieces, bx[i] + 0.62f, bz[i] + 0.15f, 0.6f, 0.6f, 0, 0.9f, BARREL);
    addBox(pieces, bx[i] + 0.3f, bz[i] - 0.55f, 0.6f, 0.6f, 0, 0.9f, BARREL);
  }

  // Tarp-covered pallet (slightly rotated in Godot; straight here).
  addBox(pieces, -6.2f, 12.4f, 1.36f, 1.16f, 0, 1.42f, OLIVE);

  return pieces;
}

// Builds a fresh world with one player and one bot. The raw pointers let
// the demo reach them; the world still owns them.
void resetWorld(GameWorld& world, DemoPlayer*& player, Bot*& bot) {
  world = GameWorld();

  // Player starts in the teal spawn bay; the bot patrols the orange side.
  auto newPlayer = std::make_unique<DemoPlayer>(Vector2(-18, 0));
  player = newPlayer.get();
  world.addEntity(std::move(newPlayer));

  std::vector<Vector2> patrol = {Vector2(10, -6.5f), Vector2(10, 4),
                                 Vector2(14, 4), Vector2(14, -4.5f)};
  // Sight range 20 m instead of the default 40 m: in this map the bot can
  // see straight into your spawn bay from its patrol route, and at 40 m it
  // shoots you before you can move.
  auto newBot = std::make_unique<Bot>(Vector2(10, -6.5f), patrol, BOT_HEALTH,
                                      DEMO_BOT_SIGHT);
  bot = newBot.get();
  world.addEntity(std::move(newBot));

  std::cout << "--- reset: player " << player->health() << " HP, bot "
            << bot->health() << " HP ---\n";
}

// ---------- Helpers: game ----------

float toRadians(float degrees) { return degrees * PI / 180.0f; }

// The direction the camera looks, from yaw (left/right) and pitch (up/down)
// in degrees. yaw 0 looks toward -z, yaw 90 looks toward +x.
Vector3 lookDirection(float yawDeg, float pitchDeg) {
  float yaw = toRadians(yawDeg);
  float pitch = toRadians(pitchDeg);
  return Vector3(std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                 -std::cos(pitch) * std::cos(yaw));
}

// True if a body standing at `ground` is inside the octagon walls.
bool insideArena(Vector2 ground) {
  float x = std::fabs(ground.x);
  float z = std::fabs(ground.y);
  float r = BODY_RADIUS;
  return x < ARENA_HALF_X - r && z < ARENA_HALF_Z - r &&
         x + z < ARENA_CORNER_SUM - r * 1.5f;
}

// True if a body standing at `ground` would walk into a piece of cover.
// Things high up (the portal lintel, bay ceiling, stacked crates) are
// skipped, since you walk under or next to them.
bool bumpsIntoCover(Vector2 ground, const std::vector<CoverBox>& cover) {
  for (const CoverBox& box : cover) {
    if (box.minCorner.y > 0.5f) {
      continue;
    }
    if (ground.x > box.minCorner.x - BODY_RADIUS &&
        ground.x < box.maxCorner.x + BODY_RADIUS &&
        ground.y > box.minCorner.z - BODY_RADIUS &&
        ground.y < box.maxCorner.z + BODY_RADIUS) {
      return true;
    }
  }
  return false;
}

// How far along the shot the first cover box is (or the full range).
float distanceToCover(const Shot& shot, const std::vector<CoverBox>& cover) {
  float closest = shot.range;
  for (const CoverBox& box : cover) {
    float d = 0.0f;
    if (rayHitsBox(shot.origin, shot.direction, shot.range, box, d) &&
        d < closest) {
      closest = d;
    }
  }
  return closest;
}

// ---------- Helpers: drawing (old-style OpenGL) ----------

// Sets up a perspective camera. Like gluPerspective, but glFrustum is
// built into OpenGL so we don't need the GLU library.
void setPerspective(float fovDeg, float aspect, float nearDist,
                    float farDist) {
  float top = nearDist * std::tan(toRadians(fovDeg) / 2.0f);
  float right = top * aspect;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-right, right, -top, top, nearDist, farDist);
}

// Moves the world so the camera is at `eye` looking along yaw / pitch.
void setCamera(Vector3 eye, float yawDeg, float pitchDeg) {
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glRotatef(-pitchDeg, 1.0f, 0.0f, 0.0f);
  glRotatef(yawDeg, 0.0f, 1.0f, 0.0f);
  glTranslatef(-eye.x, -eye.y, -eye.z);
}

// A solid box. No lighting, so each side gets a slightly darker shade to
// make the shape easy to see.
void drawBox(Vector3 mn, Vector3 mx, float r, float g, float b) {
  glBegin(GL_QUADS);
  // top and bottom
  glColor3f(r, g, b);
  glVertex3f(mn.x, mx.y, mn.z);
  glVertex3f(mx.x, mx.y, mn.z);
  glVertex3f(mx.x, mx.y, mx.z);
  glVertex3f(mn.x, mx.y, mx.z);
  glColor3f(r * 0.5f, g * 0.5f, b * 0.5f);
  glVertex3f(mn.x, mn.y, mn.z);
  glVertex3f(mx.x, mn.y, mn.z);
  glVertex3f(mx.x, mn.y, mx.z);
  glVertex3f(mn.x, mn.y, mx.z);
  // front and back (z sides)
  glColor3f(r * 0.65f, g * 0.65f, b * 0.65f);
  glVertex3f(mn.x, mn.y, mn.z);
  glVertex3f(mx.x, mn.y, mn.z);
  glVertex3f(mx.x, mx.y, mn.z);
  glVertex3f(mn.x, mx.y, mn.z);
  glVertex3f(mn.x, mn.y, mx.z);
  glVertex3f(mx.x, mn.y, mx.z);
  glVertex3f(mx.x, mx.y, mx.z);
  glVertex3f(mn.x, mx.y, mx.z);
  // left and right (x sides)
  glColor3f(r * 0.8f, g * 0.8f, b * 0.8f);
  glVertex3f(mn.x, mn.y, mn.z);
  glVertex3f(mn.x, mn.y, mx.z);
  glVertex3f(mn.x, mx.y, mx.z);
  glVertex3f(mn.x, mx.y, mn.z);
  glVertex3f(mx.x, mn.y, mn.z);
  glVertex3f(mx.x, mn.y, mx.z);
  glVertex3f(mx.x, mx.y, mx.z);
  glVertex3f(mx.x, mx.y, mn.z);
  glEnd();
}

// An upright cylinder standing on the ground at `feet` -- the same shape
// the combat code uses for a body, so what you see is what you can hit.
void drawCylinder(Vector2 feet, float radius, float height, float r, float g,
                  float b) {
  const int SEGMENTS = 16;
  glBegin(GL_QUAD_STRIP);
  for (int i = 0; i <= SEGMENTS; i++) {
    float angle = i * 2.0f * PI / SEGMENTS;
    float x = feet.x + std::cos(angle) * radius;
    float z = feet.y + std::sin(angle) * radius;
    float shade = 0.7f + 0.3f * std::cos(angle);
    glColor3f(r * shade, g * shade, b * shade);
    glVertex3f(x, 0.0f, z);
    glVertex3f(x, height, z);
  }
  glEnd();

  glBegin(GL_TRIANGLE_FAN);
  glColor3f(r, g, b);
  glVertex3f(feet.x, height, feet.y);
  for (int i = 0; i <= SEGMENTS; i++) {
    float angle = i * 2.0f * PI / SEGMENTS;
    glVertex3f(feet.x + std::cos(angle) * radius, height,
               feet.y + std::sin(angle) * radius);
  }
  glEnd();
}

// A checkerboard floor of 2 m tiles inside the octagon, so you can judge
// distance.
void drawFloor() {
  const float TILE = 2.0f;
  glBegin(GL_QUADS);
  int row = 0;
  for (float z = -ARENA_HALF_Z - 1; z < ARENA_HALF_Z + 1; z += TILE) {
    int col = 0;
    for (float x = -ARENA_HALF_X - 1; x < ARENA_HALF_X + 1; x += TILE) {
      if ((row + col) % 2 == 0) {
        glColor3f(0.42f, 0.40f, 0.36f);
      } else {
        glColor3f(0.37f, 0.35f, 0.32f);
      }
      glVertex3f(x, 0.0f, z);
      glVertex3f(x + TILE, 0.0f, z);
      glVertex3f(x + TILE, 0.0f, z + TILE);
      glVertex3f(x, 0.0f, z + TILE);
      col++;
    }
    row++;
  }
  glEnd();
}

// The eight octagon walls (just for looks; you can't walk past them).
void drawArenaWalls() {
  const float vx[8] = {-16, 16, 21, 21, 16, -16, -21, -21};
  const float vz[8] = {-17, -17, -12, 12, 17, 17, 12, -12};
  glBegin(GL_QUADS);
  for (int i = 0; i < 8; i++) {
    int next = (i + 1) % 8;
    float shade = (i % 2 == 0) ? 0.85f : 0.7f;
    glColor3f(CONCRETE.r * shade, CONCRETE.g * shade, CONCRETE.b * shade);
    glVertex3f(vx[i], 0.0f, vz[i]);
    glVertex3f(vx[next], 0.0f, vz[next]);
    glVertex3f(vx[next], WALL_HEIGHT, vz[next]);
    glVertex3f(vx[i], WALL_HEIGHT, vz[i]);
  }
  glEnd();
}

// A health bar floating above the bot's head, turned to face the camera.
void drawHealthBar(Vector2 feet, int health, int maxHealth, float yawDeg) {
  // Sideways direction on screen (camera's right, on the ground).
  float yaw = toRadians(yawDeg);
  Vector3 right(std::cos(yaw), 0.0f, std::sin(yaw));

  float width = 0.8f;
  float fraction = health / static_cast<float>(maxHealth);
  Vector3 left = groundToWorld(feet, 0.0f) - right * (width / 2.0f);
  Vector3 split = left + right * (width * fraction);
  Vector3 end = left + right * width;
  float y0 = BODY_HEIGHT + 0.25f;
  float y1 = BODY_HEIGHT + 0.35f;

  glBegin(GL_QUADS);
  glColor3f(0.0f, 0.85f, 0.0f);  // health left
  glVertex3f(left.x, y0, left.z);
  glVertex3f(split.x, y0, split.z);
  glVertex3f(split.x, y1, split.z);
  glVertex3f(left.x, y1, left.z);
  glColor3f(0.4f, 0.0f, 0.0f);  // health lost
  glVertex3f(split.x, y0, split.z);
  glVertex3f(end.x, y0, end.z);
  glVertex3f(end.x, y1, end.z);
  glVertex3f(split.x, y1, split.z);
  glEnd();
}

// Draws text with a black outline so it's readable on any background.
void drawText(sf::RenderWindow& window, const sf::Font& font,
              const std::string& message, unsigned int size, float x, float y,
              sf::Color color) {
  sf::Text text(message, font, size);
  text.setPosition(x, y);
  text.setFillColor(color);
  text.setOutlineColor(sf::Color::Black);
  text.setOutlineThickness(2.0f);
  window.draw(text);
}

// ---------- main ----------

int main() {
  sf::ContextSettings settings;
  settings.depthBits = 24;  // needed so near things hide far things
  settings.antialiasingLevel = 4;
  sf::RenderWindow window(sf::VideoMode(WINDOW_W, WINDOW_H),
                          "Fireteam combat demo (3D)",
                          sf::Style::Titlebar | sf::Style::Close, settings);
  window.setFramerateLimit(60);
  window.setMouseCursorVisible(false);
  window.setMouseCursorGrabbed(true);
  sf::Vector2i screenCenter(WINDOW_W / 2, WINDOW_H / 2);
  sf::Mouse::setPosition(screenCenter, window);

  // The font is only for the HUD. If it can't be found, the demo still
  // works; you just don't get the text.
  sf::Font font;
  bool haveFont = font.loadFromFile("C:/Windows/Fonts/arial.ttf");

  // The map: pieces for drawing, and just the boxes for the combat code.
  std::vector<ArenaPiece> arena = buildArena();
  std::vector<CoverBox> cover;
  for (const ArenaPiece& piece : arena) {
    cover.push_back(piece.box);
  }

  GameWorld world;
  DemoPlayer* player = nullptr;
  Bot* bot = nullptr;
  resetWorld(world, player, bot);

  float yawDeg = 90.0f;  // start looking toward +x (out of the teal bay)
  float pitchDeg = 0.0f;
  bool crouching = false;

  std::vector<ShotLine> shotLines;
  int lastPlayerHealth = player->health();
  float hurtFlash = 0.0f;
  float hitMarker = 0.0f;     // > 0 right after you hit the bot
  std::string lastShotResult;  // "HIT", "BLOCKED", "MISS" for the HUD
  sf::Clock clock;

  while (window.isOpen()) {
    // ----- Events -----
    sf::Event event;
    while (window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) {
        window.close();
      }
      if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
          window.close();
        }
        if (event.key.code == sf::Keyboard::R) {
          resetWorld(world, player, bot);
          lastPlayerHealth = player->health();
          shotLines.clear();
          lastShotResult = "";
        }
      }
    }
    if (!window.isOpen()) {
      break;
    }

    // Cap dt so dragging the window doesn't make a huge jump.
    float dt = clock.restart().asSeconds();
    if (dt > 0.1f) {
      dt = 0.1f;
    }

    bool focused = window.hasFocus();

    // ----- Mouse look -----
    if (focused) {
      sf::Vector2i mouse = sf::Mouse::getPosition(window);
      yawDeg += (mouse.x - screenCenter.x) * MOUSE_SENSITIVITY;
      pitchDeg -= (mouse.y - screenCenter.y) * MOUSE_SENSITIVITY;
      if (pitchDeg > 85.0f) pitchDeg = 85.0f;
      if (pitchDeg < -85.0f) pitchDeg = -85.0f;
      sf::Mouse::setPosition(screenCenter, window);
    }

    // ----- Player movement (demo only) -----
    crouching = focused && sf::Keyboard::isKeyPressed(sf::Keyboard::C);
    if (player->isAlive() && focused) {
      // Forward and right along the ground, from where you're looking.
      float yaw = toRadians(yawDeg);
      Vector2 forward(std::sin(yaw), -std::cos(yaw));
      Vector2 right(std::cos(yaw), std::sin(yaw));

      Vector2 move;
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) move += forward;
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) move -= forward;
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) move += right;
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) move -= right;

      float speed = crouching ? CROUCH_SPEED : PLAYER_SPEED;
      Vector2 step = move.normalized() * (speed * dt);
      // Try x and z separately, so you slide along walls instead of
      // getting stuck on them.
      Vector2 tryX = player->position() + Vector2(step.x, 0.0f);
      if (insideArena(tryX) && !bumpsIntoCover(tryX, cover)) {
        player->setPosition(tryX);
      }
      Vector2 tryZ = player->position() + Vector2(0.0f, step.y);
      if (insideArena(tryZ) && !bumpsIntoCover(tryZ, cover)) {
        player->setPosition(tryZ);
      }
    }

    float eyeHeight = crouching ? CROUCH_EYE_HEIGHT : STAND_EYE_HEIGHT;
    Vector3 eye = groundToWorld(player->position(), eyeHeight);
    Vector3 look = lookDirection(yawDeg, pitchDeg);

    // ----- Player shooting (the real combat code) -----
    if (player->isAlive() && focused &&
        sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
      Shot shot;
      if (player->rifle.fire(eye, look, shot)) {
        Entity* hit = resolveShot(shot, *player, world.entities(), cover);

        // Where the bullet stopped, for drawing the line.
        float endDist = 0.0f;
        sf::Color color;
        if (hit != nullptr) {
          rayHitsBody(shot.origin, shot.direction, shot.range,
                      hit->position(), BODY_HEIGHT, endDist);
          color = sf::Color(255, 230, 0);
          hitMarker = HIT_MARKER_TIME;
          lastShotResult = "HIT  -" + std::to_string(shot.damage);
          std::cout << "HIT bot for " << shot.damage << " -> bot has "
                    << hit->health() << " HP\n";
          if (!hit->isAlive()) {
            std::cout << "bot killed\n";
          }
        } else {
          endDist = distanceToCover(shot, cover);
          color = sf::Color(200, 200, 200);
          if (endDist < shot.range) {
            lastShotResult = "BLOCKED by cover";
            std::cout << "blocked by cover\n";
          } else {
            lastShotResult = "MISS";
            std::cout << "miss\n";
          }
        }

        // Start the line a bit to the right of and below your eyes (where
        // a gun would be), otherwise it's just a dot in the middle.
        float yaw = toRadians(yawDeg);
        Vector3 gun = eye +
                      Vector3(std::cos(yaw), 0.0f, std::sin(yaw)) * 0.2f +
                      Vector3(0.0f, -0.15f, 0.0f);
        Vector3 end = shot.origin + shot.direction * endDist;
        shotLines.push_back({gun, end, color, SHOT_LINE_TIME});
      }
    }

    // ----- Update the world (the real game code) -----
    world.update(dt);                   // moves bot, counts down cooldowns
    runBotAI(world.entities(), cover);  // bot looks around and shoots

    // The bot's shots happen inside runBotAI, so we notice them by the
    // player's health going down. The bot shoots eyes to eyes.
    if (player->health() < lastPlayerHealth) {
      std::cout << "bot hit you -> you have " << player->health() << " HP\n";
      Vector3 botEye = groundToWorld(bot->position(), STAND_EYE_HEIGHT);
      Vector3 playerChest = groundToWorld(player->position(), 1.2f);
      shotLines.push_back(
          {botEye, playerChest, sf::Color::Red, SHOT_LINE_TIME});
      hurtFlash = HURT_FLASH_TIME;
      if (!player->isAlive()) {
        std::cout << "you died, press R to reset\n";
      }
    }
    lastPlayerHealth = player->health();

    for (ShotLine& line : shotLines) {
      line.timeLeft -= dt;
    }
    while (!shotLines.empty() && shotLines.front().timeLeft <= 0.0f) {
      shotLines.erase(shotLines.begin());
    }
    if (hurtFlash > 0.0f) hurtFlash -= dt;
    if (hitMarker > 0.0f) hitMarker -= dt;

    // ----- Draw the 3D world -----
    glViewport(0, 0, WINDOW_W, WINDOW_H);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glClearColor(0.55f, 0.75f, 0.95f, 1.0f);  // sky
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    setPerspective(70.0f, WINDOW_W / static_cast<float>(WINDOW_H), 0.05f,
                   200.0f);
    setCamera(eye, yawDeg, pitchDeg);

    drawFloor();
    drawArenaWalls();
    for (const ArenaPiece& piece : arena) {
      drawBox(piece.box.minCorner, piece.box.maxCorner, piece.r, piece.g,
              piece.b);
    }

    if (!bot->isAlive()) {
      drawCylinder(bot->position(), BODY_RADIUS, BODY_HEIGHT, 0.2f, 0.2f, 0.2f);
    } else if (bot->state() == BotState::Engage) {
      drawCylinder(bot->position(), BODY_RADIUS, BODY_HEIGHT, 0.9f, 0.15f,
                   0.15f);
    } else {
      drawCylinder(bot->position(), BODY_RADIUS, BODY_HEIGHT, 0.25f, 0.45f,
                   1.0f);
    }
    drawHealthBar(bot->position(), bot->health(), bot->maxHealth(), yawDeg);

    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (const ShotLine& line : shotLines) {
      glColor3ub(line.color.r, line.color.g, line.color.b);
      glVertex3f(line.from.x, line.from.y, line.from.z);
      glVertex3f(line.to.x, line.to.y, line.to.z);
    }
    glEnd();

    // ----- Draw the 2D HUD on top (SFML) -----
    window.pushGLStates();

    // Crosshair: white normally, red with a bigger X right after a hit.
    sf::Color crossColor = hitMarker > 0.0f ? sf::Color::Red : sf::Color::White;
    sf::RectangleShape bar1(sf::Vector2f(14.0f, 2.0f));
    bar1.setPosition(WINDOW_W / 2.0f - 7.0f, WINDOW_H / 2.0f - 1.0f);
    bar1.setFillColor(crossColor);
    sf::RectangleShape bar2(sf::Vector2f(2.0f, 14.0f));
    bar2.setPosition(WINDOW_W / 2.0f - 1.0f, WINDOW_H / 2.0f - 7.0f);
    bar2.setFillColor(crossColor);
    window.draw(bar1);
    window.draw(bar2);
    if (hitMarker > 0.0f) {
      for (int i = 0; i < 2; i++) {
        sf::RectangleShape slash(sf::Vector2f(30.0f, 3.0f));
        slash.setOrigin(15.0f, 1.5f);
        slash.setPosition(WINDOW_W / 2.0f, WINDOW_H / 2.0f);
        slash.setRotation(i == 0 ? 45.0f : -45.0f);
        slash.setFillColor(sf::Color::Red);
        window.draw(slash);
      }
    }

    if (hurtFlash > 0.0f) {
      sf::RectangleShape flash(sf::Vector2f(WINDOW_W, WINDOW_H));
      flash.setFillColor(sf::Color(255, 0, 0, 70));
      window.draw(flash);
    }

    if (haveFont) {
      std::string state = !bot->isAlive() ? "DEAD"
                          : bot->state() == BotState::Engage ? "ENGAGE"
                                                             : "PATROL";
      std::string hud = "You: " + std::to_string(player->health()) + " HP" +
                        (crouching ? "  (CROUCHING)" : "") +
                        "      Bot: " + std::to_string(bot->health()) +
                        " HP (" + state + ")";
      drawText(window, font, hud, 20, 12.0f, 8.0f, sf::Color::White);

      if (!lastShotResult.empty()) {
        drawText(window, font, "Last shot: " + lastShotResult, 20, 12.0f,
                 36.0f,
                 hitMarker > 0.0f ? sf::Color(255, 230, 0) : sf::Color::White);
      }

      drawText(window, font,
               "WASD move   mouse look   click fire   C crouch   R reset   "
               "Esc quit",
               16, 12.0f, WINDOW_H - 30.0f, sf::Color::White);

      if (!bot->isAlive()) {
        drawText(window, font, "Bot down - press R to reset", 32,
                 WINDOW_W / 2.0f - 200.0f, WINDOW_H / 2.0f - 90.0f,
                 sf::Color(255, 230, 0));
      }
      if (!player->isAlive()) {
        drawText(window, font, "You died - press R", 40,
                 WINDOW_W / 2.0f - 170.0f, WINDOW_H / 2.0f - 80.0f,
                 sf::Color::Red);
      }
    }

    window.popGLStates();
    window.display();
  }

  return 0;
}
