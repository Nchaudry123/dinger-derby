#include <SFML/Graphics.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/Window/WindowEnums.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "DemoFpsCounter.h"
#include "RasterDemo3D.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "physics/AirResistance3D.h"
#include "physics/Body3D.h"
#include "physics/PhysicsWorld3D.h"
#include "rendering/Camera3D.h"
#include "rendering/FrameBuffer.h"
#include "rendering/Mesh3D.h"
#include "rendering/Rasterizer3D.h"
#include "rendering/BaseballVisual3D.h"
#include "rendering/BaseballAnims.h"
#include "rendering/CharacterModel3D.h"
#include "rendering/GltfLoader.h"
#include "rendering/GlRenderer.h"
#include "rendering/SkeletonAnimator.h"
#include "rendering/SkinnedModel3D.h"

namespace {

constexpr float pi = 3.1415926535f;
constexpr float fixedStep = 1.0f / 180.0f;

float smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// ~3" baseball diameter in world units (1 unit ≈ 2 feet).
constexpr float baseballRadius = 0.065f;
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
const sf::FloatRect speedSliderTrack(sf::Vector2f(34.0f, 118.0f), sf::Vector2f(300.0f, 8.0f));

// Pitch identity is defined by speed + spin. Movement comes from Magnus
// (ω × v) and spin-aware drag in AirResistance3D — not baked break forces.
// World axes: +Z plate, +Y up, +X first base. RHP glove side = −X.
struct PitchProfile {
    char hotkey;
    std::string name;
    float baseSpeedMph;
    float speedVarianceMph;
    float spinRpm;          // total spin rate
    Vector3 spinAxis;       // unit world axis of ω at release
    float spinEfficiency;   // 0..1 active-spin fraction (gyro kills movement)
    float magnusScale;      // family-tuned Magnus gain
    float dragCoefficient;
    float airScale;
    sf::Color color;
};

struct PitchFlightVariation {
    Vector3 releaseOffset;
    Vector3 commandOffset;
    Vector3 airVelocity;
    float spinRpmScale = 1.0f;
    float spinAxisJitter = 0.0f; // small random tilt of spin axis (radians)
    float dragScale = 1.0f;
    float liftOffset = 0.0f; // residual aim loft noise only
    float turbulencePhase = 0.0f;
    float turbulenceStrength = 0.0f;
};

struct PitchResult {
    Vector3 platePosition;
    std::string label;
    sf::Color color;
    bool isStrike = false;
};

struct CountState {
    int balls = 0;
    int strikes = 0;
    int pitchNumber = 0;
    int walks = 0;
    int strikeouts = 0;
    std::string lastOutcome;
};

enum class PitchPhase {
    Ready,
    Flying,
    Settled
};

enum class PitchCameraMode {
    Overview,
    Catcher,
    Pitcher,
    Delivery // side profile set → open with hips through release
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
        case PitchCameraMode::Delivery:
            return "Delivery";
        default:
            return "Overview";
    }
}

// Dynamic cam for CharacterModel3D throw: start 1B-side profile (sideways set),
// open toward plate as delivery progresses, optionally glance at the ball.
void applyDeliveryCamera(
    Camera3D& camera,
    float deliveryT01,
    const Vector3& ballPos,
    bool trackBall
) {
    float u = smoothstep(0.0f, 1.0f, std::clamp(deliveryT01, 0.0f, 1.0f));
    // Set: +X side of mound — profile of RHP closed to plate (+Z).
    Vector3 setPos(2.55f, 1.52f, moundZ + 0.15f);
    Vector3 setTarget(0.05f, 1.20f, moundZ + 0.55f);
    // Release: over throwing shoulder, looking down the lane.
    Vector3 openPos(1.05f, 1.72f, moundZ - 2.40f);
    Vector3 openTarget(0.0f, 1.25f, plateZ * 0.50f);
    Vector3 pos = setPos + (openPos - setPos) * u;
    Vector3 target = setTarget + (openTarget - setTarget) * u;
    if (trackBall) {
        target = target * 0.55f + ballPos * 0.45f;
        target.y = std::clamp(target.y, 0.6f, 2.4f);
    }
    lookAt(camera, pos, target);
    camera.fieldOfView = 840.0f - u * 60.0f;
}

void applyCameraMode(Camera3D& camera, PitchCameraMode mode) {
    switch (mode) {
        case PitchCameraMode::Overview:
            // Slight 1B bias so sideways set reads as a silhouette, not face-on.
            lookAt(camera, Vector3(1.35f, 1.72f, -3.15f), Vector3(0.0f, 1.25f, plateZ * 0.55f));
            camera.fieldOfView = 1280.0f;
            break;
        case PitchCameraMode::Catcher:
            // POV from the catcher's crouch spot (same place as the model), looking at the pitcher.
            // Catcher mesh is not drawn in this mode so the body never blocks the plate.
            lookAt(
                camera,
                Vector3(0.0f, 1.28f, plateZ + 0.95f),
                Vector3(0.0f, 1.55f, moundZ + 1.2f)
            );
            camera.fieldOfView = 700.0f;
            break;
        case PitchCameraMode::Pitcher:
            // Over the throwing shoulder, behind the mound, looking toward home.
            lookAt(
                camera,
                Vector3(1.25f, 1.80f, -2.70f),
                Vector3(0.0f, 1.20f, plateZ * 0.55f)
            );
            camera.fieldOfView = 780.0f;
            break;
        case PitchCameraMode::Delivery:
            applyDeliveryCamera(camera, 0.0f, Vector3(0.0f, 1.2f, 0.3f), false);
            break;
    }
}

float horizontalAimDeltaForCamera(PitchCameraMode mode, float screenDirection) {
    float worldDirection = mode == PitchCameraMode::Catcher
        ? -screenDirection
        : screenDirection;
    return worldDirection * 0.1f;
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
    const std::vector<SeamPoint3D>& seamA,
    const std::vector<SeamPoint3D>& seamB
) {
    auto drawSeam = [&](const std::vector<SeamPoint3D>& seam) {
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
                1.35f,
                sf::Color(170, 32, 38, 225)
            );
        }

        for (int i = 0; i < seam.size(); i += 9) {
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
    const Vector3& /*aimPoint*/,
    const PitchProfile& /*pitch*/
) {
    // Zone outline only — aim reticle is drawn once in drawCatcherTarget.
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
            2.4f,
            sf::Color(115, 230, 235, 200)
        );
    }
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
    // Single small reticle at the aim (half-gap cross + tiny center).
    Vector3 target = Vector3(aimPoint.x, aimPoint.y, plateZ + 0.06f);
    sf::Color arm = pitch.color;
    arm.a = 200;
    constexpr float armLen = 0.07f;
    constexpr float gap = 0.028f;
    constexpr float thickness = 1.6f;

    drawThickProjectedLine(
        window, camera,
        target + Vector3(-armLen - gap, 0.0f, 0.0f),
        target + Vector3(-gap, 0.0f, 0.0f),
        thickness, arm
    );
    drawThickProjectedLine(
        window, camera,
        target + Vector3(gap, 0.0f, 0.0f),
        target + Vector3(armLen + gap, 0.0f, 0.0f),
        thickness, arm
    );
    drawThickProjectedLine(
        window, camera,
        target + Vector3(0.0f, -armLen - gap, 0.0f),
        target + Vector3(0.0f, -gap, 0.0f),
        thickness, arm
    );
    drawThickProjectedLine(
        window, camera,
        target + Vector3(0.0f, gap, 0.0f),
        target + Vector3(0.0f, armLen + gap, 0.0f),
        thickness, arm
    );
    drawProjectedDot(window, camera, target, 2.5f, sf::Color(arm.r, arm.g, arm.b, 220));
}



