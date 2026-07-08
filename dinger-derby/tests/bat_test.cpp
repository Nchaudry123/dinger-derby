#include <SFML/Graphics.hpp>
#include "DemoDragLauncher.h"
#include "DemoFpsCounter.h"
#include "../src/physics/Body2D.h"
#include "../src/physics/PhysicsWorld2D.h"
#include "../src/collision/Collision2D.h"

int main() {
    // Create window
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "Bat Test"
    );

    window.setFramerateLimit(60);
    DemoFpsCounter fpsCounter("Bat Test");

    // General settings
    const float floorOffset = 100.0f;
    const float clickHitbox = 60.0f;

    // Setup physics world
    PhysicsWorld2D world;
    world.gravity = Vector2(0, 980.0f);
    world.setBounds(window.getSize().x, window.getSize().y - floorOffset);

    // Create ball
    Body2D ball(Vector2(300, 300), 1.0f);
    ball.setRadius(20.0f);
    ball.restitution = 0.8f;
    world.addBody(&ball);

    // Ball visual
    sf::CircleShape ballShape(ball.radius);
    ballShape.setOrigin(sf::Vector2f(ball.radius, ball.radius));
    ballShape.setFillColor(sf::Color::White);

    // Bat visual
    sf::RectangleShape bat(sf::Vector2f(220, 30));
    bat.setOrigin(sf::Vector2f(110, 15));
    bat.setPosition(sf::Vector2f(850, 460));
    bat.setFillColor(sf::Color(160, 100, 40));

    // Floor visual
    sf::RectangleShape floor(sf::Vector2f(window.getSize().x, 10));
    floor.setPosition(sf::Vector2f(0, window.getSize().y - floorOffset));
    floor.setFillColor(sf::Color(80, 80, 80));

    DemoDragLauncher dragLauncher(ball, clickHitbox, 5.0f);

    // Drag line
    sf::VertexArray dragLine(sf::PrimitiveType::Lines, 2);

    // Delta time clock
    sf::Clock clock;

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        // Handle events
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect(
                    sf::Vector2f(0, 0),
                    sf::Vector2f(resized->size.x, resized->size.y)
                )));

                world.setBounds(resized->size.x, resized->size.y - floorOffset);

                floor.setSize(sf::Vector2f(resized->size.x, 10));
                floor.setPosition(sf::Vector2f(0, resized->size.y - floorOffset));
            }

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                dragLauncher.handleMousePressed(*mouse);
            }

            if (const auto* move = event->getIf<sf::Event::MouseMoved>()) {
                dragLauncher.handleMouseMoved(*move);
            }

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
                dragLauncher.handleMouseReleased(*mouse);
            }
        }

        // Update physics world
        world.step(dt);

        // Resolve ball-bat collision
        resolveCircleRectangleCollision(
            ball,
            Vector2(bat.getPosition().x, bat.getPosition().y),
            bat.getSize().x,
            bat.getSize().y
        );

        // Update visuals
        ballShape.setPosition(sf::Vector2f(ball.position.x, ball.position.y));

        if (dragLauncher.isDragging()) {
            dragLauncher.updateLine(dragLine);
        }

        // Render
        window.clear();

        window.draw(floor);
        window.draw(bat);
        window.draw(ballShape);

        if (dragLauncher.isDragging()) {
            window.draw(dragLine);
        }

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
