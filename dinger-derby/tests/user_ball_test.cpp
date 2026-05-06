#include <SFML/Graphics.hpp>
#include <vector>
#include "../src/physics/Body2D.h"
#include "../src/physics/PhysicsWorld2D.h"

int main() {
    // Create window
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "User Ball Test"
    );

    window.setFramerateLimit(60);

    // Setup physics world
    PhysicsWorld2D world;
    world.gravity = Vector2(0, 980.0f);
    world.setBounds(window.getSize().x, window.getSize().y);

    // Create controllable ball
    Body2D ball(Vector2(400, 300), 1.0f);
    ball.radius = 20.0f;
    ball.restitution = 0.7f;

    world.addBody(&ball);

    // Ball visual
    sf::CircleShape ballShape(ball.radius);
    ballShape.setOrigin(sf::Vector2f(ball.radius, ball.radius));
    ballShape.setFillColor(sf::Color::White);

    // Hitbox debug circle
    float clickHitbox = 60.0f;

    sf::CircleShape hitboxShape(clickHitbox);
    hitboxShape.setOrigin(sf::Vector2f(clickHitbox, clickHitbox));
    hitboxShape.setFillColor(sf::Color::Transparent);
    hitboxShape.setOutlineColor(sf::Color::Red);
    hitboxShape.setOutlineThickness(2);

    // Floor visual
    sf::RectangleShape floorShape;
    floorShape.setFillColor(sf::Color(80, 80, 80));
    floorShape.setSize(sf::Vector2f(window.getSize().x, 10));
    floorShape.setPosition(
        sf::Vector2f(0, window.getSize().y - 10)
    );

    // Drag system
    bool isDragging = false;

    Vector2 dragStart;
    Vector2 dragCurrent;

    float powerScale = 5.0f;

    // Drag line
    sf::VertexArray dragLine(sf::PrimitiveType::Lines, 2);

    // Trajectory dots
    std::vector<sf::CircleShape> trajectoryDots;

    int trajectorySteps = 30;

    for (int i = 0; i < trajectorySteps; i++) {
        sf::CircleShape dot(3);
        dot.setOrigin(sf::Vector2f(3, 3));
        dot.setFillColor(sf::Color::Yellow);

        trajectoryDots.push_back(dot);
    }

    // Delta time clock
    sf::Clock clock;

    // Main loop
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        // Event handling
        while (const std::optional event = window.pollEvent()) {
            // Close window
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            // Resize handling
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                sf::FloatRect visibleArea(
                    sf::Vector2f(0.f, 0.f),
                    sf::Vector2f(resized->size.x, resized->size.y)
                );

                window.setView(sf::View(visibleArea));

                world.setBounds(
                    resized->size.x,
                    resized->size.y
                );

                floorShape.setSize(
                    sf::Vector2f(resized->size.x, 10)
                );

                floorShape.setPosition(
                    sf::Vector2f(0, resized->size.y - 10)
                );
            }

            // Mouse pressed
            if (const auto* mouse =
                event->getIf<sf::Event::MouseButtonPressed>()) {

                if (mouse->button == sf::Mouse::Button::Left) {
                    Vector2 mousePos(
                        mouse->position.x,
                        mouse->position.y
                    );

                    Vector2 difference =
                        mousePos - ball.position;

                    if (difference.magnitude() <= clickHitbox) {
                        isDragging = true;

                        dragStart = mousePos;
                        dragCurrent = mousePos;
                    }
                }
            }

            // Mouse moved
            if (const auto* move =
                event->getIf<sf::Event::MouseMoved>()) {

                if (isDragging) {
                    dragCurrent = Vector2(
                        move->position.x,
                        move->position.y
                    );
                }
            }

            // Mouse released
            if (const auto* mouse =
                event->getIf<sf::Event::MouseButtonReleased>()) {

                if (
                    mouse->button == sf::Mouse::Button::Left &&
                    isDragging
                ) {
                    Vector2 releasePos(
                        mouse->position.x,
                        mouse->position.y
                    );

                    Vector2 launchVector =
                        dragStart - releasePos;

                    ball.wakeUp();

                    ball.velocity =
                        launchVector * powerScale;

                    isDragging = false;
                }
            }
        }

        // Update physics
        world.step(dt);

        // Update visuals
        ballShape.setPosition(
            sf::Vector2f(
                ball.position.x,
                ball.position.y
            )
        );

        hitboxShape.setPosition(
            sf::Vector2f(
                ball.position.x,
                ball.position.y
            )
        );

        // Update drag line + trajectory
        if (isDragging) {
            dragLine[0].position =
                sf::Vector2f(
                    ball.position.x,
                    ball.position.y
                );

            dragLine[1].position =
                sf::Vector2f(
                    dragCurrent.x,
                    dragCurrent.y
                );

            dragLine[0].color = sf::Color::Green;
            dragLine[1].color = sf::Color::Green;

            Vector2 launchVector =
                dragStart - dragCurrent;

            Vector2 predictedVelocity =
                launchVector * powerScale;

            Vector2 predictedPosition =
                ball.position;

            float simDt = 0.05f;

            for (int i = 0; i < trajectorySteps; i++) {
                predictedVelocity +=
                    world.gravity * simDt;

                predictedVelocity -=
                    predictedVelocity * 0.2f * simDt;

                predictedPosition +=
                    predictedVelocity * simDt;

                if (
                    predictedPosition.y + ball.radius >
                    world.groundY
                ) {
                    predictedPosition.y =
                        world.groundY - ball.radius;
                }

                trajectoryDots[i].setPosition(
                    sf::Vector2f(
                        predictedPosition.x,
                        predictedPosition.y
                    )
                );
            }
        }

        // Rendering
        window.clear();

        window.draw(floorShape);
        window.draw(hitboxShape);
        window.draw(ballShape);

        if (isDragging) {
            window.draw(dragLine);

            for (auto& dot : trajectoryDots) {
                window.draw(dot);
            }
        }

        window.display();
    }

    return 0;
}