PitchResult classifyPitchResult(const Vector3& platePosition) {
    bool inHorizontalZone = std::abs(platePosition.x - strikeZoneCenter.x) <= strikeZoneHalfWidth;
    bool inVerticalZone = std::abs(platePosition.y - strikeZoneCenter.y) <= strikeZoneHalfHeight;

    if (inHorizontalZone && inVerticalZone) {
        return PitchResult{platePosition, "Strike", sf::Color(150, 245, 170), true};
    }

    if (platePosition.y > strikeZoneCenter.y + strikeZoneHalfHeight) {
        return PitchResult{platePosition, "Ball High", sf::Color(245, 210, 120), false};
    }

    if (platePosition.y < strikeZoneCenter.y - strikeZoneHalfHeight) {
        return PitchResult{platePosition, "Ball Low", sf::Color(245, 180, 110), false};
    }

    if (platePosition.x < strikeZoneCenter.x - strikeZoneHalfWidth) {
        return PitchResult{platePosition, "Ball Inside", sf::Color(130, 205, 245), false};
    }

    return PitchResult{platePosition, "Ball Outside", sf::Color(130, 205, 245), false};
}

std::string applyCount(CountState& count, const PitchResult& result) {
    count.pitchNumber += 1;
    count.lastOutcome.clear();

    if (result.isStrike) {
        count.strikes = std::min(count.strikes + 1, 3);
        if (count.strikes >= 3) {
            count.strikeouts += 1;
            count.balls = 0;
            count.strikes = 0;
            count.lastOutcome = "Strikeout";
            return "Strikeout";
        }
    } else {
        count.balls = std::min(count.balls + 1, 4);
        if (count.balls >= 4) {
            count.walks += 1;
            count.balls = 0;
            count.strikes = 0;
            count.lastOutcome = "Walk";
            return "Walk";
        }
    }

    return result.label;
}

// Convert rpm + unit axis → world angular velocity (rad/s).
Vector3 angularVelocityFromProfile(const PitchProfile& pitch, float rpmScale = 1.0f) {
    float axisMag = pitch.spinAxis.magnitude();
    Vector3 axis = axisMag > 1e-6f ? pitch.spinAxis * (1.0f / axisMag) : Vector3(-1.0f, 0.0f, 0.0f);
    float omega = pitch.spinRpm * rpmScale * (2.0f * pi / 60.0f);
    return axis * omega;
}

// Tilt a unit axis by a small random angle for pitch-to-pitch seam variance.
Vector3 jitterSpinAxis(const Vector3& axis, float jitterRad, std::mt19937& rng) {
    float mag = axis.magnitude();
    Vector3 a = mag > 1e-6f ? axis * (1.0f / mag) : Vector3(-1.0f, 0.0f, 0.0f);
    if (jitterRad < 1e-5f) {
        return a;
    }
    // Build orthonormal basis around a, rotate by small angles.
    Vector3 ref = std::fabs(a.y) < 0.9f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 u = a.cross(ref);
    float um = u.magnitude();
    if (um < 1e-6f) {
        return a;
    }
    u = u * (1.0f / um);
    Vector3 v = a.cross(u);
    float t1 = randomRange(rng, -jitterRad, jitterRad);
    float t2 = randomRange(rng, -jitterRad, jitterRad);
    Vector3 out = a + u * t1 + v * t2;
    float om = out.magnitude();
    return om > 1e-6f ? out * (1.0f / om) : a;
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
    const sf::Color dirt(168, 128, 82, 95);
    const sf::Color laneColor(70, 145, 145, 78);
    const sf::Color grassLine(55, 120, 95, 55);

    for (int ring = 1; ring <= 4; ring++) {
        float radius = 0.18f * static_cast<float>(ring);
        const int segments = 20;
        for (int i = 0; i < segments; i++) {
            float a0 = static_cast<float>(i) / segments * pi * 2.0f;
            float a1 = static_cast<float>(i + 1) / segments * pi * 2.0f;
            drawThickProjectedLine(
                window,
                camera,
                Vector3(std::cos(a0) * radius, 0.01f, moundZ + std::sin(a0) * radius * 0.55f),
                Vector3(std::cos(a1) * radius, 0.01f, moundZ + std::sin(a1) * radius * 0.55f),
                2.2f,
                dirt
            );
        }
    }

    drawThickProjectedLine(
        window,
        camera,
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, plateZ + 0.4f),
        1.4f,
        laneColor
    );

    for (int z = 2; z <= static_cast<int>(plateZ); z += 2) {
        float width = 0.55f + static_cast<float>(z) * 0.01f;
        drawThickProjectedLine(
            window,
            camera,
            Vector3(-width, 0.0f, static_cast<float>(z)),
            Vector3(width, 0.0f, static_cast<float>(z)),
            1.0f,
            z % 4 == 0 ? grassLine : laneColor
        );
    }

    drawThickProjectedLine(
        window,
        camera,
        Vector3(-0.62f, 0.02f, moundZ),
        Vector3(0.62f, 0.02f, moundZ),
        5.0f,
        sf::Color(205, 180, 130, 200)
    );

    drawThickProjectedLine(
        window,
        camera,
        Vector3(-strikeZoneHalfWidth - 0.55f, 0.02f, plateZ - 0.35f),
        Vector3(-strikeZoneHalfWidth - 0.12f, 0.02f, plateZ - 0.35f),
        1.5f,
        sf::Color(210, 205, 180, 100)
    );
    drawThickProjectedLine(
        window,
        camera,
        Vector3(strikeZoneHalfWidth + 0.12f, 0.02f, plateZ - 0.35f),
        Vector3(strikeZoneHalfWidth + 0.55f, 0.02f, plateZ - 0.35f),
        1.5f,
        sf::Color(210, 205, 180, 100)
    );
}

