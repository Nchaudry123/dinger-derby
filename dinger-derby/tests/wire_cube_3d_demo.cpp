#include <SFML/Graphics.hpp>

#include "DemoFpsCounter.h"
#include "Demo3D.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Wire Cube Demo"
    );
    window.setFramerateLimit(60);
    DemoFpsCounter fpsCounter("3D Wire Cube Demo");

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
        Matrix4 cubeTransform =
            Matrix4::rotationY(time * 0.9f) *
            Matrix4::rotationX(time * 0.55f) *
            Matrix4::scale(Vector3(1.35f, 1.35f, 1.35f));

        window.clear(sf::Color(12, 14, 18));
        drawAxes(window, Matrix4::identity());
        drawWireCube(window, cubeTransform, sf::Color(245, 245, 245));

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
