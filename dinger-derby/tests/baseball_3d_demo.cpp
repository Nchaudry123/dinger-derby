#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include "DemoFpsCounter.h"
#include "RasterDemo3D.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "physics/Body3D.h"
#include "physics/PhysicsWorld3D.h"
#include "rendering/Camera3D.h"
#include "rendering/FrameBuffer.h"
#include "rendering/Mesh3D.h"
#include "rendering/Rasterizer3D.h"
#include "rendering/BaseballVisual3D.h"

namespace {

constexpr float pi = 3.1415926535f;
constexpr float fixedStep = 1.0f / 120.0f;
constexpr float baseballRadius = 0.42f;
const Vector3 boxMinimum(-3.2f, -2.2f, -2.0f);
const Vector3 boxMaximum(3.2f, 2.2f, 4.0f);
const Vector3 boxCenter(0.0f, 0.0f, 1.0f);

void resetBaseball(Body3D& baseball, PhysicsWorld3D& world) {
    world = PhysicsWorld3D();
    world.setBounds(boxMinimum, boxMaximum);
    world.gravity = Vector3(0.0f, -9.8f, 0.0f);
    world.setAtmosphere(0.18f, Vector3(0.45f, 0.0f, -0.18f));

    baseball = Body3D(Vector3(-1.05f, 1.25f, 0.65f), 0.145f);
    baseball.setRadius(baseballRadius);
    baseball.restitution = 0.86f;
    baseball.dragCoefficient = 0.35f;
    baseball.airResistanceScale = 1.15f;
    baseball.velocity = Vector3(3.2f, 1.1f, 2.35f);
    world.addBody(&baseball);
}

void updateOrbitCamera(Camera3D& camera, float yaw, float pitch, float distance) {
    pitch = std::clamp(pitch, -1.15f, 1.15f);
    float horizontalDistance = std::cos(pitch) * distance;

    camera.position = boxCenter + Vector3(
        std::sin(yaw) * horizontalDistance,
        std::sin(pitch) * distance,
        -std::cos(yaw) * horizontalDistance
    );

    Vector3 toTarget = boxCenter - camera.position;
    float horizontal = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

    camera.rotation.y = std::atan2(toTarget.x, toTarget.z);
    camera.rotation.x = -std::atan2(toTarget.y, horizontal);
    camera.rotation.z = 0.0f;
}

void drawThickProjectedLine(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const Vector3& a,
    const Vector3& b,
    float thickness,
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

    sf::Vector2f a2(projectedA.position.x, projectedA.position.y);
    sf::Vector2f b2(projectedB.position.x, projectedB.position.y);
    sf::Vector2f delta = b2 - a2;
    float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);

    if (length <= 0.0f) {
        return;
    }

    sf::Vector2f normal(-delta.y / length, delta.x / length);
    sf::Vector2f offset = normal * (thickness * 0.5f);

    sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
    quad[0].position = a2 + offset;
    quad[1].position = a2 - offset;
    quad[2].position = b2 + offset;
    quad[3].position = b2 - offset;

    for (int i = 0; i < 4; i++) {
        quad[i].color = color;
    }

    window.draw(quad);
}

void drawProjectedLine(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const Vector3& a,
    const Vector3& b,
    sf::Color color
) {
    drawThickProjectedLine(window, camera, a, b, 1.4f, color);
}

bool surfaceFacesCamera(
    const Camera3D& camera,
    const Matrix4& transform,
    const Vector3& localPoint
) {
    Vector3 worldPoint = transform.transformPoint(localPoint);
    Vector3 worldNormal = transform.transformDirection(localPoint.normalized()).normalized();
    Vector3 toCamera = (camera.position - worldPoint).normalized();

    return worldNormal.dot(toCamera) > 0.0f;
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

    for (const auto& edge : edges) {
        drawProjectedLine(
            window,
            camera,
            corners[edge[0]],
            corners[edge[1]],
            sf::Color(110, 225, 235, 160)
        );
    }
}

