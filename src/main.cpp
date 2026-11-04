// Build/dependency smoke test for Fireteam: Skirmish.
//
// This file intentionally does NOT contain gameplay logic yet. Its only
// job is to prove that SFML (rendering) and ENet (networking) fetch,
// compile, and link correctly on a fresh clone -- the "build that works on
// another computer" requirement. Each teammate's subsystem (engine,
// rendering, combat, networking) will replace pieces of this as the
// project grows; see docs/proposal.md for the planned architecture.

#include <SFML/Graphics.hpp>
#include <enet/enet.h>

#include <iostream>

int main() {
  if (enet_initialize() != 0) {
    std::cerr << "Failed to initialize ENet.\n";
    return 1;
  }
  std::cout << "[smoke test] ENet initialized successfully.\n";

  sf::RenderWindow window(sf::VideoMode(640, 480),
                           "Fireteam: Skirmish - Build Smoke Test");
  sf::CircleShape placeholderPlayer(50.f);
  placeholderPlayer.setFillColor(sf::Color::Green);
  placeholderPlayer.setPosition(295.f, 190.f);

  window.clear(sf::Color(30, 30, 30));
  window.draw(placeholderPlayer);
  window.display();
  // Closed immediately: this is a headless-friendly build check, not the
  // interactive game loop.
  window.close();

  std::cout << "[smoke test] SFML window created and rendered successfully.\n";

  enet_deinitialize();
  return 0;
}
