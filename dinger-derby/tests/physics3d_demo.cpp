#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "DemoFpsCounter.h"
#include "RasterDemo3D.h"
#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"
#include "../src/physics/Body3D.h"
#include "../src/physics/PhysicsWorld3D.h"
#include "../src/rendering/Camera3D.h"
#include "../src/rendering/FrameBuffer.h"
#include "../src/rendering/Mesh3D.h"
#include "../src/rendering/Rasterizer3D.h"

namespace {

constexpr float fixedStep = 1.0f / 120.0f;

void resetBodies(std::vector<Body3D>& bodies, PhysicsWorld3D& world) {
    bodies.clear();
    world = PhysicsWorld3D();
    world.setBounds(Vector3(-5.0f, -3.0f, -1.0f), Vector3(5.0f, 4.0f, 11.0f));

    for (int z = 0; z < 3; z++) {
        for (int x = 0; x < 4; x++) {
            Body3D body(Vector3(-3.0f + x * 2.0f, 2.8f + z * 1.2f, 2.0f + z * 2.0f), 1.0f);
            body.radius = 0.45f;
            body.restitution = 0.78f;
            body.velocity = Vector3(
                (x % 2 == 0 ? 1.4f : -1.1f),
                0.0f,
                (z % 2 == 0 ? 0.8f : -0.6f)
            );
            bodies.push_back(body);
        }
    }

    for (Body3D& body : bodies) {
        world.addBody(&body);
    }
}

}

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Physics Demo | Space: impulse | R: reset | A: AA"
    );
    window.setFramerateLimit(60);

    bool antiAliasingEnabled = true;
    Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);
    DemoFpsCounter fpsCounter("3D Physics Demo | Space: impulse | R: reset | A: AA on");

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    camera.position = Vector3(0.0f, 0.0f, -8.0f);

    Mesh3D sphereMesh = Mesh3D::sphere(1.0f, 8, 14);
    RasterMeshRenderCache renderCache;
    PhysicsWorld3D world;
    std::vector<Body3D> bodies;
    resetBodies(bodies, world);

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
                            ? "3D Physics Demo | Space: impulse | R: reset | A: AA on"
                            : "3D Physics Demo | Space: impulse | R: reset | A: AA off"
                    );
                }

                if (key->code == sf::Keyboard::Key::R) {
                    resetBodies(bodies, world);
                }

                if (key->code == sf::Keyboard::Key::Space) {
                    for (int i = 0; i < bodies.size(); i++) {
                        float direction = (i % 2 == 0) ? 1.0f : -1.0f;
                        bodies[i].velocity += Vector3(direction * 2.2f, 5.5f, -1.4f);
                    }
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
            world.step(fixedStep);
            accumulator -= fixedStep;
        }

        float time = animationClock.getElapsedTime().asSeconds();

        frameBuffer.clear(sf::Color(5, 8, 14));
        frameBuffer.clearDepth(std::numeric_limits<float>::infinity());

        Matrix4 sceneRotation = Matrix4::rotationY(std::sin(time * 0.2f) * 0.18f);

        for (int i = 0; i < bodies.size(); i++) {
            const Body3D& body = bodies[i];
            Matrix4 transform =
                sceneRotation *
                Matrix4::translation(body.position) *
                Matrix4::scale(Vector3(body.radius, body.radius, body.radius));

            rasterizeMeshTriangles(
                frameBuffer,
                camera,
                sphereMesh,
                transform,
                sf::Color(120, 210, 245),
                renderCache
            );
        }

        window.clear();
        frameBuffer.present(window);
        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