void drawBaseballSeams(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const Matrix4& transform,
    const std::vector<SeamPoint3D>& seamA,
    const std::vector<SeamPoint3D>& seamB
) {
    auto drawSeam = [&](const std::vector<SeamPoint3D>& seam) {
        sf::Color seamColor(170, 32, 38, 230);
        sf::Color stitchColor(130, 18, 26, 240);

        for (int i = 0; i < seam.size(); i++) {
            const SeamPoint3D& current = seam[i];
            const SeamPoint3D& next = seam[(i + 1) % seam.size()];
            Vector3 midpoint = (current.position + next.position).normalized();

            if (!surfaceFacesCamera(camera, transform, midpoint)) {
                continue;
            }

            drawThickProjectedLine(
                window,
                camera,
                transform.transformPoint(current.position),
                transform.transformPoint(next.position),
                2.6f,
                seamColor
            );
        }

        for (int i = 0; i < seam.size(); i += 7) {
            const SeamPoint3D& point = seam[i];
            if (!surfaceFacesCamera(camera, transform, point.position.normalized())) {
                continue;
            }

            Vector3 stitchA = (point.position + point.side * 0.075f).normalized() * 1.038f;
            Vector3 stitchB = (point.position - point.side * 0.075f).normalized() * 1.038f;

            drawThickProjectedLine(
                window,
                camera,
                transform.transformPoint(stitchA),
                transform.transformPoint(stitchB),
                2.0f,
                stitchColor
            );
        }
    };

    drawSeam(seamA);
    drawSeam(seamB);
}

}

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "Physics Baseball | drag: orbit | wheel: zoom | Space: toss | R: reset | P: pause"
    );
    window.setFramerateLimit(60);

    bool antiAliasingEnabled = true;
    bool paused = false;
    Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);
    DemoFpsCounter fpsCounter("Physics Baseball | Space: toss | R: reset | P: pause | AA on");

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    float cameraYaw = 0.0f;
    float cameraPitch = 0.16f;
    float cameraDistance = 8.0f;
    bool draggingCamera = false;
    sf::Vector2i previousMousePosition;
    updateOrbitCamera(camera, cameraYaw, cameraPitch, cameraDistance);

    Mesh3D baseball = BaseballVisual3D::makeMesh(40, 80);
    std::vector<SeamPoint3D> seamA = BaseballVisual3D::makeSeamLoop(false);
    std::vector<SeamPoint3D> seamB = BaseballVisual3D::makeSeamLoop(true);
    RasterMeshRenderCache renderCache;
    PhysicsWorld3D world;
    Body3D baseballBody;
    resetBaseball(baseballBody, world);

    sf::Clock frameClock;
    float accumulator = 0.0f;
    float spinX = 0.24f;
    float spinY = 0.0f;
    float spinZ = -0.35f;

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
                            ? "Physics Baseball | Space: toss | R: reset | P: pause | AA on"
                            : "Physics Baseball | Space: toss | R: reset | P: pause | AA off"
                    );
                }

                if (key->code == sf::Keyboard::Key::Space) {
                    baseballBody.velocity += Vector3(1.85f, 5.2f, -2.4f);
                }

                if (key->code == sf::Keyboard::Key::R) {
                    resetBaseball(baseballBody, world);
                    accumulator = 0.0f;
                    spinX = 0.24f;
                    spinY = 0.0f;
                    spinZ = -0.35f;
                }

                if (key->code == sf::Keyboard::Key::P) {
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
                    cameraYaw += delta.x * 0.006f;
                    cameraPitch = std::clamp(cameraPitch + delta.y * 0.005f, -1.15f, 1.15f);
                    updateOrbitCamera(camera, cameraYaw, cameraPitch, cameraDistance);
                }
            }

            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                cameraDistance = std::clamp(cameraDistance - wheel->delta * 0.65f, 4.2f, 14.0f);
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

        float dt = std::min(frameClock.restart().asSeconds(), 0.1f);
        if (!paused) {
            accumulator += dt;
            while (accumulator >= fixedStep) {
                world.step(fixedStep);

                float rollAmount = baseballBody.velocity.magnitude() / baseballRadius * fixedStep;
                spinX += baseballBody.velocity.z * rollAmount * 0.16f;
                spinY += baseballBody.velocity.x * rollAmount * 0.12f;
                spinZ -= baseballBody.velocity.x * rollAmount * 0.18f;

                accumulator -= fixedStep;
            }
        }

        Matrix4 baseballTransform =
            Matrix4::translation(baseballBody.position) *
            Matrix4::rotationY(spinY) *
            Matrix4::rotationZ(spinZ) *
            Matrix4::rotationX(spinX) *
            Matrix4::scale(Vector3(baseballRadius, baseballRadius, baseballRadius));

        frameBuffer.clear(sf::Color(5, 8, 14));
        frameBuffer.clearDepth(std::numeric_limits<float>::infinity());

        rasterizeMeshTrianglesSupersampled(
            frameBuffer,
            camera,
            baseball,
            baseballTransform,
            sf::Color(230, 220, 205),
            renderCache,
            2.0f
        );

        window.clear();
        frameBuffer.present(window);

        Camera3D overlayCamera = camera;
        overlayCamera.fieldOfView =
            camera.fieldOfView *
            static_cast<float>(window.getSize().x) /
            static_cast<float>(frameBuffer.getWidth());
        drawViewingBox(window, overlayCamera);
        drawBaseballSeams(window, overlayCamera, baseballTransform, seamA, seamB);

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
