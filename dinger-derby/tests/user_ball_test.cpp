#include <SFML/Graphics.hpp>
#include <vector>
#include "DemoDragLauncher.h"
#include "DemoFpsCounter.h"
#include "physics/Body2D.h"
#include "physics/PhysicsWorld2D.h"

int main() {
    // Create window
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "User Ball Test"
    );

    window.setFramerateLimit(60);
    DemoFpsCounter fpsCounter("User Ball Test");
    const float floorHeight = 10.0f;
    const float floorOffset = 100.0f;

    // Setup physics world
    PhysicsWorld2D world;
    world.gravity = Vector2(0, 980.0f);
    world.setBounds(window.getSize().x, window.getSize().y - floorOffset);    


    // Create controllable ball
    Body2D ball(Vector2(400, 300), 1.0f);
    ball.setRadius(20.0f);
    ball.restitution = 0.7f;

    world.addBody(&ball);

    // Ball visual
    sf::CircleShape ballShape(ball.radius);
    ballShape.setOrigin(sf::Vector2f(ball.radius, ball.radius));
    ballShape.setFillColor(sf::Color::White);

    // Hitbox debug circle
    const float clickHitbox = 60.0f;

    sf::CircleShape hitboxShape(clickHitbox);
    hitboxShape.setOrigin(sf::Vector2f(clickHitbox, clickHitbox));
    hitboxShape.setFillColor(sf::Color::Transparent);
    hitboxShape.setOutlineColor(sf::Color::Red);
    hitboxShape.setOutlineThickness(2);

    // Floor visual
    sf::RectangleShape floorShape;
    floorShape.setFillColor(sf::Color(80, 80, 80));
    floorShape.setSize(sf::Vector2f(window.getSize().x, floorHeight));
    floorShape.setPosition(
        sf::Vector2f(0, window.getSize().y - floorOffset)
    );

    DemoDragLauncher dragLauncher(ball, clickHitbox, 5.0f);

    // Drag line
    sf::VertexArray dragLine(sf::PrimitiveType::Lines, 2);

    // Trajectory dots
    std::vector<sf::CircleShape> trajectoryDots;

    const int trajectorySteps = 200;

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
                    resized->size.y - floorOffset
                );

                floorShape.setSize(
                    sf::Vector2f(resized->size.x, floorHeight)
                );

                floorShape.setPosition(
                    sf::Vector2f(0, resized->size.y - floorOffset)
                );
            }

            if (const auto* mouse =
                event->getIf<sf::Event::MouseButtonPressed>()) {
                dragLauncher.handleMousePressed(*mouse);
            }

            if (const auto* move =
                event->getIf<sf::Event::MouseMoved>()) {
                dragLauncher.handleMouseMoved(*move);
            }

            if (const auto* mouse =
                event->getIf<sf::Event::MouseButtonReleased>()) {
                dragLauncher.handleMouseReleased(*mouse);
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
        if (dragLauncher.isDragging()) {
            dragLauncher.updateLine(dragLine);

            Vector2 launchVector = dragLauncher.currentDragVector();

            Vector2 predictedVelocity = launchVector * 5.0f;

            Vector2 predictedPosition = ball.position;

            float simDt = 0.016f;

            for (int i = 0; i < trajectorySteps; i++) {
                // Same as applyForce(gravity) + update(dt)
                predictedVelocity += world.gravity * simDt;

                // Same drag as Body2D::update()
                float dragCoefficient = 0.2f;
                predictedVelocity = predictedVelocity - predictedVelocity * dragCoefficient * simDt;

                // Same position update
                predictedPosition += predictedVelocity * simDt;

                // Same ground collision
                if (predictedPosition.y + ball.radius > world.groundY) {
                    predictedPosition.y = world.groundY - ball.radius;
                    predictedVelocity.y *= -ball.restitution;
                }

                // Same ceiling collision
                if (predictedPosition.y - ball.radius < world.ceilingY) {
                    predictedPosition.y = world.ceilingY + ball.radius;
                    predictedVelocity.y *= -ball.restitution;
                }

                // Same left wall collision
                if (predictedPosition.x - ball.radius < world.leftWall) {
                    predictedPosition.x = world.leftWall + ball.radius;
                    predictedVelocity.x *= -ball.restitution;
                }

                // Same right wall collision
                if (predictedPosition.x + ball.radius > world.rightWall) {
                    predictedPosition.x = world.rightWall - ball.radius;
                    predictedVelocity.x *= -ball.restitution;
                }

                trajectoryDots[i].setPosition(
                    sf::Vector2f(predictedPosition.x, predictedPosition.y)
                );
            }
        }

        // Rendering
        window.clear();

        window.draw(floorShape);
        window.draw(hitboxShape);
        window.draw(ballShape);

        if (dragLauncher.isDragging()) {
            window.draw(dragLine);

            for (auto& dot : trajectoryDots) {
                window.draw(dot);
            }
        }

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
