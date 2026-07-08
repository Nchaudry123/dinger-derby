#include <SFML/Graphics.hpp>
#include <limits>

#include "DemoFpsCounter.h"
#include "RasterDemo3D.h"
#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"
#include "../src/rendering/Camera3D.h"
#include "../src/rendering/FrameBuffer.h"
#include "../src/rendering/Mesh3D.h"

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Raster Depth Demo"
    );
    window.setFramerateLimit(60);
    DemoFpsCounter fpsCounter("3D Raster Depth Demo");

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    Mesh3D cubeA = Mesh3D::cube();
    Mesh3D cubeB = Mesh3D::cube();
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
                rasterSize = rasterSizeForWindow(resized->size);
                frameBuffer.resize(rasterSize.x, rasterSize.y);
            }
        }

        float time = clock.getElapsedTime().asSeconds();

        Matrix4 transformA =
            Matrix4::translation(Vector3(-0.55f, 0.0f, -0.25f)) *
            Matrix4::rotationY(time * 0.85f) *
            Matrix4::rotationX(time * 0.35f) *
            Matrix4::scale(Vector3(1.35f, 1.35f, 1.35f));

        Matrix4 transformB =
            Matrix4::translation(Vector3(0.55f, 0.0f, 0.35f)) *
            Matrix4::rotationY(-time * 0.65f) *
            Matrix4::rotationZ(time * 0.4f) *
            Matrix4::scale(Vector3(1.35f, 1.35f, 1.35f));

        frameBuffer.clear(sf::Color(7, 9, 14));
        frameBuffer.clearDepth(std::numeric_limits<float>::infinity());

        rasterizeMeshTriangles(
            frameBuffer,
            camera,
            cubeB,
            transformB,
            sf::Color(80, 140, 245)
        );

        rasterizeMeshTriangles(
            frameBuffer,
            camera,
            cubeA,
            transformA,
            sf::Color(245, 110, 80)
        );

        window.clear();
        frameBuffer.present(window);
        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
