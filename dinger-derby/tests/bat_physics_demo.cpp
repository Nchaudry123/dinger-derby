// Bat Physics Demo / HR Derby — same world as pitching_simulator_demo.
// CharacterModel3D pitcher, BaseballVisual3D ball, same plate distance / ground.
//
// Yellow silhouette = aim reticle (does NOT swing). Shows where contact will be.
// 3D bat mesh = real swing path (load → contact → finish) when you press Space/LMB.
//
// Default: HR DERBY — fixed swings; CharacterModel3D pitcher soft-tosses from the
// mound (same asset/delivery as pitching_simulator_demo). Practice/Live as before.
//
// Controls:
//   Mouse              aim reticle (PCI) — height auto-tilts bat
//   Z / X / C          Power / Contact / Regular
//   Space / LMB        swing
//   D / P / L          Derby / Practice / Live AB
//   R                  next pitch / toss
//   N                  new derby round (or new AB)
//   1 / 2 / 3          Easy / Normal / Hard (Hard unlocks with 3+ HR on Normal)
//   H                  help overlay
//   - / =              bat crack volume
//   [ ]                pitch speed (live)
//   Esc                quit

#include <SFML/Graphics.hpp>
#include <SFML/Window/ContextSettings.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "DemoFpsCounter.h"
#include "RasterDemo3D.h"
#include "audio/ProceduralSfx.h"
#include "game/DerbyBests.h"
#include "game/GameSettings.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "physics/Body3D.h"
#include "physics/PhysicsWorld3D.h"
#include "rendering/BaseballAnims.h"
#include "rendering/BaseballVisual3D.h"
#include "rendering/Camera3D.h"
#include "rendering/CharacterModel3D.h"
#include "rendering/FrameBuffer.h"
#include "rendering/GlRenderer.h"
#include "rendering/GltfLoader.h"
#include "rendering/Mesh3D.h"
#include "rendering/Rasterizer3D.h"
#include "rendering/SkeletonAnimator.h"
#include "rendering/SkinnedModel3D.h"
#include "rendering/Stadium3D.h"

namespace {

// ── Same room as pitching_simulator_demo ────────────────────────────────
constexpr float pi = 3.1415926535f;
constexpr float kDeg = 180.0f / pi;
constexpr float fixedStep = 1.0f / 180.0f;
constexpr float baseballRadius = 0.065f;
// Drawn larger than physics radius so the ball stays readable in flight.
constexpr float baseballVisualScale = 3.35f; // readability over pure scale realism
constexpr float feetPerWorldUnit = 2.0f;
constexpr float pitchingDistanceFeet = 60.5f;
constexpr float plateZ = pitchingDistanceFeet / feetPerWorldUnit; // ~30.25
constexpr float moundZ = 0.0f;
constexpr float strikeZoneHalfWidth = 0.46f;
constexpr float strikeZoneHalfHeight = 0.55f;
const Vector3 strikeZoneCenter(0.0f, 1.28f, plateZ);
// Open park: ground at y=0 only. No ceiling / side walls so fly balls
// never bounce off an invisible box and drop straight down.
const Vector3 boundsMinimum(-400.0f, 0.0f, -500.0f);
const Vector3 boundsMaximum(400.0f, 2000.0f, 400.0f);

// Same unit conversion as pitching_simulator_demo (1 world unit ≈ 2 feet).
float mphToWorldUnitsPerSecond(float mph) {
    return mph * 5280.0f / 3600.0f / feetPerWorldUnit;
}

bool loadUiFont(sf::Font& font) {
    for (const char* path : {
             "/System/Library/Fonts/Supplemental/Arial.ttf",
             "/System/Library/Fonts/Supplemental/Helvetica.ttf",
             "/System/Library/Fonts/Helvetica.ttc",
             "/System/Library/Fonts/SFNS.ttf"}) {
        if (font.openFromFile(path)) {
            return true;
        }
    }
    return false;
}

void drawText(
    sf::RenderWindow& w,
    const sf::Font& font,
    const std::string& s,
    unsigned size,
    sf::Vector2f p,
    sf::Color c
) {
    sf::Text t(font, s, size);
    t.setFillColor(c);
    t.setPosition(p);
    w.draw(t);
}

float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}
float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}
float smooth01(float t) {
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Vector3 safeNorm(const Vector3& v, const Vector3& fb = Vector3(0, 1, 0)) {
    float m = v.magnitude();
    return m > 1e-6f ? v * (1.0f / m) : fb;
}

void lookAt(Camera3D& cam, const Vector3& pos, const Vector3& target) {
    cam.position = pos;
    Vector3 to = target - pos;
    float h = std::sqrt(to.x * to.x + to.z * to.z);
    cam.rotation.y = std::atan2(to.x, to.z);
    cam.rotation.x = -std::atan2(to.y, h);
    cam.rotation.z = 0.0f;
}

void applyCatcherCamera(Camera3D& cam) {
    // Plate-scale camera: lower + slightly closer so the diamond (90 ft paths,
    // 26 ft plate circle, 18 ft mound circle) reads correctly from catcher view.
    lookAt(
        cam,
        Vector3(0.0f, 1.72f, plateZ + 1.85f),
        Vector3(0.0f, 1.05f, plateZ - 22.0f) // through plate toward mound / 2B
    );
    cam.fieldOfView = 680.0f;
    cam.nearPlane = 0.08f;
    cam.farPlane = Stadium3D::recommendedFarPlane();
}

// Elevated chase cam: pull back and up so the full arc (including drop) is visible.
void applyBallFollowCamera(Camera3D& cam, const Vector3& ballPos, const Vector3& ballVel) {
    float speed = ballVel.magnitude();
    Vector3 horiz(ballVel.x, 0.0f, ballVel.z);
    float hSpd = horiz.magnitude();
    // Horizontal flight direction; when falling nearly straight down, keep last OF bias.
    Vector3 forward = hSpd > 0.5f ? safeNorm(horiz) : Vector3(0.0f, 0.0f, -1.0f);
    if (forward.dot(Vector3(0, 0, -1)) < 0.05f && hSpd > 0.5f) {
        forward = safeNorm(forward + Vector3(0, 0, -0.4f));
    }
    Vector3 right = safeNorm(Vector3(0, 1, 0).cross(forward), Vector3(1, 0, 0));

    // Distance / elevation scale with ball height so high flies stay framed.
    float dist = clampf(10.0f + speed * 0.06f + ballPos.y * 0.55f, 11.0f, 36.0f);
    float elev = clampf(5.5f + ballPos.y * 0.65f, 6.0f, 28.0f);
    // Slight 1B-side offset for depth; more height when the ball is dropping.
    float dropBias = (ballVel.y < 0.0f) ? 1.4f : 0.0f;
    Vector3 pos =
        ballPos - forward * dist + Vector3(0.0f, elev + dropBias, 0.0f) + right * 3.2f;
    pos.y = std::max(pos.y, 5.5f);
    // Look at the ball (slightly ahead of its path when still moving out).
    Vector3 target = ballPos + forward * (hSpd > 0.5f ? 3.0f : 0.0f) + Vector3(0.0f, 0.35f, 0.0f);
    lookAt(cam, pos, target);
    cam.fieldOfView = 920.0f;
    cam.nearPlane = 0.2f;
    cam.farPlane = Stadium3D::recommendedFarPlane();
}

// Broadcast hold: elevated 1B-side angle on the landing / wall spot.
void applyLandingHoldCamera(Camera3D& cam, const Vector3& landPos) {
    Vector3 toHome = Vector3(0.0f, 0.0f, plateZ) - landPos;
    toHome.y = 0.0f;
    Vector3 fromHome = landPos - Vector3(0.0f, 0.0f, plateZ);
    fromHome.y = 0.0f;
    Vector3 radial = safeNorm(fromHome, Vector3(0, 0, -1));
    Vector3 right = safeNorm(Vector3(0, 1, 0).cross(radial), Vector3(1, 0, 0));
    Vector3 pos = landPos - radial * 14.0f + right * 9.0f + Vector3(0.0f, 9.5f, 0.0f);
    pos.y = std::max(pos.y, 6.0f);
    lookAt(cam, pos, landPos + Vector3(0.0f, 1.2f, 0.0f));
    cam.fieldOfView = 780.0f;
    cam.nearPlane = 0.2f;
    cam.farPlane = Stadium3D::recommendedFarPlane();
    (void)toHome;
}

// Smooth blend from landing hold back to the plate / catcher view.
void applyReturnToPlateCamera(Camera3D& cam, const Vector3& landPos, float t01) {
    t01 = smooth01(t01);
    Camera3D holdCam = cam;
    Camera3D plateCam = cam;
    applyLandingHoldCamera(holdCam, landPos);
    applyCatcherCamera(plateCam);
    cam.position = holdCam.position * (1.0f - t01) + plateCam.position * t01;
    // Slerp-ish look: blend targets via lookAt on blended eye
    Vector3 holdT = landPos + Vector3(0.0f, 1.2f, 0.0f);
    Vector3 plateT(0.0f, 1.55f, plateZ - 14.0f);
    Vector3 target = holdT * (1.0f - t01) + plateT * t01;
    lookAt(cam, cam.position, target);
    cam.fieldOfView = lerp(780.0f, 760.0f, t01);
    cam.nearPlane = 0.12f;
    cam.farPlane = Stadium3D::recommendedFarPlane();
}

void applyCameraShake(Camera3D& cam, float shakeTimer, float intensity) {
    if (shakeTimer <= 0.0f || intensity <= 0.0f) {
        return;
    }
    float a = shakeTimer * 55.0f;
    float amp = intensity * shakeTimer;
    cam.position.x += std::sin(a * 1.7f) * amp * 0.12f;
    cam.position.y += std::sin(a * 2.3f) * amp * 0.08f;
    cam.position.z += std::cos(a * 1.9f) * amp * 0.10f;
}

Matrix4 pitcherWorldTransform() {
    // Match pitching sim: centered on the rubber, facing +Z toward home.
    return Matrix4::translation(Vector3(0.0f, 0.0f, moundZ));
}

// ── Overlay helpers (same field language as pitching sim) ───────────────

void drawThickProjectedLine(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const Vector3& a,
    const Vector3& b,
    float thickness,
    sf::Color color
) {
    ProjectedPoint3D pa = camera.projectPoint(
        a, static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)
    );
    ProjectedPoint3D pb = camera.projectPoint(
        b, static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)
    );
    if (!pa.visible || !pb.visible) {
        return;
    }
    sf::Vector2f a2(pa.position.x, pa.position.y);
    sf::Vector2f b2(pb.position.x, pb.position.y);
    sf::Vector2f d = b2 - a2;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-3f) {
        return;
    }
    sf::Vector2f n(-d.y / len, d.x / len);
    sf::Vector2f o = n * (thickness * 0.5f);
    sf::Vertex q[] = {
        sf::Vertex{a2 + o, color},
        sf::Vertex{a2 - o, color},
        sf::Vertex{b2 - o, color},
        sf::Vertex{b2 + o, color}
    };
    window.draw(q, 4, sf::PrimitiveType::TriangleFan);
}

void drawHomePlate(sf::RenderWindow& window, const Camera3D& camera) {
    const float halfWidth = 0.36f;
    const float frontZ = plateZ + 0.08f;
    const float shoulderZ = plateZ - 0.14f;
    const float pointZ = plateZ - 0.42f;
    Vector3 plate[5] = {
        Vector3(-halfWidth, 0.02f, frontZ),
        Vector3(halfWidth, 0.02f, frontZ),
        Vector3(halfWidth, 0.02f, shoulderZ),
        Vector3(0.0f, 0.02f, pointZ),
        Vector3(-halfWidth, 0.02f, shoulderZ)
    };
    for (int i = 0; i < 5; i++) {
        drawThickProjectedLine(
            window, camera, plate[i], plate[(i + 1) % 5], 2.0f, sf::Color(235, 230, 205, 190)
        );
    }
}

void drawFieldGuide(sf::RenderWindow& window, const Camera3D& camera) {
    const sf::Color dirt(168, 128, 82, 95);
    const sf::Color laneColor(70, 145, 145, 78);
    for (int ring = 1; ring <= 4; ring++) {
        float radius = 0.18f * static_cast<float>(ring);
        const int segments = 20;
        for (int i = 0; i < segments; i++) {
            float a0 = static_cast<float>(i) / segments * pi * 2.0f;
            float a1 = static_cast<float>(i + 1) / segments * pi * 2.0f;
            drawThickProjectedLine(
                window, camera,
                Vector3(std::cos(a0) * radius, 0.01f, moundZ + std::sin(a0) * radius * 0.55f),
                Vector3(std::cos(a1) * radius, 0.01f, moundZ + std::sin(a1) * radius * 0.55f),
                2.2f, dirt
            );
        }
    }
    drawThickProjectedLine(
        window, camera, Vector3(0, 0, 0), Vector3(0, 0, plateZ + 0.4f), 1.4f, laneColor
    );
    drawThickProjectedLine(
        window, camera,
        Vector3(-0.62f, 0.02f, moundZ), Vector3(0.62f, 0.02f, moundZ),
        5.0f, sf::Color(205, 180, 130, 200)
    );
}

void drawStrikeZone(sf::RenderWindow& window, const Camera3D& camera, sf::Color col) {
    Vector3 c = strikeZoneCenter;
    float hw = strikeZoneHalfWidth;
    float hh = strikeZoneHalfHeight;
    Vector3 corners[4] = {
        c + Vector3(-hw, -hh, 0),
        c + Vector3(hw, -hh, 0),
        c + Vector3(hw, hh, 0),
        c + Vector3(-hw, hh, 0)
    };
    for (int i = 0; i < 4; i++) {
        drawThickProjectedLine(window, camera, corners[i], corners[(i + 1) % 4], 2.4f, col);
    }
}

// ── Swing types ─────────────────────────────────────────────────────────

enum class SwingType { Power = 0, Contact = 1, Regular = 2 };

struct SwingProfile {
    const char* name;
    float power;
    float duration;
    float sweetScale;
    float corBonus;
    float massScale;
    float pciR;
    sf::Color color;
};

const SwingProfile& profileOf(SwingType t) {
    // Durations are full load→finish; contact dwell ~0.34–0.52 of the clip.
    // Tuned for readable, powerful, game-feel swings.
    static const SwingProfile k[] = {
        {"POWER", 1.28f, 0.32f, 0.82f, 0.06f, 1.14f, 0.070f, sf::Color(255, 130, 90)},
        {"CONTACT", 0.72f, 0.40f, 1.70f, 0.00f, 0.94f, 0.13f, sf::Color(110, 210, 255)},
        {"REGULAR", 1.00f, 0.36f, 1.20f, 0.02f, 1.00f, 0.095f, sf::Color(255, 225, 70)},
    };
    return k[static_cast<int>(t)];
}

// ── Bat ─────────────────────────────────────────────────────────────────

struct BatConfig {
    float length = 0.86f;
    float barrelR = 0.033f;
    float handleR = 0.014f;
    float mass = 0.90f;
    float sweetFromKnob = 0.56f;
    float sweetWidth = 0.14f; // slightly fatter barrel for demo hit rate
    float minCor = 0.40f; // handle / mishit
    float maxCor = 0.55f; // barrel (slightly generous for demo feel)
};

// Aim reticle — yellow silhouette. Tilt is ALWAYS automatic from PCI height.
struct AimReticle {
    Vector3 pci = strikeZoneCenter;
    // RHB: barrel toward +X (1B). Fully driven by height — no manual override.
    float plateAngle = 0.15f;
    float plateAngleDisplay = 0.15f; // smoothed for silky UI / draw
};

// Map PCI height → bat silhouette tilt (radians) in the plate plane.
// Low aim → tip down / plane through (negative angle); high aim → tip up.
// (Flipped from the earlier uppercut mapping so the silhouette matches the swing plane.)
float plateAngleFromPciHeight(const Vector3& pci) {
    float t = (strikeZoneCenter.y - pci.y) / std::max(strikeZoneHalfHeight, 0.01f);
    t = clampf(t, -1.0f, 1.0f); // +1 bottom, −1 top
    float shaped = t >= 0.0f ? std::pow(t, 0.85f) : -std::pow(-t, 0.90f);
    // bottom ≈ −0.72, heart ≈ −0.12, top ≈ +0.48
    return -(shaped * 0.60f + 0.12f);
}

void updateReticleAngle(AimReticle& reticle, float dt = 1.0f / 60.0f) {
    float target = clampf(plateAngleFromPciHeight(reticle.pci), -0.85f, 0.95f);
    reticle.plateAngle = target; // snap for gameplay / swing bake
    // Smooth display angle so the yellow silhouette eases, not snaps.
    float a = 1.0f - std::exp(-clampf(dt, 0.0f, 0.05f) * 18.0f);
    reticle.plateAngleDisplay += (target - reticle.plateAngleDisplay) * a;
}

// Animated 3D bat used for swing mesh + collision.
struct BatPose {
    // RHB on 3B side (−X), facing pitcher (toward −Z / mound)
    Vector3 hands{-0.55f, 1.05f, plateZ - 0.25f};
    Vector3 axis{0.55f, 0.08f, -0.82f};
    float roll = 0.0f;
    float swingT = -1.0f; // <0 = not swinging (no mesh anim)
    float omega = 0.0f;
    Vector3 swingAxis{0, 1, 0};
    SwingType type = SwingType::Regular;
    Vector3 pci = strikeZoneCenter; // contact target locked at swing start
    bool locked = false;

    // Key poses frozen at swing start (load → contact → finish)
    Vector3 loadHands, loadAxis;
    Vector3 contactHands, contactAxis;
    Vector3 finishHands, finishAxis;
    Vector3 prevHands, prevAxis;
    float prevT = 0.0f;

    bool swinging() const {
        return swingT >= 0.0f && swingT < 1.0f;
    }
};

Vector3 batPoint(const BatPose& b, float s) {
    return b.hands + b.axis * s;
}

float batRadius(const BatConfig& c, float s) {
    float t = clampf(s / c.length, 0.0f, 1.0f);
    if (t < 0.42f) {
        return lerp(c.handleR, c.barrelR * 0.75f, t / 0.42f);
    }
    return lerp(c.barrelR * 0.75f, c.barrelR, (t - 0.42f) / 0.58f);
}

float sweetFactor(const BatConfig& c, float s, float widthScale) {
    float w = c.sweetWidth * widthScale;
    float d = std::abs(s - c.sweetFromKnob);
    float half = w * 0.5f;
    if (d <= half) {
        return 1.0f;
    }
    float extra = (d - half) / std::max(c.length * 0.35f, 0.01f);
    return clampf(1.0f - extra * 0.9f, 0.12f, 1.0f);
}

Vector3 slerpDir(const Vector3& a, const Vector3& b, float t) {
    Vector3 na = safeNorm(a);
    Vector3 nb = safeNorm(b);
    float d = clampf(na.dot(nb), -1.0f, 1.0f);
    if (d > 0.9995f) {
        return safeNorm(na + (nb - na) * t);
    }
    float omega = std::acos(d);
    float s0 = std::sin((1.0f - t) * omega) / std::sin(omega);
    float s1 = std::sin(t * omega) / std::sin(omega);
    return safeNorm(na * s0 + nb * s1);
}

