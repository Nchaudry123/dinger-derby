#include <SFML/Graphics.hpp>

#include "DemoFpsCounter.h"
#include "Demo3D.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Transform Stack Demo"
    );
    window.setFramerateLimit(60);
    DemoFpsCounter fpsCounter("3D Transform Stack Demo");

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

        Matrix4 parent =
            Matrix4::rotationY(time * 0.6f) *
            Matrix4::rotationZ(time * 0.25f) *
            Matrix4::scale(Vector3(0.9f, 0.9f, 0.9f));

        Matrix4 childA =
            parent *
            Matrix4::rotationY(time * 1.7f) *
            Matrix4::translation(Vector3(2.4f, 0.0f, 0.0f)) *
            Matrix4::scale(Vector3(0.35f, 0.35f, 0.35f));

        Matrix4 childB =
            parent *
            Matrix4::rotationX(time * 1.2f) *
            Matrix4::translation(Vector3(0.0f, 2.0f, 0.0f)) *
            Matrix4::scale(Vector3(0.28f, 0.28f, 0.28f));

        Matrix4 childC =
            parent *
            Matrix4::rotationZ(time * -1.4f) *
            Matrix4::translation(Vector3(0.0f, 0.0f, 2.2f)) *
            Matrix4::scale(Vector3(0.3f, 0.3f, 0.3f));

        window.clear(sf::Color(13, 16, 20));

        drawAxes(window, parent);
        drawWireCube(window, parent, sf::Color(230, 230, 235));
        drawWireCube(window, childA, sf::Color(255, 120, 105));
        drawWireCube(window, childB, sf::Color(110, 230, 150));
        drawWireCube(window, childC, sf::Color(120, 170, 255));

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
