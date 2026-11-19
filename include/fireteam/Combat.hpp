// Combat: decides who a shot hits and applies the damage.
//
// These are free functions, not part of GameWorld or Player, so that:
//   - GameWorld only owns and updates entities,
//   - Weapon only handles cooldowns and builds Shots,
//   - combat only needs Entity, so it works with Player, Bot, or anything
//     else that is an Entity.
//
// Everything here is 3D and in metres, y is up (same as the Godot arena
// in godot/scripts/build_arena.gd). Entity only stores a ground position
// (Vector2), so combat adds the height itself: every body is treated as
// an upright cylinder standing on the ground at the entity's position.
//
// How a frame uses it (once Player exists):
//   Shot shot;
//   Vector3 eye = groundToWorld(player.position(), STAND_EYE_HEIGHT);
//   if (weapon->fire(eye, aim, shot)) {
//     resolveShot(shot, player, world.entities(), arenaCover);
//   }
#pragma once

#include <memory>
#include <vector>

#include "fireteam/Entity.hpp"
#include "fireteam/Vector3.hpp"
#include "fireteam/Weapon.hpp"

namespace fireteam {

// ---------- Body sizes (metres) ----------
// Same numbers Koushik used to pick the cover heights in the arena:
// 1.28 m low cover hides a crouching player but not a standing one, and
// 2.10 m tall cover hides a standing player completely.
const float BODY_HEIGHT = 1.80f;  // standing, feet to top of head
const float BODY_RADIUS = 0.25f;  // about half the shoulder width
const float STAND_EYE_HEIGHT = 1.63f;
const float CROUCH_HEIGHT = 1.10f;
const float CROUCH_EYE_HEIGHT = 0.92f;

// ---------- Cover ----------

// A solid box that stops bullets (a barrier, crate, wall, ...).
// It is lined up with the x and z axes (not rotated). A rotated prop in
// the arena can be covered with one or more of these boxes.
struct CoverBox {
  Vector3 minCorner;  // smallest x, y, z
  Vector3 maxCorner;  // biggest x, y, z
};

// Makes a box sitting on the ground. `groundCenter` is the middle of
// the box on the ground, sizeX and sizeZ are its width and depth, and
// `height` is how tall it is.
// Example: makeCover(Vector2(8, 4), 3.0f, 0.65f, 1.28f) is a low barrier.
CoverBox makeCover(Vector2 groundCenter, float sizeX, float sizeZ,
                   float height);

// ---------- Ray tests ----------
// A "ray" here is a shot: it starts at `origin`, goes along the unit
// vector `direction`, and stops after `maxDist` metres.
// Each function returns true on a hit and puts how far along the ray
// the hit is into `hitDist`.

// Does the ray hit this box?
// If the ray starts inside the box, it counts as a hit at distance 0.
bool rayHitsBox(Vector3 origin, Vector3 direction, float maxDist,
                const CoverBox& box, float& hitDist);

// Does the ray hit a body (upright cylinder, BODY_RADIUS wide) standing
// at `feet` on the ground and `bodyHeight` tall?
// Pass BODY_HEIGHT for a standing body or CROUCH_HEIGHT for a crouching
// one. A shot going perfectly straight up or down never hits a body.
bool rayHitsBody(Vector3 origin, Vector3 direction, float maxDist,
                 Vector2 feet, float bodyHeight, float& hitDist);

// True if no cover is between the two points (you can see / shoot from
// one to the other). The bot will use this to check if it can see you.
bool lineOfSightClear(Vector3 from, Vector3 to,
                      const std::vector<CoverBox>& cover);

// ---------- Hitscan ----------

// Finds the entity the shot hits and applies the shot's damage to it.
//
// Rules:
//   - The shot is a straight 3D line from shot.origin along
//     shot.direction, up to shot.range metres long.
//   - A target is hit if that line goes through its body (a standing
//     body, BODY_HEIGHT tall). Aiming the wrong way, beside someone, or
//     over their head is a miss.
//   - The shooter can't hit themselves (they're skipped).
//   - Dead entities are skipped, so shooting a body does nothing.
//   - If the line goes through several targets, the closest one is hit.
//     A rifle bullet stops at the first thing it hits, so only ONE
//     target is hit.
//   - If a cover box is closer than that target, the bullet stops in the
//     cover and nobody is hit.
//   - A dead shooter can't hit anything.
//   - Only hitscan shots are resolved here. A grenade shot returns nullptr;
//     the caller should make a Projectile from it instead.
//
// Returns a pointer to the entity that was hit, or nullptr if nothing was
// hit. The pointer doesn't own anything; the entity is still owned by
// the vector.
//
// `cover` can be left out (an open field with nothing to hide behind).
Entity* resolveShot(const Shot& shot, const Entity& shooter,
                    const std::vector<std::unique_ptr<Entity>>& entities,
                    const std::vector<CoverBox>& cover = {});

// ---------- Explosions (grenades) ----------

// How much blast damage something takes at `dist` from the blast center.
//   - At the center: the full maxDamage.
//   - Falls off in a straight line the farther away you are.
//   - Anywhere inside the radius (including exactly on the edge) takes at
//     least 1, so being "just inside" always counts.
//   - Outside the radius: 0.
int blastDamageAt(float dist, float radius, int maxDamage);

// Damages every living entity the blast reaches, using blastDamageAt().
//
// `center` is a 3D point (a grenade can go off in mid-air). The distance
// is measured to the closest part of each body (feet to head), so a
// blast right above someone's head still hurts them a lot.
//
// Cover: the blast reaches someone if there's no cover between the
// center and either their feet or their head. So a wall between you and
// the grenade protects you, but a low barrier only protects you if it
// blocks both lines.
//
// Unlike a rifle shot, a blast hits EVERYONE it reaches, including the
// person who threw it. Projectiles are skipped, so one grenade doesn't
// destroy another. Returns how many entities were damaged.
int applyBlast(Vector3 center, float radius, int maxDamage,
               const std::vector<std::unique_ptr<Entity>>& entities,
               const std::vector<CoverBox>& cover = {});

// Finds every Projectile in the list whose fuse has run out and makes it
// explode. Call this once per frame after updating the entities.
// Returns how many projectiles exploded.
int explodeReadyProjectiles(
    const std::vector<std::unique_ptr<Entity>>& entities,
    const std::vector<CoverBox>& cover = {});

}  // namespace fireteam