// Snap animated bat pose so the sweet spot sits ON the PCI (aim point).
// Uses gameplay plateAngle (not smoothed) so contact keys match the reticle.
void orientBatFromReticle(BatPose& bat, const AimReticle& reticle, const BatConfig& cfg) {
    bat.pci = reticle.pci;
    float ang = reticle.plateAngle;
    // Barrel axis in the plate plane: tip direction from knob through sweet spot.
    // Small −Z lean so the face squares the incoming pitch (+Z travel).
    Vector3 dir(
        std::cos(ang),
        std::sin(ang),
        -0.14f - 0.05f * std::abs(ang)
    );
    bat.axis = safeNorm(dir, Vector3(1, 0, -0.15f));
    // Sweet spot (hands + axis * sweetFromKnob) == PCI exactly.
    bat.hands = reticle.pci - bat.axis * cfg.sweetFromKnob;
    bat.hands.z = plateZ - 0.12f; // slight depth in front of plate face
    // Keep PCI locked on plate plane for collision fairness.
    bat.pci.z = plateZ;
    Vector3 up(0, 1, 0);
    Vector3 side = safeNorm(up.cross(bat.axis), Vector3(1, 0, 0));
    bat.swingAxis = safeNorm(bat.axis.cross(side), up);
    bat.omega = 0.0f;
}

// Point velocity from finite difference of the animated bat path.
Vector3 batPointVelocity(const BatPose& bat, float s, float dt) {
    if (dt < 1e-5f || bat.prevT < 0.0f) {
        return (bat.swingAxis * bat.omega).cross(bat.axis * s);
    }
    Vector3 prevPt = bat.prevHands + bat.prevAxis * s;
    Vector3 curPt = bat.hands + bat.axis * s;
    return (curPt - prevPt) * (1.0f / dt);
}

void closestOnBat(
    const BatPose& bat,
    const BatConfig& cfg,
    const Vector3& p,
    float& outS,
    Vector3& outPt
) {
    Vector3 a = bat.hands;
    Vector3 b = batPoint(bat, cfg.length);
    Vector3 ab = b - a;
    float ab2 = ab.dot(ab);
    float t = ab2 > 1e-8f ? clampf((p - a).dot(ab) / ab2, 0.0f, 1.0f) : 0.0f;
    outPt = a + ab * t;
    outS = t * cfg.length;
}

// RHB path (body on 3B / −X): load → square at PCI angle → high wrap.
// Load/finish vertical shape follows contact attack angle (auto tilt).
void bakeSwingKeys(BatPose& bat) {
    bat.contactHands = bat.hands;
    bat.contactAxis = bat.axis;

    // tipUp > 0 = uppercut (low pitch); tipUp < 0 = down through (high pitch).
    float tipUp = clampf(bat.contactAxis.y, -0.65f, 0.75f);

    // Load: hands back toward batter/foul, tip cocked high (higher for uppercuts).
    bat.loadHands = bat.contactHands + Vector3(
        -0.24f,
        0.14f + std::max(0.0f, tipUp) * 0.16f,
        0.28f
    );
    bat.loadAxis = slerpDir(
        bat.contactAxis,
        safeNorm(Vector3(-0.38f, 0.78f + tipUp * 0.12f, 0.32f)),
        0.82f
    );

    // Finish: drive through to 1B / field; wrap rises with attack angle.
    bat.finishHands = bat.contactHands + Vector3(
        0.38f,
        0.08f + tipUp * 0.14f,
        -0.44f
    );
    bat.finishAxis = slerpDir(
        bat.contactAxis,
        safeNorm(Vector3(0.52f, 0.48f + tipUp * 0.18f, -0.48f)),
        0.88f
    );

    bat.swingAxis = safeNorm(bat.loadAxis.cross(bat.finishAxis), Vector3(0, 1, 0));
    if (bat.swingAxis.dot(Vector3(0, 1, 0)) < 0.0f) {
        bat.swingAxis = bat.swingAxis * -1.0f;
    }
}

// Evaluate swing pose at t∈[0,1]: load → whip accelerate → dwell square → finish.
void sampleSwingPose(
    const BatPose& keys,
    float t01,
    Vector3& outHands,
    Vector3& outAxis
) {
    t01 = clampf(t01, 0.0f, 1.0f);
    // Longer square dwell for forgiving, premium contact feel.
    const float tApproachEnd = 0.34f;
    const float tContactHold = 0.52f;
    if (t01 <= tApproachEnd) {
        float u = t01 / tApproachEnd;
        // Strong ease-in (slow load → violent whip into the zone)
        u = u * u * u * (1.6f - 0.6f * u);
        u = clampf(u, 0.0f, 1.0f);
        outHands = keys.loadHands + (keys.contactHands - keys.loadHands) * u;
        outAxis = slerpDir(keys.loadAxis, keys.contactAxis, u);
    } else if (t01 <= tContactHold) {
        float u = (t01 - tApproachEnd) / (tContactHold - tApproachEnd);
        u = smooth01(u) * 0.10f;
        outHands = keys.contactHands + (keys.finishHands - keys.contactHands) * u;
        outAxis = slerpDir(keys.contactAxis, keys.finishAxis, u);
    } else {
        float u = (t01 - tContactHold) / (1.0f - tContactHold);
        // Ease-out finish (barrel stays on plane, then wraps)
        u = 1.0f - (1.0f - u) * (1.0f - u);
        float u0 = 0.10f;
        float uu = u0 + (1.0f - u0) * u;
        outHands = keys.contactHands + (keys.finishHands - keys.contactHands) * uu;
        outAxis = slerpDir(keys.contactAxis, keys.finishAxis, uu);
    }
}

void startSwing(BatPose& bat) {
    if (bat.swinging()) {
        return;
    }
    bakeSwingKeys(bat);
    bat.swingT = 0.0f;
    bat.omega = 0.0f;
    bat.locked = true;
    bat.prevT = -1.0f;
    sampleSwingPose(bat, 0.0f, bat.hands, bat.axis);
    bat.prevHands = bat.hands;
    bat.prevAxis = bat.axis;
}

// Seamless load: keep contact on PCI, start load from the hands/grip pose.
void startSwingFromGrip(
    BatPose& bat,
    const AimReticle& reticle,
    const BatConfig& cfg,
    const Vector3& gripHands,
    const Vector3& gripAxis
) {
    if (bat.swinging()) {
        return;
    }
    orientBatFromReticle(bat, reticle, cfg);
    bakeSwingKeys(bat);
    // Anchor load to where the bat currently is in the hands.
    bat.loadHands = gripHands;
    bat.loadAxis = safeNorm(gripAxis, bat.loadAxis);
    bat.swingT = 0.0f;
    bat.omega = 0.0f;
    bat.locked = true;
    bat.prevT = -1.0f;
    sampleSwingPose(bat, 0.0f, bat.hands, bat.axis);
    bat.prevHands = bat.hands;
    bat.prevAxis = bat.axis;
}

void updateSwing(BatPose& bat, const SwingProfile& prof, float dt) {
    if (bat.swingT < 0.0f) {
        bat.omega = 0.0f;
        return;
    }

    bat.prevHands = bat.hands;
    bat.prevAxis = bat.axis;
    bat.prevT = bat.swingT;

    // Power shortens the swing (faster path).
    float dur = prof.duration / std::max(prof.power, 0.55f);
    bat.swingT = std::min(1.0f, bat.swingT + dt / dur);

    sampleSwingPose(bat, bat.swingT, bat.hands, bat.axis);

    // Angular rate from axis change (for impulse scaling).
    if (dt > 1e-5f && bat.prevT >= 0.0f) {
        float d = clampf(bat.prevAxis.dot(bat.axis), -1.0f, 1.0f);
        float ang = std::acos(d);
        bat.omega = ang / dt;
        // Also fold hand translation into effective tip speed via finite difference.
    } else {
        bat.omega = 0.0f;
    }

    if (bat.swingT >= 1.0f) {
        bat.omega = 0.0f;
        bat.swingT = -1.0f;
        bat.locked = false;
        bat.prevT = -1.0f;
    }
}

struct HitInfo {
    bool hit = false;
    float exitMph = 0;
    float launchDeg = 0;
    float batMph = 0;
    float sweet = 0;
    float s = 0;
    Vector3 point;
    Vector3 exitPos; // contact position for wall-clear re-sim
    Vector3 exitVel;
    // Filled at contact via 3D wall-clear trajectory; refined on wall/land.
    bool fair = true;
    bool clearsWall = false;
    bool hitsWallFace = false;
    float sprayDeg = 0;       // 0 = straight to CF (−Z), + = 1B, − = 3B
    float distanceFeet = 0;   // landing / wall distance from home
    float fenceFeet = 0;
    float heightAtFence = 0;  // ball Y at fence radius
    float wallMarginFeet = 0; // clear margin over wall top (ft)
    const char* quality = "Contact";
};

// HR anatomy (chart target):
//   ~96% of HRs: LA 20–40°, EV often 100–115 mph
//   Moonballs:   LA 25–29°, EV 105–109 mph, avg ~437 ft
//   Jaw-droppers: ~114–118 mph @ ~29–31° → 500+ ft
//   Too low:     LA ≲ 15–18° rarely clears unless laser over short porch
//
// Wall clear is 3D: ball must cross fence radius ABOVE wall top height,
// not merely land past the fence distance on the ground.
void classifyHit(
    HitInfo& h,
    const Vector3& velocity,
    const Vector3& position,
    const Stadium3D::Layout& park
) {
    float horiz = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    h.launchDeg = std::atan2(velocity.y, std::max(horiz, 1e-4f)) * kDeg;
    h.exitMph = velocity.magnitude() * 2.236936f;
    h.sprayDeg = std::atan2(velocity.x, -velocity.z) * kDeg;
    h.fair = std::abs(h.sprayDeg) <= park.foulAngleDegrees + 0.5f;
    h.exitPos = position;
    h.exitVel = velocity;

    // Match post-contact atmosphere: density 0.07, Cd 0.35, scale 0.95, r=0.065, m=0.145
    // k ≈ 0.5 * rho * Cd * A * scale / m
    constexpr float dragK = 0.012f;
    Stadium3D::WallClearResult clear =
        Stadium3D::evaluateWallClear(park, position, velocity, -9.8f, dragK);

    h.fair = clear.fair;
    h.clearsWall = clear.clearsWall;
    h.hitsWallFace = clear.hitsWallFace;
    h.sprayDeg = clear.sprayDeg;
    h.fenceFeet = clear.fenceFeet;
    h.heightAtFence = clear.heightAtFence;
    h.wallMarginFeet = clear.marginFeet;
    // Distance is always a non-negative ground-range from home.
    float fallbackR = std::sqrt(
        position.x * position.x + (position.z - plateZ) * (position.z - plateZ)
    ) * feetPerWorldUnit;
    h.distanceFeet = clear.landFeet > 0.5f ? clear.landFeet : fallbackR;
    if (!(h.distanceFeet >= 0.0f) || h.distanceFeet > 900.0f) {
        h.distanceFeet = fallbackR;
    }
    h.distanceFeet = std::max(0.0f, h.distanceFeet);

    if (!h.fair) {
        h.quality = "Foul";
        return;
    }

    const bool clearsWall = h.clearsWall;

    // Quality buckets from the chart + true 3D wall clear.
    if (h.hitsWallFace) {
        h.quality = (h.exitMph >= 95.0f) ? "Wall Ball" : "Off the Wall";
        h.distanceFeet = h.fenceFeet;
        return;
    }

    if (h.launchDeg < 10.0f) {
        h.quality = "Grounder";
    } else if (h.launchDeg < 18.0f) {
        h.quality = (h.exitMph >= 100.0f) ? "Line Drive" : "Soft Liner";
        if (clearsWall && h.exitMph >= 105.0f && h.launchDeg >= 15.0f) {
            h.quality = "Home Run"; // laser over porch
        }
    } else if (h.launchDeg <= 40.0f) {
        bool moon =
            h.launchDeg >= 25.0f && h.launchDeg <= 32.0f && h.exitMph >= 104.0f;
        bool jaw =
            h.exitMph >= 112.0f && h.launchDeg >= 27.0f && h.launchDeg <= 34.0f &&
            h.distanceFeet >= 480.0f;
        if (jaw && clearsWall) {
            h.quality = "Jaw Dropper";
        } else if (moon && clearsWall && h.distanceFeet >= 400.0f) {
            h.quality = "Moonball";
        } else if (clearsWall && h.exitMph >= 95.0f) {
            h.quality = "Home Run";
        } else if (h.exitMph >= 90.0f) {
            h.quality = "Fly Ball";
        } else {
            h.quality = "Flare";
        }
    } else if (h.launchDeg <= 50.0f) {
        if (clearsWall && h.exitMph >= 100.0f) {
            h.quality = "Moonball";
        } else {
            h.quality = "Pop Up";
        }
    } else {
        h.quality = "Weak Pop";
    }
}

// Project landing distance with pure gravity (for immediate post-contact readout).
float projectLandingDistanceFeet(const Vector3& pos, const Vector3& vel) {
    if (vel.y <= 0.0f && pos.y <= 0.2f) {
        float dx = pos.x;
        float dz = pos.z - plateZ;
        return std::sqrt(dx * dx + dz * dz) * feetPerWorldUnit;
    }
    // Solve y + vy t - 0.5 g t^2 = 0.05
    float g = 9.8f;
    float y0 = pos.y - 0.05f;
    float disc = vel.y * vel.y + 2.0f * g * y0;
    if (disc < 0.0f) {
        disc = 0.0f;
    }
    float t = (vel.y + std::sqrt(disc)) / g;
    t = std::max(t, 0.05f);
    Vector3 land = pos + Vector3(vel.x * t, 0.0f, vel.z * t);
    land.y = 0.05f;
    float dx = land.x;
    float dz = land.z - plateZ;
    return std::sqrt(dx * dx + dz * dz) * feetPerWorldUnit;
}

