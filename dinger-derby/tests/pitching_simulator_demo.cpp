#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>
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

constexpr float pi = 3.1415926535f;
constexpr float fixedStep = 1.0f / 180.0f;
constexpr float baseballRadius = 0.145f;
constexpr float plateZ = 18.0f;
constexpr float moundZ = -0.5f;
const Vector3 releasePoint(-0.28f, 1.88f, moundZ);
const Vector3 strikeZoneCenter(0.0f, 1.28f, plateZ);
const Vector3 boundsMinimum(-5.5f, -0.4f, -3.0f);
const Vector3 boundsMaximum(5.5f, 4.2f, 21.5f);

struct SeamPoint {
    Vector3 position;
    Vector3 side;
};

struct PitchProfile {
    char hotkey;
    std::string name;
    float speed;
    float liftCompensation;
    Vector3 breakAcceleration;
    float dragCoefficient;
    float airScale;
    sf::Color color;
};

bool loadUiFont(sf::Font& font) {
    const std::vector<std::filesystem::path> candidates = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/SFNS.ttf"
    };

    for (const std::filesystem::path& candidate : candidates) {
        if (font.openFromFile(candidate)) {
            return true;
        }
    }

    return false;
}

void drawText(
    sf::RenderWindow& window,
    const sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color
) {
    sf::Text text(font, value, size);
    text.setPosition(position);
    text.setFillColor(color);
    window.draw(text);
}

void lookAt(Camera3D& camera, const Vector3& position, const Vector3& target) {
    camera.position = position;
    Vector3 toTarget = target - position;
    float horizontal = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

    camera.rotation.y = std::atan2(toTarget.x, toTarget.z);
    camera.rotation.x = -std::atan2(toTarget.y, horizontal);
    camera.rotation.z = 0.0f;
}

Mesh3D makeBaseballMesh() {
    Mesh3D mesh = Mesh3D::sphere(1.0f, 28, 56);
    mesh.triangleColors.clear();
    mesh.triangleColors.reserve(mesh.triangles.size());

    Vector3 light = Vector3(-0.35f, 0.75f, -0.55f).normalized();

    for (int i = 0; i < mesh.triangles.size(); i++) {
        Vector3 normal = mesh.triangleNormals[i].normalized();
        float lightAmount = std::max(0.0f, normal.dot(light));
        float shade = 0.65f + lightAmount * 0.32f;
        float warmPanel = 0.5f + 0.5f * std::sin(normal.x * 11.0f + normal.y * 7.0f);

        int red = static_cast<int>((224.0f + warmPanel * 14.0f) * shade);
        int green = static_cast<int>((216.0f + warmPanel * 12.0f) * shade);
        int blue = static_cast<int>((200.0f + warmPanel * 10.0f) * shade);

        mesh.triangleColors.push_back(sf::Color(
            static_cast<std::uint8_t>(std::clamp(red, 0, 255)),
            static_cast<std::uint8_t>(std::clamp(green, 0, 255)),
            static_cast<std::uint8_t>(std::clamp(blue, 0, 255))
        ));
    }

    return mesh;
}

std::vector<SeamPoint> makeSeamLoop(bool mirrored) {
    const int pointCount = 180;
    std::vector<Vector3> positions;
    positions.reserve(pointCount);
    std::vector<SeamPoint> points;
    points.reserve(pointCount);

    for (int i = 0; i < pointCount; i++) {
        float t = static_cast<float>(i) / pointCount * pi * 2.0f;
        float wave = 0.42f * std::sin(t * 2.0f);

        if (mirrored) {
            wave = -wave;
        }

        positions.push_back(Vector3(std::cos(t), wave, std::sin(t)).normalized());
    }

    for (int i = 0; i < pointCount; i++) {
        Vector3 previous = positions[(i + pointCount - 1) % pointCount];
        Vector3 next = positions[(i + 1) % pointCount];
        Vector3 tangent = (next - previous).normalized();
        Vector3 normal = positions[i].normalized();
        Vector3 side = normal.cross(tangent).normalized();
        points.push_back(SeamPoint{normal * 1.018f, side});
    }

    return points;
}

