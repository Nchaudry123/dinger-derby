#include <SFML/Graphics.hpp>
#include <cmath>

#include "Demo3D.h"
#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Point Cloud Demo"
    );
    window.setFramerateLimit(60);

    Vector3 points[9] = {
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(1.0f, 1.0f, 1.0f),
        Vector3(-1.0f, 1.0f, 1.0f),
        Vector3(1.0f, -1.0f, 1.0f),
        Vector3(-1.0f, -1.0f, 1.0f),
        Vector3(1.0f, 1.0f, -1.0f),
        Vector3(-1.0f, 1.0f, -1.0f),
        Vector3(1.0f, -1.0f, -1.0f),
        Vector3(-1.0f, -1.0f, -1.0f)
    };

    sf::Clock clock;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect(
                    sf::Vector2f(0.0f, 0.0f),
                    sf::Vector2f(resized->size.x, resized->size.y)
                )));
            }
        }

        float time = clock.getElapsedTime().asSeconds();
        Matrix4 transform =
            Matrix4::rotationY(time) *
            Matrix4::rotationX(time * 0.65f);

        window.clear(sf::Color(15, 18, 22));
        drawAxes(window, transform);

        for (int i = 0; i < 9; i++) {
            Vector3 point = transform.transformPoint(points[i]);
            sf::Color color = i == 0
                ? sf::Color(255, 230, 120)
                : sf::Color(120, 210, 255);

            drawPoint3D(window, point, i == 0 ? 7.0f : 5.0f, color);
        }

        window.display();
    }

    return 0;
}
