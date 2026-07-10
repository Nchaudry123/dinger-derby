#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <random>
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
constexpr float baseballRadius = 0.2f;
constexpr float feetPerWorldUnit = 2.0f;
constexpr float pitchingDistanceFeet = 60.5f;
constexpr float pitchAirDensity = 0.075f;
constexpr float plateZ = pitchingDistanceFeet / feetPerWorldUnit;
constexpr float moundZ = 0.0f;
constexpr float strikeZoneHalfWidth = 0.46f;
constexpr float strikeZoneHalfHeight = 0.55f;
const Vector3 releasePoint(-0.22f, 1.72f, moundZ);
const Vector3 strikeZoneCenter(0.0f, 1.28f, plateZ);
const Vector3 boundsMinimum(-3.2f, -40.0f, -2.0f);
const Vector3 boundsMaximum(3.2f, 3.6f, plateZ + 4.0f);
const sf::FloatRect speedSliderTrack(sf::Vector2f(34.0f, 86.0f), sf::Vector2f(300.0f, 8.0f));

struct SeamPoint {
    Vector3 position;
    Vector3 side;
};

struct PitchProfile {
    char hotkey;
    std::string name;
    float baseSpeedMph;
    float speedVarianceMph;
    float liftCompensation;
    Vector3 breakAcceleration;
    float breakStartZ;
    float dragCoefficient;
    float airScale;
    sf::Color color;
};

struct PitchFlightVariation {
    Vector3 releaseOffset;
    Vector3 commandOffset;
    Vector3 airVelocity;
    Vector3 breakScale;
    float dragScale = 1.0f;
    float liftOffset = 0.0f;
    float turbulencePhase = 0.0f;
    float turbulenceStrength = 0.0f;
};

struct PitchResult {
    Vector3 platePosition;
    std::string label;
    sf::Color color;
};

