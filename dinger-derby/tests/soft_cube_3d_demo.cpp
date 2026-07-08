#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

#include "DemoFpsCounter.h"
#include "RasterDemo3D.h"
#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"
#include "../src/physics/SoftBodyMesh3D.h"
#include "../src/rendering/Camera3D.h"
#include "../src/rendering/FrameBuffer.h"
#include "../src/rendering/Mesh3D.h"
#include "../src/rendering/Rasterizer3D.h"

namespace {

constexpr float fixedStep = 1.0f / 120.0f;

}

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Soft Cube Demo | Space: punch | R: reset | AA: on"
    );
    window.setFramerateLimit(60);

    bool antiAliasingEnabled = true;
    DemoFpsCounter fpsCounter("3D Soft Cube Demo | Space: punch | R: reset | AA: on");

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    SoftBodyMesh3D softCube = SoftBodyMesh3D::cube(1.8f);
    sf::Clock frameClock;
    sf::Clock animationClock;
    float accumulator = 0.0f;

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
                            ? "3D Soft Cube Demo | Space: punch | R: reset | AA: on"
                            : "3D Soft Cube Demo | Space: punch | R: reset | AA: off"
                    );
                }

                if (key->code == sf::Keyboard::Key::R) {
                    softCube = SoftBodyMesh3D::cube(1.8f);
                }

                if (key->code == sf::Keyboard::Key::Space) {
                    softCube.applyImpulse(
                        Vector3(0.9f, 0.45f, 0.9f),
                        2.2f,
                        Vector3(-1.8f, 1.2f, 7.5f)
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

        accumulator += std::min(frameClock.restart().asSeconds(), 0.1f);
        while (accumulator >= fixedStep) {
            softCube.step(fixedStep);
            accumulator -= fixedStep;
        }

        float time = animationClock.getElapsedTime().asSeconds();
        Matrix4 transform =
            Matrix4::translation(Vector3(0.0f, 0.0f, 0.85f)) *
            Matrix4::rotationY(time * 0.55f) *
            Matrix4::rotationX(std::sin(time * 0.7f) * 0.18f);

        Mesh3D mesh = softCube.toMesh();

        frameBuffer.clear(sf::Color(7, 10, 16));
        frameBuffer.clearDepth(std::numeric_limits<float>::infinity());
        Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);

        rasterizeMeshTriangles(
            frameBuffer,
            camera,
            mesh,
            transform,
            sf::Color(180, 220, 255)
        );

        window.clear();
        frameBuffer.present(window);
        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
