#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
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
const Vector3 boxMinimum(-5.0f, -3.0f, -1.0f);
const Vector3 boxMaximum(5.0f, 4.0f, 11.0f);
const Vector3 boxCenter(0.0f, 0.5f, 5.0f);

void resetBodies(std::vector<Body3D>& bodies, PhysicsWorld3D& world) {
    bodies.clear();
    world = PhysicsWorld3D();
    world.setBounds(boxMinimum, boxMaximum);

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

void updateOrbitCamera(Camera3D& camera, float yaw, float pitch, float distance) {
    float clampedPitch = std::clamp(pitch, -1.15f, 1.15f);
    float horizontalDistance = std::cos(clampedPitch) * distance;

    camera.position = boxCenter + Vector3(
        std::sin(yaw) * horizontalDistance,
        std::sin(clampedPitch) * distance,
        -std::cos(yaw) * horizontalDistance
    );

    Vector3 toTarget = boxCenter - camera.position;
    float horizontal = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

    camera.rotation.y = std::atan2(toTarget.x, toTarget.z);
    camera.rotation.x = -std::atan2(toTarget.y, horizontal);
    camera.rotation.z = 0.0f;
}

void drawProjectedLine(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const Vector3& a,
    const Vector3& b,
    sf::Color color
) {
    ProjectedPoint3D projectedA = camera.projectPoint(
        a,
        static_cast<float>(window.getSize().x),
        static_cast<float>(window.getSize().y)
    );
    ProjectedPoint3D projectedB = camera.projectPoint(
        b,
        static_cast<float>(window.getSize().x),
        static_cast<float>(window.getSize().y)
    );

    if (!projectedA.visible || !projectedB.visible) {
        return;
    }

    sf::VertexArray line(sf::PrimitiveType::Lines, 2);
    line[0].position = sf::Vector2f(projectedA.position.x, projectedA.position.y);
    line[0].color = color;
    line[1].position = sf::Vector2f(projectedB.position.x, projectedB.position.y);
    line[1].color = color;
    window.draw(line);
}

void drawViewingBox(sf::RenderWindow& window, const Camera3D& camera) {
    std::array<Vector3, 8> corners = {
        Vector3(boxMinimum.x, boxMinimum.y, boxMinimum.z),
        Vector3(boxMaximum.x, boxMinimum.y, boxMinimum.z),
        Vector3(boxMaximum.x, boxMaximum.y, boxMinimum.z),
        Vector3(boxMinimum.x, boxMaximum.y, boxMinimum.z),
        Vector3(boxMinimum.x, boxMinimum.y, boxMaximum.z),
        Vector3(boxMaximum.x, boxMinimum.y, boxMaximum.z),
        Vector3(boxMaximum.x, boxMaximum.y, boxMaximum.z),
        Vector3(boxMinimum.x, boxMaximum.y, boxMaximum.z)
    };

    const std::array<std::array<int, 2>, 12> edges = {{
        {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
        {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
        {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}
    }};

    sf::Color edgeColor(120, 230, 235, 185);
    for (const auto& edge : edges) {
        drawProjectedLine(window, camera, corners[edge[0]], corners[edge[1]], edgeColor);
    }

    sf::Color gridColor(80, 160, 170, 90);
    for (int i = 1; i < 5; i++) {
        float t = i / 5.0f;
        float x = boxMinimum.x + (boxMaximum.x - boxMinimum.x) * t;
        float z = boxMinimum.z + (boxMaximum.z - boxMinimum.z) * t;

        drawProjectedLine(
            window,
            camera,
            Vector3(x, boxMinimum.y, boxMinimum.z),
            Vector3(x, boxMinimum.y, boxMaximum.z),
            gridColor
        );
        drawProjectedLine(
            window,
            camera,
            Vector3(boxMinimum.x, boxMinimum.y, z),
            Vector3(boxMaximum.x, boxMinimum.y, z),
            gridColor
        );
    }
}

}

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Physics Demo | drag: orbit | wheel: zoom | Space: impulse"
    );
    window.setFramerateLimit(60);

    bool antiAliasingEnabled = true;
    Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);
    DemoFpsCounter fpsCounter("3D Physics Demo | drag: orbit | wheel: zoom | Space: impulse | AA on");

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    float cameraYaw = 0.0f;
    float cameraPitch = 0.12f;
    float cameraDistance = 16.0f;
    bool draggingCamera = false;
    sf::Vector2i previousMousePosition;
    updateOrbitCamera(camera, cameraYaw, cameraPitch, cameraDistance);

    Mesh3D sphereMesh = Mesh3D::sphere(1.0f, 8, 14);
    RasterMeshRenderCache renderCache;
    PhysicsWorld3D world;
    std::vector<Body3D> bodies;
    resetBodies(bodies, world);

    sf::Clock frameClock;
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
                            ? "3D Physics Demo | drag: orbit | wheel: zoom | Space: impulse | AA on"
                            : "3D Physics Demo | drag: orbit | wheel: zoom | Space: impulse | AA off"
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

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    draggingCamera = true;
                    previousMousePosition = mouse->position;
                }
            }

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    draggingCamera = false;
                }
            }

            if (const auto* move = event->getIf<sf::Event::MouseMoved>()) {
                if (draggingCamera) {
                    sf::Vector2i current = move->position;
                    sf::Vector2i delta = current - previousMousePosition;
                    previousMousePosition = current;

                    cameraYaw += delta.x * 0.007f;
                    cameraPitch = std::clamp(cameraPitch + delta.y * 0.006f, -1.15f, 1.15f);
                    updateOrbitCamera(camera, cameraYaw, cameraPitch, cameraDistance);
                }
            }

            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                cameraDistance = std::clamp(cameraDistance - wheel->delta * 1.2f, 8.0f, 28.0f);
                updateOrbitCamera(camera, cameraYaw, cameraPitch, cameraDistance);
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

        frameBuffer.clear(sf::Color(5, 8, 14));
        frameBuffer.clearDepth(std::numeric_limits<float>::infinity());

        for (int i = 0; i < bodies.size(); i++) {
            const Body3D& body = bodies[i];
            Matrix4 transform =
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

        Camera3D overlayCamera = camera;
        overlayCamera.fieldOfView =
            camera.fieldOfView *
            static_cast<float>(window.getSize().x) /
            static_cast<float>(frameBuffer.getWidth());
        drawViewingBox(window, overlayCamera);

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