HitInfo tryHit(
    Body3D& ball,
    const BatPose& bat,
    const BatConfig& cfg,
    const SwingProfile& prof,
    bool& hasHit,
    float dt,
    bool practiceMode,
    float contactEase = 1.0f // 1 = normal easy, higher = fatter sweet (derby difficulty)
) {
    HitInfo h;
    if (hasHit || !bat.swinging()) {
        return h;
    }

    float s = 0.0f;
    Vector3 closest;
    closestOnBat(bat, cfg, ball.position, s, closest);

    // Fatter collision + sweet magnet. contactEase scales both practice and derby.
    contactEase = clampf(contactEase, 0.55f, 1.55f);
    float rScale = (practiceMode ? 2.9f : 1.95f) * contactEase;
    float rBat = batRadius(cfg, s) * rScale;
    Vector3 delta = ball.position - closest;
    float dist = delta.magnitude();
    float minD = ball.radius + rBat;

    // Timing window aligns with barrel dwell (sampleSwingPose contact ~0.34–0.52).
    // Also magnet toward the locked PCI (where the yellow reticle was aimed).
    Vector3 sweetPt = batPoint(bat, cfg.sweetFromKnob);
    Vector3 aimPt = bat.pci;
    aimPt.z = plateZ;
    float dSweet = (ball.position - sweetPt).magnitude();
    float dAim = (ball.position - aimPt).magnitude();
    float nearPlate = std::abs(ball.position.z - plateZ);
    bool contactWindow = bat.swingT >= 0.28f && bat.swingT <= 0.58f;
    float magnetR = (practiceMode ? 0.36f : 0.28f) * contactEase;
    float plateBand = (practiceMode ? 0.70f : 0.58f) * contactEase;
    // Prefer magnet when ball is near aim OR near barrel sweet — prediction feel.
    bool nearAim = dAim < magnetR * 1.15f || dSweet < magnetR;
    if (contactWindow && nearPlate < plateBand && nearAim) {
        // Pull toward the PCI first (what the player aimed at), then sweet on bat.
        Vector3 target = aimPt * 0.55f + sweetPt * 0.45f;
        if (practiceMode) {
            closest = target;
            s = cfg.sweetFromKnob;
            delta = ball.position - closest;
            dist = std::max(delta.magnitude(), 1e-4f);
            minD = ball.radius + rBat;
            Vector3 nSnap = delta * (1.0f / dist);
            ball.position = closest + nSnap * (ball.radius + batRadius(cfg, s));
            delta = ball.position - closest;
            dist = delta.magnitude();
        } else {
            float pull = clampf(0.60f + 0.22f * contactEase, 0.50f, 0.88f);
            // Stronger pull when ball is closer to the reticle aim.
            float aimBonus = clampf(1.0f - dAim / (magnetR * 1.4f), 0.0f, 1.0f);
            pull = clampf(pull + 0.12f * aimBonus, 0.50f, 0.92f);
            closest = closest * (1.0f - pull) + target * pull;
            s = lerp(s, cfg.sweetFromKnob, pull);
            delta = ball.position - closest;
            dist = std::max(delta.magnitude(), 1e-4f);
            minD = ball.radius + rBat;
        }
    }

    if (dist > minD || dist < 1e-5f) {
        return h;
    }

    // Geometric normal: bat surface → ball center (where you hit on the ball).
    Vector3 n = delta * (1.0f / dist);
    // Orient so n faces the swing (outfield side when squared up).
    Vector3 vBat = batPointVelocity(bat, s, dt);
    if (vBat.magnitude() > 0.5f) {
        Vector3 batDir = safeNorm(vBat);
        if (n.dot(batDir) < 0.0f) {
            n = n * -1.0f;
        }
    } else if (n.dot(Vector3(0, 0, -1)) < 0.0f) {
        n = n * -1.0f;
    }

    // Vertical contact quality: geometry + bat attack angle (auto tilt from aim height).
    // Low aim (uppercut axis) → more loft; high aim → more line drive / top spin.
    float undercut = clampf((ball.position.y - closest.y) / std::max(ball.radius, 0.02f), -1.2f, 1.2f);
    undercut = clampf(undercut + bat.axis.y * 0.55f, -1.3f, 1.4f);

    Vector3 vRel = ball.velocity - vBat;
    float approach = vRel.dot(n);
    // Grace so near-zero approach still registers (friendlier timing).
    if (approach >= (practiceMode ? 2.5f : 1.4f)) {
        return h;
    }
    if (approach > -4.0f) {
        approach = -std::max(5.0f, vBat.magnitude() * 0.32f);
        vRel = n * approach + (vRel - n * vRel.dot(n));
    }

    float sweetScale = practiceMode ? prof.sweetScale * 1.60f : prof.sweetScale * 1.28f;
    float sweet = sweetFactor(cfg, s, sweetScale);
    if (practiceMode) {
        sweet = clampf(sweet + 0.14f, 0.0f, 1.0f);
    } else {
        sweet = clampf(sweet + 0.06f * contactEase, 0.0f, 1.0f);
    }
    float cor = clampf(lerp(cfg.minCor, cfg.maxCor + prof.corBonus * 0.5f, sweet), 0.32f, 0.56f);
    float tip = clampf(s / cfg.length, 0.0f, 1.0f);
    float mEff =
        cfg.mass * prof.massScale * lerp(1.35f, 0.55f, tip) * lerp(0.70f, 1.08f, sweet);
    float mBall = std::max(ball.mass, 0.05f);

    float j = -(1.0f + cor) * approach / (1.0f / mBall + 1.0f / mEff);
    j *= lerp(0.95f, 1.14f, sweet) * lerp(0.96f, 1.10f, (prof.power - 0.7f) / 0.55f);
    if (practiceMode) {
        j *= 1.14f;
    }
    ball.velocity = ball.velocity + n * (j / mBall);

    Vector3 tanAxis = safeNorm(n.cross(Vector3(0, 1, 0)), Vector3(1, 0, 0));
    Vector3 bitangent = safeNorm(n.cross(tanAxis), Vector3(0, 1, 0));
    for (const Vector3& t : {tanAxis, bitangent}) {
        float vt = (ball.velocity - vBat).dot(t);
        float mu = 0.16f * lerp(0.7f, 1.0f, sweet);
        float jt = clampf(-vt * mBall * mu, -std::abs(j) * 0.18f, std::abs(j) * 0.18f);
        ball.velocity = ball.velocity + t * (jt / mBall);
    }

    if (vBat.magnitude() > 0.8f) {
        float batMph = vBat.magnitude() * 2.236936f;
        float tipScale = clampf(batMph / 78.0f, 0.45f, 1.25f);
        float carry = lerp(0.14f, 0.34f, sweet) * tipScale;
        if (practiceMode) {
            carry *= 1.12f;
        }
        float alongN = std::max(0.0f, safeNorm(vBat).dot(n));
        ball.velocity = ball.velocity + n * (vBat.magnitude() * alongN * carry);
    }

    // Shape exit toward HR-derby chart: EV ~90–110 mph, LA 15–35° for barrels.
    // Hard cap prevents 160+ mph teleports / physics blowups.
    {
        Vector3 raw = ball.velocity;
        // Prefer spray toward PCI-relative aim (bat face), not pure raw scatter.
        Vector3 towardOut = safeNorm(Vector3(n.x, 0.0f, n.z), Vector3(0, 0, -1));
        if (towardOut.dot(Vector3(0, 0, -1)) < 0.15f) {
            // Ensure mostly outfield-ward (toward −Z / CF from plate).
            towardOut = safeNorm(towardOut + Vector3(0, 0, -0.85f), Vector3(0, 0, -1));
        }
        float spray = std::atan2(towardOut.x, -towardOut.z);
        // Soft clamp spray to fair-ish for solid contact; mishits can foul.
        float maxSpray = sweet > 0.55f ? 0.70f : 1.15f; // rad ≈ 40° / 66°
        spray = clampf(spray, -maxSpray, maxSpray);

        float q = std::pow(clampf(sweet, 0.0f, 1.0f), 0.85f);

        float desiredMph = lerp(52.0f, 106.0f, q);
        desiredMph *= lerp(0.94f, 1.05f, (prof.power - 0.7f) / 0.55f);
        if (practiceMode) {
            desiredMph = lerp(68.0f, 102.0f, std::pow(clampf(sweet, 0.0f, 1.0f), 0.72f));
        }
        if (sweet < 0.35f) {
            desiredMph = std::min(desiredMph, lerp(42.0f, 72.0f, sweet / 0.35f));
        }
        desiredMph = clampf(desiredMph, 35.0f, 112.0f); // hard product cap

        // Undercut → loft; on top → line drive. Keep within realistic band.
        float desiredLA = 10.0f + undercut * 14.0f + sweet * 7.0f;
        desiredLA = clampf(desiredLA, -8.0f, 38.0f);
        if (sweet > 0.75f && undercut > 0.12f) {
            desiredLA = clampf(desiredLA, 16.0f, 34.0f);
        }
        if (sweet < 0.40f) {
            desiredLA = lerp(desiredLA, 5.0f, 0.50f);
        }

        float speed = mphToWorldUnitsPerSecond(desiredMph);
        float la = desiredLA * pi / 180.0f;
        Vector3 horizDir(std::sin(spray), 0.0f, -std::cos(spray));
        horizDir = safeNorm(horizDir, Vector3(0, 0, -1));
        Vector3 chartVel =
            horizDir * (speed * std::cos(la)) + Vector3(0.0f, speed * std::sin(la), 0.0f);

        float blend = lerp(0.45f, 0.92f, q);
        if (practiceMode) {
            blend = lerp(0.50f, 0.94f, q);
        }
        ball.velocity = raw * (1.0f - blend) + chartVel * blend;

        // Final speed clamp (world units/s).
        float spd = ball.velocity.magnitude();
        const float maxSpd = mphToWorldUnitsPerSecond(114.0f);
        if (spd > maxSpd) {
            ball.velocity = ball.velocity * (maxSpd / spd);
        }
    }

    // Small separation only — never large teleports off the bat.
    float sep = std::min(minD - dist + 0.004f, 0.12f);
    ball.position = ball.position + n * sep;
    ball.restitution = 0.0f;

    hasHit = true;
    h.hit = true;
    h.s = s;
    h.sweet = sweet;
    h.point = closest;
    h.batMph = vBat.magnitude() * 2.236936f;
    h.exitMph = ball.velocity.magnitude() * 2.236936f;
    float horiz = std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.z * ball.velocity.z);
    h.launchDeg = std::atan2(ball.velocity.y, std::max(horiz, 1e-4f)) * kDeg;
    classifyHit(h, ball.velocity, ball.position, Stadium3D::defaultPlayLayout());
    return h;
}

// ── Meshes ──────────────────────────────────────────────────────────────

Mesh3D makeBatMesh(const BatConfig& cfg) {
    Mesh3D mesh;
    const int slices = 20;
    const int along = 28;
    for (int i = 0; i <= along; i++) {
        float t = static_cast<float>(i) / along;
        float s = t * cfg.length;
        float r = batRadius(cfg, s);
        for (int j = 0; j < slices; j++) {
            float a = (static_cast<float>(j) / slices) * pi * 2.0f;
            Vector3 n(std::cos(a), 0.0f, std::sin(a));
            mesh.vertices.push_back(n * r + Vector3(0.0f, s, 0.0f));
            mesh.vertexNormals.push_back(n);
        }
    }
    for (int i = 0; i < along; i++) {
        for (int j = 0; j < slices; j++) {
            int j1 = (j + 1) % slices;
            int a = i * slices + j;
            int b = i * slices + j1;
            int c = (i + 1) * slices + j;
            int d = (i + 1) * slices + j1;
            mesh.triangles.push_back({a, c, b});
            mesh.triangles.push_back({b, c, d});
            bool barrel = static_cast<float>(i) / along > 0.4f;
            sf::Color col = barrel ? sf::Color(210, 150, 70) : sf::Color(175, 120, 60);
            mesh.triangleColors.push_back(col);
            mesh.triangleColors.push_back(col);
        }
    }
    mesh.rebuildNormals();
    return mesh;
}

// Row-major: x' = m0 x + m1 y + m2 z + m3 — local +Y → bat.axis
Matrix4 batModelMatrix(const BatPose& bat) {
    Vector3 y = bat.axis;
    Vector3 x = safeNorm(Vector3(0, 1, 0).cross(y), Vector3(1, 0, 0));
    if (x.magnitude() < 0.2f) {
        x = safeNorm(Vector3(1, 0, 0).cross(y));
    }
    Vector3 z = safeNorm(x.cross(y));
    x = safeNorm(y.cross(z));
    float c = std::cos(bat.roll);
    float s = std::sin(bat.roll);
    Vector3 xr = x * c + z * s;
    Vector3 zr = z * c - x * s;

    Matrix4 r = Matrix4::identity();
    r.values[0] = xr.x;
    r.values[4] = xr.y;
    r.values[8] = xr.z;
    r.values[1] = y.x;
    r.values[5] = y.y;
    r.values[9] = y.z;
    r.values[2] = zr.x;
    r.values[6] = zr.y;
    r.values[10] = zr.z;
    return Matrix4::translation(bat.hands) * r;
}

// Gravity-only launch aimed at the strike zone — same idea as the pitching
// iterative solver (no Magnus here so path stays readable for batting).
Vector3 launchVelocityTowardPlate(
    const Vector3& start,
    const Vector3& aim,
    float pitchSpeedMph
) {
    float pitchSpeed = mphToWorldUnitsPerSecond(pitchSpeedMph);
    float distance = std::max(1.0f, aim.z - start.z);
    float flightTime = distance / std::max(1.0f, pitchSpeed * 0.94f);

    float vx = (aim.x - start.x) / flightTime;
    float vy = (aim.y - start.y + 0.5f * 9.8f * flightTime * flightTime) / flightTime;

    auto assemble = [&](float xv, float yv) {
        float maxSide = pitchSpeed * 0.22f;
        xv = clampf(xv, -maxSide, maxSide);
        yv = clampf(yv, pitchSpeed * std::tan(-12.0f * pi / 180.0f), pitchSpeed * std::tan(24.0f * pi / 180.0f));
        float lat2 = xv * xv + yv * yv;
        float maxLat = pitchSpeed * 0.55f;
        if (lat2 > maxLat * maxLat && lat2 > 1e-8f) {
            float sc = maxLat / std::sqrt(lat2);
            xv *= sc;
            yv *= sc;
            lat2 = xv * xv + yv * yv;
        }
        float zv = std::sqrt(std::max(pitchSpeed * pitchSpeed - lat2, pitchSpeed * pitchSpeed * 0.55f));
        return Vector3(xv, yv, zv);
    };

    Vector3 vel = assemble(vx, vy);

    // 1–2 correction passes with gravity integration (matches pitching feel).
    for (int iter = 0; iter < 6; iter++) {
        Vector3 p = start;
        Vector3 v = vel;
        Vector3 prev = p;
        bool crossed = false;
        Vector3 plateHit = p;
        for (int step = 0; step < 900; step++) {
            prev = p;
            v.y -= 9.8f * fixedStep;
            p = p + v * fixedStep;
            if (p.z >= plateZ && v.z > 0.0f) {
                float seg = p.z - prev.z;
                float t = seg <= 1e-6f ? 1.0f : (plateZ - prev.z) / seg;
                t = clampf(t, 0.0f, 1.0f);
                plateHit = prev + (p - prev) * t;
                plateHit.z = plateZ;
                crossed = true;
                break;
            }
            if (p.y < -1.0f || p.z > plateZ + 8.0f) {
                break;
            }
        }
        if (!crossed) {
            vel = assemble(vel.x, vel.y + 1.0f);
            continue;
        }
        float errX = aim.x - plateHit.x;
        float errY = aim.y - plateHit.y;
        if (errX * errX + errY * errY < 0.0004f) {
            break;
        }
        float T = clampf(distance / std::max(1.0f, vel.z * 0.94f), 0.25f, 1.2f);
        vel = assemble(vel.x + errX / T * 0.92f, vel.y + errY / T * 0.92f);
    }
    return vel;
}

// Coach soft toss: pick flight time and solve so the ball reaches aim under gravity.
// Unlike launchVelocityTowardPlate (full-mound speed clamps), this always hits the
// plate by construction — no post-hoc vy/vz hacks.
Vector3 softTossVelocity(const Vector3& start, const Vector3& aim, float flightTimeSec) {
    // Allow longer arcs so derby soft toss is readable (time to load & swing).
    float T = clampf(flightTimeSec, 0.55f, 1.55f);
    float dx = aim.x - start.x;
    float dy = aim.y - start.y;
    float dz = aim.z - start.z;
    // Must travel +Z to the plate.
    if (dz < 0.5f) {
        dz = 0.5f;
    }
    float vx = dx / T;
    float vz = dz / T;
    float vy = (dy + 0.5f * 9.8f * T * T) / T;
    return Vector3(vx, vy, vz);
}

// Inverse of Camera3D::projectPoint (same FOV scale model).
// Mouse maps to the plate plane so the yellow PCI sits under the cursor.
Vector3 mouseToPci(const Camera3D& cam, float mx, float my, float sw, float sh) {
    // Camera-space ray through pixel (matches project: sx = sw/2 + cx * fov/z).
    float fov = std::max(cam.fieldOfView, 1.0f);
    float cx = (mx - sw * 0.5f) / fov;
    float cy = -(my - sh * 0.5f) / fov;
    Vector3 dirCam = safeNorm(Vector3(cx, cy, 1.0f), Vector3(0, 0, 1));

    // Camera → world: undo worldToCamera rotations (Z then X then Y positive).
    auto rotX = [](const Vector3& v, float a) {
        float c = std::cos(a), s = std::sin(a);
        return Vector3(v.x, c * v.y - s * v.z, s * v.y + c * v.z);
    };
    auto rotY = [](const Vector3& v, float a) {
        float c = std::cos(a), s = std::sin(a);
        return Vector3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
    };
    auto rotZ = [](const Vector3& v, float a) {
        float c = std::cos(a), s = std::sin(a);
        return Vector3(c * v.x - s * v.y, s * v.x + c * v.y, v.z);
    };
    Vector3 dir = dirCam;
    dir = rotZ(dir, cam.rotation.z);
    dir = rotX(dir, cam.rotation.x);
    dir = rotY(dir, cam.rotation.y);
    dir = safeNorm(dir, Vector3(0, 0, -1));

    const float planeZ = plateZ;
    if (std::abs(dir.z) < 1e-5f) {
        return strikeZoneCenter;
    }
    float t = (planeZ - cam.position.z) / dir.z;
    if (t < 0.05f) {
        // Ray not hitting plate plane in front — keep last center.
        return strikeZoneCenter;
    }
    Vector3 hit = cam.position + dir * t;
    // Soft pad outside zone so edge pitches are aimable, still clamped.
    const float padX = 1.08f;
    const float padY = 1.08f;
    hit.x = clampf(hit.x, -strikeZoneHalfWidth * padX, strikeZoneHalfWidth * padX);
    hit.y = clampf(
        hit.y,
        strikeZoneCenter.y - strikeZoneHalfHeight * padY,
        strikeZoneCenter.y + strikeZoneHalfHeight * padY
    );
    hit.z = planeZ;
    return hit;
}

// Predict where a free-flying ball will cross the plate plane (z = plateZ).
// Returns false if it won't reach the plate soon.
bool predictPlateCrossing(
    const Vector3& pos,
    const Vector3& vel,
    Vector3& outCross,
    float gravityY = -9.8f
) {
    if (vel.z < 0.15f && pos.z > plateZ - 0.5f) {
        return false;
    }
    // Analytic time to plate under constant vz (drag neglected for short soft-toss).
    float dz = plateZ - pos.z;
    if (std::abs(vel.z) < 1e-4f) {
        return false;
    }
    float t = dz / vel.z;
    if (t < -0.05f || t > 2.5f) {
        return false;
    }
    t = std::max(t, 0.0f);
    outCross.x = pos.x + vel.x * t;
    outCross.y = pos.y + vel.y * t + 0.5f * gravityY * t * t;
    outCross.z = plateZ;
    return true;
}

// Yellow aim reticle — always a flat plate-plane outline + small sweet circle.
// Does NOT follow the swing; shows where you intended to make contact.
void drawBatReticle(
    sf::RenderWindow& window,
    const Camera3D& cam,
    const AimReticle& reticle,
    const BatConfig& cfg
) {
    const sf::Color yellow(255, 230, 40, 255);
    const sf::Color yellowSoft(255, 230, 40, 90);

    const int segs = 32;
    float sw = static_cast<float>(window.getSize().x);
    float sh = static_cast<float>(window.getSize().y);

    // Use smoothed display angle so the yellow bat eases with aim height.
    float ang = reticle.plateAngleDisplay;
    Vector3 axisFlat = safeNorm(
        Vector3(std::cos(ang), std::sin(ang), 0.0f),
        Vector3(1, 0, 0)
    );
    Vector3 side(-axisFlat.y, axisFlat.x, 0.0f);
    Vector3 knob = reticle.pci - axisFlat * cfg.sweetFromKnob;
    knob.z = plateZ;
    Vector3 sweetWorld = reticle.pci;
    sweetWorld.z = plateZ;

    std::vector<sf::Vector2f> L, R;
    L.reserve(segs + 1);
    R.reserve(segs + 1);
    for (int i = 0; i <= segs; i++) {
        float t = static_cast<float>(i) / segs;
        float s = t * cfg.length;
        float r;
        if (t < 0.28f) {
            r = lerp(0.016f, 0.026f, t / 0.28f);
        } else if (t < 0.55f) {
            r = lerp(0.026f, 0.048f, (t - 0.28f) / 0.27f);
        } else {
            r = lerp(0.048f, 0.055f, (t - 0.55f) / 0.45f);
        }
        Vector3 c = knob + axisFlat * s;
        c.z = plateZ;
        ProjectedPoint3D pl = cam.projectPoint(c + side * r, sw, sh);
        ProjectedPoint3D pr = cam.projectPoint(c - side * r, sw, sh);
        if (pl.visible) {
            L.push_back({pl.position.x, pl.position.y});
        }
        if (pr.visible) {
            R.push_back({pr.position.x, pr.position.y});
        }
    }

    auto thickSeg = [&](sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color c) {
        sf::Vector2f d = b - a;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 1e-3f) {
            return;
        }
        sf::Vector2f n(-d.y / len, d.x / len);
        sf::Vector2f o = n * (thickness * 0.5f);
        sf::Vertex q[] = {
            sf::Vertex{a + o, c},
            sf::Vertex{a - o, c},
            sf::Vertex{b - o, c},
            sf::Vertex{b + o, c}
        };
        window.draw(q, 4, sf::PrimitiveType::TriangleFan);
    };

    auto thickPoly = [&](const std::vector<sf::Vector2f>& pts, float thickness, sf::Color c) {
        for (size_t i = 1; i < pts.size(); i++) {
            thickSeg(pts[i - 1], pts[i], thickness, c);
        }
    };

    // Closed yellow outline only.
    if (L.size() >= 2 && R.size() >= 2) {
        auto drawEndCap = [&](sf::Vector2f a, sf::Vector2f b, sf::Vector2f outward, float thickness, sf::Color c) {
            sf::Vector2f mid((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
            sf::Vector2f ab = b - a;
            float half = 0.5f * std::sqrt(ab.x * ab.x + ab.y * ab.y);
            float olen = std::sqrt(outward.x * outward.x + outward.y * outward.y);
            if (half < 0.5f || olen < 1e-4f) {
                thickSeg(a, b, thickness, c);
                return;
            }
            sf::Vector2f ax(outward.x / olen, outward.y / olen);
            sf::Vector2f perp(-ax.y, ax.x);
            sf::Vector2f toA = a - mid;
            float sideSign = (toA.x * perp.x + toA.y * perp.y) >= 0.0f ? 1.0f : -1.0f;
            sf::Vector2f prev = a;
            const int n = 10;
            for (int i = 1; i <= n; i++) {
                float t = static_cast<float>(i) / n;
                float ang = pi * t;
                sf::Vector2f pt =
                    mid + perp * (sideSign * half * std::cos(ang)) + ax * (half * std::sin(ang));
                thickSeg(prev, pt, thickness, c);
                prev = pt;
            }
        };

        sf::Vector2f tipMid((L.back().x + R.back().x) * 0.5f, (L.back().y + R.back().y) * 0.5f);
        sf::Vector2f knobMid((L.front().x + R.front().x) * 0.5f, (L.front().y + R.front().y) * 0.5f);
        sf::Vector2f axisScreen = tipMid - knobMid;

        // Thin dark under-stroke so yellow reads on any background.
        sf::Color under(0, 0, 0, 150);
        thickPoly(L, 4.5f, under);
        thickPoly(R, 4.5f, under);
        drawEndCap(L.back(), R.back(), axisScreen, 4.5f, under);
        drawEndCap(L.front(), R.front(), -axisScreen, 4.5f, under);

        // Bright yellow outline.
        thickPoly(L, 2.6f, yellow);
        thickPoly(R, 2.6f, yellow);
        drawEndCap(L.back(), R.back(), axisScreen, 2.6f, yellow);
        drawEndCap(L.front(), R.front(), -axisScreen, 2.6f, yellow);
    }

    // Zone frame (helps read aim vs zone heart).
    {
        float hw = strikeZoneHalfWidth;
        float hh = strikeZoneHalfHeight;
        Vector3 zc = strikeZoneCenter;
        Vector3 corners[4] = {
            Vector3(zc.x - hw, zc.y - hh, plateZ),
            Vector3(zc.x + hw, zc.y - hh, plateZ),
            Vector3(zc.x + hw, zc.y + hh, plateZ),
            Vector3(zc.x - hw, zc.y + hh, plateZ)
        };
        sf::Vector2f sc[4];
        bool ok = true;
        for (int i = 0; i < 4; i++) {
            ProjectedPoint3D p = cam.projectPoint(corners[i], sw, sh);
            if (!p.visible) {
                ok = false;
                break;
            }
            sc[i] = {p.position.x, p.position.y};
        }
        if (ok) {
            sf::Color zone(255, 255, 255, 55);
            for (int i = 0; i < 4; i++) {
                thickSeg(sc[i], sc[(i + 1) % 4], 1.6f, zone);
            }
            // Heart crosshair
            ProjectedPoint3D c = cam.projectPoint(zc, sw, sh);
            if (c.visible) {
                thickSeg(
                    {c.position.x - 7.0f, c.position.y},
                    {c.position.x + 7.0f, c.position.y},
                    1.4f, zone
                );
                thickSeg(
                    {c.position.x, c.position.y - 7.0f},
                    {c.position.x, c.position.y + 7.0f},
                    1.4f, zone
                );
            }
        }
    }

    // Small solid yellow circle = best contact (sweet spot / PCI).
    ProjectedPoint3D sp = cam.projectPoint(sweetWorld, sw, sh);
    if (sp.visible) {
        constexpr float pxR = 5.5f; // small — not a big reticle
        sf::CircleShape soft(pxR + 2.0f);
        soft.setOrigin({pxR + 2.0f, pxR + 2.0f});
        soft.setPosition({sp.position.x, sp.position.y});
        soft.setFillColor(yellowSoft);
        window.draw(soft);

        sf::CircleShape core(pxR);
        core.setOrigin({pxR, pxR});
        core.setPosition({sp.position.x, sp.position.y});
        core.setFillColor(yellow);
        core.setOutlineThickness(1.2f);
        core.setOutlineColor(sf::Color(40, 35, 10, 220));
        window.draw(core);
    }
}

} // namespace

