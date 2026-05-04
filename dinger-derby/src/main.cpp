#include <SFML/Graphics.hpp>
#include <vector>
#include <deque>
#include <cstdlib>
#include "physics/Body2D.h"
#include "physics/PhysicsWorld2D.h"

int main() {
    // Create window
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(800, 600)),
        "Dinger Derby 2D Physics"
    );

    // Cap framerate
    window.setFramerateLimit(60);

    // Get window size
    sf::Vector2u windowSize = window.getSize();

    // Setup physics world
    PhysicsWorld2D world;
    world.gravity = Vector2(0, 980.0f);
    world.setBounds(windowSize.x, windowSize.y);

    // Store spawned balls
    std::deque<Body2D> balls;
    std::vector<sf::CircleShape> ballShapes;

    // Spawn settings
    const float ballRadius = 20.0f;

    // Floor visual
    sf::RectangleShape floorShape;
    floorShape.setFillColor(sf::Color(80, 80, 80));
    floorShape.setSize(sf::Vector2f(windowSize.x, 10));
    floorShape.setPosition(sf::Vector2f(0, windowSize.y - 10));

    // Clock for delta time
    sf::Clock clock;

    // Main loop
    while (window.isOpen()) {
        // Calculate frame time
        float dt = clock.restart().asSeconds();

        // Handle events
        while (const std::optional event = window.pollEvent()) {
            // Close window
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            // Handle resize
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                sf::FloatRect visibleArea(
                    sf::Vector2f(0.f, 0.f),
                    sf::Vector2f(resized->size.x, resized->size.y)
                );

                window.setView(sf::View(visibleArea));
                world.setBounds(resized->size.x, resized->size.y);

                floorShape.setSize(sf::Vector2f(resized->size.x, 10));
                floorShape.setPosition(sf::Vector2f(0, resized->size.y - 10));
            }

            // Press E to spawn a random ball
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::E) {
                    sf::Vector2u size = window.getSize();

                    float x = static_cast<float>(ballRadius + rand() % static_cast<int>(size.x - ballRadius * 2));
                    float y = static_cast<float>(ballRadius + rand() % static_cast<int>(size.y - ballRadius * 2));

                    Body2D newBall(Vector2(x, y), 1.0f);
                    newBall.radius = ballRadius;
                    newBall.restitution = 0.75f;
                    newBall.velocity = Vector2(
                        static_cast<float>((rand() % 500) - 250),
                        static_cast<float>((rand() % 500) - 250)
                    );

                    balls.push_back(newBall);
                    world.addBody(&balls.back());

                    sf::CircleShape shape(newBall.radius);
                    shape.setOrigin(sf::Vector2f(newBall.radius, newBall.radius));
                    shape.setFillColor(sf::Color(
                        static_cast<std::uint8_t>(rand() % 255),
                        static_cast<std::uint8_t>(rand() % 255),
                        static_cast<std::uint8_t>(rand() % 255)
                    ));

                    ballShapes.push_back(shape);
                }
            }
        }

        // Update physics
        world.step(dt);

        // Update visuals
        for (int i = 0; i < balls.size(); i++) {
            ballShapes[i].setPosition(
                sf::Vector2f(balls[i].position.x, balls[i].position.y)
            );
        }

        // Render everything
        window.clear();

        window.draw(floorShape);

        for (auto& shape : ballShapes) {
            window.draw(shape);
        }

        window.display();
    }

    return 0;
}