enum class PitchCameraMode {
    Overview,
    Catcher,
    Pitcher
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

float randomRange(std::mt19937& randomGenerator, float minimum, float maximum) {
    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(randomGenerator);
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

float speedScaleFromSliderX(float x) {
    float t = (x - speedSliderTrack.position.x) / speedSliderTrack.size.x;
    return 0.75f + std::clamp(t, 0.0f, 1.0f) * 0.5f;
}

float sliderXFromSpeedScale(float speedScale) {
    float t = (std::clamp(speedScale, 0.75f, 1.25f) - 0.75f) / 0.5f;
    return speedSliderTrack.position.x + speedSliderTrack.size.x * t;
}

void lookAt(Camera3D& camera, const Vector3& position, const Vector3& target) {
    camera.position = position;
    Vector3 toTarget = target - position;
    float horizontal = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

    camera.rotation.y = std::atan2(toTarget.x, toTarget.z);
    camera.rotation.x = -std::atan2(toTarget.y, horizontal);
    camera.rotation.z = 0.0f;
}

std::string cameraModeName(PitchCameraMode mode) {
    switch (mode) {
        case PitchCameraMode::Overview:
            return "Overview";
        case PitchCameraMode::Catcher:
            return "Catcher";
        case PitchCameraMode::Pitcher:
            return "Pitcher";
        default:
            return "Overview";
    }
}

void applyCameraMode(Camera3D& camera, PitchCameraMode mode) {
    switch (mode) {
        case PitchCameraMode::Overview:
            lookAt(camera, Vector3(0.0f, 1.68f, -3.35f), Vector3(0.0f, 1.25f, plateZ));
            camera.fieldOfView = 1450.0f;
            break;
        case PitchCameraMode::Catcher:
            lookAt(camera, Vector3(0.0f, 1.42f, plateZ + 2.65f), Vector3(0.0f, 1.48f, plateZ - 8.0f));
            camera.fieldOfView = 1020.0f;
            break;
        case PitchCameraMode::Pitcher:
            lookAt(camera, Vector3(0.0f, 1.78f, -1.8f), Vector3(0.0f, 1.35f, plateZ));
            camera.fieldOfView = 1180.0f;
            break;
    }
}

float horizontalAimDeltaForCamera(PitchCameraMode mode, float screenDirection) {
    float worldDirection = mode == PitchCameraMode::Catcher
        ? -screenDirection
        : screenDirection;
    return worldDirection * 0.1f;
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

void drawProjectedDot(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const Vector3& point,
    float radius,
    sf::Color color
) {
    ProjectedPoint3D projected = project(camera, window, point);

    if (!projected.visible) {
        return;
    }

    sf::CircleShape dot(radius);
    dot.setOrigin(sf::Vector2f(radius, radius));
    dot.setPosition(sf::Vector2f(projected.position.x, projected.position.y));
    dot.setFillColor(color);
    dot.setOutlineThickness(1.0f);
    dot.setOutlineColor(sf::Color(5, 8, 14, 210));
    window.draw(dot);
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
    std::array<Vector3, 4> corners = {
        Vector3(-strikeZoneHalfWidth, -strikeZoneHalfHeight, 0.0f) + strikeZoneCenter,
        Vector3(strikeZoneHalfWidth, -strikeZoneHalfHeight, 0.0f) + strikeZoneCenter,
        Vector3(strikeZoneHalfWidth, strikeZoneHalfHeight, 0.0f) + strikeZoneCenter,
        Vector3(-strikeZoneHalfWidth, strikeZoneHalfHeight, 0.0f) + strikeZoneCenter
    };

    for (int i = 0; i < 4; i++) {
        drawThickProjectedLine(
            window,
            camera,
            corners[i],
            corners[(i + 1) % 4],
            3.0f,
            sf::Color(115, 230, 235, 210)
        );
    }

    drawThickProjectedLine(
        window,
        camera,
        Vector3(aimPoint.x - 0.22f, aimPoint.y, plateZ),
        Vector3(aimPoint.x + 0.22f, aimPoint.y, plateZ),
        3.6f,
        pitch.color
    );
    drawThickProjectedLine(
        window,
        camera,
        Vector3(aimPoint.x, aimPoint.y - 0.22f, plateZ),
        Vector3(aimPoint.x, aimPoint.y + 0.22f, plateZ),
        3.6f,
        pitch.color
    );
}

void drawHomePlate(sf::RenderWindow& window, const Camera3D& camera) {
    const float halfWidth = 0.36f;
    const float frontZ = plateZ + 0.08f;
    const float shoulderZ = plateZ - 0.14f;
    const float pointZ = plateZ - 0.42f;
    std::array<Vector3, 5> plate = {
        Vector3(-halfWidth, 0.02f, frontZ),
        Vector3(halfWidth, 0.02f, frontZ),
        Vector3(halfWidth, 0.02f, shoulderZ),
        Vector3(0.0f, 0.02f, pointZ),
        Vector3(-halfWidth, 0.02f, shoulderZ)
    };

    for (int i = 0; i < plate.size(); i++) {
        drawThickProjectedLine(
            window,
            camera,
            plate[i],
            plate[(i + 1) % plate.size()],
            2.0f,
            sf::Color(235, 230, 205, 190)
        );
    }
}

void drawCatcherTarget(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const Vector3& aimPoint,
    const PitchProfile& pitch
) {
    Vector3 target = Vector3(aimPoint.x, aimPoint.y, plateZ + 0.12f);
    sf::Color ghost = pitch.color;
    ghost.a = 90;
    drawProjectedDot(window, camera, target, 10.0f, sf::Color(25, 35, 42, 135));
    drawThickProjectedLine(window, camera, target + Vector3(-0.18f, 0.0f, 0.0f), target + Vector3(-0.07f, 0.0f, 0.0f), 2.2f, ghost);
    drawThickProjectedLine(window, camera, target + Vector3(0.07f, 0.0f, 0.0f), target + Vector3(0.18f, 0.0f, 0.0f), 2.2f, ghost);
    drawThickProjectedLine(window, camera, target + Vector3(0.0f, -0.18f, 0.0f), target + Vector3(0.0f, -0.07f, 0.0f), 2.2f, ghost);
    drawThickProjectedLine(window, camera, target + Vector3(0.0f, 0.07f, 0.0f), target + Vector3(0.0f, 0.18f, 0.0f), 2.2f, ghost);
}

void drawCatcherSilhouette(sf::RenderWindow& window, const Camera3D& camera) {
    const float z = plateZ + 0.82f;
    const sf::Color gear(25, 43, 55, 190);
    const sf::Color outline(95, 185, 190, 105);
    const sf::Color mitt(150, 110, 65, 215);

    drawProjectedDot(window, camera, Vector3(0.0f, 2.02f, z), 15.0f, gear);
    drawThickProjectedLine(window, camera, Vector3(-0.19f, 1.88f, z), Vector3(0.19f, 1.88f, z), 4.0f, outline);
    drawThickProjectedLine(window, camera, Vector3(-0.24f, 1.66f, z), Vector3(0.24f, 1.66f, z), 20.0f, gear);
    drawThickProjectedLine(window, camera, Vector3(-0.18f, 1.85f, z), Vector3(-0.32f, 1.47f, z), 5.0f, outline);
    drawThickProjectedLine(window, camera, Vector3(0.18f, 1.85f, z), Vector3(0.32f, 1.47f, z), 5.0f, outline);
    drawThickProjectedLine(window, camera, Vector3(-0.32f, 1.45f, z), Vector3(-0.56f, 1.12f, z + 0.06f), 7.0f, gear);
    drawThickProjectedLine(window, camera, Vector3(0.32f, 1.45f, z), Vector3(0.56f, 1.12f, z + 0.06f), 7.0f, gear);
    drawThickProjectedLine(window, camera, Vector3(-0.16f, 1.30f, z), Vector3(-0.42f, 0.70f, z + 0.10f), 8.0f, gear);
    drawThickProjectedLine(window, camera, Vector3(0.16f, 1.30f, z), Vector3(0.42f, 0.70f, z + 0.10f), 8.0f, gear);
    drawProjectedDot(window, camera, Vector3(-0.58f, 1.26f, z - 0.08f), 13.0f, mitt);
}

PitchResult classifyPitchResult(const Vector3& platePosition) {
    bool inHorizontalZone = std::abs(platePosition.x - strikeZoneCenter.x) <= strikeZoneHalfWidth;
    bool inVerticalZone = std::abs(platePosition.y - strikeZoneCenter.y) <= strikeZoneHalfHeight;

    if (inHorizontalZone && inVerticalZone) {
        return PitchResult{platePosition, "Strike", sf::Color(150, 245, 170)};
    }

    if (platePosition.y > strikeZoneCenter.y + strikeZoneHalfHeight) {
        return PitchResult{platePosition, "Ball High", sf::Color(245, 210, 120)};
    }

    if (platePosition.y < strikeZoneCenter.y - strikeZoneHalfHeight) {
        return PitchResult{platePosition, "Ball Low", sf::Color(245, 180, 110)};
    }

    if (platePosition.x < strikeZoneCenter.x - strikeZoneHalfWidth) {
        return PitchResult{platePosition, "Ball Inside", sf::Color(130, 205, 245)};
    }

    return PitchResult{platePosition, "Ball Outside", sf::Color(130, 205, 245)};
}

void drawPitchResultHistory(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const std::vector<PitchResult>& results
) {
    for (int i = 0; i < results.size(); i++) {
        float age = static_cast<float>(i + 1) / static_cast<float>(results.size());
        sf::Color color = results[i].color;
        color.a = static_cast<std::uint8_t>(95 + age * 150.0f);
        drawProjectedDot(window, camera, results[i].platePosition, 4.0f + age * 2.0f, color);
    }
}

void drawFieldGuide(sf::RenderWindow& window, const Camera3D& camera) {
    sf::Color laneColor(70, 145, 145, 70);
    drawThickProjectedLine(
        window,
        camera,
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, plateZ + 0.4f),
        1.0f,
        laneColor
    );

    for (int z = 2; z <= static_cast<int>(plateZ); z += 2) {
        drawThickProjectedLine(
            window,
            camera,
            Vector3(-0.65f, 0.0f, static_cast<float>(z)),
            Vector3(0.65f, 0.0f, static_cast<float>(z)),
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

std::array<PitchProfile, 5> makePitchProfiles() {
    return {{
        PitchProfile{'F', "Four-Seam", 96.1f, 2.0f, 0.06f, Vector3(0.02f, 0.24f, 0.0f), 0.34f, 0.12f, 0.82f, sf::Color(245, 235, 180)},
        PitchProfile{'P', "Splitter", 91.5f, 1.8f, 0.02f, Vector3(0.10f, -1.18f, 0.0f), 0.62f, 0.34f, 0.88f, sf::Color(190, 245, 160)},
        PitchProfile{'C', "Curve", 77.1f, 2.2f, 0.08f, Vector3(-0.14f, -1.28f, 0.0f), 0.50f, 0.30f, 0.92f, sf::Color(245, 145, 90)},
        PitchProfile{'T', "Cutter", 91.4f, 1.9f, 0.03f, Vector3(0.72f, -0.06f, 0.0f), 0.42f, 0.28f, 0.82f, sf::Color(145, 220, 245)},
        PitchProfile{'S', "Slider", 87.2f, 1.7f, 0.03f, Vector3(1.45f, -0.28f, 0.0f), 0.48f, 0.30f, 0.86f, sf::Color(190, 160, 245)}
    }};
}

float mphToWorldUnitsPerSecond(float mph) {
    return mph * 5280.0f / 3600.0f / feetPerWorldUnit;
}

float commandRadiusForPitch(const PitchProfile& pitch) {
    switch (pitch.hotkey) {
        case 'F':
            return 0.11f;
        case 'T':
            return 0.14f;
        case 'P':
            return 0.18f;
        case 'S':
            return 0.21f;
        case 'C':
            return 0.24f;
        default:
            return 0.16f;
    }
}

Vector3 clampAimPoint(const Vector3& point) {
    return Vector3(
        std::clamp(point.x, -0.72f, 0.72f),
        std::clamp(point.y, strikeZoneCenter.y - 0.85f, strikeZoneCenter.y + 0.85f),
        plateZ
    );
}

Vector3 movementAccelerationForPitch(
    const PitchProfile& pitch,
    const PitchFlightVariation& variation,
    const Vector3& position,
    float pitchAge
) {
    float progress = std::clamp(
        (position.z - releasePoint.z) / (plateZ - releasePoint.z),
        0.0f,
        1.0f
    );
    float breakRamp = progress <= pitch.breakStartZ
        ? 0.0f
        : (progress - pitch.breakStartZ) / (1.0f - pitch.breakStartZ);
    breakRamp = breakRamp * breakRamp * (3.0f - 2.0f * breakRamp);

    Vector3 acceleration = Vector3(
        pitch.breakAcceleration.x * variation.breakScale.x,
        pitch.breakAcceleration.y * variation.breakScale.y,
        0.0f
    ) * breakRamp;

    float turbulenceRamp = progress * progress;
    Vector3 turbulenceForce(
        std::sin(pitchAge * 18.0f + variation.turbulencePhase),
        std::sin(pitchAge * 13.0f + variation.turbulencePhase * 0.7f),
        0.0f
    );

    return acceleration + turbulenceForce * variation.turbulenceStrength * turbulenceRamp;
}

Vector3 calculateLaunchVelocity(
    const PitchProfile& pitch,
    const Vector3& aimPoint,
    float pitchSpeedMph,
    const PitchFlightVariation& variation
) {
    float pitchSpeed = mphToWorldUnitsPerSecond(pitchSpeedMph);
    Vector3 actualReleasePoint = releasePoint + variation.releaseOffset;
    float distance = aimPoint.z - actualReleasePoint.z;
    float dragSlowdownEstimate = std::clamp(0.95f - pitch.dragCoefficient * pitch.airScale * 0.10f, 0.86f, 0.95f);
    float flightTime = distance / std::max(1.0f, pitchSpeed * dragSlowdownEstimate);
    float movementInfluence = std::max(0.0f, 1.0f - pitch.breakStartZ) * 0.24f;
    float estimatedAx = pitch.breakAcceleration.x * variation.breakScale.x * movementInfluence;
    float estimatedAy = -9.8f + pitch.breakAcceleration.y * variation.breakScale.y * movementInfluence;
    float desiredVx = (aimPoint.x - actualReleasePoint.x - 0.5f * estimatedAx * flightTime * flightTime) / flightTime;
    float desiredVy =
        (aimPoint.y - actualReleasePoint.y - 0.5f * estimatedAy * flightTime * flightTime) / flightTime +
        pitch.liftCompensation +
        variation.liftOffset;

    float maxSideVelocity = pitchSpeed * 0.16f;
    float minVerticalVelocity = pitchSpeed * std::tan(-4.0f * pi / 180.0f);
    float maxVerticalVelocity = pitchSpeed * std::tan(6.8f * pi / 180.0f);
    desiredVx = std::clamp(desiredVx, -maxSideVelocity, maxSideVelocity);
    desiredVy = std::clamp(desiredVy, minVerticalVelocity, maxVerticalVelocity);

    float forwardVelocitySquared = pitchSpeed * pitchSpeed - desiredVx * desiredVx - desiredVy * desiredVy;
    float desiredVz = std::sqrt(std::max(forwardVelocitySquared, pitchSpeed * pitchSpeed * 0.82f));
    return Vector3(desiredVx, desiredVy, desiredVz);
}

float rollPitchSpeed(
    const PitchProfile& pitch,
    float globalSpeedScale,
    std::mt19937& randomGenerator
) {
    std::uniform_real_distribution<float> speedOffset(-pitch.speedVarianceMph, pitch.speedVarianceMph);
    return std::max(1.0f, (pitch.baseSpeedMph + speedOffset(randomGenerator)) * globalSpeedScale);
}

PitchFlightVariation rollPitchVariation(
    const PitchProfile& pitch,
    std::mt19937& randomGenerator
) {
    float movementNoise = pitch.hotkey == 'F' ? 0.05f : 0.10f;
    float turbulence = pitch.hotkey == 'P' ? 0.24f : 0.18f;

    if (pitch.hotkey == 'C') {
        turbulence = 0.16f;
    }

    float commandRadius = commandRadiusForPitch(pitch);
    float commandAngle = randomRange(randomGenerator, 0.0f, pi * 2.0f);
    float commandDistance = std::sqrt(randomRange(randomGenerator, 0.0f, 1.0f)) * commandRadius;

    return PitchFlightVariation{
        Vector3(
            randomRange(randomGenerator, -0.045f, 0.045f),
            randomRange(randomGenerator, -0.035f, 0.035f),
            0.0f
        ),
        Vector3(
            std::cos(commandAngle) * commandDistance,
            std::sin(commandAngle) * commandDistance * 0.82f,
            0.0f
        ),
        Vector3(
            randomRange(randomGenerator, -0.22f, 0.22f),
            randomRange(randomGenerator, -0.04f, 0.04f),
            randomRange(randomGenerator, -0.12f, 0.08f)
        ),
        Vector3(
            randomRange(randomGenerator, 1.0f - movementNoise, 1.0f + movementNoise),
            randomRange(randomGenerator, 1.0f - movementNoise, 1.0f + movementNoise),
            1.0f
        ),
        randomRange(randomGenerator, 0.94f, 1.08f),
        randomRange(randomGenerator, -0.035f, 0.035f),
        randomRange(randomGenerator, 0.0f, pi * 2.0f),
        randomRange(randomGenerator, turbulence * 0.45f, turbulence)
    };
}

void launchPitch(
    Body3D& baseball,
    PhysicsWorld3D& world,
    const PitchProfile& pitch,
    const Vector3& aimPoint,
    std::vector<Vector3>& trail,
    float pitchSpeedMph,
    const PitchFlightVariation& variation
) {
    world = PhysicsWorld3D();
    world.setBounds(boundsMinimum, boundsMaximum);
    world.gravity = Vector3(0.0f, -9.8f, 0.0f);
    world.setAtmosphere(pitchAirDensity * variation.dragScale, variation.airVelocity);

    baseball = Body3D(releasePoint + variation.releaseOffset, 0.145f);
    baseball.setRadius(baseballRadius);
    baseball.restitution = 0.3f;
    baseball.dragCoefficient = pitch.dragCoefficient * variation.dragScale;
    baseball.airResistanceScale = pitch.airScale;
    Vector3 commandedAimPoint = clampAimPoint(aimPoint + variation.commandOffset);
    baseball.velocity = calculateLaunchVelocity(pitch, commandedAimPoint, pitchSpeedMph, variation);
    world.addBody(&baseball);

    trail.clear();
    trail.push_back(baseball.position);
}

bool freezePitchAtPlate(Body3D& baseball, std::vector<Vector3>& trail) {
    if (baseball.position.z < plateZ || baseball.velocity.z <= 0.0f) {
        return false;
    }

    baseball.position.z = plateZ;
    baseball.velocity = Vector3();
    baseball.acceleration = Vector3();

    if (trail.empty() || (baseball.position - trail.back()).magnitude() > 0.01f) {
        trail.push_back(baseball.position);
    }

    return true;
}

sf::Keyboard::Key pitchKeyForProfile(const PitchProfile& pitch) {
    switch (pitch.hotkey) {
        case 'F':
            return sf::Keyboard::Key::F;
        case 'C':
            return sf::Keyboard::Key::C;
        case 'P':
            return sf::Keyboard::Key::P;
        case 'T':
            return sf::Keyboard::Key::T;
        case 'S':
            return sf::Keyboard::Key::S;
        default:
            return sf::Keyboard::Key::F;
    }
}

}

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "Pitching Simulator | F/P/C/T/S select | R throw"
    );
    window.setFramerateLimit(60);

    bool antiAliasingEnabled = true;
    Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);
    DemoFpsCounter fpsCounter("Pitching Simulator | R throws | AA on");

    sf::Font font;
    bool fontLoaded = loadUiFont(font);

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    PitchCameraMode cameraMode = PitchCameraMode::Overview;
    applyCameraMode(camera, cameraMode);

    Mesh3D baseballMesh = makeBaseballMesh();
    std::vector<SeamPoint> seamA = makeSeamLoop(false);
    std::vector<SeamPoint> seamB = makeSeamLoop(true);
    RasterMeshRenderCache renderCache;

    std::array<PitchProfile, 5> pitches = makePitchProfiles();
    int selectedPitch = 0;
    int activePitch = selectedPitch;
    Vector3 aimPoint = strikeZoneCenter;
    PhysicsWorld3D world;
    Body3D baseball;
    std::vector<Vector3> trail;
    std::vector<PitchResult> pitchResults;
    std::mt19937 randomGenerator(std::random_device{}());
    float globalSpeedScale = 1.0f;
    float currentPitchSpeedMph = rollPitchSpeed(pitches[selectedPitch], globalSpeedScale, randomGenerator);
    PitchFlightVariation currentVariation = rollPitchVariation(pitches[selectedPitch], randomGenerator);
    launchPitch(baseball, world, pitches[selectedPitch], aimPoint, trail, currentPitchSpeedMph, currentVariation);

    sf::Clock frameClock;
    float accumulator = 0.0f;
    float pitchAge = 0.0f;
    float spinX = 0.0f;
    float spinY = 0.0f;
    float spinZ = 0.0f;
    bool paused = false;
    bool draggingSpeedSlider = false;
    bool pitchFrozen = true;
    std::string latestResult = "Ready";

    auto relaunchCurrentPitch = [&]() {
        activePitch = selectedPitch;
        currentPitchSpeedMph = rollPitchSpeed(pitches[selectedPitch], globalSpeedScale, randomGenerator);
        currentVariation = rollPitchVariation(pitches[selectedPitch], randomGenerator);
        launchPitch(baseball, world, pitches[selectedPitch], aimPoint, trail, currentPitchSpeedMph, currentVariation);
        accumulator = 0.0f;
        pitchAge = 0.0f;
        spinX = 0.0f;
        spinY = 0.0f;
        spinZ = 0.0f;
        pitchFrozen = false;
    };

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
                            ? "Pitching Simulator | R throws | AA on"
                            : "Pitching Simulator | R throws | AA off"
                    );
                }

                if (key->code == sf::Keyboard::Key::Space) {
                    paused = !paused;
                }

                if (key->code == sf::Keyboard::Key::R) {
                    relaunchCurrentPitch();
                }

                if (key->code == sf::Keyboard::Key::LBracket) {
                    globalSpeedScale = std::clamp(globalSpeedScale - 0.05f, 0.75f, 1.25f);
                }

                if (key->code == sf::Keyboard::Key::RBracket) {
                    globalSpeedScale = std::clamp(globalSpeedScale + 0.05f, 0.75f, 1.25f);
                }

                if (key->code == sf::Keyboard::Key::Num1) {
                    cameraMode = PitchCameraMode::Overview;
                    applyCameraMode(camera, cameraMode);
                }

                if (key->code == sf::Keyboard::Key::Num2) {
                    cameraMode = PitchCameraMode::Catcher;
                    applyCameraMode(camera, cameraMode);
                }

                if (key->code == sf::Keyboard::Key::Num3) {
                    cameraMode = PitchCameraMode::Pitcher;
                    applyCameraMode(camera, cameraMode);
                }

                if (key->code == sf::Keyboard::Key::Left) {
                    aimPoint.x = std::clamp(
                        aimPoint.x + horizontalAimDeltaForCamera(cameraMode, -1.0f),
                        -strikeZoneHalfWidth * 1.5f,
                        strikeZoneHalfWidth * 1.5f
                    );
                }

                if (key->code == sf::Keyboard::Key::Right) {
                    aimPoint.x = std::clamp(
                        aimPoint.x + horizontalAimDeltaForCamera(cameraMode, 1.0f),
                        -strikeZoneHalfWidth * 1.5f,
                        strikeZoneHalfWidth * 1.5f
                    );
                }

                if (key->code == sf::Keyboard::Key::Up) {
                    aimPoint.y = std::clamp(aimPoint.y + 0.1f, strikeZoneCenter.y - strikeZoneHalfHeight * 1.35f, strikeZoneCenter.y + strikeZoneHalfHeight * 1.35f);
                }

                if (key->code == sf::Keyboard::Key::Down) {
                    aimPoint.y = std::clamp(aimPoint.y - 0.1f, strikeZoneCenter.y - strikeZoneHalfHeight * 1.35f, strikeZoneCenter.y + strikeZoneHalfHeight * 1.35f);
                }

                for (int i = 0; i < pitches.size(); i++) {
                    if (key->code == pitchKeyForProfile(pitches[i])) {
                        selectedPitch = i;
                    }
                }
            }

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mousePosition(
                        static_cast<float>(mouse->position.x),
                        static_cast<float>(mouse->position.y)
                    );
                    if (speedSliderTrack.contains(mousePosition)) {
                        draggingSpeedSlider = true;
                        globalSpeedScale = speedScaleFromSliderX(mousePosition.x);
                    }
                }
            }

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    draggingSpeedSlider = false;
                }
            }

            if (const auto* move = event->getIf<sf::Event::MouseMoved>()) {
                if (draggingSpeedSlider) {
                    globalSpeedScale = speedScaleFromSliderX(static_cast<float>(move->position.x));
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
        if (!paused && !pitchFrozen) {
            accumulator += dt;
            while (accumulator >= fixedStep) {
                const PitchProfile& pitch = pitches[activePitch];
                Vector3 breakAcceleration =
                    movementAccelerationForPitch(pitch, currentVariation, baseball.position, pitchAge);
                baseball.applyForce(breakAcceleration * baseball.mass);
                world.step(fixedStep);
                pitchFrozen = freezePitchAtPlate(baseball, trail);
                if (pitchFrozen) {
                    PitchResult result = classifyPitchResult(baseball.position);
                    latestResult = result.label;
                    pitchResults.push_back(result);
                    if (pitchResults.size() > 5) {
                        pitchResults.erase(pitchResults.begin());
                    }
                }
                pitchAge += fixedStep;
                accumulator -= fixedStep;

                if (pitchFrozen) {
                    break;
                }

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
        drawHomePlate(window, overlayCamera);
        drawCatcherSilhouette(window, overlayCamera);
        drawCatcherTarget(window, overlayCamera, aimPoint, pitches[selectedPitch]);
        drawProjectedPolyline(window, overlayCamera, trail, pitches[activePitch].color);
        drawStrikeZone(window, overlayCamera, aimPoint, pitches[selectedPitch]);
        drawPitchResultHistory(window, overlayCamera, pitchResults);
        drawBaseballSeams(window, overlayCamera, baseballTransform, seamA, seamB);

        if (fontLoaded) {
            sf::RectangleShape panel(sf::Vector2f(430.0f, 132.0f));
            panel.setPosition(sf::Vector2f(18.0f, 18.0f));
            panel.setFillColor(sf::Color(5, 8, 14, 180));
            panel.setOutlineThickness(1.0f);
            panel.setOutlineColor(sf::Color(85, 185, 190, 115));
            window.draw(panel);

            sf::RectangleShape sliderTrack(speedSliderTrack.size);
            sliderTrack.setPosition(speedSliderTrack.position);
            sliderTrack.setFillColor(sf::Color(45, 80, 88, 210));
            window.draw(sliderTrack);

            float sliderX = sliderXFromSpeedScale(globalSpeedScale);
            sf::RectangleShape sliderFill(sf::Vector2f(sliderX - speedSliderTrack.position.x, speedSliderTrack.size.y));
            sliderFill.setPosition(speedSliderTrack.position);
            sliderFill.setFillColor(sf::Color(120, 225, 220, 210));
            window.draw(sliderFill);

            sf::CircleShape sliderKnob(7.0f);
            sliderKnob.setOrigin(sf::Vector2f(7.0f, 7.0f));
            sliderKnob.setPosition(sf::Vector2f(sliderX, speedSliderTrack.position.y + speedSliderTrack.size.y * 0.5f));
            sliderKnob.setFillColor(sf::Color(235, 245, 220));
            window.draw(sliderKnob);

            std::ostringstream aimLabel;
            aimLabel << std::fixed << std::setprecision(2) << "Aim " << aimPoint.x << ", " << aimPoint.y;
            std::ostringstream speedLabel;
            speedLabel << std::fixed << std::setprecision(1)
                << "Next " << currentPitchSpeedMph << " mph  x" << globalSpeedScale;

            drawText(window, font, pitches[selectedPitch].name, 17, sf::Vector2f(34.0f, 28.0f), pitches[selectedPitch].color);
            drawText(window, font, "F 4S  P SPL  C CB  T CUT  S SL", 12, sf::Vector2f(34.0f, 54.0f), sf::Color(180, 215, 220));
            drawText(window, font, "Drag speed for next throw", 11, sf::Vector2f(34.0f, 70.0f), sf::Color(120, 175, 185));
            drawText(window, font, "R throw | Space pause | arrows aim | 1/2/3 camera", 12, sf::Vector2f(34.0f, 112.0f), sf::Color(155, 195, 200));
            drawText(window, font, aimLabel.str(), 12, sf::Vector2f(286.0f, 30.0f), sf::Color(135, 195, 200));
            drawText(window, font, speedLabel.str(), 12, sf::Vector2f(238.0f, 100.0f), sf::Color(175, 215, 180));
            drawText(window, font, cameraModeName(cameraMode), 11, sf::Vector2f(348.0f, 54.0f), sf::Color(130, 190, 205));
            drawText(window, font, latestResult, 13, sf::Vector2f(338.0f, 72.0f), sf::Color(225, 235, 205));
        }

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
