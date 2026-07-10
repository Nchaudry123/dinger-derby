#include <SFML/Graphics.hpp>

#include "DemoFpsCounter.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "rendering/Mesh3D.h"
#include "rendering/SoftwareRenderer3D.h"

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Filled Cube Demo"
    );
    window.setFramerateLimit(60);
    DemoFpsCounter fpsCounter("3D Filled Cube Demo");

    Mesh3D cube = Mesh3D::cube();
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
            Matrix4::rotationY(time * 0.75f) *
            Matrix4::rotationX(time * 0.45f) *
            Matrix4::scale(Vector3(1.35f, 1.35f, 1.35f));

        SoftwareRenderer3D renderer(window);

        window.clear(sf::Color(12, 14, 18));
        renderer.drawMeshTriangles(cube, transform, sf::Color(180, 180, 180));
        renderer.drawMeshEdges(cube, transform, sf::Color(20, 22, 26));
        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
