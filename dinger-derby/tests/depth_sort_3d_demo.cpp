#include <SFML/Graphics.hpp>

#include "DemoFpsCounter.h"
#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"
#include "../src/rendering/Mesh3D.h"
#include "../src/rendering/SoftwareRenderer3D.h"

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Depth Sort Demo"
    );
    window.setFramerateLimit(60);
    DemoFpsCounter fpsCounter("3D Depth Sort Demo");

    Mesh3D nearCube = Mesh3D::cube();
    Mesh3D farCube = Mesh3D::cube();

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

        Matrix4 sharedRotation =
            Matrix4::rotationY(time * 0.55f) *
            Matrix4::rotationX(time * 0.35f);

        Matrix4 farTransform =
            Matrix4::translation(Vector3(0.65f, 0.05f, 1.1f)) *
            sharedRotation *
            Matrix4::scale(Vector3(1.15f, 1.15f, 1.15f));

        Matrix4 nearTransform =
            Matrix4::translation(Vector3(-0.65f, -0.05f, -0.35f)) *
            Matrix4::rotationY(-time * 0.75f) *
            Matrix4::rotationZ(time * 0.3f) *
            Matrix4::scale(Vector3(1.15f, 1.15f, 1.15f));

        SoftwareRenderer3D renderer(window);

        window.clear(sf::Color(11, 13, 18));

        renderer.drawMeshTriangles(farCube, farTransform, sf::Color(90, 140, 240));
        renderer.drawMeshTriangles(nearCube, nearTransform, sf::Color(240, 110, 90));

        renderer.drawMeshEdges(farCube, farTransform, sf::Color(15, 20, 34));
        renderer.drawMeshEdges(nearCube, nearTransform, sf::Color(34, 18, 16));

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
