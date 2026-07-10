#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

#include "DemoFpsCounter.h"
#include "RasterDemo3D.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "rendering/Camera3D.h"
#include "rendering/FrameBuffer.h"
#include "rendering/Mesh3D.h"
#include "rendering/Rasterizer3D.h"

namespace {

void updateOrbitCamera(Camera3D& camera, float yaw, float pitch, float distance) {
    Vector3 target(0.0f, 0.0f, 2.0f);
    pitch = std::clamp(pitch, -1.1f, 1.1f);
    float horizontalDistance = std::cos(pitch) * distance;

    camera.position = target + Vector3(
        std::sin(yaw) * horizontalDistance,
        std::sin(pitch) * distance,
        -std::cos(yaw) * horizontalDistance
    );

    Vector3 toTarget = target - camera.position;
    float horizontal = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

    camera.rotation.y = std::atan2(toTarget.x, toTarget.z);
    camera.rotation.x = -std::atan2(toTarget.y, horizontal);
    camera.rotation.z = 0.0f;
}

}

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Rotation Demo | drag: orbit | A: AA | Space: pause"
    );
    window.setFramerateLimit(60);

    bool antiAliasingEnabled = false;
    bool paused = false;
    Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);
    DemoFpsCounter fpsCounter("3D Rotation Demo | drag: orbit | A: AA off | Space: pause");

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    float cameraYaw = 0.0f;
    float cameraPitch = 0.2f;
    float cameraDistance = 10.5f;
    bool draggingCamera = false;
    sf::Vector2i previousMousePosition;
    updateOrbitCamera(camera, cameraYaw, cameraPitch, cameraDistance);

    Mesh3D cube = Mesh3D::cube();
    Mesh3D sphere = Mesh3D::sphere(1.0f, 8, 14);
    RasterMeshRenderCache renderCache;

    sf::Clock frameClock;
    float rotationTime = 0.0f;

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
                            ? "3D Rotation Demo | drag: orbit | A: AA off | Space: pause"
                            : "3D Rotation Demo | drag: orbit | A: AA off | Space: pause"
                    );
                }

                if (key->code == sf::Keyboard::Key::Space) {
                    paused = !paused;
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
                    cameraPitch = std::clamp(cameraPitch + delta.y * 0.006f, -1.1f, 1.1f);
                    updateOrbitCamera(camera, cameraYaw, cameraPitch, cameraDistance);
                }
            }

            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                cameraDistance = std::clamp(cameraDistance - wheel->delta * 0.85f, 5.5f, 18.0f);
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

        float dt = std::min(frameClock.restart().asSeconds(), 1.0f / 30.0f);
        if (!paused) {
            rotationTime += dt;
        }

        frameBuffer.clear(sf::Color(6, 9, 16));
        frameBuffer.clearDepth(std::numeric_limits<float>::infinity());

        Matrix4 yawCube =
            Matrix4::translation(Vector3(-2.8f, 0.0f, 2.0f)) *
            Matrix4::rotationY(rotationTime * 1.4f) *
            Matrix4::scale(Vector3(0.9f, 0.9f, 0.9f));

        Matrix4 pitchCube =
            Matrix4::translation(Vector3(0.0f, 0.0f, 2.0f)) *
            Matrix4::rotationX(rotationTime * 1.2f) *
            Matrix4::rotationZ(rotationTime * 0.35f) *
            Matrix4::scale(Vector3(0.9f, 0.9f, 0.9f));

        Matrix4 rollSphere =
            Matrix4::translation(Vector3(2.8f, 0.0f, 2.0f)) *
            Matrix4::rotationZ(rotationTime * 1.7f) *
            Matrix4::rotationY(rotationTime * 0.55f) *
            Matrix4::scale(Vector3(0.85f, 0.85f, 0.85f));

        rasterizeMeshTriangles(frameBuffer, camera, cube, yawCube, sf::Color(230, 120, 90), renderCache);
        rasterizeMeshTriangles(frameBuffer, camera, cube, pitchCube, sf::Color(110, 220, 150), renderCache);
        rasterizeMeshTriangles(frameBuffer, camera, sphere, rollSphere, sf::Color(110, 175, 245), renderCache);

        window.clear();
        frameBuffer.present(window);
        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
