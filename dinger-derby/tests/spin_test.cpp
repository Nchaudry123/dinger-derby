#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>

#include "DemoFpsCounter.h"
#include "../src/physics/Body2D.h"
#include "../src/physics/PhysicsWorld2D.h"

namespace {

void resetBall(Body2D& ball) {
    ball.position = Vector2(280.0f, 280.0f);
    ball.velocity = Vector2(360.0f, 0.0f);
    ball.acceleration = Vector2();
    ball.rotation = 0.0f;
    ball.angularVelocity = 8.0f;
    ball.angularAcceleration = 0.0f;
    ball.torque = 0.0f;
    ball.wakeUp();
}

void drawSpoke(
    sf::RenderWindow& window,
    const Vector2& center,
    float radius,
    float angle,
    sf::Color color
) {
    sf::RectangleShape spoke(sf::Vector2f(radius * 0.86f, 4.0f));
    spoke.setOrigin(sf::Vector2f(0.0f, 2.0f));
    spoke.setPosition(sf::Vector2f(center.x, center.y));
    spoke.setRotation(sf::radians(angle));
    spoke.setFillColor(color);
    window.draw(spoke);
}

}

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "2D Rotation Demo | Space: spin | Left/Right: push | R: reset"
    );
    window.setFramerateLimit(60);
    DemoFpsCounter fpsCounter("2D Rotation Demo | Space: spin | Left/Right: push | R: reset");

    PhysicsWorld2D world;
    world.gravity = Vector2(0.0f, 980.0f);
    world.setBounds(window.getSize().x, window.getSize().y);

    Body2D ball(Vector2(280.0f, 280.0f), 1.0f);
    ball.setRadius(52.0f);
    ball.restitution = 0.68f;
    resetBall(ball);
    world.addBody(&ball);

    sf::CircleShape ballShape(ball.radius);
    ballShape.setOrigin(sf::Vector2f(ball.radius, ball.radius));
    ballShape.setFillColor(sf::Color(238, 245, 250));
    ballShape.setOutlineThickness(3.0f);
    ballShape.setOutlineColor(sf::Color(60, 95, 110));

    sf::RectangleShape ground(sf::Vector2f(1280.0f, 6.0f));
    ground.setPosition(sf::Vector2f(0.0f, 668.0f));
    ground.setFillColor(sf::Color(55, 85, 90));

    sf::Clock clock;

    while (window.isOpen()) {
        float dt = std::min(clock.restart().asSeconds(), 1.0f / 30.0f);

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Space) {
                    ball.angularVelocity += 12.0f;
                    ball.velocity.x += 170.0f;
                    ball.wakeUp();
                }

                if (key->code == sf::Keyboard::Key::R) {
                    resetBall(ball);
                }
            }
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) {
            ball.applyForce(Vector2(-900.0f, 0.0f));
            ball.applyTorque(-18000.0f);
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) {
            ball.applyForce(Vector2(900.0f, 0.0f));
            ball.applyTorque(18000.0f);
        }

        world.step(dt);

        ballShape.setPosition(sf::Vector2f(ball.position.x, ball.position.y));

        window.clear(sf::Color(8, 12, 18));
        window.draw(ground);

        for (int i = 0; i < 18; i++) {
            sf::RectangleShape tick(sf::Vector2f(34.0f, 2.0f));
            tick.setPosition(sf::Vector2f(i * 80.0f, 650.0f));
            tick.setFillColor(sf::Color(35, 58, 66));
            window.draw(tick);
        }

        window.draw(ballShape);

        for (int i = 0; i < 6; i++) {
            float angle = ball.rotation + i * 3.1415926535f / 3.0f;
            drawSpoke(
                window,
                ball.position,
                ball.radius,
                angle,
                i % 2 == 0 ? sf::Color(235, 70, 75) : sf::Color(65, 145, 220)
            );
        }

        sf::CircleShape hub(7.0f);
        hub.setOrigin(sf::Vector2f(7.0f, 7.0f));
        hub.setPosition(sf::Vector2f(ball.position.x, ball.position.y));
        hub.setFillColor(sf::Color(20, 28, 36));
        window.draw(hub);

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
