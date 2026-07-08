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
        "3D Raster Cube Demo"
    );
    window.setFramerateLimit(60);
    DemoFpsCounter fpsCounter("3D Raster Cube Demo");

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
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
                rasterSize = rasterSizeForWindow(resized->size);
                frameBuffer.resize(rasterSize.x, rasterSize.y);
            }
        }

        float time = clock.getElapsedTime().asSeconds();
        Matrix4 transform =
            Matrix4::rotationY(time * 0.75f) *
            Matrix4::rotationX(time * 0.45f) *
            Matrix4::scale(Vector3(1.45f, 1.45f, 1.45f));

        frameBuffer.clear(sf::Color(9, 11, 16));
        frameBuffer.clearDepth(std::numeric_limits<float>::infinity());
        rasterizeMeshTriangles(
            frameBuffer,
            camera,
            cube,
            transform,
            sf::Color(180, 180, 180)
        );

        window.clear();
        frameBuffer.present(window);
        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