ProjectedPoint3D project(
    const Camera3D& camera,
    const sf::RenderWindow& window,
    const Vector3& point
) {
    return camera.projectPoint(
        point,
        static_cast<float>(window.getSize().x),
        static_cast<float>(window.getSize().y)
    );
}

void drawThickProjectedLine(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const Vector3& a,
    const Vector3& b,
    float thickness,
    sf::Color color
) {
    ProjectedPoint3D projectedA = project(camera, window, a);
    ProjectedPoint3D projectedB = project(camera, window, b);

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

bool surfaceFacesCamera(const Camera3D& camera, const Matrix4& transform, const Vector3& localPoint) {
    Vector3 worldPoint = transform.transformPoint(localPoint);
    Vector3 worldNormal = transform.transformDirection(localPoint.normalized()).normalized();
    Vector3 toCamera = (camera.position - worldPoint).normalized();
    return worldNormal.dot(toCamera) > 0.0f;
}

void drawBaseballSeams(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const Matrix4& transform,
    const std::vector<SeamPoint>& seamA,
    const std::vector<SeamPoint>& seamB
) {
    auto drawSeam = [&](const std::vector<SeamPoint>& seam) {
        for (int i = 0; i < seam.size(); i++) {
            const SeamPoint& current = seam[i];
            const SeamPoint& next = seam[(i + 1) % seam.size()];
            Vector3 midpoint = (current.position + next.position).normalized();

            if (!surfaceFacesCamera(camera, transform, midpoint)) {
                continue;
            }

            drawThickProjectedLine(
                window,
                camera,
                transform.transformPoint(current.position),
                transform.transformPoint(next.position),
                1.35f,
                sf::Color(170, 32, 38, 225)
            );
        }

        for (int i = 0; i < seam.size(); i += 9) {
            const SeamPoint& point = seam[i];

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
                1.2f,
                sf::Color(130, 18, 26, 235)
            );
        }
    };

    drawSeam(seamA);
    drawSeam(seamB);
}

void drawProjectedPolyline(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const std::vector<Vector3>& points,
    sf::Color color
) {
    if (points.size() < 2) {
        return;
    }

    for (int i = 1; i < points.size(); i++) {
        float fade = static_cast<float>(i) / static_cast<float>(points.size() - 1);
        sf::Color segmentColor = color;
        segmentColor.a = static_cast<std::uint8_t>(60 + fade * 165.0f);
        drawThickProjectedLine(window, camera, points[i - 1], points[i], 2.2f, segmentColor);
    }
}

void drawStrikeZone(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const Vector3& aimPoint,
    const PitchProfile& pitch
) {
    const float halfWidth = 0.62f;
    const float halfHeight = 0.82f;
    std::array<Vector3, 4> corners = {
        Vector3(-halfWidth, -halfHeight, 0.0f) + strikeZoneCenter,
        Vector3(halfWidth, -halfHeight, 0.0f) + strikeZoneCenter,
        Vector3(halfWidth, halfHeight, 0.0f) + strikeZoneCenter,
        Vector3(-halfWidth, halfHeight, 0.0f) + strikeZoneCenter
    };

    for (int i = 0; i < 4; i++) {
        drawThickProjectedLine(
            window,
            camera,
            corners[i],
            corners[(i + 1) % 4],
            2.4f,
            sf::Color(115, 230, 235, 210)
        );
    }

    for (int i = 1; i < 3; i++) {
        float x = -halfWidth + i * (halfWidth * 2.0f / 3.0f);
        float y = -halfHeight + i * (halfHeight * 2.0f / 3.0f);
        drawThickProjectedLine(
            window,
            camera,
            Vector3(strikeZoneCenter.x + x, strikeZoneCenter.y - halfHeight, plateZ),
            Vector3(strikeZoneCenter.x + x, strikeZoneCenter.y + halfHeight, plateZ),
            1.2f,
            sf::Color(70, 150, 160, 120)
        );
        drawThickProjectedLine(
            window,
            camera,
            Vector3(strikeZoneCenter.x - halfWidth, strikeZoneCenter.y + y, plateZ),
            Vector3(strikeZoneCenter.x + halfWidth, strikeZoneCenter.y + y, plateZ),
            1.2f,
            sf::Color(70, 150, 160, 120)
        );
    }

    drawThickProjectedLine(
        window,
        camera,
        Vector3(aimPoint.x - 0.18f, aimPoint.y, plateZ),
        Vector3(aimPoint.x + 0.18f, aimPoint.y, plateZ),
        3.2f,
        pitch.color
    );
    drawThickProjectedLine(
        window,
        camera,
        Vector3(aimPoint.x, aimPoint.y - 0.18f, plateZ),
        Vector3(aimPoint.x, aimPoint.y + 0.18f, plateZ),
        3.2f,
        pitch.color
    );
}