void drawBallShadow(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const Vector3& ballPosition,
    float radius
) {
    Vector3 shadow(ballPosition.x, 0.03f, ballPosition.z);
    float height = std::max(0.0f, ballPosition.y);
    float scale = std::clamp(1.15f - height * 0.18f, 0.35f, 1.1f);
    float alpha = std::clamp(150.0f - height * 28.0f, 40.0f, 150.0f);
    drawProjectedDot(
        window,
        camera,
        shadow,
        radius * 18.0f * scale,
        sf::Color(8, 12, 18, static_cast<std::uint8_t>(alpha))
    );
}

std::array<PitchProfile, 5> makePitchProfiles() {
    // Spin axes (RHP): ω × v drives movement.
    //   backspin ω≈(−X) → lift (+Y)
    //   topspin  ω≈(+X) → drop (−Y)
    //   glove sidespin ω≈(−Y) → break toward −X (3B / glove)
    // RPM / efficiency from typical MLB tracking ranges.
    return {{
        // Four-seam: high backspin ride, little side.
        PitchProfile{
            'F', "Four-Seam", 96.1f, 2.0f,
            2450.0f, Vector3(-1.0f, 0.06f, 0.05f), 0.98f, 1.15f,
            0.30f, 0.88f, sf::Color(245, 235, 180)
        },
        // Splitter: low spin, mostly gyro → dies / tumbles down.
        PitchProfile{
            'P', "Splitter", 91.5f, 1.8f,
            1300.0f, Vector3(-0.25f, 0.20f, 0.92f), 0.40f, 0.55f,
            0.42f, 1.05f, sf::Color(190, 245, 160)
        },
        // Curve: topspin + glove sidespin (12–6 / 1–7 shape).
        PitchProfile{
            'C', "Curve", 77.1f, 2.2f,
            2800.0f, Vector3(0.78f, -0.55f, -0.18f), 0.92f, 1.25f,
            0.36f, 0.95f, sf::Color(245, 145, 90)
        },
        // Cutter: mild glove-side cut, some backspin.
        PitchProfile{
            'T', "Cutter", 91.4f, 1.9f,
            2500.0f, Vector3(-0.55f, -0.72f, 0.20f), 0.90f, 1.05f,
            0.32f, 0.90f, sf::Color(145, 220, 245)
        },
        // Slider: stronger glove sidespin + some topspin / gyro.
        PitchProfile{
            'S', "Slider", 87.2f, 1.7f,
            2650.0f, Vector3(0.30f, -0.82f, 0.40f), 0.88f, 1.20f,
            0.34f, 0.92f, sf::Color(190, 160, 245)
        }
    }};
}

float mphToWorldUnitsPerSecond(float mph) {
    return mph * 5280.0f / 3600.0f / feetPerWorldUnit;
}

// Command scatter at the plate (world units). Four-seam is the "accuracy" pitch.
float commandRadiusForPitch(const PitchProfile& pitch) {
    switch (pitch.hotkey) {
        case 'F':
            return 0.045f; // ~1.1" radius — sticks near the target
        case 'T':
            return 0.085f;
        case 'P':
            return 0.12f;
        case 'S':
            return 0.13f;
        case 'C':
            return 0.15f;
        default:
            return 0.10f;
    }
}

Vector3 clampAimPoint(const Vector3& point) {
    return Vector3(
        std::clamp(point.x, -0.72f, 0.72f),
        std::clamp(point.y, strikeZoneCenter.y - 0.85f, strikeZoneCenter.y + 0.85f),
        plateZ
    );
}

// Tiny residual wake noise only — real movement is Magnus on the body.
Vector3 residualTurbulence(
    const PitchFlightVariation& variation,
    const Vector3& position,
    float pitchAge
) {
    float progress = std::clamp(
        (position.z - releasePoint.z) / (plateZ - releasePoint.z),
        0.0f,
        1.0f
    );
    float ramp = progress * progress;
    return Vector3(
        std::sin(pitchAge * 18.0f + variation.turbulencePhase),
        std::sin(pitchAge * 13.0f + variation.turbulencePhase * 0.7f),
        0.0f
    ) * (variation.turbulenceStrength * ramp * 0.35f);
}

// Clamp lateral components and rebuild vz so |v| ≈ pitchSpeed.
Vector3 assembleLaunchVelocity(float pitchSpeed, float vx, float vy) {
    float maxSideVelocity = pitchSpeed * 0.22f;
    float minVerticalVelocity = pitchSpeed * std::tan(-12.0f * pi / 180.0f);
    float maxVerticalVelocity = pitchSpeed * std::tan(24.0f * pi / 180.0f);
    vx = std::clamp(vx, -maxSideVelocity, maxSideVelocity);
    vy = std::clamp(vy, minVerticalVelocity, maxVerticalVelocity);

    float lateralSquared = vx * vx + vy * vy;
    float maxLat = pitchSpeed * 0.55f;
    if (lateralSquared > maxLat * maxLat && lateralSquared > 1e-8f) {
        float scale = maxLat / std::sqrt(lateralSquared);
        vx *= scale;
        vy *= scale;
        lateralSquared = vx * vx + vy * vy;
    }
    float vz = std::sqrt(std::max(pitchSpeed * pitchSpeed - lateralSquared, pitchSpeed * pitchSpeed * 0.55f));
    return Vector3(vx, vy, vz);
}

// Deterministic plate crossing with the same physics as live flight (no wake noise).
bool simulatePlateCrossing(
    const PitchProfile& pitch,
    const Vector3& startPosition,
    const Vector3& initialVelocity,
    const Vector3& angularVelocity,
    float airDensity,
    const Vector3& airVelocity,
    float dragScale,
    Vector3& outPlate
) {
    PhysicsWorld3D world;
    world.gravity = Vector3(0.0f, -9.8f, 0.0f);
    world.setAtmosphere(airDensity, airVelocity);
    world.airResistanceEnabled = true;
    world.setBounds(boundsMinimum, boundsMaximum);

    Body3D ball(startPosition, 0.145f);
    ball.setRadius(baseballRadius);
    ball.restitution = 0.3f;
    ball.dragCoefficient = pitch.dragCoefficient * dragScale;
    ball.airResistanceScale = pitch.airScale;
    ball.spinEfficiency = pitch.spinEfficiency;
    ball.magnusScale = pitch.magnusScale;
    ball.angularVelocity = angularVelocity;
    ball.velocity = initialVelocity;
    world.addBody(&ball);

    Vector3 previous = ball.position;
    for (int step = 0; step < 900; step++) {
        previous = ball.position;
        world.step(fixedStep);
        if (ball.position.z >= plateZ && ball.velocity.z > 0.0f) {
            float seg = ball.position.z - previous.z;
            float t = seg <= 1e-6f ? 1.0f : (plateZ - previous.z) / seg;
            t = std::clamp(t, 0.0f, 1.0f);
            outPlate = previous + (ball.position - previous) * t;
            outPlate.z = plateZ;
            return true;
        }
        if (ball.position.y < -1.0f || ball.position.z > plateZ + 8.0f) {
            break;
        }
    }
    outPlate = ball.position;
    return false;
}

