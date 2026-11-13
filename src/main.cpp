// Build/dependency smoke test for Fireteam: Skirmish.
//
// This file intentionally does NOT contain gameplay logic yet. Its only
// job is to prove that raylib (3D rendering) and ENet (networking) fetch,
// compile, and link correctly on a fresh clone -- the "build that works on
// another computer" requirement. Each teammate's subsystem (engine,
// rendering, combat, networking) will replace pieces of this as the
// project grows; see docs/proposal.md for the planned architecture.

#include <enet/enet.h>
#include <raylib.h>

#include <iostream>

int main() {
  if (enet_initialize() != 0) {
    std::cerr << "Failed to initialize ENet.\n";
    return 1;
  }
  std::cout << "[smoke test] ENet initialized successfully.\n";

  InitWindow(800, 600, "Fireteam: Skirmish - 3D Build Smoke Test");
  Camera3D camera{};
  camera.position = {4.0f, 4.0f, 4.0f};
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.up = {0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  BeginDrawing();
  ClearBackground(RAYWHITE);
  BeginMode3D(camera);
  DrawPlane({0.0f, 0.0f, 0.0f}, {20.0f, 20.0f}, LIGHTGRAY);
  DrawCube({0.0f, 1.0f, 0.0f}, 2.0f, 2.0f, 2.0f, BLUE);
  DrawCubeWires({0.0f, 1.0f, 0.0f}, 2.0f, 2.0f, 2.0f, DARKBLUE);
  EndMode3D();
  DrawText("3D renderer smoke test", 20, 20, 20, DARKGRAY);
  EndDrawing();
  CloseWindow();

  std::cout << "[smoke test] raylib 3D scene rendered successfully.\n";

  enet_deinitialize();
  return 0;
}