void drawFieldGuide(sf::RenderWindow& window, const Camera3D& camera) {
    sf::Color laneColor(70, 145, 145, 95);
    for (float x : {-1.0f, 0.0f, 1.0f}) {
        drawThickProjectedLine(
            window,
            camera,
            Vector3(x, 0.0f, 0.0f),
            Vector3(x, 0.0f, plateZ + 1.0f),
            1.0f,
            laneColor
        );
    }

    for (int z = 2; z <= 18; z += 2) {
        drawThickProjectedLine(
            window,
            camera,
            Vector3(-1.4f, 0.0f, static_cast<float>(z)),
            Vector3(1.4f, 0.0f, static_cast<float>(z)),
            1.0f,
            laneColor
        );
    }

    drawThickProjectedLine(
        window,
        camera,
        Vector3(-0.55f, 0.0f, moundZ),
        Vector3(0.55f, 0.0f, moundZ),
        4.0f,
        sf::Color(205, 180, 130, 180)
    );
}

std::array<PitchProfile, 4> makePitchProfiles() {
    return {{
        PitchProfile{'F', "Four-Seam Fastball", 33.5f, 0.12f, Vector3(0.05f, 1.35f, 0.0f), 0.33f, 1.0f, sf::Color(245, 235, 180)},
        PitchProfile{'C', "Curveball", 26.5f, -0.05f, Vector3(-0.15f, -5.4f, 0.0f), 0.42f, 1.25f, sf::Color(245, 145, 90)},
        PitchProfile{'S', "Slider", 29.0f, 0.02f, Vector3(4.8f, -1.2f, 0.0f), 0.39f, 1.12f, sf::Color(145, 220, 245)},
        PitchProfile{'K', "Knuckleball", 23.0f, 0.08f, Vector3(0.0f, 0.0f, 0.0f), 0.55f, 1.45f, sf::Color(190, 245, 160)}
    }};
}

Vector3 calculateLaunchVelocity(const PitchProfile& pitch, const Vector3& aimPoint) {
    float distance = aimPoint.z - releasePoint.z;
    float flightTime = distance / pitch.speed;
    Vector3 flatVelocity(
        (aimPoint.x - releasePoint.x) / flightTime,
        (aimPoint.y - releasePoint.y) / flightTime + pitch.liftCompensation,
        pitch.speed
    );

    return flatVelocity;
}

void launchPitch(
    Body3D& baseball,
    PhysicsWorld3D& world,
    const PitchProfile& pitch,
    const Vector3& aimPoint,
    std::vector<Vector3>& trail
) {
    world = PhysicsWorld3D();
    world.setBounds(boundsMinimum, boundsMaximum);
    world.gravity = Vector3(0.0f, -9.8f, 0.0f);
    world.setAtmosphere(0.18f, Vector3(0.4f, 0.0f, -0.12f));

    baseball = Body3D(releasePoint, 0.145f);
    baseball.setRadius(baseballRadius);
    baseball.restitution = 0.3f;
    baseball.dragCoefficient = pitch.dragCoefficient;
    baseball.airResistanceScale = pitch.airScale;
    baseball.velocity = calculateLaunchVelocity(pitch, aimPoint);
    world.addBody(&baseball);

    trail.clear();
    trail.push_back(baseball.position);
}

