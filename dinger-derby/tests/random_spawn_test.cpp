#include <SFML/Graphics.hpp>
#include <deque>
#include <vector>
#include <cstdlib>
#include <ctime>

#include "../src/physics/Body2D.h"
#include "../src/physics/PhysicsWorld2D.h"

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "Random Spawn Test"
    );

    window.setFramerateLimit(60);
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    PhysicsWorld2D world;
    world.gravity = Vector2(0, 980.0f);
    world.setBounds(window.getSize().x, window.getSize().y);

    std::deque<Body2D> balls;
    std::vector<sf::CircleShape> shapes;

    const float radius = 20.0f;

    sf::RectangleShape floor(sf::Vector2f(window.getSize().x, 10));
    floor.setPosition(sf::Vector2f(0, window.getSize().y - 10));
    floor.setFillColor(sf::Color(80, 80, 80));

    sf::Clock clock;

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect(
                    sf::Vector2f(0, 0),
                    sf::Vector2f(resized->size.x, resized->size.y)
                )));

                world.setBounds(resized->size.x, resized->size.y);

                floor.setSize(sf::Vector2f(resized->size.x, 10));
                floor.setPosition(sf::Vector2f(0, resized->size.y - 10));
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::E) {
                    sf::Vector2u size = window.getSize();

                    float x = radius + std::rand() % static_cast<int>(size.x - radius * 2);
                    float y = radius + std::rand() % static_cast<int>(size.y - radius * 2);

                    Body2D ball(Vector2(x, y), 1.0f);
                    ball.setRadius(radius);
                    ball.restitution = 0.65f;
                    ball.velocity = Vector2(
                        static_cast<float>((std::rand() % 500) - 250),
                        static_cast<float>((std::rand() % 500) - 250)
                    );

                    balls.push_back(ball);
                    world.addBody(&balls.back());

                    sf::CircleShape shape(radius);
                    shape.setOrigin(sf::Vector2f(radius, radius));
                    shape.setFillColor(sf::Color(
                        std::rand() % 255,
                        std::rand() % 255,
                        std::rand() % 255
                    ));

                    shapes.push_back(shape);
                }
            }
        }

        world.step(dt);

        for (int i = 0; i < balls.size(); i++) {
            shapes[i].setPosition(sf::Vector2f(
                balls[i].position.x,
                balls[i].position.y
            ));
        }

        window.clear();
        window.draw(floor);

        for (auto& shape : shapes) {
            window.draw(shape);
        }

        window.display();
    }

    return 0;
}