#include <SFML/Graphics.hpp>

int main() {
    // The game window. SFML 2.6 API.
    const unsigned int width  = 960;
    const unsigned int height = 540;
    sf::RenderWindow window(sf::VideoMode(width, height), "Fireteam: Skirmish");
    window.setVerticalSyncEnabled(true);  // cap to refresh rate, don't peg the CPU

    // Placeholder square standing in for a player until Player (#6) exists.
    sf::RectangleShape placeholder(sf::Vector2f(40.f, 40.f));
    placeholder.setFillColor(sf::Color(80, 200, 120));
    placeholder.setPosition(width / 2.f - 20.f, height / 2.f - 20.f);

    // Measures real time between frames. deltaSeconds is what makes movement
    // frame-rate independent (position += velocity * deltaSeconds), which
    // Player (#6) will depend on.
    sf::Clock clock;

    // Main loop: input -> update -> render, every frame, until the window closes.
    while (window.isOpen()) {
        const float deltaSeconds = clock.restart().asSeconds();

        // 1) Input: drain the event queue. Closing the window ends the loop.
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            if (event.type == sf::Event::KeyPressed &&
                event.key.code == sf::Keyboard::Escape)
                window.close();
        }

        // 2) Update: game state advances here using deltaSeconds.
        //    Entity update() calls go here once #14 lands. Nothing moves yet.
        (void)deltaSeconds;

        // 3) Render: clear, draw everything, show the frame.
        window.clear(sf::Color(30, 30, 30));
        window.draw(placeholder);
        window.display();
    }

    return 0;  // RenderWindow's destructor cleans up the window (RAII).
}