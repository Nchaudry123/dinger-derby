#include <SFML/Graphics.hpp>
#include <limits>

#include "DemoFpsCounter.h"
#include "RasterDemo3D.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "rendering/Camera3D.h"
#include "rendering/FrameBuffer.h"
#include "rendering/Mesh3D.h"
#include "rendering/Rasterizer3D.h"

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Raster Cube Demo"
    );
    window.setFramerateLimit(60);
    bool antiAliasingEnabled = true;
    DemoFpsCounter fpsCounter("3D Raster Cube Demo | AA: on");

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    Mesh3D cube = Mesh3D::cube();
    RasterMeshRenderCache renderCache;
    sf::Clock clock;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::A) {
                    antiAliasingEnabled = !antiAliasingEnabled;
                    Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);
                    fpsCounter.setTitle(
                        antiAliasingEnabled
                            ? "3D Raster Cube Demo | AA: on"
                            : "3D Raster Cube Demo | AA: off"
                    );
                }
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
            Matrix4::translation(Vector3(0.0f, 0.0f, 0.75f)) *
            Matrix4::rotationY(time * 0.75f) *
            Matrix4::rotationX(time * 0.45f) *
            Matrix4::scale(Vector3(0.95f, 0.95f, 0.95f));

        frameBuffer.clear(sf::Color(9, 11, 16));
        frameBuffer.clearDepth(std::numeric_limits<float>::infinity());
        rasterizeMeshTriangles(
            frameBuffer,
            camera,
            cube,
            transform,
            sf::Color(180, 180, 180),
            renderCache
        );

        window.clear();
        frameBuffer.present(window);
        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