// Iterative "shooting" aim: correct vx/vy until the simulated plate hit matches
// the target. Uses the exact release spin that will be on the live ball so
// four-seam ride / curve drop are compensated correctly.
Vector3 calculateLaunchVelocity(
    const PitchProfile& pitch,
    const Vector3& aimPoint,
    float pitchSpeedMph,
    const PitchFlightVariation& variation,
    const Vector3& startPosition,
    const Vector3& releaseAngularVelocity
) {
    float pitchSpeed = mphToWorldUnitsPerSecond(pitchSpeedMph);
    float distance = std::max(1.0f, aimPoint.z - startPosition.z);
    float dragSlowdownEstimate = std::clamp(
        0.94f - pitch.dragCoefficient * pitch.airScale * 0.12f,
        0.84f,
        0.96f
    );
    float flightTime = distance / std::max(1.0f, pitchSpeed * dragSlowdownEstimate);

    // Initial ballistic guess (gravity only) — iteration absorbs Magnus/drag.
    float vx = (aimPoint.x - startPosition.x) / flightTime;
    float vy =
        (aimPoint.y - startPosition.y + 0.5f * 9.8f * flightTime * flightTime) / flightTime +
        variation.liftOffset;
    Vector3 velocity = assembleLaunchVelocity(pitchSpeed, vx, vy);

    const float airDensity = pitchAirDensity * variation.dragScale;
    const float gain = 0.92f; // under-relax for stability

    for (int iter = 0; iter < 8; iter++) {
        Vector3 plateHit;
        bool ok = simulatePlateCrossing(
            pitch,
            startPosition,
            velocity,
            releaseAngularVelocity,
            airDensity,
            variation.airVelocity,
            variation.dragScale,
            plateHit
        );
        if (!ok) {
            // Fell short / went wild — add loft and retry.
            velocity = assembleLaunchVelocity(pitchSpeed, velocity.x, velocity.y + 1.2f);
            continue;
        }

        float errX = aimPoint.x - plateHit.x;
        float errY = aimPoint.y - plateHit.y;
        if (errX * errX + errY * errY < 0.0004f) { // ~0.02 m / ~0.8"
            break;
        }

        // Convert plate error into velocity correction using measured flight time.
        float measuredT = distance / std::max(1.0f, velocity.z * dragSlowdownEstimate);
        measuredT = std::clamp(measuredT, 0.25f, 1.2f);
        velocity = assembleLaunchVelocity(
            pitchSpeed,
            velocity.x + errX / measuredT * gain,
            velocity.y + errY / measuredT * gain
        );
    }

    return velocity;
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
    // Fastball: tight RPM / axis / command so the ball tracks the aim point.
    const bool isFastball = pitch.hotkey == 'F';
    float rpmNoise = isFastball ? 0.015f : 0.07f;
    float turbulence = pitch.hotkey == 'P' ? 0.12f : (isFastball ? 0.03f : 0.08f);
    if (pitch.hotkey == 'C') {
        turbulence = 0.06f;
    }

    float commandRadius = commandRadiusForPitch(pitch);
    float commandAngle = randomRange(randomGenerator, 0.0f, pi * 2.0f);
    float commandDistance = std::sqrt(randomRange(randomGenerator, 0.0f, 1.0f)) * commandRadius;

    return PitchFlightVariation{
        Vector3(
            randomRange(randomGenerator, isFastball ? -0.015f : -0.04f, isFastball ? 0.015f : 0.04f),
            randomRange(randomGenerator, isFastball ? -0.012f : -0.03f, isFastball ? 0.012f : 0.03f),
            0.0f
        ),
        Vector3(
            std::cos(commandAngle) * commandDistance,
            std::sin(commandAngle) * commandDistance * 0.82f,
            0.0f
        ),
        Vector3(
            randomRange(randomGenerator, isFastball ? -0.06f : -0.16f, isFastball ? 0.06f : 0.16f),
            randomRange(randomGenerator, -0.02f, 0.02f),
            randomRange(randomGenerator, -0.06f, 0.04f)
        ),
        randomRange(randomGenerator, 1.0f - rpmNoise, 1.0f + rpmNoise),
        randomRange(randomGenerator, isFastball ? 0.005f : 0.02f, isFastball ? 0.02f : 0.09f),
        randomRange(randomGenerator, isFastball ? 0.98f : 0.94f, isFastball ? 1.02f : 1.08f),
        randomRange(randomGenerator, isFastball ? -0.008f : -0.02f, isFastball ? 0.008f : 0.02f),
        randomRange(randomGenerator, 0.0f, pi * 2.0f),
        randomRange(randomGenerator, turbulence * 0.3f, turbulence)
    };
}

void resetPitchOnMound(
    Body3D& baseball,
    PhysicsWorld3D& world,
    const PitchProfile& pitch,
    std::vector<Vector3>& trail
) {
    world = PhysicsWorld3D();
    world.setBounds(boundsMinimum, boundsMaximum);
    world.gravity = Vector3(0.0f, -9.8f, 0.0f);
    world.setAtmosphere(pitchAirDensity);
    world.airResistanceEnabled = false;

    baseball = Body3D(releasePoint, 0.145f);
    baseball.setRadius(baseballRadius);
    baseball.restitution = 0.3f;
    baseball.dragCoefficient = pitch.dragCoefficient;
    baseball.airResistanceScale = pitch.airScale;
    baseball.velocity = Vector3();
    baseball.angularVelocity = Vector3();
    baseball.spinEfficiency = pitch.spinEfficiency;
    baseball.magnusScale = pitch.magnusScale;
    world.addBody(&baseball);

    trail.clear();
    trail.push_back(baseball.position);
}