int main() {
    sf::ContextSettings glSettings;
    glSettings.depthBits = 24;
    glSettings.stencilBits = 8;
    glSettings.antiAliasingLevel = 4;

    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "HR Derby | soft toss + bat physics",
        sf::Style::Default,
        sf::State::Windowed,
        glSettings
    );
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    DemoFpsCounter fps("HR Derby | shared pitcher + ball + field");
    sf::Font font;
    bool fontOk = loadUiFont(font);

    // Start in catcher view; switches to ball-follow after a hit.
    Camera3D camera;
    applyCatcherCamera(camera);
    bool followBallCam = false;
    // Broadcast package: plate → chase → hold land → return to plate.
    enum class BroadcastCam { Plate = 0, Chase, HoldLand, ReturnPlate };
    BroadcastCam broadcastCam = BroadcastCam::Plate;
    float holdLandTimer = 0.0f;
    float returnPlateTimer = 0.0f;
    float returnPlateDuration = 0.55f;
    Vector3 holdLandPos(0.0f, 0.5f, plateZ - 20.0f);
    float camShakeTimer = 0.0f;
    float camShakeIntensity = 0.0f;
    float fovPunch = 0.0f; // barrel FOV kick on contact
    float hitFreezeTimer = 0.0f; // brief hit-stop for premium contact feel
    float contactFlash = 0.0f;   // full-screen white flash (0..1)
    // Result callout card (HR / wall / fly / foul / miss) — always clear.
    float resultBannerTimer = 0.0f;
    std::string resultBannerTitle;
    std::string resultBannerSub;
    sf::Color resultBannerCol = sf::Color(255, 220, 80);

    // Same assets as pitching sim
    Mesh3D baseballMesh = BaseballVisual3D::makeMesh(56, 112);
    // Highest procedural detail (CharacterModel3D High); glTF overrides if present.
    SkinnedModel3D pitcherModel = loadCharacterOrProcedural("pitcher", false, 2);
    // Batter silhouette at the plate for scale / product feel (faces mound / −Z).
    SkinnedModel3D batterModel =
        CharacterModel3D::build(CharacterModel3D::Role::Athlete, CharacterModel3D::Detail::High);
    AnimationClip deliveryClip;
    if (const AnimationClip* c = pitcherModel.findClip("throw_preview")) {
        deliveryClip = *c;
    } else if (const AnimationClip* c = pitcherModel.findClip("yamamoto_windup")) {
        deliveryClip = *c;
    } else {
        deliveryClip = BaseballAnims::yamamotoWindup(pitcherModel);
    }
    AnimationClip idleClip;
    if (const AnimationClip* c = pitcherModel.findClip("idle")) {
        idleClip = *c;
    } else {
        idleClip = BaseballAnims::pitcherIdle(pitcherModel);
    }
    // RHB stance loop + swing keyed to bat.swingT (contact ≈ 0.42).
    AnimationClip batterStanceClip = BaseballAnims::batterStance(batterModel);
    AnimationClip batterSwingClip = BaseballAnims::batterSwing(batterModel);

    SkeletonAnimator pitcherAnim;
    pitcherAnim.setModel(pitcherModel);
    pitcherAnim.applyClip(idleClip, 0.0f, true);
    Mesh3D pitcherMesh = pitcherModel.skinToMesh(pitcherAnim.skinMatrices());

    SkeletonAnimator batterAnim;
    batterAnim.setModel(batterModel);
    batterAnim.applyClip(batterStanceClip, 0.0f, true);
    Mesh3D batterMesh = batterModel.skinToMesh(batterAnim.skinMatrices());

    BatConfig batCfg;
    AimReticle reticle;
    updateReticleAngle(reticle, 1.0f);
    reticle.plateAngleDisplay = reticle.plateAngle;
    BatPose bat;
    orientBatFromReticle(bat, reticle, batCfg);
    // Last grip pose from hands (stance) — seamless swing start.
    Vector3 lastGripHands = bat.hands;
    Vector3 lastGripAxis = bat.axis;
    Mesh3D batMesh = makeBatMesh(batCfg);

    GlRenderer gl;
    bool useGL = gl.initialize(window);
    GlMesh glPitcher;
    GlMesh glBatter;
    GlMesh glBall;
    GlMesh glBat;
    GlMesh glStadiumField;
    GlMesh glStadiumWalls;
    GlMesh glStadiumStands;
    GlMesh glStadiumLines;
    GlMesh glStadiumCity;
    GlMesh glStadiumBoard;
    GlMesh glStadiumSky;
    GlMesh glStadiumClouds;
    std::vector<GlMesh> glStadiumFans(Stadium3D::kFanSectorCount);
    std::vector<GlMesh> glStadiumFlags(Stadium3D::kFlagCount);
    Stadium3D::Layout stadiumLayout = Stadium3D::defaultPlayLayout();
    Stadium3D::Meshes stadiumMeshes = Stadium3D::build(stadiumLayout);
    if (useGL) {
        glPitcher.upload(pitcherMesh);
        glBatter.upload(batterMesh);
        glBall.upload(baseballMesh);
        glBat.upload(batMesh);
        glStadiumField.upload(stadiumMeshes.field);
        glStadiumWalls.upload(stadiumMeshes.walls);
        glStadiumStands.upload(stadiumMeshes.stands);
        glStadiumLines.upload(stadiumMeshes.lines);
        glStadiumCity.upload(stadiumMeshes.city);
        glStadiumBoard.upload(stadiumMeshes.scoreboardScreen);
        glStadiumSky.upload(stadiumMeshes.skyDome);
        glStadiumClouds.upload(stadiumMeshes.clouds);
        for (int i = 0; i < Stadium3D::kFanSectorCount; i++) {
            if (i < static_cast<int>(stadiumMeshes.fanSectors.size())) {
                glStadiumFans[i].upload(stadiumMeshes.fanSectors[i]);
            }
        }
        for (int i = 0; i < Stadium3D::kFlagCount; i++) {
            if (i < static_cast<int>(stadiumMeshes.flagMeshes.size())) {
                glStadiumFlags[i].upload(stadiumMeshes.flagMeshes[i]);
            }
        }
    }
    float stadiumCheerTime = 0.0f;
    float crowdCheerBoost = 1.15f; // visual crowd surge after SFX events
    float crowdCheerTimer = 0.0f;
    ProceduralSfx::BatParkSfx sfx;

    FrameBuffer frameBuffer(window.getSize().x, window.getSize().y);
    RasterMeshRenderCache pitcherCache;
    RasterMeshRenderCache batterCache;
    RasterMeshRenderCache ballCache;
    RasterMeshRenderCache batCache;

    PhysicsWorld3D world;
    Body3D baseball;

    // ── Play modes ─────────────────────────────────────────────────────
    enum class PlayMode { Derby, Practice, Live };
    PlayMode playMode = PlayMode::Derby; // product default: HR Derby
    // Derby difficulty: Easy meatball → Normal → Hard command.
    enum class DerbyDiff { Easy = 0, Normal = 1, Hard = 2 };
    DerbyDiff derbyDiff = DerbyDiff::Normal;
    auto derbySwingsFor = [](DerbyDiff d) {
        switch (d) {
            case DerbyDiff::Easy:
                return 12;
            case DerbyDiff::Hard:
                return 8;
            default:
                return 10;
        }
    };
    auto derbyDiffName = [](DerbyDiff d) -> const char* {
        switch (d) {
            case DerbyDiff::Easy:
                return "EASY";
            case DerbyDiff::Hard:
                return "HARD";
            default:
                return "NORMAL";
        }
    };
    int kDerbySwings = derbySwingsFor(derbyDiff);
    // Soft-toss flight time — longer = more readable, not necessarily easier.
    auto derbyFlightSec = [&]() {
        switch (derbyDiff) {
            case DerbyDiff::Easy:
                return 1.42f;
            case DerbyDiff::Hard:
                return 0.98f;
            default:
                return 1.20f;
        }
    };
    // Base scatter (world units) around chosen pitch archetype center.
    auto derbyScatter = [&]() {
        switch (derbyDiff) {
            case DerbyDiff::Easy:
                return 0.018f;
            case DerbyDiff::Hard:
                return 0.095f;
            default:
                return 0.042f;
        }
    };
    // PCI forgiveness — Easy is generous, Hard demands cover.
    auto derbyContactEase = [&]() {
        switch (derbyDiff) {
            case DerbyDiff::Easy:
                return 1.48f;
            case DerbyDiff::Hard:
                return 0.88f;
            default:
                return 1.18f;
        }
    };
    // Soft-toss pitch archetypes for variety (not pure random scatter).
    enum class TossSpot { Heart = 0, High, Low, In, Away, Challenge };
    auto pickTossSpot = [&]() -> TossSpot {
        static std::uint32_t spotRng = 0xBEEFu;
        spotRng = spotRng * 1664525u + 1013904223u;
        unsigned r = spotRng % 100u;
        switch (derbyDiff) {
            case DerbyDiff::Easy:
                // Mostly heart / belt, rare challenge.
                if (r < 55) return TossSpot::Heart;
                if (r < 75) return TossSpot::Low;
                if (r < 90) return TossSpot::High;
                return TossSpot::In;
            case DerbyDiff::Hard:
                if (r < 18) return TossSpot::Heart;
                if (r < 38) return TossSpot::High;
                if (r < 55) return TossSpot::Low;
                if (r < 72) return TossSpot::In;
                if (r < 88) return TossSpot::Away;
                return TossSpot::Challenge;
            default:
                if (r < 32) return TossSpot::Heart;
                if (r < 48) return TossSpot::High;
                if (r < 64) return TossSpot::Low;
                if (r < 80) return TossSpot::In;
                if (r < 92) return TossSpot::Away;
                return TossSpot::Challenge;
        }
    };
    auto tossSpotAim = [&](TossSpot spot) -> Vector3 {
        Vector3 a = strikeZoneCenter;
        const float hw = strikeZoneHalfWidth;
        const float hh = strikeZoneHalfHeight;
        switch (spot) {
            case TossSpot::High:
                a.y += hh * 0.42f;
                break;
            case TossSpot::Low:
                a.y -= hh * 0.48f;
                break;
            case TossSpot::In: // RHB inside = toward 3B (−X)
                a.x -= hw * 0.55f;
                a.y -= hh * 0.08f;
                break;
            case TossSpot::Away:
                a.x += hw * 0.58f;
                a.y += hh * 0.05f;
                break;
            case TossSpot::Challenge:
                a.x += hw * 0.72f;
                a.y += hh * 0.55f;
                break;
            default: // Heart — slightly below belt meatball
                a.y -= 0.05f;
                break;
        }
        return a;
    };
    constexpr float kPracticeMph = 52.0f;
    float pitchMph = 44.0f;
    float normalPitchMph = 90.0f;

    struct DerbyState {
        int swingsLeft = 10;
        int hrCount = 0;
        float longestHrFeet = 0.0f;
        float bestExitMph = 0.0f;
        int totalSwings = 0;
        int moonballs = 0;
        bool roundOver = false;
        float roundOverTimer = 0.0f; // celebration overlay
        std::string lastResult = "--";
        bool goalMet = false;
        bool goalCelebrated = false;
    };
    DerbyState derby;
    derby.swingsLeft = kDerbySwings;
    DerbyBests::Stats careerBests = DerbyBests::load();
    GameSettings::Data settings = GameSettings::load();
    derbyDiff = static_cast<DerbyDiff>(std::clamp(settings.derbyDiff, 0, 2));
    // Hard locked until unlocked (still allow if already set in settings after unlock).
    if (derbyDiff == DerbyDiff::Hard && careerBests.hardUnlocked == 0) {
        derbyDiff = DerbyDiff::Normal;
        settings.derbyDiff = 1;
    }
    kDerbySwings = derbySwingsFor(derbyDiff);
    derby.swingsLeft = kDerbySwings;
    int sessionGoalHrs = DerbyBests::sessionGoalHrs(static_cast<int>(derbyDiff));
    sfx.setMasterVolumes(settings.sfxVolume, 0.0f);
    bool showHelp = settings.showHelpOnLaunch;
    bool careerSavedThisRound = false;
    std::string careerFlash; // short "NEW BEST" toast
    float careerFlashTimer = 0.0f;
    float goalFlashTimer = 0.0f;

    auto easyContact = [&]() {
        // Generous PCI magnet for derby + practice soft toss.
        return playMode == PlayMode::Derby || playMode == PlayMode::Practice;
    };
    auto modeTitle = [&]() -> const char* {
        switch (playMode) {
            case PlayMode::Derby:
                return "HR DERBY";
            case PlayMode::Practice:
                return "PRACTICE";
            default:
                return "LIVE AB";
        }
    };

    float deliveryAge = -1.0f;
    float deliveryDuration = deliveryClip.duration > 0.1f ? deliveryClip.duration : 2.2f;
    float releaseNorm = (deliveryClip.name == "throw_preview") ? (1.18f / deliveryDuration) : 0.66f;
    bool ballReleased = false;
    bool hasHit = false;
    HitInfo lastHit;
    float poseClock = 0.0f;
    float rebuildTimer = 0.0f;
    float practiceRepitchTimer = -1.0f;
    std::string status = "HR DERBY  ·  aim reticle (height = bat tilt)  ·  Space swing";
    sf::Color statusCol(255, 220, 80);
    std::vector<Vector3> trail;
    float spinX = 0, spinY = 0, spinZ = 0;

    // HUD accent — matches bat yellow outline.
    const sf::Color batOutlineCol(255, 230, 40);

    // ── At-bat loop (live) / derby swing budget ────────────────────────
    struct AtBatCount {
        int balls = 0;
        int strikes = 0;
        int outs = 0;
        int hits = 0;
        int walks = 0;
        int pitchNum = 0;
    };
    AtBatCount count;
    bool swungThisPitch = false;
    bool pitchResolved = false;
    bool landingLogged = false;
    bool ballSettled = false; // true once batted ball sticks on the ground
    bool wallResolved = false; // live fence cross handled this flight
    bool swingConsumed = false; // derby: one swing budget spent this pitch
    float hrBannerTimer = 0.0f;
    float prevBallR = 0.0f; // radial distance from home (for fence cross detect)
    Vector3 plateCrossPos = strikeZoneCenter;
    float prevBallZ = moundZ;

    auto isDingerQuality = [](const char* q) {
        if (!q) {
            return false;
        }
        std::string s(q);
        return s == "Home Run" || s == "Moonball" || s == "Jaw Dropper";
    };

    auto countString = [&]() {
        std::ostringstream oss;
        if (playMode == PlayMode::Derby) {
            oss << "Swings " << derby.swingsLeft << "/" << kDerbySwings
                << "   HR " << derby.hrCount
                << "   Longest ";
            if (derby.longestHrFeet > 0.5f) {
                oss << std::fixed << std::setprecision(0) << derby.longestHrFeet << " ft";
            } else {
                oss << "--";
            }
            if (derby.roundOver) {
                oss << "   ROUND OVER";
            }
        } else {
            oss << "Count " << count.balls << "-" << count.strikes
                << "   P#" << count.pitchNum
                << "   H " << count.hits
                << "   BB " << count.walks
                << "   K/Out " << count.outs;
        }
        return oss.str();
    };

    auto resetAtBat = [&]() {
        count.balls = 0;
        count.strikes = 0;
        // keep outs / hits / walks as session stats
    };

    auto resetDerbyRound = [&]() {
        kDerbySwings = derbySwingsFor(derbyDiff);
        sessionGoalHrs = DerbyBests::sessionGoalHrs(static_cast<int>(derbyDiff));
        derby = DerbyState{};
        derby.swingsLeft = kDerbySwings;
        count = AtBatCount{};
        careerSavedThisRound = false;
        goalFlashTimer = 0.0f;
        resultBannerTimer = 0.0f;
    };

    auto finalizeDerbyRoundIfNeeded = [&]() {
        if (playMode != PlayMode::Derby || !derby.roundOver || careerSavedThisRound) {
            return;
        }
        careerSavedThisRound = true;
        if (derby.goalMet) {
            careerBests.goalsCleared += 1;
        }
        bool improved = DerbyBests::recordRound(
            careerBests,
            derby.hrCount,
            derby.longestHrFeet,
            derby.bestExitMph,
            static_cast<int>(derbyDiff),
            derby.moonballs
        );
        if (improved) {
            careerFlash = "NEW CAREER BEST SAVED";
            careerFlashTimer = 3.2f;
        } else {
            careerFlash = "Round saved";
            careerFlashTimer = 1.6f;
        }
    };

    auto scheduleNextPitch = [&](float delay) {
        practiceRepitchTimer = delay;
    };

    auto consumeDerbySwing = [&]() {
        if (playMode != PlayMode::Derby || swingConsumed || derby.roundOver) {
            return;
        }
        swingConsumed = true;
        derby.swingsLeft = std::max(0, derby.swingsLeft - 1);
        derby.totalSwings += 1;
        if (derby.swingsLeft <= 0) {
            derby.roundOver = true;
        }
    };

    auto noteDerbyExit = [&]() {
        if (playMode != PlayMode::Derby || !lastHit.hit) {
            return;
        }
        if (lastHit.exitMph > derby.bestExitMph) {
            derby.bestExitMph = lastHit.exitMph;
        }
    };

    auto playHrAtmosphere = [&](bool jawOrMoon) {
        sfx.playCrowdPop(true);
        // Visual crowd only (bat crack is the sole SFX policy) — big bob on yard.
        crowdCheerBoost = jawOrMoon ? 2.85f : 2.45f;
        crowdCheerTimer = jawOrMoon ? 4.0f : 3.2f;
    };

    auto noteDerbyDinger = [&]() {
        // Predicted dinger at contact — land path may confirm or reverse.
        if (playMode != PlayMode::Derby || !lastHit.fair || !isDingerQuality(lastHit.quality)) {
            return;
        }
        derby.hrCount += 1;
        if (lastHit.distanceFeet > derby.longestHrFeet) {
            derby.longestHrFeet = lastHit.distanceFeet;
        }
        noteDerbyExit();
        hrBannerTimer = 3.2f;
        std::string q = lastHit.quality ? lastHit.quality : "";
        playHrAtmosphere(q == "Jaw Dropper" || q == "Moonball");
        if (!derby.goalMet && derby.hrCount >= sessionGoalHrs) {
            derby.goalMet = true;
            goalFlashTimer = 3.5f;
            careerFlash = "GOAL CLEARED  ·  " + std::to_string(sessionGoalHrs) + " HR";
            careerFlashTimer = 3.5f;
        }
    };

    auto setDerbyLastResult = [&](const std::string& call) {
        if (playMode != PlayMode::Derby) {
            return;
        }
        std::ostringstream oss;
        oss << call;
        if (lastHit.hit) {
            oss << std::fixed << std::setprecision(0)
                << "  " << lastHit.exitMph << " mph  "
                << lastHit.distanceFeet << " ft";
        }
        derby.lastResult = oss.str();
        noteDerbyExit();
    };

    auto resolvePitch = [&](const char* kind) {
        if (pitchResolved) {
            return;
        }
        pitchResolved = true;
        count.pitchNum += 1;

        std::string call = kind;

        if (playMode == PlayMode::Derby) {
            // Derby: swings are the budget; takes don't cost a swing.
            if (std::string(kind) == "HIT") {
                consumeDerbySwing();
                if (lastHit.fair && isDingerQuality(lastHit.quality)) {
                    noteDerbyDinger();
                    call = lastHit.quality ? lastHit.quality : "Home Run";
                    statusCol = sf::Color(255, 220, 80);
                } else if (lastHit.fair) {
                    call = lastHit.quality ? lastHit.quality : "In play";
                    statusCol = sf::Color(200, 220, 160);
                    noteDerbyExit();
                } else {
                    call = "Foul";
                    statusCol = sf::Color(255, 200, 140);
                    noteDerbyExit();
                }
                setDerbyLastResult(call);
                // Hold next toss until the ball finishes its full flight to the ground.
                // Short post-land pause only (timer freezes while airborne).
                // Post-land pause only (timer freezes during flight + broadcast cam).
                scheduleNextPitch(isDingerQuality(lastHit.quality) ? 0.85f : 0.55f);
            } else if (std::string(kind) == "SWINGING_STRIKE") {
                consumeDerbySwing();
                call = "Miss";
                statusCol = sf::Color(255, 160, 120);
                setDerbyLastResult(call);
                resultBannerTimer = 1.4f;
                resultBannerTitle = "WHIFF";
                resultBannerSub = "Cover the ball next toss";
                resultBannerCol = sf::Color(255, 140, 120);
                scheduleNextPitch(0.7f);
            } else if (std::string(kind) == "CALLED_STRIKE" || std::string(kind) == "BALL") {
                // No swing — soft toss again without burning budget.
                call = "Take — no swing";
                statusCol = sf::Color(180, 200, 220);
                scheduleNextPitch(0.55f);
            }
            if (derby.roundOver) {
                derby.roundOverTimer = 5.5f;
                finalizeDerbyRoundIfNeeded();
                std::ostringstream fin;
                fin << "ROUND OVER  ·  " << derby.hrCount << " HR  ·  longest ";
                if (derby.longestHrFeet > 0.5f) {
                    fin << std::fixed << std::setprecision(0) << derby.longestHrFeet << " ft";
                } else {
                    fin << "--";
                }
                if (derby.bestExitMph > 0.5f) {
                    fin << "  ·  best EV " << std::fixed << std::setprecision(0)
                        << derby.bestExitMph << " mph";
                }
                fin << "  ·  N new round";
                status = fin.str();
                statusCol = sf::Color(255, 210, 100);
                practiceRepitchTimer = -1.0f; // stop auto-toss until N
                return;
            }
            std::ostringstream oss;
            oss << call << "   ·   " << countString();
            status = oss.str();
            return;
        }

        // Practice / Live AB rules
        if (std::string(kind) == "HIT") {
            if (lastHit.fair) {
                count.hits += 1;
                resetAtBat();
                statusCol = sf::Color(120, 255, 160);
                if (isDingerQuality(lastHit.quality)) {
                    hrBannerTimer = 2.8f;
                    statusCol = sf::Color(255, 220, 80);
                    std::string q = lastHit.quality ? lastHit.quality : "";
                    playHrAtmosphere(q == "Jaw Dropper" || q == "Moonball");
                }
                // Post-land pause only — full drop always plays out first.
                scheduleNextPitch(playMode == PlayMode::Practice ? 1.5f : 1.25f);
            } else {
                if (count.strikes < 2) {
                    count.strikes += 1;
                }
                call = "Foul";
                statusCol = sf::Color(255, 200, 140);
                scheduleNextPitch(1.2f);
            }
        } else if (std::string(kind) == "SWINGING_STRIKE" || std::string(kind) == "CALLED_STRIKE") {
            count.strikes = std::min(3, count.strikes + 1);
            if (count.strikes >= 3) {
                count.outs += 1;
                call = (std::string(kind) == "SWINGING_STRIKE") ? "STRIKEOUT swinging" : "STRIKEOUT looking";
                resetAtBat();
                statusCol = sf::Color(255, 140, 120);
            } else {
                call = (std::string(kind) == "SWINGING_STRIKE") ? "Swinging strike" : "Called strike";
                statusCol = sf::Color(255, 200, 120);
            }
            scheduleNextPitch(1.6f);
        } else if (std::string(kind) == "BALL") {
            count.balls = std::min(4, count.balls + 1);
            if (count.balls >= 4) {
                count.walks += 1;
                call = "WALK";
                resetAtBat();
                statusCol = sf::Color(140, 200, 255);
            } else {
                call = "Ball";
                statusCol = sf::Color(180, 210, 255);
            }
            scheduleNextPitch(1.4f);
        }

        std::ostringstream oss;
        oss << call << "   ·   " << countString();
        status = oss.str();
    };

    auto beginPitch = [&]() {
        if (playMode == PlayMode::Derby && derby.roundOver) {
            status = "ROUND OVER  ·  N new round  ·  " + countString();
            statusCol = sf::Color(255, 210, 100);
            practiceRepitchTimer = -1.0f;
            return;
        }
        world = PhysicsWorld3D();
        world.gravity = Vector3(0, -9.8f, 0);
        // Open park — no tight box walls/ceiling clamping exit trajectories.
        world.setBounds(boundsMinimum, boundsMaximum);
        world.airResistanceEnabled = false;
        // Same CharacterModel3D pitcher as pitching_simulator_demo — ball starts in the hand.
        Vector3 hand0 = pitcherAnim.throwHandWorld(pitcherWorldTransform());
        baseball = Body3D(hand0, 0.145f);
        baseball.setRadius(baseballRadius);
        baseball.restitution = 0.15f; // pitch path; set to 0 after contact
        baseball.velocity = Vector3();
        baseball.angularVelocity = Vector3();
        world.addBody(&baseball);
        // Full mound delivery for every mode (Derby = soft meatball after release).
        deliveryAge = 0.0f;
        pitcherAnim.applyClipNormalized(deliveryClip, 0.0f);
        ballReleased = false;
        hasHit = false;
        swungThisPitch = false;
        pitchResolved = false;
        landingLogged = false;
        ballSettled = false;
        wallResolved = false;
        swingConsumed = false;
        hrBannerTimer = 0.0f;
        prevBallZ = hand0.z;
        prevBallR = 0.0f;
        plateCrossPos = strikeZoneCenter;
        followBallCam = false;
        broadcastCam = BroadcastCam::Plate;
        holdLandTimer = 0.0f;
        returnPlateTimer = 0.0f;
        camShakeTimer = 0.0f;
        camShakeIntensity = 0.0f;
        fovPunch = 0.0f;
        hitFreezeTimer = 0.0f;
        contactFlash = 0.0f;
        resultBannerTimer = 0.0f;
        applyCatcherCamera(camera);
        practiceRepitchTimer = -1.0f;
        trail.clear();
        spinX = spinY = spinZ = 0;
        bat.swingT = -1.0f;
        bat.locked = false;
        bat.omega = 0;
        if (playMode == PlayMode::Derby) {
            bat.type = SwingType::Contact;
            status = std::string("Pitcher delivering  ·  ") + derbyDiffName(derbyDiff) +
                     "  ·  " + countString();
            statusCol = sf::Color(255, 220, 80);
        } else if (playMode == PlayMode::Practice) {
            pitchMph = kPracticeMph;
            bat.type = SwingType::Contact;
            status = "PRACTICE  ·  " + countString() + "  ·  aim yellow reticle, Space swing";
            statusCol = sf::Color(120, 255, 160);
        } else {
            pitchMph = normalPitchMph;
            status = "Pitch  ·  " + countString();
            statusCol = sf::Color(255, 230, 140);
        }
    };

    auto releaseBall = [&]() {
        // Always leave the pitcher's throw hand (same path as pitching sim).
        Vector3 start = pitcherAnim.throwHandWorld(pitcherWorldTransform());
        Vector3 aim = strikeZoneCenter;
        if (playMode == PlayMode::Derby) {
            // Soft toss: archetype spot + mild scatter (readable variety).
            aim = tossSpotAim(pickTossSpot());
            float sc = derbyScatter();
            static std::uint32_t tossRng = 0xA11CEu;
            tossRng = tossRng * 1664525u + 1013904223u;
            float ux = (static_cast<float>(tossRng & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
            tossRng = tossRng * 1664525u + 1013904223u;
            float uy = (static_cast<float>(tossRng & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
            aim.x = clampf(
                aim.x + ux * sc,
                -strikeZoneHalfWidth * 0.92f,
                strikeZoneHalfWidth * 0.92f
            );
            aim.y = clampf(
                aim.y + uy * sc,
                strikeZoneCenter.y - strikeZoneHalfHeight * 0.88f,
                strikeZoneCenter.y + strikeZoneHalfHeight * 0.82f
            );
            // Slight flight-time jitter so rhythm isn't metronomic.
            tossRng = tossRng * 1664525u + 1013904223u;
            float tJit = ((static_cast<float>(tossRng & 0xFFFF) / 65535.0f) - 0.5f) * 0.10f;
            float flight = std::max(0.85f, derbyFlightSec() + tJit);
            baseball.position = start;
            baseball.velocity = softTossVelocity(start, aim, flight);
            pitchMph = baseball.velocity.magnitude() * 2.236936f;
        } else {
            if (playMode == PlayMode::Live) {
                static std::uint32_t rng = 0xC0FFEEu;
                rng = rng * 1664525u + 1013904223u;
                float ux = (static_cast<float>(rng & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
                rng = rng * 1664525u + 1013904223u;
                float uy = (static_cast<float>(rng & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
                aim.x += ux * 0.18f;
                aim.y += uy * 0.16f;
                aim.x = clampf(aim.x, -strikeZoneHalfWidth * 1.15f, strikeZoneHalfWidth * 1.15f);
                aim.y = clampf(
                    aim.y,
                    strikeZoneCenter.y - strikeZoneHalfHeight * 1.15f,
                    strikeZoneCenter.y + strikeZoneHalfHeight * 1.15f
                );
            }
            baseball.position = start;
            baseball.velocity = launchVelocityTowardPlate(start, aim, pitchMph);
        }
        prevBallZ = start.z;
        ballReleased = true;
        status = std::string(modeTitle()) + "  ·  " + countString() + "  ·  cover reticle, Space/LMB";
        statusCol = sf::Color(180, 230, 255);
    };

    beginPitch();

    sf::Clock clock;
    while (window.isOpen()) {
        float rawDt = std::min(clock.restart().asSeconds(), 0.05f);
        // Hit-stop: freeze gameplay briefly; still drain UI timers on rawDt.
        float dt = rawDt;
        if (hitFreezeTimer > 0.0f) {
            hitFreezeTimer = std::max(0.0f, hitFreezeTimer - rawDt);
            dt = rawDt * 0.08f; // near-freeze, not full stall
        }
        if (contactFlash > 0.0f) {
            contactFlash = std::max(0.0f, contactFlash - rawDt * 3.2f);
        }
        if (resultBannerTimer > 0.0f) {
            resultBannerTimer = std::max(0.0f, resultBannerTimer - rawDt);
        }
        if (goalFlashTimer > 0.0f) {
            goalFlashTimer = std::max(0.0f, goalFlashTimer - rawDt);
        }
        const SwingProfile& prof = profileOf(bat.type);

        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            if (const auto* r = event->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect(
                    {0, 0},
                    {static_cast<float>(r->size.x), static_cast<float>(r->size.y)}
                )));
                frameBuffer.resize(static_cast<int>(r->size.x), static_cast<int>(r->size.y));
            }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                using K = sf::Keyboard::Key;
                if (key->code == K::Escape) {
                    window.close();
                } else if (key->code == K::Z) {
                    bat.type = SwingType::Power;
                    status = "POWER";
                    statusCol = profileOf(SwingType::Power).color;
                } else if (key->code == K::X) {
                    bat.type = SwingType::Contact;
                    status = "CONTACT";
                    statusCol = profileOf(SwingType::Contact).color;
                } else if (key->code == K::C) {
                    bat.type = SwingType::Regular;
                    status = "REGULAR";
                    statusCol = profileOf(SwingType::Regular).color;
                } else if (key->code == K::R) {
                    beginPitch();
                } else if (key->code == K::N) {
                    if (playMode == PlayMode::Derby) {
                        resetDerbyRound();
                        beginPitch();
                        status = "New derby round  ·  " + countString();
                        statusCol = sf::Color(255, 220, 80);
                    } else {
                        count = AtBatCount{};
                        beginPitch();
                        status = "New at-bat  ·  " + countString();
                        statusCol = sf::Color(200, 230, 255);
                    }
                } else if (key->code == K::D) {
                    playMode = PlayMode::Derby;
                    resetDerbyRound();
                    bat.type = SwingType::Contact;
                    status = std::string("HR DERBY ") + derbyDiffName(derbyDiff) + " — " +
                             std::to_string(kDerbySwings) + " swings";
                    statusCol = sf::Color(255, 220, 80);
                    beginPitch();
                } else if (key->code == K::Num1 || key->code == K::Numpad1) {
                    derbyDiff = DerbyDiff::Easy;
                    settings.derbyDiff = 0;
                    GameSettings::save(settings);
                    if (playMode == PlayMode::Derby) {
                        resetDerbyRound();
                        beginPitch();
                    }
                    status = "EASY  ·  fat PCI  ·  goal " + std::to_string(sessionGoalHrs) +
                             " HR  ·  " + std::to_string(kDerbySwings) + " swings";
                    statusCol = sf::Color(120, 255, 160);
                } else if (key->code == K::Num2 || key->code == K::Numpad2) {
                    derbyDiff = DerbyDiff::Normal;
                    settings.derbyDiff = 1;
                    GameSettings::save(settings);
                    if (playMode == PlayMode::Derby) {
                        resetDerbyRound();
                        beginPitch();
                    }
                    status = "NORMAL  ·  goal " + std::to_string(sessionGoalHrs) +
                             " HR  ·  " + std::to_string(kDerbySwings) + " swings";
                    statusCol = sf::Color(255, 220, 80);
                } else if (key->code == K::Num3 || key->code == K::Numpad3) {
                    if (careerBests.hardUnlocked == 0) {
                        status = "HARD locked  ·  hit 3+ HR on NORMAL in one round";
                        statusCol = sf::Color(255, 160, 120);
                    } else {
                        derbyDiff = DerbyDiff::Hard;
                        settings.derbyDiff = 2;
                        GameSettings::save(settings);
                        if (playMode == PlayMode::Derby) {
                            resetDerbyRound();
                            beginPitch();
                        }
                        status = "HARD  ·  tight PCI  ·  goal " +
                                 std::to_string(sessionGoalHrs) + " HR  ·  " +
                                 std::to_string(kDerbySwings) + " swings";
                        statusCol = sf::Color(255, 120, 100);
                    }
                } else if (key->code == K::P) {
                    playMode = PlayMode::Practice;
                    pitchMph = kPracticeMph;
                    bat.type = SwingType::Contact;
                    status = "PRACTICE — slow meatball + easy contact";
                    statusCol = sf::Color(120, 255, 160);
                    beginPitch();
                } else if (key->code == K::L) {
                    playMode = PlayMode::Live;
                    pitchMph = normalPitchMph;
                    status = "LIVE AB — full speed, balls/strikes";
                    statusCol = sf::Color(255, 200, 120);
                    beginPitch();
                } else if (key->code == K::LBracket) {
                    if (playMode == PlayMode::Live) {
                        pitchMph = std::max(60.0f, pitchMph - 2.0f);
                        normalPitchMph = pitchMph;
                    }
                } else if (key->code == K::RBracket) {
                    if (playMode == PlayMode::Live) {
                        pitchMph = std::min(105.0f, pitchMph + 2.0f);
                        normalPitchMph = pitchMph;
                    }
                } else if (key->code == K::H) {
                    showHelp = !showHelp;
                } else if (key->code == K::Equal || key->code == K::Add) {
                    settings.sfxVolume = clampf(settings.sfxVolume + 0.08f, 0.0f, 1.0f);
                    sfx.setMasterVolumes(settings.sfxVolume, 0.0f);
                    GameSettings::save(settings);
                } else if (key->code == K::Hyphen || key->code == K::Subtract) {
                    settings.sfxVolume = clampf(settings.sfxVolume - 0.08f, 0.0f, 1.0f);
                    sfx.setMasterVolumes(settings.sfxVolume, 0.0f);
                    GameSettings::save(settings);
                } else if (key->code == K::Space) {
                    // Swing only on press — reticle stays put; 3D bat animates.
                    if (!bat.swinging() && !pitchResolved && ballReleased &&
                        !(playMode == PlayMode::Derby && derby.roundOver)) {
                        startSwingFromGrip(bat, reticle, batCfg, lastGripHands, lastGripAxis);
                        swungThisPitch = true;
                        hasHit = false;
                        showHelp = false;
                        status = std::string(prof.name) + " swing!  ·  " + countString();
                        statusCol = prof.color;
                    }
                }
            }
            if (const auto* m = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (m->button == sf::Mouse::Button::Left && !bat.swinging() && !pitchResolved &&
                    ballReleased && !(playMode == PlayMode::Derby && derby.roundOver)) {
                    startSwingFromGrip(bat, reticle, batCfg, lastGripHands, lastGripAxis);
                    swungThisPitch = true;
                    hasHit = false;
                    showHelp = false;
                    status = std::string(prof.name) + " swing!  ·  " + countString();
                    statusCol = prof.color;
                }
            }
        }

        poseClock += dt;
        stadiumCheerTime += rawDt; // park atmosphere keeps moving during hit-stop
        if (hrBannerTimer > 0.0f) {
            hrBannerTimer = std::max(0.0f, hrBannerTimer - rawDt);
        }
        if (crowdCheerTimer > 0.0f) {
            crowdCheerTimer = std::max(0.0f, crowdCheerTimer - rawDt);
            if (crowdCheerTimer <= 0.0f) {
                crowdCheerBoost = 1.15f;
            }
        }
        if (careerFlashTimer > 0.0f) {
            careerFlashTimer = std::max(0.0f, careerFlashTimer - rawDt);
        }
        if (derby.roundOverTimer > 0.0f) {
            derby.roundOverTimer = std::max(0.0f, derby.roundOverTimer - rawDt);
        }

        // Pitcher animation (same CharacterModel3D delivery as pitching sim).
        // Ball glued to throw hand until release frame.
        if (deliveryAge >= 0.0f) {
            deliveryAge += dt;
            float t01 = clampf(deliveryAge / deliveryDuration, 0.0f, 1.0f);
            pitcherAnim.applyClipNormalized(deliveryClip, t01);
            if (!ballReleased) {
                baseball.position = pitcherAnim.throwHandWorld(pitcherWorldTransform());
                baseball.velocity = Vector3();
                baseball.acceleration = Vector3();
                if (t01 >= releaseNorm) {
                    releaseBall();
                }
            }
            if (deliveryAge > deliveryDuration + 2.5f && !bat.swinging()) {
                pitcherAnim.applyClip(idleClip, poseClock, true);
            }
        } else {
            pitcherAnim.applyClip(idleClip, poseClock, true);
            if (!ballReleased) {
                baseball.position = pitcherAnim.throwHandWorld(pitcherWorldTransform());
                baseball.velocity = Vector3();
            }
        }

        // Approach cue — ball entering the hit zone.
        if (ballReleased && !hasHit && !pitchResolved && !bat.swinging() &&
            baseball.position.z > plateZ - 9.0f && baseball.position.z < plateZ + 1.5f &&
            baseball.velocity.z > 0.5f) {
            status = std::string(prof.name) + "  ·  SWING WINDOW  ·  Space/LMB  ·  " + countString();
            statusCol = sf::Color(255, 240, 120);
        }

        // Reticle: mouse aims the yellow silhouette (never swings).
        // Tilt auto-follows PCI height. 3D bat: grip lock idle; path while swinging.
        if (!bat.swinging() && !followBallCam) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            reticle.pci = mouseToPci(
                camera,
                static_cast<float>(mp.x),
                static_cast<float>(mp.y),
                static_cast<float>(window.getSize().x),
                static_cast<float>(window.getSize().y)
            );
            updateReticleAngle(reticle, dt);
            // Contact keys locked to reticle; load pose ready for Space/LMB.
            orientBatFromReticle(bat, reticle, batCfg);
            bakeSwingKeys(bat);
            sampleSwingPose(bat, 0.0f, bat.hands, bat.axis);
        } else if (bat.swinging()) {
            updateSwing(bat, prof, dt);
        }

        // Flight + bat contact + at-bat resolution
        if (ballReleased) {
            float acc = dt;
            while (acc >= fixedStep) {
                prevBallZ = baseball.position.z;

                if (ballSettled) {
                    // Stay planted — no bounce, no roll.
                    baseball.velocity = Vector3();
                    baseball.angularVelocity = Vector3();
                    baseball.acceleration = Vector3();
                    baseball.position.y = std::max(baseball.position.y, baseball.radius + 0.01f);
                } else {
                    world.step(fixedStep);
                    // Solid park: fence, backstop, dugouts, board, stands, ground.
                    // Stick only when ball is slow and near the ground (never mid-air stick/teleport).
                    bool nearGroundStick = hasHit && baseball.position.y < 2.5f &&
                        baseball.velocity.magnitude() < 12.0f;
                    Stadium3D::BallCollisionHit col = Stadium3D::collideBall(
                        stadiumLayout,
                        baseball.position,
                        baseball.velocity,
                        baseball.radius,
                        nearGroundStick
                    );
                    if (hasHit && !ballSettled) {
                        if (col.surface == Stadium3D::HitSurface::FenceTopClear) {
                            // Only arm "out" if live height actually cleared the wall top.
                            float margin = (col.impactY - col.wallTopY) * feetPerWorldUnit;
                            if (margin >= 0.5f) {
                                wallResolved = true;
                                lastHit.clearsWall = true;
                                lastHit.hitsWallFace = false;
                                lastHit.heightAtFence = col.impactY;
                                lastHit.wallMarginFeet = margin;
                                lastHit.fenceFeet = col.fenceFeet;
                                lastHit.sprayDeg = col.sprayDeg;
                            }
                            // else: graze — let face collision resolve as wall ball next steps
                        } else if (col.surface == Stadium3D::HitSurface::Fence ||
                                   col.surface == Stadium3D::HitSurface::FoulPole ||
                                   col.surface == Stadium3D::HitSurface::Scoreboard ||
                                   col.surface == Stadium3D::HitSurface::Stands ||
                                   col.surface == Stadium3D::HitSurface::Dugout ||
                                   col.surface == Stadium3D::HitSurface::Backstop) {
                            wallResolved = true;
                            if (col.surface == Stadium3D::HitSurface::Fence ||
                                col.surface == Stadium3D::HitSurface::Scoreboard ||
                                col.surface == Stadium3D::HitSurface::FoulPole) {
                                sfx.playWallBang(lastHit.exitMph);
                            }
                            if (col.surface == Stadium3D::HitSurface::Fence) {
                                const bool wasDinger = isDingerQuality(lastHit.quality);
                                lastHit.hitsWallFace = true;
                                lastHit.clearsWall = false;
                                lastHit.fair = true;
                                lastHit.fenceFeet = col.fenceFeet;
                                lastHit.distanceFeet = col.fenceFeet;
                                lastHit.heightAtFence = col.impactY;
                                lastHit.wallMarginFeet =
                                    (col.impactY - col.wallTopY) * feetPerWorldUnit;
                                lastHit.sprayDeg = col.sprayDeg;
                                lastHit.quality =
                                    (lastHit.exitMph >= 95.0f) ? "Wall Ball" : "Off the Wall";
                                if (playMode == PlayMode::Derby && wasDinger && derby.hrCount > 0) {
                                    derby.hrCount -= 1;
                                }
                            } else if (col.surface == Stadium3D::HitSurface::Stands) {
                                // Seats are past the fence — only a true over-the-wall flight counts.
                                // If we never got FenceTopClear with margin, this is not "out".
                                if (!lastHit.clearsWall || lastHit.wallMarginFeet < 0.5f) {
                                    const bool wasDinger = isDingerQuality(lastHit.quality);
                                    lastHit.clearsWall = false;
                                    lastHit.hitsWallFace = true;
                                    lastHit.quality =
                                        (lastHit.exitMph >= 95.0f) ? "Wall Ball" : "Off the Wall";
                                    lastHit.distanceFeet = lastHit.fenceFeet > 1.0f
                                        ? lastHit.fenceFeet
                                        : col.fenceFeet;
                                    if (playMode == PlayMode::Derby && wasDinger &&
                                        derby.hrCount > 0) {
                                        derby.hrCount -= 1;
                                    }
                                } else {
                                    // Confirmed clear into the seats = HR distance at seat row.
                                    float landR = stadiumLayout.radiusFromHome(baseball.position);
                                    lastHit.distanceFeet = landR * feetPerWorldUnit;
                                    lastHit.clearsWall = true;
                                    lastHit.hitsWallFace = false;
                                }
                            }
                            if (col.stuck) {
                                baseball.angularVelocity = Vector3();
                                baseball.acceleration = Vector3();
                                ballSettled = true;
                                if (!landingLogged) {
                                    landingLogged = true;
                                    if (playMode == PlayMode::Derby && lastHit.exitMph > derby.bestExitMph) {
                                        derby.bestExitMph = lastHit.exitMph;
                                    }
                                    if (lastHit.quality) {
                                        setDerbyLastResult(lastHit.quality);
                                    }
                                    std::ostringstream oss;
                                    oss << std::fixed << std::setprecision(0)
                                        << (lastHit.quality ? lastHit.quality : "Blocked")
                                        << "  solid park  " << lastHit.exitMph << " mph  ·  "
                                        << countString();
                                    status = oss.str();
                                    statusCol = sf::Color(255, 170, 90);
                                }
                            }
                        } else if (col.surface == Stadium3D::HitSurface::Ground && col.stuck) {
                            baseball.angularVelocity = Vector3();
                            baseball.acceleration = Vector3();
                            ballSettled = true;
                            if (!landingLogged) {
                                landingLogged = true;
                                const bool wasDinger = isDingerQuality(lastHit.quality);
                                float landR = stadiumLayout.radiusFromHome(baseball.position);
                                float landAng = 0.0f;
                                float landR2 = 0.0f;
                                stadiumLayout.polarFromHome(baseball.position, landR2, landAng);
                                float wallAtLand = stadiumLayout.wallRAtAngle(landAng);
                                // Truth from where the ball actually is: inside fence = not a HR.
                                const bool landedBeyondFence =
                                    lastHit.fair && landR > wallAtLand + 0.75f;
                                if (wallResolved && lastHit.clearsWall && landedBeyondFence) {
                                    // Confirmed over-the-wall — keep clear, use live distance.
                                    lastHit.distanceFeet = landR * feetPerWorldUnit;
                                    if (!isDingerQuality(lastHit.quality) && lastHit.exitMph >= 95.0f) {
                                        lastHit.quality = "Home Run";
                                    }
                                } else {
                                    // Never left the yard — reclassify from exit (no false HR).
                                    classifyHit(
                                        lastHit, lastHit.exitVel, lastHit.exitPos, stadiumLayout
                                    );
                                    lastHit.clearsWall = false;
                                    lastHit.hitsWallFace =
                                        lastHit.hitsWallFace ||
                                        (lastHit.fair && landR >= wallAtLand - 1.5f);
                                    lastHit.distanceFeet = landR * feetPerWorldUnit;
                                    if (lastHit.hitsWallFace && lastHit.fair) {
                                        lastHit.quality =
                                            (lastHit.exitMph >= 95.0f) ? "Wall Ball" : "Off the Wall";
                                        lastHit.distanceFeet = lastHit.fenceFeet > 1.0f
                                            ? lastHit.fenceFeet
                                            : wallAtLand * feetPerWorldUnit;
                                    } else if (isDingerQuality(lastHit.quality)) {
                                        // Predicted HR but ball is inside — downgrade.
                                        lastHit.quality =
                                            lastHit.exitMph >= 90.0f ? "Fly Ball" : "Flare";
                                    }
                                }
                                const bool isDinger =
                                    isDingerQuality(lastHit.quality) && lastHit.clearsWall &&
                                    landedBeyondFence;
                                if (playMode == PlayMode::Derby && lastHit.fair) {
                                    if (isDinger && !wasDinger) {
                                        derby.hrCount += 1;
                                        hrBannerTimer = 3.2f;
                                        resultBannerTimer = 3.0f;
                                        resultBannerTitle =
                                            (lastHit.quality && std::string(lastHit.quality) == "Moonball")
                                                ? "MOONBALL"
                                                : (lastHit.quality &&
                                                           std::string(lastHit.quality) == "Jaw Dropper"
                                                       ? "JAW DROPPER"
                                                       : "HOME RUN");
                                        resultBannerCol = sf::Color(255, 220, 60);
                                        {
                                            std::ostringstream sub;
                                            sub << std::fixed << std::setprecision(0)
                                                << lastHit.distanceFeet << " ft   "
                                                << lastHit.exitMph << " mph";
                                            if (lastHit.clearsWall) {
                                                sub << "   CLEAR +" << lastHit.wallMarginFeet << " ft";
                                            }
                                            resultBannerSub = sub.str();
                                        }
                                        if (lastHit.quality &&
                                            (std::string(lastHit.quality) == "Moonball" ||
                                             std::string(lastHit.quality) == "Jaw Dropper")) {
                                            derby.moonballs += 1;
                                        }
                                        playHrAtmosphere(
                                            lastHit.quality &&
                                            (std::string(lastHit.quality) == "Jaw Dropper" ||
                                             std::string(lastHit.quality) == "Moonball")
                                        );
                                        // Session goal check.
                                        if (!derby.goalMet && derby.hrCount >= sessionGoalHrs) {
                                            derby.goalMet = true;
                                            goalFlashTimer = 3.5f;
                                            careerFlash = "GOAL CLEARED  ·  " +
                                                          std::to_string(sessionGoalHrs) + " HR";
                                            careerFlashTimer = 3.5f;
                                        }
                                    } else if (!isDinger && wasDinger && derby.hrCount > 0) {
                                        derby.hrCount -= 1;
                                    }
                                    if (isDinger && lastHit.distanceFeet > derby.longestHrFeet) {
                                        derby.longestHrFeet = lastHit.distanceFeet;
                                    }
                                    if (lastHit.exitMph > derby.bestExitMph) {
                                        derby.bestExitMph = lastHit.exitMph;
                                    }
                                    if (lastHit.quality) {
                                        setDerbyLastResult(lastHit.quality);
                                    }
                                    // Non-HR land result card.
                                    if (!isDinger && lastHit.quality) {
                                        resultBannerTimer = 2.0f;
                                        if (lastHit.hitsWallFace) {
                                            resultBannerTitle = "WARNING TRACK";
                                            resultBannerCol = sf::Color(255, 170, 80);
                                        } else if (lastHit.launchDeg >= 30.0f) {
                                            resultBannerTitle = "CAN OF CORN";
                                            resultBannerCol = sf::Color(160, 210, 255);
                                        } else {
                                            resultBannerTitle = lastHit.quality;
                                            resultBannerCol = sf::Color(230, 230, 200);
                                        }
                                        std::ostringstream sub;
                                        sub << std::fixed << std::setprecision(0)
                                            << lastHit.distanceFeet << " ft   "
                                            << lastHit.exitMph << " mph";
                                        resultBannerSub = sub.str();
                                    }
                                }
                                std::ostringstream oss;
                                oss << std::fixed << std::setprecision(0)
                                    << lastHit.quality << " lands  " << lastHit.distanceFeet
                                    << " ft  " << lastHit.exitMph << " mph  LA "
                                    << lastHit.launchDeg << " deg  "
                                    << (lastHit.fair ? "FAIR" : "FOUL");
                                if (lastHit.clearsWall) {
                                    oss << "  CLEAR +" << lastHit.wallMarginFeet << " ft";
                                }
                                oss << "  ·  " << countString();
                                status = oss.str();
                            }
                        }
                    } else if (!hasHit) {
                        // Soft toss / pitch: still cannot fall through ground or enter stands.
                        // Don't stick — light bounce already handled when stick=false.
                        (void)col;
                    }
                    prevBallR = stadiumLayout.radiusFromHome(baseball.position);
                }

                float ease = easyContact()
                    ? (playMode == PlayMode::Derby ? derbyContactEase() : 1.0f)
                    : 1.0f;
                HitInfo hit = tryHit(
                    baseball, bat, batCfg, prof, hasHit, fixedStep, easyContact(), ease
                );
                if (hit.hit) {
                    lastHit = hit;
                    followBallCam = true;
                    wallResolved = false;
                    prevBallR = stadiumLayout.radiusFromHome(baseball.position);
                    // Flight: drag on, no bounce when it eventually lands.
                    world.airResistanceEnabled = true;
                    world.setAtmosphere(0.07f);
                    baseball.dragCoefficient = 0.35f;
                    baseball.airResistanceScale = 0.95f;
                    baseball.restitution = 0.0f;
                    baseball.magnusScale = 0.0f; // no weird post-contact rise from residual spin
                    // Bat crack / thud on contact; crowd waits for confirmed HR call.
                    sfx.playContact(
                        lastHit.sweet,
                        isDingerQuality(lastHit.quality),
                        lastHit.exitMph
                    );
                    // Broadcast: leave plate, chase the ball; shake on solid wood.
                    broadcastCam = BroadcastCam::Chase;
                    followBallCam = true;
                    // Premium hit-stop + screen flash (scales with sweet/EV).
                    float wood = clampf(lastHit.sweet * 0.65f + (lastHit.exitMph / 120.0f) * 0.35f, 0.0f, 1.0f);
                    hitFreezeTimer = 0.045f + wood * 0.085f;
                    contactFlash = 0.55f + wood * 0.45f;
                    camShakeTimer = 0.18f + lastHit.sweet * 0.16f;
                    camShakeIntensity = 0.50f + lastHit.sweet * 0.95f;
                    if (isDingerQuality(lastHit.quality)) {
                        camShakeIntensity += 0.40f;
                        fovPunch = 0.95f + lastHit.sweet * 0.40f;
                        hitFreezeTimer += 0.04f;
                    } else if (lastHit.sweet > 0.7f && lastHit.exitMph >= 95.0f) {
                        fovPunch = 0.55f;
                    } else if (lastHit.sweet > 0.4f) {
                        fovPunch = 0.22f;
                    } else {
                        fovPunch = 0.08f;
                    }
                    // Immediate result card (refined on land for HR/wall).
                    {
                        resultBannerTimer = 2.1f;
                        if (!lastHit.fair) {
                            resultBannerTitle = "FOUL";
                            resultBannerCol = sf::Color(255, 180, 100);
                        } else if (isDingerQuality(lastHit.quality)) {
                            resultBannerTitle = lastHit.quality ? lastHit.quality : "HOME RUN";
                            if (resultBannerTitle == "Home Run") {
                                resultBannerTitle = "HOME RUN?";
                            }
                            resultBannerCol = sf::Color(255, 220, 70);
                        } else if (lastHit.hitsWallFace) {
                            resultBannerTitle = "OFF THE WALL";
                            resultBannerCol = sf::Color(255, 170, 90);
                        } else if (lastHit.launchDeg >= 28.0f) {
                            resultBannerTitle = "FLY BALL";
                            resultBannerCol = sf::Color(180, 220, 255);
                        } else if (lastHit.launchDeg <= 8.0f && lastHit.exitMph >= 88.0f) {
                            resultBannerTitle = "LINE DRIVE";
                            resultBannerCol = sf::Color(140, 255, 180);
                        } else if (lastHit.exitMph < 70.0f) {
                            resultBannerTitle = "WEAK CONTACT";
                            resultBannerCol = sf::Color(255, 140, 120);
                        } else {
                            resultBannerTitle = lastHit.quality ? lastHit.quality : "IN PLAY";
                            resultBannerCol = sf::Color(255, 230, 140);
                        }
                        std::ostringstream sub;
                        sub << std::fixed << std::setprecision(0)
                            << lastHit.exitMph << " mph   LA " << lastHit.launchDeg
                            << "°   ~" << lastHit.distanceFeet << " ft";
                        resultBannerSub = sub.str();
                    }
                    resolvePitch("HIT");
                    float zErr = baseball.position.z - plateZ;
                    const char* timing = "On time";
                    if (zErr < -0.28f) {
                        timing = "Early";
                    } else if (zErr > 0.22f) {
                        timing = "Late";
                    }
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(0)
                        << lastHit.quality << "  " << lastHit.exitMph << " mph  LA "
                        << lastHit.launchDeg << "°  ~" << lastHit.distanceFeet << " ft  "
                        << (lastHit.fair ? "FAIR" : "FOUL");
                    if (lastHit.clearsWall) {
                        oss << "  CLEAR +" << lastHit.wallMarginFeet << " ft";
                    } else if (lastHit.hitsWallFace) {
                        oss << "  WALL";
                    }
                    oss << "  " << timing
                        << "  spray " << lastHit.sprayDeg << "°  ·  " << countString();
                    status = oss.str();
                    if (!lastHit.fair) {
                        statusCol = sf::Color(255, 190, 120);
                    } else if (isDingerQuality(lastHit.quality)) {
                        statusCol = sf::Color(255, 220, 80);
                    } else if (lastHit.hitsWallFace) {
                        statusCol = sf::Color(255, 170, 90);
                    } else {
                        statusCol = lastHit.sweet > 0.85f ? sf::Color(120, 255, 160)
                            : (lastHit.sweet > 0.5f ? sf::Color(255, 230, 120) : sf::Color(255, 150, 100));
                    }
                }

                // Sample plate-crossing xy for called balls/strikes.
                if (!pitchResolved && prevBallZ < plateZ && baseball.position.z >= plateZ) {
                    float seg = baseball.position.z - prevBallZ;
                    float t = seg <= 1e-6f ? 1.0f : (plateZ - prevBallZ) / seg;
                    t = clampf(t, 0.0f, 1.0f);
                    plateCrossPos = baseball.position; // close enough mid-step
                    plateCrossPos.z = plateZ;
                    (void)t;
                }
                // Past the plate → resolve take / whiff.
                if (!pitchResolved && baseball.position.z > plateZ + 0.55f) {
                    if (swungThisPitch || bat.swinging()) {
                        resolvePitch("SWINGING_STRIKE");
                    } else {
                        bool inZone =
                            std::abs(plateCrossPos.x - strikeZoneCenter.x) <= strikeZoneHalfWidth &&
                            std::abs(plateCrossPos.y - strikeZoneCenter.y) <= strikeZoneHalfHeight;
                        resolvePitch(inZone ? "CALLED_STRIKE" : "BALL");
                    }
                }
                // Dirt ball short of a clean hit.
                if (!pitchResolved && baseball.position.y < 0.12f &&
                    baseball.position.z > plateZ - 2.0f) {
                    if (swungThisPitch || bat.swinging()) {
                        resolvePitch("SWINGING_STRIKE");
                    } else {
                        resolvePitch("BALL");
                    }
                }

                spinY += 8.0f * fixedStep;
                acc -= fixedStep;
            }
            if (trail.empty() || (baseball.position - trail.back()).magnitude() > 0.12f) {
                trail.push_back(baseball.position);
                if (trail.size() > 320) {
                    trail.erase(trail.begin());
                }
            }
        }
        // Enter broadcast hold the frame the batted ball settles.
        if (hasHit && ballSettled && broadcastCam == BroadcastCam::Chase) {
            holdLandPos = baseball.position;
            bool dinger = isDingerQuality(lastHit.quality);
            // Longer hold for jaw-droppers / moonballs; snappy otherwise.
            bool hero =
                lastHit.quality &&
                (std::string(lastHit.quality) == "Jaw Dropper" ||
                 std::string(lastHit.quality) == "Moonball");
            holdLandTimer = hero ? 1.65f
                : (dinger ? 1.15f
                   : (lastHit.hitsWallFace ? 0.75f : 0.55f));
            broadcastCam = BroadcastCam::HoldLand;
        }

        // Auto next pitch after result delay — never cut flight or the hold/return package.
        if (practiceRepitchTimer >= 0.0f) {
            bool packageBusy =
                hasHit && (broadcastCam == BroadcastCam::Chase ||
                           broadcastCam == BroadcastCam::HoldLand ||
                           broadcastCam == BroadcastCam::ReturnPlate);
            if (packageBusy) {
                // Freeze countdown until full broadcast package finishes.
            } else {
                practiceRepitchTimer -= dt;
                if (practiceRepitchTimer <= 0.0f && !bat.swinging()) {
                    beginPitch();
                }
            }
        }

        // Broadcast camera package: plate → chase → hold land → return.
        if (camShakeTimer > 0.0f) {
            camShakeTimer = std::max(0.0f, camShakeTimer - dt);
        }
        if (fovPunch > 0.0f) {
            fovPunch = std::max(0.0f, fovPunch - dt * 1.8f);
        }
        if (broadcastCam == BroadcastCam::Chase && hasHit && !ballSettled) {
            applyBallFollowCamera(camera, baseball.position, baseball.velocity);
            camera.fieldOfView += fovPunch * 95.0f;
            applyCameraShake(camera, camShakeTimer, camShakeIntensity);
        } else if (broadcastCam == BroadcastCam::HoldLand) {
            holdLandTimer -= dt;
            applyLandingHoldCamera(camera, holdLandPos);
            applyCameraShake(camera, camShakeTimer, camShakeIntensity * 0.35f);
            if (holdLandTimer <= 0.0f) {
                broadcastCam = BroadcastCam::ReturnPlate;
                returnPlateTimer = returnPlateDuration;
            }
        } else if (broadcastCam == BroadcastCam::ReturnPlate) {
            returnPlateTimer -= dt;
            float t01 = 1.0f - clampf(returnPlateTimer / returnPlateDuration, 0.0f, 1.0f);
            applyReturnToPlateCamera(camera, holdLandPos, t01);
            if (returnPlateTimer <= 0.0f) {
                broadcastCam = BroadcastCam::Plate;
                followBallCam = false;
                applyCatcherCamera(camera);
            }
        } else if (!followBallCam) {
            // Stay on plate cam during pitch / aim.
            // (beginPitch already snaps catcher view)
        }

        // Skin pitcher + plate batter.
        // Idle→swing: first ~12% of swing eases out of stance so the cut isn't a pop.
        if (bat.swinging() && batterSwingClip.duration > 0.0f) {
            float st = clampf(bat.swingT, 0.0f, 1.0f);
            if (st < 0.12f && batterStanceClip.duration > 0.0f) {
                // Blend: apply stance then swing overwrites with same pose clock start.
                // SkeletonAnimator is full replace per apply — approximate by
                // delaying swing pose until load has moved (use eased swingT).
                float u = st / 0.12f;
                u = u * u * (3.0f - 2.0f * u); // smoothstep
                float eased = u * 0.12f;
                batterAnim.applyClipNormalized(batterSwingClip, eased);
            } else {
                batterAnim.applyClipNormalized(batterSwingClip, st);
            }
        } else {
            batterAnim.applyClip(batterStanceClip, poseClock, true);
        }
        rebuildTimer += dt;
        if (rebuildTimer >= 1.0f / 60.0f) {
            rebuildTimer = 0.0f;
            pitcherModel.skinInto(pitcherAnim.skinMatrices(), pitcherMesh);
            batterModel.skinInto(batterAnim.skinMatrices(), batterMesh);
            if (useGL) {
                glPitcher.updatePositionsNormals(pitcherMesh);
                glBatter.updatePositionsNormals(batterMesh);
            }
        }

        Matrix4 pitcherXform = pitcherWorldTransform();
        // RHB stands in the 3B-side batter's box (stadium chalk at x≈−1.65).
        // 1B/RF = +X, 3B = −X. Inner half of the box, next to the plate —
        // not out in foul dirt and not inside the strike-zone volume.
        // Box center (−1.65, plateZ−0.35), half-size 1.0 × 1.5.
        constexpr float kBatterBoxX = -1.05f;
        constexpr float kBatterBoxZ = plateZ - 0.28f;
        Matrix4 batterXform =
            Matrix4::translation(Vector3(kBatterBoxX, 0.0f, kBatterBoxZ)) *
            Matrix4::rotationY(pi); // model +Z faces mound (−Z)

        // Bat–hand lockup: classic high-tip stance (barrel ~45–60° overhead /
        // behind the head). Reticle tilt still biases tip direction so aim reads.
        if (!bat.swinging()) {
            Vector3 palmLocal = batterAnim.jointWorldPosition("Palm_R");
            Vector3 palmLLocal = batterAnim.jointWorldPosition("Palm_L");
            Vector3 wristR = batterAnim.jointWorldPosition("Wrist_R");
            Vector3 palmW = batterXform.transformPoint(palmLocal);
            Vector3 palmLW = batterXform.transformPoint(palmLLocal);
            Vector3 wristW = batterXform.transformPoint(wristR);
            Vector3 gripMid = palmW * 0.58f + palmLW * 0.42f;
            gripMid = gripMid * 0.70f + wristW * 0.30f;
            float ang = reticle.plateAngleDisplay;
            // Strong tip-up (overhead) + small horizontal bias from PCI height.
            // y dominant = bat tip high behind head, not flat across the zone.
            Vector3 tipDir = safeNorm(
                Vector3(
                    0.22f + 0.28f * std::cos(ang),
                    0.86f + 0.10f * std::sin(ang),
                    0.12f - 0.06f * std::abs(ang)
                ),
                Vector3(0.25f, 0.92f, 0.12f)
            );
            bat.hands = gripMid - tipDir * 0.05f;
            bat.axis = safeNorm(lastGripAxis * 0.40f + tipDir * 0.60f, tipDir);
            lastGripHands = bat.hands;
            lastGripAxis = bat.axis;
        }

        const float ballDrawR = baseballRadius * baseballVisualScale;
        Matrix4 ballXform =
            Matrix4::translation(baseball.position) *
            Matrix4::rotationY(spinY) *
            Matrix4::scale(Vector3(ballDrawR, ballDrawR, ballDrawR));
        // Wooden bat always visible at the plate (stance load + swing path).
        // Yellow silhouette reticle is the aim PCI; 3D bat is the physical swing.
        const bool drawBatMesh = !followBallCam || broadcastCam == BroadcastCam::Plate;
        Matrix4 batXform = batModelMatrix(bat);

        Matrix4 stadiumXform = Matrix4::identity();
        if (useGL) {
            gl.beginFrame(window, camera, Stadium3D::skyColor());
            const float gr = stadiumLayout.maxWallR() + 220.0f;
            // Sky dome + drifting clouds (draw first for depth).
            gl.drawMesh(glStadiumSky, stadiumXform);
            float cloudDrift = stadiumCheerTime * 1.8f;
            gl.drawMesh(
                glStadiumClouds,
                Matrix4::translation(Vector3(cloudDrift * 0.15f, 0.0f, cloudDrift * 0.05f)) *
                    stadiumXform,
                0.92f
            );
            // Match field grass — no color seam under the park.
            gl.drawGround(gr, plateZ - gr, plateZ + gr, Stadium3D::groundClearColor());
            gl.drawMesh(glStadiumCity, stadiumXform);
            gl.drawMesh(glStadiumField, stadiumXform);
            // Contact shadows (ball + pitcher feet) for outdoor depth.
            {
                // Larger / softer shadow so the ball's ground track is obvious.
                float ballShadowR = 0.38f + baseball.position.y * 0.055f;
                ballShadowR = clampf(ballShadowR, 0.30f, 1.15f);
                float ballAlpha = clampf(0.50f - baseball.position.y * 0.028f, 0.14f, 0.50f);
                gl.drawGroundShadow(baseball.position, ballShadowR, ballAlpha);
                gl.drawGroundShadow(Vector3(0.0f, 0.0f, moundZ), 0.55f, 0.28f);
            }
            gl.drawMesh(glStadiumWalls, stadiumXform);
            gl.drawMesh(glStadiumStands, stadiumXform);
            gl.drawMesh(glStadiumLines, stadiumXform);
            // Live CF scoreboard face (pulses with HRs / excitement)
            float excitement = (hrBannerTimer > 0.0f) ? 1.0f : 0.12f;
            excitement = std::min(1.0f, excitement + static_cast<float>(derby.hrCount) * 0.08f);
            if (derby.roundOver) {
                excitement = std::max(excitement, 0.75f);
            }
            float boardA = Stadium3D::scoreboardPulse(stadiumCheerTime, excitement);
            gl.drawMesh(glStadiumBoard, stadiumXform, 0.50f + 0.50f * boardA);
            // Crowd cheer wave (stronger after a big hit / round end)
            float cheerBoost = crowdCheerBoost;
            if (hrBannerTimer > 0.0f) {
                cheerBoost = std::max(cheerBoost, 1.95f);
            }
            if (derby.roundOver && derby.roundOverTimer > 0.0f) {
                cheerBoost = std::max(cheerBoost, 2.2f);
            }
            for (int i = 0; i < Stadium3D::kFanSectorCount; i++) {
                if (!glStadiumFans[i].valid()) {
                    continue;
                }
                float bob = Stadium3D::fanCheerOffsetY(i, stadiumCheerTime, cheerBoost);
                float sway = Stadium3D::fanCheerOffsetX(i, stadiumCheerTime, cheerBoost);
                gl.drawMesh(
                    glStadiumFans[i],
                    Matrix4::translation(Vector3(sway, bob, 0.0f)) * stadiumXform
                );
            }
            // Wind-blown flags around the bowl
            for (int i = 0; i < Stadium3D::kFlagCount; i++) {
                if (!glStadiumFlags[i].valid() ||
                    i >= static_cast<int>(stadiumMeshes.flagBases.size())) {
                    continue;
                }
                Vector3 base = stadiumMeshes.flagBases[i];
                float yaw = Stadium3D::flagSwayYaw(i, stadiumCheerTime);
                Matrix4 flagX =
                    Matrix4::translation(base) * Matrix4::rotationY(yaw);
                gl.drawMesh(glStadiumFlags[i], flagX);
            }
            // Mound pitcher + plate batter silhouette for scale / product look.
            gl.drawMesh(glPitcher, pitcherXform);
            if (!followBallCam || broadcastCam == BroadcastCam::Plate) {
                gl.drawMesh(glBatter, batterXform);
            }
            if (drawBatMesh) {
                gl.drawMesh(glBat, batXform);
            }
            gl.drawMesh(glBall, ballXform);
            gl.endFrame(window);
        } else {
            frameBuffer.clear(sf::Color(5, 8, 14));
            frameBuffer.clearDepth(std::numeric_limits<float>::infinity());
            RasterMeshRenderCache stadiumCache;
            rasterizeMeshTriangles(
                frameBuffer, camera, stadiumMeshes.field, stadiumXform,
                sf::Color(40, 100, 50), stadiumCache
            );
            if (!followBallCam) {
                rasterizeMeshTriangles(
                    frameBuffer, camera, pitcherMesh, pitcherXform, sf::Color(230, 230, 235), pitcherCache
                );
                rasterizeMeshTriangles(
                    frameBuffer, camera, batterMesh, batterXform, sf::Color(220, 200, 180), batterCache
                );
            }
            if (drawBatMesh) {
                rasterizeMeshTriangles(
                    frameBuffer, camera, batMesh, batXform, sf::Color(210, 150, 70), batCache
                );
            }
            rasterizeMeshTrianglesSupersampled(
                frameBuffer, camera, baseballMesh, ballXform, sf::Color(230, 220, 205), ballCache, 2
            );
            window.clear();
            frameBuffer.present(window);
        }

        if (!followBallCam) {
            // Soft zone — reticle is the focus, not a heavy box.
            drawStrikeZone(window, camera, sf::Color(200, 215, 220, 110));
            // Yellow silhouette reticle — tilt follows PCI height.
            drawBatReticle(window, camera, reticle, batCfg);
            // Predicted plate arrival of the pitch — match yellow PCI to this.
            if (ballReleased && !hasHit && !bat.swinging()) {
                Vector3 cross;
                if (predictPlateCrossing(baseball.position, baseball.velocity, cross)) {
                    ProjectedPoint3D cp = camera.projectPoint(
                        cross,
                        static_cast<float>(window.getSize().x),
                        static_cast<float>(window.getSize().y)
                    );
                    if (cp.visible) {
                        float dAim = (cross - reticle.pci).magnitude();
                        bool covered = dAim < 0.28f;
                        sf::Color ring = covered
                            ? sf::Color(120, 255, 160, 220)
                            : sf::Color(255, 180, 80, 200);
                        sf::CircleShape pred(11.0f);
                        pred.setOrigin({11.0f, 11.0f});
                        pred.setPosition({cp.position.x, cp.position.y});
                        pred.setFillColor(sf::Color(0, 0, 0, 0));
                        pred.setOutlineThickness(covered ? 3.0f : 2.2f);
                        pred.setOutlineColor(ring);
                        window.draw(pred);
                        sf::CircleShape dot(3.5f);
                        dot.setOrigin({3.5f, 3.5f});
                        dot.setPosition({cp.position.x, cp.position.y});
                        dot.setFillColor(ring);
                        window.draw(dot);
                    }
                }
            }
        }
        for (size_t i = 1; i < trail.size(); i++) {
            float a = static_cast<float>(i) / static_cast<float>(trail.size());
            // Dual-pass trail: soft outer glow + bright core
            sf::Color outer(255, 200, 80, static_cast<std::uint8_t>(30 + a * 90));
            sf::Color core(255, 250, 210, static_cast<std::uint8_t>(70 + a * 185));
            drawThickProjectedLine(window, camera, trail[i - 1], trail[i], 6.5f, outer);
            drawThickProjectedLine(window, camera, trail[i - 1], trail[i], 3.2f, core);
        }

        // Screen-space halo so the ball stays easy to track in flight / on drop.
        if (ballReleased) {
            ProjectedPoint3D bp = camera.projectPoint(
                baseball.position,
                static_cast<float>(window.getSize().x),
                static_cast<float>(window.getSize().y)
            );
            if (bp.visible) {
                float rOuter = followBallCam ? 18.0f : 14.0f;
                sf::CircleShape glow(rOuter * 1.35f);
                glow.setOrigin({rOuter * 1.35f, rOuter * 1.35f});
                glow.setPosition({bp.position.x, bp.position.y});
                glow.setFillColor(sf::Color(255, 230, 100, 35));
                window.draw(glow);
                sf::CircleShape halo(rOuter);
                halo.setOrigin({rOuter, rOuter});
                halo.setPosition({bp.position.x, bp.position.y});
                halo.setFillColor(sf::Color(0, 0, 0, 0));
                halo.setOutlineThickness(2.8f);
                halo.setOutlineColor(sf::Color(255, 245, 120, 230));
                window.draw(halo);
                sf::CircleShape core(rOuter * 0.48f);
                core.setOrigin({rOuter * 0.48f, rOuter * 0.48f});
                core.setPosition({bp.position.x, bp.position.y});
                core.setFillColor(sf::Color(255, 250, 230, 110));
                window.draw(core);
            }
        }

        if (lastHit.hit && hasHit) {
            ProjectedPoint3D p = camera.projectPoint(
                lastHit.point,
                static_cast<float>(window.getSize().x),
                static_cast<float>(window.getSize().y)
            );
            if (p.visible) {
                sf::CircleShape flash(7.0f);
                flash.setOrigin({7, 7});
                flash.setPosition({p.position.x, p.position.y});
                flash.setFillColor(sf::Color(255, 255, 120, 170));
                window.draw(flash);
            }
        }

        // Live stats on CF board — hide while aiming so the reticle stays clear.
        if (fontOk && playMode == PlayMode::Derby &&
            (followBallCam || bat.swinging() || broadcastCam != BroadcastCam::Plate)) {
            Vector3 boardWorld = stadiumLayout.scoreboardCenter() + Vector3(0.0f, 0.0f, 2.2f);
            ProjectedPoint3D bp = camera.projectPoint(
                boardWorld,
                static_cast<float>(window.getSize().x),
                static_cast<float>(window.getSize().y)
            );
            // Only when the board is in view and not tiny / behind.
            if (bp.visible && bp.position.x > 40.0f && bp.position.x < static_cast<float>(window.getSize().x) - 40.0f) {
                float px = bp.position.x;
                float py = bp.position.y;
                const float pw = 168.0f;
                const float ph = 92.0f;
                sf::RectangleShape panel({pw, ph});
                panel.setOrigin({pw * 0.5f, ph * 0.5f});
                panel.setPosition({px, py});
                panel.setFillColor(sf::Color(8, 18, 12, 200));
                panel.setOutlineColor(sf::Color(255, 220, 80, 200));
                panel.setOutlineThickness(1.5f);
                window.draw(panel);
                drawText(
                    window, font, "PARK BOARD", 11,
                    {px - 52.0f, py - 40.0f},
                    sf::Color(255, 220, 100)
                );
                std::ostringstream b1;
                b1 << "HR  " << derby.hrCount;
                drawText(
                    window, font, b1.str(), 22,
                    {px - 48.0f, py - 22.0f},
                    sf::Color(120, 255, 160)
                );
                std::ostringstream b2;
                b2 << std::fixed << std::setprecision(0) << "LONG  ";
                if (derby.longestHrFeet > 0.5f) {
                    b2 << derby.longestHrFeet << " ft";
                } else {
                    b2 << "--";
                }
                drawText(
                    window, font, b2.str(), 14,
                    {px - 60.0f, py + 6.0f},
                    sf::Color(255, 230, 140)
                );
                std::ostringstream b3;
                b3 << "SW  " << derby.swingsLeft << "/" << kDerbySwings;
                if (derby.bestExitMph > 0.5f) {
                    b3 << "  EV " << std::fixed << std::setprecision(0) << derby.bestExitMph;
                }
                drawText(
                    window, font, b3.str(), 12,
                    {px - 70.0f, py + 28.0f},
                    sf::Color(200, 230, 255)
                );
            }
        }

        // Contact white flash (full-screen, fades fast).
        if (contactFlash > 0.01f && fontOk) {
            float w = static_cast<float>(window.getSize().x);
            float h = static_cast<float>(window.getSize().y);
            sf::RectangleShape flash({w, h});
            flash.setFillColor(sf::Color(255, 255, 245, static_cast<std::uint8_t>(contactFlash * 90.0f)));
            window.draw(flash);
        }

        // Result callout card — HR, wall, fly, foul, weak, etc.
        if (resultBannerTimer > 0.0f && fontOk && !resultBannerTitle.empty()) {
            float w = static_cast<float>(window.getSize().x);
            float h = static_cast<float>(window.getSize().y);
            float pulse = 0.55f + 0.45f * std::sin(poseClock * 9.0f);
            float pop = std::clamp(resultBannerTimer / 2.5f, 0.0f, 1.0f);
            const bool bigHr = resultBannerTitle == "HOME RUN" ||
                resultBannerTitle == "MOONBALL" || resultBannerTitle == "JAW DROPPER";
            float cardW = bigHr ? 540.0f : 460.0f;
            float cardH = bigHr ? 120.0f : 96.0f;
            sf::RectangleShape card({cardW, cardH});
            card.setOrigin({cardW * 0.5f, cardH * 0.5f});
            card.setPosition({w * 0.5f, h * 0.20f});
            card.setFillColor(sf::Color(6, 12, 10, static_cast<std::uint8_t>(175 + pop * 55)));
            sf::Color edge = resultBannerCol;
            edge.a = static_cast<std::uint8_t>(170 + pulse * 70);
            card.setOutlineColor(edge);
            card.setOutlineThickness(bigHr ? 2.8f : 2.0f);
            window.draw(card);
            unsigned titleSize = bigHr
                ? (resultBannerTitle.size() > 10 ? 38 : 50)
                : 32;
            float titleW = static_cast<float>(resultBannerTitle.size()) * titleSize * 0.30f;
            sf::Color titleCol = resultBannerCol;
            titleCol.a = static_cast<std::uint8_t>(220 + pulse * 30);
            drawText(
                window, font, resultBannerTitle, titleSize,
                {w * 0.5f - titleW * 0.5f, h * 0.20f - (bigHr ? 40.0f : 28.0f)},
                titleCol
            );
            if (!resultBannerSub.empty()) {
                drawText(
                    window, font, resultBannerSub, 18,
                    {w * 0.5f - 150.0f, h * 0.20f + (bigHr ? 22.0f : 14.0f)},
                    sf::Color(255, 245, 200)
                );
            }
        }

        // hrBannerTimer drained in main update loop (drives crowd cheer).

        // ROUND OVER celebration card
        if (fontOk && playMode == PlayMode::Derby && derby.roundOver && derby.roundOverTimer > 0.0f) {
            float w = static_cast<float>(window.getSize().x);
            float h = static_cast<float>(window.getSize().y);
            float pulse = 0.6f + 0.4f * std::sin(poseClock * 6.0f);
            sf::RectangleShape card({420.0f, 168.0f});
            card.setPosition({w * 0.5f - 210.0f, h * 0.34f});
            card.setFillColor(sf::Color(8, 16, 14, static_cast<std::uint8_t>(210 + pulse * 30)));
            card.setOutlineColor(sf::Color(255, 210, 70, 220));
            card.setOutlineThickness(2.0f);
            window.draw(card);
            drawText(
                window, font, "ROUND OVER", 36,
                {w * 0.5f - 118.0f, h * 0.34f + 14.0f},
                sf::Color(255, 220, 80)
            );
            std::ostringstream sum;
            sum << std::fixed << std::setprecision(0)
                << derby.hrCount << " HR    longest ";
            if (derby.longestHrFeet > 0.5f) {
                sum << derby.longestHrFeet << " ft";
            } else {
                sum << "--";
            }
            if (derby.goalMet) {
                sum << "   GOAL ✓";
            }
            drawText(
                window, font, sum.str(), 20,
                {w * 0.5f - 150.0f, h * 0.34f + 64.0f},
                derby.goalMet ? sf::Color(120, 255, 180) : sf::Color(200, 255, 180)
            );
            std::ostringstream sum2;
            sum2 << std::fixed << std::setprecision(0) << "Best EV  ";
            if (derby.bestExitMph > 0.5f) {
                sum2 << derby.bestExitMph << " mph";
            } else {
                sum2 << "--";
            }
            sum2 << "    swings  " << derby.totalSwings;
            if (careerBests.hardUnlocked) {
                sum2 << "    HARD open";
            }
            drawText(
                window, font, sum2.str(), 16,
                {w * 0.5f - 150.0f, h * 0.34f + 96.0f},
                sf::Color(220, 230, 210)
            );
            drawText(
                window, font, "Press N for a new round", 14,
                {w * 0.5f - 100.0f, h * 0.34f + 128.0f},
                sf::Color(180, 200, 190)
            );
        }

        if (fontOk) {
            const float W = static_cast<float>(window.getSize().x);
            const float H = static_cast<float>(window.getSize().y);
            const bool chasing = followBallCam || broadcastCam == BroadcastCam::Chase ||
                broadcastCam == BroadcastCam::HoldLand ||
                broadcastCam == BroadcastCam::ReturnPlate;
            // Compact title + one status line (no duplicated count spam).
            sf::Color titleCol = playMode == PlayMode::Derby ? sf::Color(255, 220, 80)
                : (playMode == PlayMode::Practice ? sf::Color(120, 255, 160) : sf::Color(240, 245, 240));
            std::string title = modeTitle();
            if (playMode == PlayMode::Derby) {
                title += std::string("  ·  ") + derbyDiffName(derbyDiff);
            }
            // Show active swing type without a separate tilt panel.
            title += std::string("  ·  ") + prof.name;
            drawText(window, font, title, 20, {22, 12}, titleCol);
            // Truncate long status so it doesn't cover the park.
            std::string statusLine = status;
            if (statusLine.size() > 72) {
                statusLine = statusLine.substr(0, 71) + "...";
            }
            drawText(window, font, statusLine, 14, {22, 38}, statusCol);

            // Derby goal + career strip (product-facing).
            if (playMode == PlayMode::Derby && !chasing) {
                std::ostringstream goal;
                goal << "GOAL  " << derby.hrCount << " / " << sessionGoalHrs << " HR";
                if (derby.goalMet) {
                    goal << "  ✓";
                }
                goal << "   ·   swings " << derby.swingsLeft << "/" << kDerbySwings;
                drawText(
                    window, font, goal.str(), 14, {22, 58},
                    derby.goalMet ? sf::Color(120, 255, 170) : sf::Color(200, 220, 255)
                );
                std::ostringstream career;
                career << "Career best  " << careerBests.mostHrsInRound << " HR";
                if (careerBests.longestHrFeet > 0.5f) {
                    career << "  ·  " << std::fixed << std::setprecision(0)
                           << careerBests.longestHrFeet << " ft";
                }
                if (careerBests.hardUnlocked) {
                    career << "  ·  HARD unlocked";
                }
                drawText(window, font, career.str(), 12, {22, 78}, sf::Color(150, 170, 165));
                // Park dimensions / signature quirk.
                drawText(
                    window, font,
                    "PARK  LF 330  ·  CF 400  ·  RF porch 318",
                    12, {22, 98},
                    sf::Color(130, 175, 145)
                );
                drawText(
                    window, font, "H help   1/2/3 difficulty   -/= volume", 11,
                    {22, 118}, sf::Color(120, 140, 135)
                );
            } else {
                drawText(
                    window, font, "H help   -/= bat crack volume", 12,
                    {22, 58}, sf::Color(140, 160, 150)
                );
            }

            // Goal-cleared toast.
            if (goalFlashTimer > 0.0f && playMode == PlayMode::Derby) {
                float a = std::clamp(goalFlashTimer / 3.5f, 0.0f, 1.0f);
                drawText(
                    window, font,
                    "SESSION GOAL CLEARED",
                    22,
                    {W * 0.5f - 140.0f, H * 0.12f},
                    sf::Color(120, 255, 180, static_cast<std::uint8_t>(200 * a + 40))
                );
            }

            // How-to-play overlay (first launch + H).
            if (showHelp) {
                const float pw = 480.0f;
                const float ph = 340.0f;
                const float px = W * 0.5f - pw * 0.5f;
                const float py = H * 0.22f;
                sf::RectangleShape dim({W, H});
                dim.setFillColor(sf::Color(0, 0, 0, 130));
                window.draw(dim);
                sf::RectangleShape card({pw, ph});
                card.setPosition({px, py});
                card.setFillColor(sf::Color(10, 18, 14, 240));
                card.setOutlineColor(sf::Color(255, 220, 80, 200));
                card.setOutlineThickness(2.0f);
                window.draw(card);
                drawText(window, font, "HR DERBY", 26, {px + 24, py + 14}, sf::Color(255, 220, 80));
                drawText(
                    window, font,
                    "Mouse      aim reticle  ·  height tilts the bat\n"
                    "Space/LMB  swing through the soft toss\n"
                    "Z X C      Power / Contact / Regular\n"
                    "1 2 3      Easy / Normal / Hard (unlock Hard: 3+ HR)\n"
                    "D P L      Derby / Practice / Live AB\n"
                    "R / N      next toss  /  new round\n"
                    "- / =      bat crack volume\n"
                    "\n"
                    "Session goal: hit the HR target before swings run out.\n"
                    "Park: 90' bases · 60'6\" mound · LF 330 · CF 400 · RF porch 318",
                    14, {px + 28, py + 52}, sf::Color(220, 235, 225)
                );
                drawText(
                    window, font, "Press H or swing to dismiss", 13,
                    {px + 28, py + ph - 34}, sf::Color(160, 180, 170)
                );
                if (settings.showHelpOnLaunch) {
                    settings.showHelpOnLaunch = false;
                    GameSettings::save(settings);
                }
            }

            // Scoreboard panel (top-right) — primary stats for Derby.
            if (playMode == PlayMode::Derby && !chasing) {
                const float pw = 200.0f;
                const float ph = 138.0f;
                const float px = W - pw - 14.0f;
                const float py = 12.0f;
                sf::RectangleShape panel({pw, ph});
                panel.setPosition({px, py});
                panel.setFillColor(sf::Color(12, 22, 18, 210));
                panel.setOutlineColor(
                    derby.roundOver ? sf::Color(255, 160, 60, 220)
                        : (derby.goalMet ? sf::Color(100, 255, 160, 200) : sf::Color(255, 210, 70, 160))
                );
                panel.setOutlineThickness(1.5f);
                window.draw(panel);
                drawText(
                    window, font, derby.roundOver ? "FINAL" : "SCOREBOARD", 12,
                    {px + 12, py + 8}, sf::Color(255, 220, 100)
                );
                std::ostringstream s1;
                s1 << "Swings  " << derby.swingsLeft << "/" << kDerbySwings;
                drawText(window, font, s1.str(), 14, {px + 12, py + 28}, sf::Color(230, 240, 235));
                std::ostringstream s2;
                s2 << "HR  " << derby.hrCount << " / " << sessionGoalHrs;
                if (derby.goalMet) {
                    s2 << " ✓";
                }
                drawText(
                    window, font, s2.str(), 18, {px + 12, py + 48},
                    derby.goalMet ? sf::Color(120, 255, 170) : sf::Color(120, 255, 160)
                );
                std::ostringstream s3;
                s3 << std::fixed << std::setprecision(0) << "Long  ";
                s3 << (derby.longestHrFeet > 0.5f
                           ? std::to_string(static_cast<int>(derby.longestHrFeet)) + " ft"
                           : std::string("--"));
                drawText(window, font, s3.str(), 13, {px + 12, py + 74}, sf::Color(255, 230, 140));
                std::ostringstream s4;
                s4 << std::fixed << std::setprecision(0) << "EV  ";
                s4 << (derby.bestExitMph > 0.5f
                           ? std::to_string(static_cast<int>(derby.bestExitMph)) + " mph"
                           : std::string("--"));
                drawText(window, font, s4.str(), 13, {px + 12, py + 94}, sf::Color(180, 220, 255));
                drawText(
                    window, font, "RF porch 318 · CF 400", 11,
                    {px + 12, py + 114}, sf::Color(140, 180, 150)
                );

                std::ostringstream cb;
                cb << "Career " << careerBests.mostHrsInRound << " HR";
                if (careerBests.hardUnlocked) {
                    cb << "  HARD";
                }
                drawText(window, font, cb.str(), 11, {px + 12, py + ph + 6}, sf::Color(150, 170, 160));
            }

            if (careerFlashTimer > 0.0f && !careerFlash.empty()) {
                drawText(
                    window, font, careerFlash, 16,
                    {W * 0.5f - 110.0f, 90.0f},
                    sf::Color(255, 230, 100, static_cast<std::uint8_t>(200 + careerFlashTimer * 15))
                );
            }

            // Bottom strip: only the last result (not duplicated "Last:" line).
            if (lastHit.hit && hasHit) {
                sf::RectangleShape strip({W - 40.0f, 26.0f});
                strip.setPosition({20.0f, H - 38.0f});
                strip.setFillColor(sf::Color(10, 18, 14, 185));
                strip.setOutlineColor(sf::Color(255, 210, 80, 90));
                strip.setOutlineThickness(1.0f);
                window.draw(strip);
                std::ostringstream stripTxt;
                stripTxt << std::fixed << std::setprecision(0)
                         << (lastHit.quality ? lastHit.quality : "Contact")
                         << "   " << lastHit.exitMph << " mph  "
                         << lastHit.distanceFeet << " ft";
                if (lastHit.clearsWall) {
                    stripTxt << "  CLEAR";
                } else if (lastHit.hitsWallFace) {
                    stripTxt << "  WALL";
                }
                drawText(
                    window, font, stripTxt.str(), 13,
                    {28.0f, H - 33.0f},
                    isDingerQuality(lastHit.quality) ? sf::Color(255, 220, 80)
                                                     : sf::Color(220, 230, 220)
                );
            }

            // Controls: one quiet line (hidden while chasing the ball).
            if (!chasing) {
                std::ostringstream hud;
                hud << "[" << prof.name << "]  Space swing   1/2/3 difficulty   D/P/L   N "
                    << (playMode == PlayMode::Derby ? "round" : "AB");
                drawText(window, font, hud.str(), 12, {22, 62}, sf::Color(140, 165, 155));
            }
        }

        fps.frame(window);
        window.display();
    }

    return 0;
}
