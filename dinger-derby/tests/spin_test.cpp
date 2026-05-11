#include <SFML/Graphics.hpp>

#include "../src/physics/Body2D.h"
#include "../src/physics/PhysicsWorld2D.h"

int main() {

    // Create window
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "Spin Test"
    );

    window.setFramerateLimit(60);

    // Setup physics world
    PhysicsWorld2D world;
    world.gravity = Vector2(0, 980.0f);
    world.setBounds(window.getSize().x, window.getSize().y);

    // Create spinning ball
    Body2D ball(Vector2(640, 300), 1.0f);

    ball.radius = 40.0f;
    ball.restitution = 0.8f;

    world.addBody(&ball);

    // Ball visual
    sf::CircleShape ballShape(ball.radius);

    ballShape.setOrigin(
        sf::Vector2f(ball.radius, ball.radius)
    );

    ballShape.setFillColor(sf::Color::White);

    // Rotation indicator
    sf::RectangleShape spinLine(
        sf::Vector2f(ball.radius, 4)
    );

    spinLine.setOrigin(
        sf::Vector2f(0, 2)
    );

    spinLine.setFillColor(sf::Color::Red);

    // Delta time clock
    sf::Clock clock;

    // Main loop
    while (window.isOpen()) {

        float dt = clock.restart().asSeconds();

        // Handle events
        while (const std::optional event = window.pollEvent()) {

            // Close window
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            // Press SPACE to add spin
            if (const auto* key =
                event->getIf<sf::Event::KeyPressed>()) {

                if (key->code == sf::Keyboard::Key::Space) {

                    ball.wakeUp();

                    ball.angularVelocity += 15.0f;
                }
            }
        }

        // Update physics
        world.step(dt);

        // Update visuals
        ballShape.setPosition(
            sf::Vector2f(ball.position.x, ball.position.y)
        );

        spinLine.setPosition(
            sf::Vector2f(ball.position.x, ball.position.y)
        );

        spinLine.setRotation(
            sf::radians(ball.rotation)
        );

        // Render everything
        window.clear();

        window.draw(ballShape);
        window.draw(spinLine);

        window.display();
    }

    return 0;
}