void launchPitch(
    Body3D& baseball,
    PhysicsWorld3D& world,
    const PitchProfile& pitch,
    const Vector3& aimPoint,
    std::vector<Vector3>& trail,
    float pitchSpeedMph,
    const PitchFlightVariation& variation,
    const Vector3& startPosition,
    std::mt19937& randomGenerator
) {
    world = PhysicsWorld3D();
    world.setBounds(boundsMinimum, boundsMaximum);
    world.gravity = Vector3(0.0f, -9.8f, 0.0f);
    world.setAtmosphere(pitchAirDensity * variation.dragScale, variation.airVelocity);
    world.airResistanceEnabled = true; // drag + Magnus from spin

    baseball = Body3D(startPosition, 0.145f);
    baseball.setRadius(baseballRadius);
    baseball.restitution = 0.3f;
    baseball.dragCoefficient = pitch.dragCoefficient * variation.dragScale;
    baseball.airResistanceScale = pitch.airScale;
    baseball.spinEfficiency = pitch.spinEfficiency;
    baseball.magnusScale = pitch.magnusScale;

    // Finalize spin first, then aim with THAT exact ω so Magnus is compensated.
    Vector3 axis = jitterSpinAxis(pitch.spinAxis, variation.spinAxisJitter, randomGenerator);
    float omega = pitch.spinRpm * variation.spinRpmScale * (2.0f * pi / 60.0f);
    baseball.angularVelocity = axis * omega;

    Vector3 commandedAimPoint = clampAimPoint(aimPoint + variation.commandOffset);
    baseball.velocity = calculateLaunchVelocity(
        pitch,
        commandedAimPoint,
        pitchSpeedMph,
        variation,
        startPosition,
        baseball.angularVelocity
    );
    world.addBody(&baseball);

    trail.clear();
    trail.push_back(baseball.position);
}

// Pitcher root transform in the scene (must match draw).
Matrix4 pitcherWorldTransform() {
    return Matrix4::translation(Vector3(0.0f, 0.0f, moundZ + 0.15f));
}