sf::Keyboard::Key pitchKeyForProfile(const PitchProfile& pitch) {
    switch (pitch.hotkey) {
        case 'F':
            return sf::Keyboard::Key::F;
        case 'C':
            return sf::Keyboard::Key::C;
        case 'S':
            return sf::Keyboard::Key::S;
        default:
            return sf::Keyboard::Key::K;
    }
}

}

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "Pitching Simulator | F/C/S/K throw | arrows aim | R reset"
    );
    window.setFramerateLimit(60);

    bool antiAliasingEnabled = true;
    Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);
    DemoFpsCounter fpsCounter("Pitching Simulator | F fastball | C curve | S slider | K knuckle | AA on");

    sf::Font font;
    bool fontLoaded = loadUiFont(font);

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    lookAt(camera, Vector3(0.0f, 1.75f, -5.8f), Vector3(0.0f, 1.35f, plateZ));
    camera.fieldOfView = 68.0f;

    Mesh3D baseballMesh = makeBaseballMesh();
    std::vector<SeamPoint> seamA = makeSeamLoop(false);
    std::vector<SeamPoint> seamB = makeSeamLoop(true);
    RasterMeshRenderCache renderCache;

    std::array<PitchProfile, 4> pitches = makePitchProfiles();
    int selectedPitch = 0;
    Vector3 aimPoint = strikeZoneCenter;
    PhysicsWorld3D world;
    Body3D baseball;
    std::vector<Vector3> trail;
    launchPitch(baseball, world, pitches[selectedPitch], aimPoint, trail);

    sf::Clock frameClock;
    float accumulator = 0.0f;
    float pitchAge = 0.0f;
    float spinX = 0.0f;
    float spinY = 0.0f;
    float spinZ = 0.0f;
    bool paused = false;

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
                            ? "Pitching Simulator | F fastball | C curve | S slider | K knuckle | AA on"
                            : "Pitching Simulator | F fastball | C curve | S slider | K knuckle | AA off"
                    );
                }

                if (key->code == sf::Keyboard::Key::P) {
                    paused = !paused;
                }

                if (key->code == sf::Keyboard::Key::R) {
                    launchPitch(baseball, world, pitches[selectedPitch], aimPoint, trail);
                    accumulator = 0.0f;
                    pitchAge = 0.0f;
                    spinX = 0.0f;
                    spinY = 0.0f;
                    spinZ = 0.0f;
                }

                if (key->code == sf::Keyboard::Key::Left) {
                    aimPoint.x = std::clamp(aimPoint.x - 0.12f, -0.62f, 0.62f);
                }

                if (key->code == sf::Keyboard::Key::Right) {
                    aimPoint.x = std::clamp(aimPoint.x + 0.12f, -0.62f, 0.62f);
                }

                if (key->code == sf::Keyboard::Key::Up) {
                    aimPoint.y = std::clamp(aimPoint.y + 0.12f, strikeZoneCenter.y - 0.82f, strikeZoneCenter.y + 0.82f);
                }

                if (key->code == sf::Keyboard::Key::Down) {
                    aimPoint.y = std::clamp(aimPoint.y - 0.12f, strikeZoneCenter.y - 0.82f, strikeZoneCenter.y + 0.82f);
                }

                for (int i = 0; i < pitches.size(); i++) {
                    if (key->code == pitchKeyForProfile(pitches[i])) {
                        selectedPitch = i;
                        launchPitch(baseball, world, pitches[selectedPitch], aimPoint, trail);
                        accumulator = 0.0f;
                        pitchAge = 0.0f;
                        spinX = 0.0f;
                        spinY = 0.0f;
                        spinZ = 0.0f;
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

        float dt = std::min(frameClock.restart().asSeconds(), 0.1f);
        if (!paused) {
            accumulator += dt;
            while (accumulator >= fixedStep) {
                const PitchProfile& pitch = pitches[selectedPitch];
                Vector3 breakAcceleration = pitch.breakAcceleration;

                if (pitch.hotkey == 'K') {
                    breakAcceleration = Vector3(
                        std::sin(pitchAge * 19.0f) * 3.2f,
                        std::sin(pitchAge * 14.5f + 1.1f) * 1.6f,
                        0.0f
                    );
                }

                baseball.applyForce(breakAcceleration * baseball.mass);
                world.step(fixedStep);
                pitchAge += fixedStep;
                accumulator -= fixedStep;

                float rollAmount = baseball.velocity.magnitude() / baseballRadius * fixedStep;
                spinX += baseball.velocity.z * rollAmount * 0.08f;
                spinY += baseball.velocity.x * rollAmount * 0.1f;
                spinZ -= baseball.velocity.x * rollAmount * 0.12f;

                if (trail.empty() || (baseball.position - trail.back()).magnitude() > 0.12f) {
                    trail.push_back(baseball.position);
                    if (trail.size() > 160) {
                        trail.erase(trail.begin());
                    }
                }

                if (baseball.position.z > plateZ + 2.4f || baseball.position.y < -0.25f) {
                    baseball.velocity = Vector3();
                }
            }
        }

        Matrix4 baseballTransform =
            Matrix4::translation(baseball.position) *
            Matrix4::rotationY(spinY) *
            Matrix4::rotationZ(spinZ) *
            Matrix4::rotationX(spinX) *
            Matrix4::scale(Vector3(baseballRadius, baseballRadius, baseballRadius));

        frameBuffer.clear(sf::Color(5, 8, 14));
        frameBuffer.clearDepth(std::numeric_limits<float>::infinity());
        rasterizeMeshTriangles(
            frameBuffer,
            camera,
            baseballMesh,
            baseballTransform,
            sf::Color(230, 220, 205),
            renderCache
        );

        window.clear();
        frameBuffer.present(window);

        Camera3D overlayCamera = camera;
        overlayCamera.fieldOfView =
            camera.fieldOfView *
            static_cast<float>(window.getSize().x) /
            static_cast<float>(frameBuffer.getWidth());

        drawFieldGuide(window, overlayCamera);
        drawProjectedPolyline(window, overlayCamera, trail, pitches[selectedPitch].color);
        drawStrikeZone(window, overlayCamera, aimPoint, pitches[selectedPitch]);
        drawBaseballSeams(window, overlayCamera, baseballTransform, seamA, seamB);

        if (fontLoaded) {
            sf::RectangleShape panel(sf::Vector2f(430.0f, 156.0f));
            panel.setPosition(sf::Vector2f(22.0f, 20.0f));
            panel.setFillColor(sf::Color(5, 8, 14, 205));
            panel.setOutlineThickness(1.0f);
            panel.setOutlineColor(sf::Color(85, 185, 190, 170));
            window.draw(panel);

            std::ostringstream aimLabel;
            aimLabel << "Aim: x " << aimPoint.x << "  y " << aimPoint.y;

            drawText(window, font, "Pitching Simulator", 24, sf::Vector2f(42.0f, 34.0f), sf::Color(245, 250, 255));
            drawText(window, font, "Selected: " + pitches[selectedPitch].name, 17, sf::Vector2f(42.0f, 70.0f), pitches[selectedPitch].color);
            drawText(window, font, "F Fastball   C Curve   S Slider   K Knuckle", 15, sf::Vector2f(42.0f, 100.0f), sf::Color(190, 225, 225));
            drawText(window, font, "Arrows aim   R rethrow   P pause   A anti-alias", 15, sf::Vector2f(42.0f, 126.0f), sf::Color(155, 185, 195));
            drawText(window, font, aimLabel.str(), 14, sf::Vector2f(42.0f, 150.0f), sf::Color(145, 210, 215));
        }

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