bool freezePitchAtPlate(
    Body3D& baseball,
    const Vector3& previousPosition,
    std::vector<Vector3>& trail
) {
    if (baseball.position.z < plateZ || baseball.velocity.z <= 0.0f) {
        return false;
    }

    // Interpolate the exact plate-crossing point instead of clamping z only.
    float segmentLength = baseball.position.z - previousPosition.z;
    float t = segmentLength <= 0.0f
        ? 1.0f
        : (plateZ - previousPosition.z) / segmentLength;
    t = std::clamp(t, 0.0f, 1.0f);
    baseball.position = previousPosition + (baseball.position - previousPosition) * t;
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
    // macOS: SFML's graphics module needs a legacy (OpenGL 2.1) context.
    // A Core 3.2 profile makes all SFML 2D draws (UI, strike zone, text) invisible.
    // The GPU mesh path uses GLSL 1.20 so it works on that same legacy context.
    sf::ContextSettings glSettings;
    glSettings.depthBits = 24;
    glSettings.stencilBits = 8;
    glSettings.antiAliasingLevel = 4;

    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "Pitching Simulator | R throw | 4 delivery cam",
        sf::Style::Default,
        sf::State::Windowed,
        glSettings
    );
    // Cap presentation rate; sim runs every frame.
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    bool fullQuality = true;
    bool antiAliasingEnabled = false;
    Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);
    DemoFpsCounter fpsCounter("Pitching Simulator | Q quality | AA off | FULL (ball 2x)");

    sf::Font font;
    bool fontLoaded = loadUiFont(font);

    auto resizeRaster = [&](sf::Vector2u windowSize) {
        return rasterSizeForWindow(
            windowSize,
            rasterScaleForQuality(fullQuality, antiAliasingEnabled)
        );
    };

    auto qualityTitle = [&]() {
        if (fullQuality) {
            return antiAliasingEnabled
                ? "Pitching Simulator | Q quality | AA off | FULL ball-2x"
                : "Pitching Simulator | Q quality | AA off | FULL ball-2x";
        }

        return antiAliasingEnabled
            ? "Pitching Simulator | Q quality | AA off | fast"
            : "Pitching Simulator | Q quality | AA off | fast";
    };

    sf::Vector2u rasterSize = resizeRaster(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    PitchCameraMode cameraMode = PitchCameraMode::Delivery;
    applyCameraMode(camera, cameraMode);

    Mesh3D baseballMesh = BaseballVisual3D::makeMesh(fullQuality ? 48 : 28, fullQuality ? 96 : 48);
    std::vector<SeamPoint3D> seamA = BaseballVisual3D::makeSeamLoop(false);
    std::vector<SeamPoint3D> seamB = BaseballVisual3D::makeSeamLoop(true);
    const float catcherWorldX = 0.05f;
    const float catcherWorldZ = plateZ + 0.95f;
    auto playerDetail = [&]() { return fullQuality ? 2 : 2; }; // always high model detail

    // Skinned path: glTF if present, else CharacterModel3D (multi-bone throw).
    SkinnedModel3D pitcherModel = loadCharacterOrProcedural("pitcher", false, playerDetail());
    SkinnedModel3D catcherModel = loadCharacterOrProcedural("catcher", true, playerDetail());

    // Delivery clip preference: throw_preview (CharacterModel3D) → yamamoto → generated.
    AnimationClip deliveryClip;
    if (const AnimationClip* c = pitcherModel.findClip("throw_preview")) {
        deliveryClip = *c;
    } else if (const AnimationClip* c = pitcherModel.findClip("yamamoto_windup")) {
        deliveryClip = *c;
    } else {
        deliveryClip = BaseballAnims::yamamotoWindup(pitcherModel);
    }

    AnimationClip pitcherIdleClip;
    if (const AnimationClip* c = pitcherModel.findClip("idle")) {
        pitcherIdleClip = *c;
    } else {
        pitcherIdleClip = BaseballAnims::pitcherIdle(pitcherModel);
    }

    AnimationClip catcherIdleClip;
    if (const AnimationClip* c = catcherModel.findClip("catcher_idle")) {
        catcherIdleClip = *c;
    } else if (const AnimationClip* c = catcherModel.findClip("crouch")) {
        catcherIdleClip = *c;
    } else if (const AnimationClip* c = catcherModel.findClip("idle")) {
        catcherIdleClip = *c;
    } else {
        catcherIdleClip = BaseballAnims::catcherIdle(catcherModel);
    }

    AnimationClip catcherReceiveClip = catcherIdleClip;
    if (const AnimationClip* c = catcherModel.findClip("receive")) {
        catcherReceiveClip = *c;
    }

    SkeletonAnimator pitcherAnim;
    SkeletonAnimator catcherAnim;
    pitcherAnim.setModel(pitcherModel);
    catcherAnim.setModel(catcherModel);
    pitcherAnim.applyClip(pitcherIdleClip, 0.0f, false);
    catcherAnim.applyClip(catcherIdleClip, 0.0f);

    Mesh3D pitcherMesh = pitcherModel.skinToMesh(pitcherAnim.skinMatrices());
    Mesh3D catcherMesh = catcherModel.skinToMesh(catcherAnim.skinMatrices());
    RasterMeshRenderCache renderCache;
    RasterMeshRenderCache pitcherCache;
    RasterMeshRenderCache catcherCache;
    renderCache.reserveFor(baseballMesh);

    // OpenGL path for characters + ball (software raster kept as optional fallback).
    GlRenderer gl;
    bool useOpenGL = gl.initialize(window);
    if (useOpenGL) {
        std::cerr << "Rendering: OpenGL (GPU lit meshes)" << std::endl;
    } else {
        std::cerr << "Rendering: software raster fallback" << std::endl;
    }
    GlMesh glPitcher;
    GlMesh glCatcher;
    GlMesh glBall;
    if (useOpenGL) {
        glPitcher.upload(pitcherMesh);
        glCatcher.upload(catcherMesh);
        glBall.upload(baseballMesh);
    }

    std::array<PitchProfile, 5> pitches = makePitchProfiles();
    int selectedPitch = 0;
    int activePitch = selectedPitch;
    Vector3 aimPoint = strikeZoneCenter;
    PhysicsWorld3D world;
    Body3D baseball;
    std::vector<Vector3> trail;
    std::vector<PitchResult> pitchResults;
    CountState count;
    PitchPhase phase = PitchPhase::Ready;
    std::mt19937 randomGenerator(std::random_device{}());
    float globalSpeedScale = 1.0f;
    float currentPitchSpeedMph = rollPitchSpeed(pitches[selectedPitch], globalSpeedScale, randomGenerator);
    PitchFlightVariation currentVariation = rollPitchVariation(pitches[selectedPitch], randomGenerator);
    resetPitchOnMound(baseball, world, pitches[selectedPitch], trail);

    sf::Clock frameClock;
    float accumulator = 0.0f;
    float pitchAge = 0.0f;
    float spinX = 0.0f;
    float spinY = 0.0f;
    float spinZ = 0.0f;
    float resultBannerTimer = 0.0f;
    float poseClock = 0.0f;
    float deliveryAge = -1.0f; // < 0 => idle; seconds into delivery clip
    float playerRebuildTimer = 0.0f;
    bool ballReleased = false;
    // throw_preview: release key at 1.22s / 2.20s ≈ 0.555. Older yamamoto used 0.66.
    const float deliveryDuration = deliveryClip.duration > 1e-3f ? deliveryClip.duration : 2.20f;
    const float releaseNormalized = (deliveryClip.name == "throw_preview")
        ? (1.22f / deliveryDuration)
        : 0.66f;
    constexpr float playerRebuildHz = 60.0f;
    bool paused = false;
    bool draggingSpeedSlider = false;
    std::string latestResult = "Ready — press R to throw";
    sf::Color latestResultColor(225, 235, 205);

    auto rebuildSkinnedPlayers = [&]() {
        pitcherModel.skinInto(pitcherAnim.skinMatrices(), pitcherMesh);
        catcherModel.skinInto(catcherAnim.skinMatrices(), catcherMesh);
        pitcherCache.reserveFor(pitcherMesh);
        catcherCache.reserveFor(catcherMesh);
        if (useOpenGL) {
            glPitcher.updatePositionsNormals(pitcherMesh);
            glCatcher.updatePositionsNormals(catcherMesh);
        }
    };

    auto throwHandWorldSkinned = [&]() {
        return pitcherAnim.throwHandWorld(pitcherWorldTransform());
    };

    // Last glued hand positions for a tiny release-velocity assist (hand whip).
    Vector3 prevHandPos = throwHandWorldSkinned();
    Vector3 handVelocity;

    auto prepareReadyState = [&]() {
        resetPitchOnMound(baseball, world, pitches[selectedPitch], trail);
        currentPitchSpeedMph = rollPitchSpeed(pitches[selectedPitch], globalSpeedScale, randomGenerator);
        currentVariation = rollPitchVariation(pitches[selectedPitch], randomGenerator);
        accumulator = 0.0f;
        pitchAge = 0.0f;
        spinX = 0.0f;
        spinY = 0.0f;
        spinZ = 0.0f;
        deliveryAge = -1.0f;
        ballReleased = false;
        phase = PitchPhase::Ready;
        prevHandPos = throwHandWorldSkinned();
        handVelocity = Vector3();
        baseball.position = prevHandPos;
    };

    auto startDelivery = [&]() {
        activePitch = selectedPitch;
        currentPitchSpeedMph = rollPitchSpeed(pitches[selectedPitch], globalSpeedScale, randomGenerator);
        currentVariation = rollPitchVariation(pitches[selectedPitch], randomGenerator);
        resetPitchOnMound(baseball, world, pitches[selectedPitch], trail);
        accumulator = 0.0f;
        pitchAge = 0.0f;
        spinX = 0.0f;
        spinY = 0.0f;
        spinZ = 0.0f;
        deliveryAge = 0.0f;
        ballReleased = false;
        phase = PitchPhase::Ready; // ball glued to hand until release frame
        // Pose delivery frame 0 so the ball starts in the glove box.
        pitcherAnim.applyClipNormalized(deliveryClip, 0.0f);
        prevHandPos = throwHandWorldSkinned();
        handVelocity = Vector3();
        baseball.position = prevHandPos;
        latestResult = pitches[selectedPitch].name + " — windup";
        latestResultColor = pitches[selectedPitch].color;
        resultBannerTimer = 0.0f;
    };

    auto releasePitch = [&]() {
        Vector3 hand = throwHandWorldSkinned();
        // Prefer the animated hand location so flight starts exactly where the
        // fingers let go (Ball joint / Palm_R), not a fixed mound offset.
        launchPitch(
            baseball,
            world,
            pitches[selectedPitch],
            aimPoint,
            trail,
            currentPitchSpeedMph,
            currentVariation,
            hand,
            randomGenerator
        );
        // Nudge with recent hand motion so the first frame doesn't look stuck.
        float whip = handVelocity.magnitude();
        if (whip > 0.5f && whip < 40.0f) {
            Vector3 whipDir = handVelocity * (1.0f / whip);
            baseball.velocity = baseball.velocity + whipDir * std::min(whip * 0.08f, 2.5f);
        }
        ballReleased = true;
        phase = PitchPhase::Flying;
        const PitchProfile& p = pitches[selectedPitch];
        latestResult = p.name + "  " +
            std::to_string(static_cast<int>(p.spinRpm * currentVariation.spinRpmScale + 0.5f)) +
            " rpm";
        latestResultColor = p.color;
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
                    rasterSize = resizeRaster(window.getSize());
                    frameBuffer.resize(rasterSize.x, rasterSize.y);
                    fpsCounter.setTitle(qualityTitle());
                }

                if (key->code == sf::Keyboard::Key::Q) {
                    fullQuality = !fullQuality;
                    baseballMesh = BaseballVisual3D::makeMesh(fullQuality ? 48 : 28, fullQuality ? 96 : 48);
                    renderCache.reserveFor(baseballMesh);
                    playerRebuildTimer = 1.0f; // force pose rebuild this frame
                    // Full quality always wants coverage AA; fast mode keeps current AA flag.
                    // AA stays user-controlled via A; full quality only changes mesh/SS.
                    rasterSize = resizeRaster(window.getSize());
                    frameBuffer.resize(rasterSize.x, rasterSize.y);
                    fpsCounter.setTitle(qualityTitle());
                }

                if (key->code == sf::Keyboard::Key::Space) {
                    paused = !paused;
                }

                if (key->code == sf::Keyboard::Key::R) {
                    // Ignore re-trigger mid-delivery until ball is out or settled.
                    if (deliveryAge < 0.0f || ballReleased || phase == PitchPhase::Settled) {
                        startDelivery();
                    }
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

                if (key->code == sf::Keyboard::Key::Num4) {
                    cameraMode = PitchCameraMode::Delivery;
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
                        if (phase == PitchPhase::Ready) {
                            prepareReadyState();
                            latestResult = pitches[selectedPitch].name + " ready — press R";
                            latestResultColor = pitches[selectedPitch].color;
                        }
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
                rasterSize = resizeRaster(resized->size);
                frameBuffer.resize(rasterSize.x, rasterSize.y);
            }
        }

        float dt = std::min(frameClock.restart().asSeconds(), 0.1f);
        if (resultBannerTimer > 0.0f) {
            resultBannerTimer = std::max(0.0f, resultBannerTimer - dt);
        }

        // Skinned pose first so release spawns from the throw hand / Ball joint.
        if (deliveryAge >= 0.0f) {
            float deliveryT = std::clamp(deliveryAge / deliveryDuration, 0.0f, 1.0f);
            pitcherAnim.applyClipNormalized(deliveryClip, deliveryT);
        } else {
            // Loop idle when not delivering (breathing / weight shift).
            pitcherAnim.applyClip(pitcherIdleClip, poseClock, true);
        }
        // Catcher: idle crouch between pitches; receive while the ball is live.
        if (phase == PitchPhase::Flying || (deliveryAge >= 0.0f && ballReleased)) {
            float recvT = catcherReceiveClip.duration > 1e-4f
                ? std::fmod(std::max(pitchAge, 0.0f), catcherReceiveClip.duration)
                : 0.0f;
            catcherAnim.applyClip(catcherReceiveClip, recvT, false);
        } else {
            catcherAnim.applyClip(catcherIdleClip, poseClock, true);
        }

        if (!paused) {
            poseClock += dt;

            if (deliveryAge >= 0.0f) {
                deliveryAge += dt;
                float deliveryT = std::clamp(deliveryAge / deliveryDuration, 0.0f, 1.0f);
                pitcherAnim.applyClipNormalized(deliveryClip, deliveryT);

                // Glue physics ball to Ball/Palm until release (one ball, no mesh prop).
                if (!ballReleased) {
                    Vector3 hand = throwHandWorldSkinned();
                    if (dt > 1e-5f) {
                        handVelocity = (hand - prevHandPos) * (1.0f / dt);
                    }
                    prevHandPos = hand;
                    baseball.position = hand;
                    baseball.velocity = Vector3();
                    baseball.acceleration = Vector3();
                }

                if (!ballReleased && deliveryT >= releaseNormalized) {
                    releasePitch();
                }

                if (phase == PitchPhase::Settled && deliveryAge >= deliveryDuration + 0.55f) {
                    deliveryAge = -1.0f;
                }
            } else if (phase == PitchPhase::Ready || phase == PitchPhase::Settled) {
                // Between pitches: ball rests in the glove (hand attach).
                // After a plate result, leave the settled ball until the next R.
                if (phase == PitchPhase::Ready) {
                    Vector3 hand = throwHandWorldSkinned();
                    prevHandPos = hand;
                    handVelocity = Vector3();
                    baseball.position = hand;
                    baseball.velocity = Vector3();
                    baseball.acceleration = Vector3();
                }
            }
        }

        playerRebuildTimer += dt;
        if (playerRebuildTimer >= (1.0f / playerRebuildHz)) {
            playerRebuildTimer = 0.0f;
            rebuildSkinnedPlayers();
        }

        if (!paused && phase == PitchPhase::Flying) {
            accumulator += dt;
            while (accumulator >= fixedStep) {
                Vector3 previousPosition = baseball.position;
                // Drag + Magnus applied inside PhysicsWorld from baseball.angularVelocity.
                // Only a tiny residual wake remains as artificial noise.
                Vector3 wake = residualTurbulence(currentVariation, baseball.position, pitchAge);
                baseball.applyForce(wake * baseball.mass);
                world.step(fixedStep);
                bool reachedPlate = freezePitchAtPlate(baseball, previousPosition, trail);
                if (reachedPlate) {
                    PitchResult result = classifyPitchResult(baseball.position);
                    latestResult = applyCount(count, result);
                    latestResultColor = count.lastOutcome.empty() ? result.color : sf::Color(255, 220, 120);
                    if (!count.lastOutcome.empty()) {
                        latestResultColor = count.lastOutcome == "Strikeout"
                            ? sf::Color(255, 120, 110)
                            : sf::Color(120, 200, 255);
                    }
                    pitchResults.push_back(result);
                    if (pitchResults.size() > 8) {
                        pitchResults.erase(pitchResults.begin());
                    }
                    phase = PitchPhase::Settled;
                    resultBannerTimer = 2.4f;
                }
                pitchAge += fixedStep;
                accumulator -= fixedStep;

                if (phase != PitchPhase::Flying) {
                    break;
                }

                // Visual spin matches physical ω (scaled for readable seams).
                float visualScale = 0.055f;
                spinX += baseball.angularVelocity.x * fixedStep * visualScale;
                spinY += baseball.angularVelocity.y * fixedStep * visualScale;
                spinZ += baseball.angularVelocity.z * fixedStep * visualScale;

                if (trail.empty() || (baseball.position - trail.back()).magnitude() > 0.12f) {
                    trail.push_back(baseball.position);
                    if (trail.size() > 160) {
                        trail.erase(trail.begin());
                    }
                }
            }
        }

        // Delivery cam tracks set → open (and the ball once free).
        if (cameraMode == PitchCameraMode::Delivery) {
            float camT = 0.0f;
            if (deliveryAge >= 0.0f) {
                camT = std::clamp(deliveryAge / deliveryDuration, 0.0f, 1.0f);
            }
            bool trackBall = ballReleased || phase == PitchPhase::Flying || phase == PitchPhase::Settled;
            applyDeliveryCamera(camera, camT, baseball.position, trackBall);
        }

        Matrix4 baseballTransform =
            Matrix4::translation(baseball.position) *
            Matrix4::rotationY(spinY) *
            Matrix4::rotationZ(spinZ) *
            Matrix4::rotationX(spinX) *
            Matrix4::scale(Vector3(baseballRadius, baseballRadius, baseballRadius));

        Matrix4 pitcherTransform = pitcherWorldTransform();

        // Catcher crouch behind plate, model faces -Z toward mound so rotate 180°.
        Matrix4 catcherTransform =
            Matrix4::translation(Vector3(catcherWorldX, 0.0f, catcherWorldZ)) *
            Matrix4::rotationY(pi);

        Camera3D overlayCamera = camera;

        if (useOpenGL) {
            gl.beginFrame(window, camera, sf::Color(5, 8, 14));
            gl.drawGround(4.0f, -2.0f, plateZ + 4.0f, sf::Color(20, 28, 24));
            gl.drawMesh(glPitcher, pitcherTransform);
            if (cameraMode != PitchCameraMode::Catcher) {
                gl.drawMesh(glCatcher, catcherTransform);
            }
            gl.drawMesh(glBall, baseballTransform);
            gl.endFrame(window);
        } else {
            frameBuffer.clear(sf::Color(5, 8, 14));
            frameBuffer.clearDepth(std::numeric_limits<float>::infinity());
            rasterizeMeshTriangles(
                frameBuffer, camera, pitcherMesh, pitcherTransform,
                sf::Color(230, 230, 235), pitcherCache
            );
            if (cameraMode != PitchCameraMode::Catcher) {
                rasterizeMeshTriangles(
                    frameBuffer, camera, catcherMesh, catcherTransform,
                    sf::Color(40, 50, 70), catcherCache
                );
            }
            rasterizeMeshTrianglesSupersampled(
                frameBuffer, camera, baseballMesh, baseballTransform,
                sf::Color(230, 220, 205), renderCache,
                ballSuperSampleForQuality(fullQuality)
            );
            window.clear();
            frameBuffer.present(window);
        }

        drawFieldGuide(window, overlayCamera);
        drawHomePlate(window, overlayCamera);
        drawCatcherTarget(window, overlayCamera, aimPoint, pitches[selectedPitch]);
        drawProjectedPolyline(window, overlayCamera, trail, pitches[activePitch].color);
        drawStrikeZone(window, overlayCamera, aimPoint, pitches[selectedPitch]);
        drawPitchResultHistory(window, overlayCamera, pitchResults);
        drawBallShadow(window, overlayCamera, baseball.position, baseballRadius);
        drawBaseballSeams(window, overlayCamera, baseballTransform, seamA, seamB);

        if (fontLoaded) {
            sf::RectangleShape panel(sf::Vector2f(460.0f, 168.0f));
            panel.setPosition(sf::Vector2f(18.0f, 18.0f));
            panel.setFillColor(sf::Color(5, 8, 14, 190));
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
                << "Release " << currentPitchSpeedMph << " mph  x" << globalSpeedScale;

            std::ostringstream countLabel;
            countLabel << "Count " << count.balls << "-" << count.strikes
                << "   P#" << count.pitchNumber
                << "   K " << count.strikeouts
                << "  BB " << count.walks;
            const char* phaseLabel = "Ready";
            if (phase == PitchPhase::Flying) {
                phaseLabel = "In flight";
            } else if (phase == PitchPhase::Settled) {
                phaseLabel = "Settled";
            }

            drawText(window, font, pitches[selectedPitch].name, 18, sf::Vector2f(34.0f, 26.0f), pitches[selectedPitch].color);
            {
                const PitchProfile& sel = pitches[selectedPitch];
                std::ostringstream spinHud;
                spinHud << std::fixed << std::setprecision(0)
                        << static_cast<int>(sel.spinRpm + 0.5f) << " rpm  eff "
                        << std::setprecision(0) << (sel.spinEfficiency * 100.0f) << "%";
                drawText(window, font, spinHud.str(), 12, sf::Vector2f(200.0f, 30.0f), sf::Color(200, 210, 180));
            }
            drawText(window, font, countLabel.str(), 14, sf::Vector2f(34.0f, 50.0f), sf::Color(235, 230, 190));
            drawText(window, font, "F 4S  P SPL  C CB  T CUT  S SL  · spin drives break", 12, sf::Vector2f(34.0f, 72.0f), sf::Color(180, 215, 220));
            drawText(window, font, "R throw | Space pause | arrows aim | 1-4 camera (4=delivery)", 12, sf::Vector2f(34.0f, 94.0f), sf::Color(155, 195, 200));
            drawText(window, font, "Drag speed for next throw", 11, sf::Vector2f(34.0f, 132.0f), sf::Color(120, 175, 185));
            drawText(window, font, aimLabel.str(), 12, sf::Vector2f(300.0f, 28.0f), sf::Color(135, 195, 200));
            drawText(window, font, phaseLabel, 12, sf::Vector2f(360.0f, 50.0f), sf::Color(150, 210, 220));
            drawText(window, font, cameraModeName(cameraMode), 11, sf::Vector2f(360.0f, 72.0f), sf::Color(130, 190, 205));
            drawText(window, font, speedLabel.str(), 12, sf::Vector2f(230.0f, 146.0f), sf::Color(175, 215, 180));
            drawText(window, font, latestResult, 13, sf::Vector2f(34.0f, 150.0f), latestResultColor);

            if (resultBannerTimer > 0.0f && phase == PitchPhase::Settled) {
                float alpha = std::clamp(resultBannerTimer / 2.4f, 0.0f, 1.0f);
                sf::RectangleShape banner(sf::Vector2f(360.0f, 52.0f));
                banner.setPosition(sf::Vector2f(
                    static_cast<float>(window.getSize().x) * 0.5f - 180.0f,
                    78.0f
                ));
                banner.setFillColor(sf::Color(8, 14, 20, static_cast<std::uint8_t>(200 * alpha)));
                banner.setOutlineThickness(1.5f);
                banner.setOutlineColor(sf::Color(
                    latestResultColor.r,
                    latestResultColor.g,
                    latestResultColor.b,
                    static_cast<std::uint8_t>(220 * alpha)
                ));
                window.draw(banner);
                drawText(
                    window,
                    font,
                    latestResult,
                    22,
                    sf::Vector2f(static_cast<float>(window.getSize().x) * 0.5f - 150.0f, 90.0f),
                    sf::Color(
                        latestResultColor.r,
                        latestResultColor.g,
                        latestResultColor.b,
                        static_cast<std::uint8_t>(255 * alpha)
                    )
                );
            }
        }

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
