// Bat Physics Demo — same world as pitching_simulator_demo.
// CharacterModel3D pitcher, BaseballVisual3D ball, same plate distance / ground.
//
// Yellow silhouette = aim reticle (does NOT swing). Shows where contact will be.
// 3D bat mesh = real swing path (load → contact → finish) when you press Space/LMB.
// Practice mode (default) throws a slow meatball with generous contact.
//
// Controls:
//   Mouse              aim reticle (PCI / sweet spot)
//   Q / E / wheel      bat roll (reticle angle)
//   Z / X / C          Power / Contact / Regular
//   Space / LMB        swing (3D bat animates)
//   P                  toggle practice mode
//   R                  new pitch (same count)
//   N                  new at-bat (reset count)
//   [ ]                pitch speed (normal mode)
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
constexpr float feetPerWorldUnit = 2.0f;
constexpr float pitchingDistanceFeet = 60.5f;
constexpr float plateZ = pitchingDistanceFeet / feetPerWorldUnit; // ~30.25
constexpr float moundZ = 0.0f;
constexpr float strikeZoneHalfWidth = 0.46f;
constexpr float strikeZoneHalfHeight = 0.55f;
const Vector3 strikeZoneCenter(0.0f, 1.28f, plateZ);
// Huge open field — no practical wall/ceiling box (ball can fly freely).
// Ground plane is y = 0 (ball sits on radius when settled).
const Vector3 boundsMinimum(-80.0f, 0.0f, -120.0f);
const Vector3 boundsMaximum(80.0f, 80.0f, 80.0f);

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
    lookAt(
        cam,
        Vector3(0.0f, 1.28f, plateZ + 0.95f),
        Vector3(0.0f, 1.55f, moundZ + 1.2f)
    );
    cam.fieldOfView = 700.0f;
    cam.nearPlane = 0.08f;
    cam.farPlane = Stadium3D::recommendedFarPlane();
}

// Chase cam: sit behind/above the ball looking along its flight.
void applyBallFollowCamera(Camera3D& cam, const Vector3& ballPos, const Vector3& ballVel) {
    float speed = ballVel.magnitude();
    Vector3 forward = speed > 0.8f ? safeNorm(ballVel) : Vector3(0.0f, 0.15f, -1.0f);
    // Prefer staying on the outfield side of the ball (toward −Z from plate).
    if (forward.dot(Vector3(0, 0, -1)) < 0.15f && speed > 0.8f) {
        // Still useful when ball is sliced foul — keep a readable chase offset.
        forward = safeNorm(forward + Vector3(0, 0.05f, -0.35f));
    }
    Vector3 right = safeNorm(Vector3(0, 1, 0).cross(forward), Vector3(1, 0, 0));
    float dist = clampf(2.8f + speed * 0.04f, 2.6f, 7.5f);
    Vector3 pos = ballPos - forward * dist + Vector3(0, 1.15f, 0) + right * 0.15f;
    pos.y = std::max(pos.y, 0.85f);
    Vector3 target = ballPos + forward * 2.2f + Vector3(0, 0.15f, 0);
    lookAt(cam, pos, target);
    cam.fieldOfView = 820.0f;
    cam.nearPlane = 0.12f;
    cam.farPlane = Stadium3D::recommendedFarPlane();
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
    static const SwingProfile k[] = {
        {"POWER", 1.25f, 0.20f, 0.68f, 0.05f, 1.12f, 0.055f, sf::Color(255, 130, 90)},
        {"CONTACT", 0.70f, 0.30f, 1.55f, -0.01f, 0.92f, 0.10f, sf::Color(110, 210, 255)},
        {"REGULAR", 1.00f, 0.24f, 1.00f, 0.00f, 1.00f, 0.07f, sf::Color(255, 225, 70)},
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
    float sweetWidth = 0.11f;
    float minCor = 0.38f; // handle / mishit
    float maxCor = 0.52f; // barrel sweet spot (closer to real baseball COR)
};

// Aim reticle — yellow outline on the plate. Never animates with the swing.
struct AimReticle {
    Vector3 pci = strikeZoneCenter;
    float plateAngle = -0.55f; // bat angle in plate plane (rad)
};

// Animated 3D bat used for swing mesh + collision.
struct BatPose {
    // RHB at plate, facing pitcher (toward −Z / mound)
    Vector3 hands{0.42f, 1.05f, plateZ - 0.35f};
    Vector3 axis{-0.35f, 0.05f, -0.93f};
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

// Snap animated bat pose to the aim reticle (used as contact key when swing starts).
void orientBatFromReticle(BatPose& bat, const AimReticle& reticle, const BatConfig& cfg) {
    bat.pci = reticle.pci;
    Vector3 dir(std::cos(reticle.plateAngle), std::sin(reticle.plateAngle), -0.12f);
    bat.axis = safeNorm(dir);
    bat.hands = reticle.pci - bat.axis * cfg.sweetFromKnob;
    bat.hands.z = plateZ - 0.20f;
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

// Build a level-ish RHB path: load → contact (square) → finish through the zone.
// Flatter plane so exit launch isn't artificially sky-high.
void bakeSwingKeys(BatPose& bat) {
    bat.contactHands = bat.hands;
    bat.contactAxis = bat.axis;

    // Load: tip cocked up/back, hands load — not extreme uppercut.
    bat.loadHands = bat.contactHands + Vector3(0.10f, 0.10f, 0.18f);
    bat.loadAxis = slerpDir(
        bat.contactAxis,
        safeNorm(Vector3(0.55f, 0.38f, 0.22f)),
        0.72f
    );

    // Finish: drive through toward the field, slight downward follow-through.
    bat.finishHands = bat.contactHands + Vector3(-0.26f, -0.04f, -0.42f);
    bat.finishAxis = slerpDir(
        bat.contactAxis,
        safeNorm(Vector3(-0.72f, 0.08f, -0.52f)),
        0.82f
    );

    bat.swingAxis = safeNorm(bat.loadAxis.cross(bat.finishAxis), Vector3(0, 1, 0));
    if (bat.swingAxis.dot(Vector3(0, 1, 0)) < 0.0f) {
        bat.swingAxis = bat.swingAxis * -1.0f;
    }
}

// Evaluate swing pose at t∈[0,1]: load (0) → contact (~0.42) → finish (1).
void sampleSwingPose(
    const BatPose& keys,
    float t01,
    Vector3& outHands,
    Vector3& outAxis
) {
    t01 = clampf(t01, 0.0f, 1.0f);
    // Contact window centered ~0.40–0.48 so the barrel is square at the PCI.
    const float tContact = 0.42f;
    if (t01 <= tContact) {
        float u = t01 / tContact;
        // Ease-in to contact (accelerate)
        u = u * u * (1.2f - 0.2f * u);
        u = clampf(u, 0.0f, 1.0f);
        outHands = keys.loadHands + (keys.contactHands - keys.loadHands) * u;
        outAxis = slerpDir(keys.loadAxis, keys.contactAxis, u);
    } else {
        float u = (t01 - tContact) / (1.0f - tContact);
        // Ease-out after contact
        u = smooth01(u);
        outHands = keys.contactHands + (keys.finishHands - keys.contactHands) * u;
        outAxis = slerpDir(keys.contactAxis, keys.finishAxis, u);
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
    // Filled at contact, refined on landing.
    bool fair = true;
    float sprayDeg = 0;       // 0 = straight to CF (−Z), + = 1B, − = 3B
    float distanceFeet = 0;   // estimated / landing
    const char* quality = "Contact";
};

// Classify batted ball from exit velocity + landing (or projected flight).
void classifyHit(HitInfo& h, const Vector3& velocity, const Vector3& landingOrNow) {
    float horiz = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    h.launchDeg = std::atan2(velocity.y, std::max(horiz, 1e-4f)) * kDeg;
    h.exitMph = velocity.magnitude() * 2.236936f;
    // Spray relative to outfield center (−Z from plate).
    h.sprayDeg = std::atan2(velocity.x, -velocity.z) * kDeg;
    h.fair = std::abs(h.sprayDeg) <= 45.0f;

    // Distance from home plate in feet.
    float dx = landingOrNow.x;
    float dz = landingOrNow.z - plateZ;
    h.distanceFeet = std::sqrt(dx * dx + dz * dz) * feetPerWorldUnit;

    if (!h.fair) {
        h.quality = "Foul";
        return;
    }
    // Simple quality buckets (The Show–ish).
    if (h.launchDeg < 8.0f) {
        h.quality = "Grounder";
    } else if (h.launchDeg < 20.0f) {
        h.quality = (h.exitMph >= 90.0f) ? "Line Drive" : "Soft Liner";
    } else if (h.launchDeg < 38.0f) {
        if (h.exitMph >= 95.0f && h.distanceFeet >= 320.0f) {
            h.quality = "Home Run";
        } else if (h.exitMph >= 85.0f) {
            h.quality = "Fly Ball";
        } else {
            h.quality = "Flare";
        }
    } else {
        h.quality = (h.exitMph >= 80.0f) ? "Pop Up" : "Weak Pop";
    }
    // Upgrade to HR if fair and very deep even if LA a bit high.
    if (h.fair && h.distanceFeet >= 340.0f && h.exitMph >= 92.0f && h.launchDeg >= 18.0f &&
        h.launchDeg <= 42.0f) {
        h.quality = "Home Run";
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
    bool practiceMode
) {
    HitInfo h;
    if (hasHit || !bat.swinging()) {
        return h;
    }

    float s = 0.0f;
    Vector3 closest;
    closestOnBat(bat, cfg, ball.position, s, closest);

    // Practice: slightly fatter bat + mild sweet-spot magnet.
    float rScale = practiceMode ? 2.0f : 1.20f;
    float rBat = batRadius(cfg, s) * rScale;
    Vector3 delta = ball.position - closest;
    float dist = delta.magnitude();
    float minD = ball.radius + rBat;

    if (practiceMode) {
        Vector3 sweetPt = batPoint(bat, cfg.sweetFromKnob);
        float dSweet = (ball.position - sweetPt).magnitude();
        float nearPlate = std::abs(ball.position.z - plateZ);
        bool contactWindow = bat.swingT >= 0.30f && bat.swingT <= 0.55f;
        if (contactWindow && nearPlate < 0.35f && dSweet < 0.18f) {
            closest = sweetPt;
            s = cfg.sweetFromKnob;
            delta = ball.position - closest;
            dist = std::max(delta.magnitude(), 1e-4f);
            minD = ball.radius + rBat;
            Vector3 nSnap = delta * (1.0f / dist);
            ball.position = closest + nSnap * (ball.radius + batRadius(cfg, s));
            delta = ball.position - closest;
            dist = delta.magnitude();
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

    // Vertical contact quality: hitting under the ball (contact low on ball)
    // adds loft; hitting on top drives it down. Derived from geometry, not free juice.
    float undercut = clampf((ball.position.y - closest.y) / std::max(ball.radius, 0.02f), -1.2f, 1.2f);
    // undercut > 0 → bat below ball center → more launch
    // undercut < 0 → bat above center → more ground ball

    Vector3 vRel = ball.velocity - vBat;
    float approach = vRel.dot(n);
    if (approach >= 0.0f) {
        return h; // separating — no hit
    }

    float sweetScale = practiceMode ? prof.sweetScale * 1.25f : prof.sweetScale;
    float sweet = sweetFactor(cfg, s, sweetScale);
    // Real wood/aluminum COR is modest; sweet spot is king.
    float cor = clampf(lerp(cfg.minCor, cfg.maxCor + prof.corBonus * 0.5f, sweet), 0.28f, 0.50f);
    float tip = clampf(s / cfg.length, 0.0f, 1.0f);
    // Effective bat mass: higher near knob, lower at tip (end-loaded feel).
    float mEff =
        cfg.mass * prof.massScale * lerp(1.35f, 0.55f, tip) * lerp(0.65f, 1.05f, sweet);
    float mBall = std::max(ball.mass, 0.05f);

    // Moving-bat elastic collision along contact normal.
    float j = -(1.0f + cor) * approach / (1.0f / mBall + 1.0f / mEff);
    // Power/sweet: small efficiency change only.
    j *= lerp(0.88f, 1.06f, sweet) * lerp(0.94f, 1.06f, (prof.power - 0.7f) / 0.55f);
    if (practiceMode) {
        j *= 1.05f;
    }
    ball.velocity = ball.velocity + n * (j / mBall);

    // Tangential friction (spray / slice) — no loft here.
    Vector3 tanAxis = safeNorm(n.cross(Vector3(0, 1, 0)), Vector3(1, 0, 0));
    Vector3 bitangent = safeNorm(n.cross(tanAxis), Vector3(0, 1, 0));
    for (const Vector3& t : {tanAxis, bitangent}) {
        float vt = (ball.velocity - vBat).dot(t);
        float mu = 0.18f * lerp(0.7f, 1.0f, sweet);
        float jt = clampf(-vt * mBall * mu, -std::abs(j) * 0.20f, std::abs(j) * 0.20f);
        ball.velocity = ball.velocity + t * (jt / mBall);
    }

    // Attack angle: transfer a fraction of bat velocity (includes swing plane loft).
    // Only the part "through" the ball — level swings stay level.
    if (vBat.magnitude() > 1.0f) {
        float batMph = vBat.magnitude() * 2.236936f;
        // Soft-cap bat tip speed so FD noise doesn't explode exit (real BB ~70–90 mph tip).
        float tipScale = clampf(batMph / 85.0f, 0.35f, 1.15f);
        float carry = lerp(0.08f, 0.22f, sweet) * tipScale;
        if (practiceMode) {
            carry *= 1.06f;
        }
        float alongN = std::max(0.0f, safeNorm(vBat).dot(n));
        ball.velocity = ball.velocity + n * (vBat.magnitude() * alongN * carry);
    }

    // Undercut / over-the-top: modest launch tweak from contact height only.
    {
        float horiz = std::sqrt(
            ball.velocity.x * ball.velocity.x + ball.velocity.z * ball.velocity.z
        );
        // undercut +1 → add loft; −1 → subtract (chop)
        float loftKick = undercut * lerp(2.5f, 5.5f, sweet);
        ball.velocity.y += loftKick;
        // Realistic launch window: chops to high flies, no sky-rockets.
        if (horiz > 0.8f) {
            float maxUp = horiz * std::tan(38.0f * pi / 180.0f);
            float minUp = -horiz * std::tan(22.0f * pi / 180.0f);
            ball.velocity.y = clampf(ball.velocity.y, minUp, maxUp);
        }
        // Handle/mishit: deaden exit and kill loft.
        if (sweet < 0.40f) {
            ball.velocity = ball.velocity * lerp(0.55f, 0.85f, sweet / 0.40f);
        }
    }

    // Separate surfaces.
    ball.position = ball.position + n * (minD - dist + 0.003f);
    // Dead bounce on ground later — zero restitution for batted ball.
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
    h.distanceFeet = projectLandingDistanceFeet(ball.position, ball.velocity);
    classifyHit(h, ball.velocity, ball.position + ball.velocity * 0.25f);
    h.distanceFeet = projectLandingDistanceFeet(ball.position, ball.velocity);
    if (h.fair && h.distanceFeet >= 340.0f && h.exitMph >= 95.0f && h.launchDeg >= 20.0f &&
        h.launchDeg <= 38.0f) {
        h.quality = "Home Run";
    } else if (!h.fair) {
        h.quality = "Foul";
    }
    return h;
}

// ── Meshes ──────────────────────────────────────────────────────────────

Mesh3D makeBatMesh(const BatConfig& cfg) {
    Mesh3D mesh;
    const int slices = 12;
    const int along = 18;
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

Vector3 mouseToPci(const Camera3D& cam, float mx, float my, float sw, float sh) {
    float nx = (mx / sw) * 2.0f - 1.0f;
    float ny = 1.0f - (my / sh) * 2.0f;
    float cy = std::cos(cam.rotation.y);
    float sy = std::sin(cam.rotation.y);
    float cx = std::cos(cam.rotation.x);
    float sx = std::sin(cam.rotation.x);
    Vector3 forward = safeNorm(Vector3(sy * cx, -sx, cy * cx));
    Vector3 right = safeNorm(Vector3(0, 1, 0).cross(forward), Vector3(1, 0, 0));
    Vector3 up = safeNorm(forward.cross(right));
    float aspect = sw / std::max(sh, 1.0f);
    // Match catcher FOV (~700 “units” in this engine ≈ ~50–55° vertical)
    float tanHalf = std::tan(0.48f);
    Vector3 dir = safeNorm(forward + right * (nx * tanHalf * aspect) + up * (ny * tanHalf));
    float planeZ = plateZ;
    if (std::abs(dir.z) < 1e-5f) {
        return strikeZoneCenter;
    }
    float t = (planeZ - cam.position.z) / dir.z;
    if (t < 0.05f) {
        t = 2.0f;
    }
    Vector3 hit = cam.position + dir * t;
    hit.x = clampf(hit.x, -strikeZoneHalfWidth, strikeZoneHalfWidth);
    hit.y = clampf(
        hit.y,
        strikeZoneCenter.y - strikeZoneHalfHeight,
        strikeZoneCenter.y + strikeZoneHalfHeight
    );
    hit.z = planeZ;
    return hit;
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

    Vector3 axisFlat = safeNorm(
        Vector3(std::cos(reticle.plateAngle), std::sin(reticle.plateAngle), 0.0f),
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
        "Bat Physics 3D | same field as pitching sim",
        sf::Style::Default,
        sf::State::Windowed,
        glSettings
    );
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    DemoFpsCounter fps("Bat Physics 3D | shared pitcher + ball + field");
    sf::Font font;
    bool fontOk = loadUiFont(font);

    // Start in catcher view; switches to ball-follow after a hit.
    Camera3D camera;
    applyCatcherCamera(camera);
    bool followBallCam = false;

    // Same assets as pitching sim
    Mesh3D baseballMesh = BaseballVisual3D::makeMesh(48, 96);
    SkinnedModel3D pitcherModel = loadCharacterOrProcedural("pitcher", false, 2);
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

    SkeletonAnimator pitcherAnim;
    pitcherAnim.setModel(pitcherModel);
    pitcherAnim.applyClip(idleClip, 0.0f, true);
    Mesh3D pitcherMesh = pitcherModel.skinToMesh(pitcherAnim.skinMatrices());

    BatConfig batCfg;
    AimReticle reticle;
    BatPose bat;
    orientBatFromReticle(bat, reticle, batCfg);
    Mesh3D batMesh = makeBatMesh(batCfg);

    GlRenderer gl;
    bool useGL = gl.initialize(window);
    GlMesh glPitcher;
    GlMesh glBall;
    GlMesh glBat;
    GlMesh glStadiumField;
    GlMesh glStadiumWalls;
    GlMesh glStadiumStands;
    GlMesh glStadiumLines;
    Stadium3D::Layout stadiumLayout = Stadium3D::defaultPlayLayout();
    Stadium3D::Meshes stadiumMeshes = Stadium3D::build(stadiumLayout);
    if (useGL) {
        glPitcher.upload(pitcherMesh);
        glBall.upload(baseballMesh);
        glBat.upload(batMesh);
        glStadiumField.upload(stadiumMeshes.field);
        glStadiumWalls.upload(stadiumMeshes.walls);
        glStadiumStands.upload(stadiumMeshes.stands);
        glStadiumLines.upload(stadiumMeshes.lines);
    }

    FrameBuffer frameBuffer(window.getSize().x, window.getSize().y);
    RasterMeshRenderCache pitcherCache;
    RasterMeshRenderCache ballCache;
    RasterMeshRenderCache batCache;

    PhysicsWorld3D world;
    Body3D baseball;
    bool practiceMode = true; // default: slow meatball, easy contact
    float pitchMph = 52.0f;   // practice default
    float normalPitchMph = 90.0f;
    float deliveryAge = -1.0f;
    float deliveryDuration = deliveryClip.duration > 0.1f ? deliveryClip.duration : 2.2f;
    float releaseNorm = (deliveryClip.name == "throw_preview") ? (1.18f / deliveryDuration) : 0.66f;
    bool ballReleased = false;
    bool hasHit = false;
    HitInfo lastHit;
    float poseClock = 0.0f;
    float rebuildTimer = 0.0f;
    float practiceRepitchTimer = -1.0f;
    std::string status = "PRACTICE · slow meatball · aim yellow circle · Space swing";
    sf::Color statusCol(220, 230, 220);
    std::vector<Vector3> trail;
    float spinX = 0, spinY = 0, spinZ = 0;

        // HUD accent — matches bat yellow outline.
    const sf::Color batOutlineCol(255, 230, 40);

    // ── At-bat loop: pitch → swing/take → count → next pitch ───────────
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
    float hrBannerTimer = 0.0f;
    Vector3 plateCrossPos = strikeZoneCenter;
    float prevBallZ = moundZ;

    auto countString = [&]() {
        std::ostringstream oss;
        oss << "Count " << count.balls << "-" << count.strikes
            << "   P#" << count.pitchNum
            << "   H " << count.hits
            << "   BB " << count.walks
            << "   K/Out " << count.outs;
        return oss.str();
    };

    auto resetAtBat = [&]() {
        count.balls = 0;
        count.strikes = 0;
        // keep outs / hits / walks as session stats
    };

    auto scheduleNextPitch = [&](float delay) {
        practiceRepitchTimer = delay;
    };

    auto resolvePitch = [&](const char* kind) {
        if (pitchResolved) {
            return;
        }
        pitchResolved = true;
        count.pitchNum += 1;

        std::string call = kind;
        if (std::string(kind) == "HIT") {
            // Fair hit ends AB; foul contact is still a strike (unless 2 strikes).
            if (lastHit.fair) {
                count.hits += 1;
                resetAtBat();
                statusCol = sf::Color(120, 255, 160);
                if (std::string(lastHit.quality) == "Home Run") {
                    hrBannerTimer = 2.8f;
                    statusCol = sf::Color(255, 220, 80);
                }
                scheduleNextPitch(practiceMode ? 5.5f : 4.0f);
            } else {
                // Foul ball: strike if under 2, otherwise stays 2.
                if (count.strikes < 2) {
                    count.strikes += 1;
                }
                call = "Foul";
                statusCol = sf::Color(255, 200, 140);
                scheduleNextPitch(2.2f);
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
        world = PhysicsWorld3D();
        world.gravity = Vector3(0, -9.8f, 0);
        // Open park — no tight box walls/ceiling clamping exit trajectories.
        world.setBounds(boundsMinimum, boundsMaximum);
        world.airResistanceEnabled = false;
        baseball = Body3D(Vector3(0, 1.5f, moundZ), 0.145f);
        baseball.setRadius(baseballRadius);
        baseball.restitution = 0.15f; // pitch path; set to 0 after contact
        baseball.velocity = Vector3();
        baseball.angularVelocity = Vector3();
        world.addBody(&baseball);
        deliveryAge = 0.0f;
        ballReleased = false;
        hasHit = false;
        swungThisPitch = false;
        pitchResolved = false;
        landingLogged = false;
        ballSettled = false;
        hrBannerTimer = 0.0f;
        prevBallZ = moundZ;
        plateCrossPos = strikeZoneCenter;
        followBallCam = false;
        applyCatcherCamera(camera);
        practiceRepitchTimer = -1.0f;
        trail.clear();
        spinX = spinY = spinZ = 0;
        bat.swingT = -1.0f;
        bat.locked = false;
        bat.omega = 0;
        if (practiceMode) {
            pitchMph = 52.0f;
            bat.type = SwingType::Contact;
            status = "PRACTICE  ·  " + countString() + "  ·  aim yellow reticle, Space swing";
            statusCol = sf::Color(120, 255, 160);
        } else {
            status = "Pitch  ·  " + countString();
            statusCol = sf::Color(255, 230, 140);
        }
    };

    auto releaseBall = [&]() {
        Vector3 hand = pitcherAnim.throwHandWorld(pitcherWorldTransform());
        // Practice: meatball heart. Normal: small command scatter like pitching sim.
        Vector3 aim = strikeZoneCenter;
        if (!practiceMode) {
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
        baseball.position = hand;
        baseball.velocity = launchVelocityTowardPlate(hand, aim, pitchMph);
        prevBallZ = hand.z;
        ballReleased = true;
        status = practiceMode
            ? "Live pitch  ·  " + countString() + "  ·  cover reticle, Space/LMB"
            : "Ball in flight  ·  " + countString();
        statusCol = sf::Color(180, 230, 255);
    };

    beginPitch();

    sf::Clock clock;
    while (window.isOpen()) {
        float dt = std::min(clock.restart().asSeconds(), 0.05f);
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
                    count = AtBatCount{};
                    beginPitch();
                    status = "New at-bat  ·  " + countString();
                    statusCol = sf::Color(200, 230, 255);
                } else if (key->code == K::P) {
                    practiceMode = !practiceMode;
                    if (practiceMode) {
                        pitchMph = 52.0f;
                        bat.type = SwingType::Contact;
                        status = "PRACTICE ON — slow meatball + easy contact";
                        statusCol = sf::Color(120, 255, 160);
                    } else {
                        pitchMph = normalPitchMph;
                        status = "PRACTICE OFF — full speed";
                        statusCol = sf::Color(255, 200, 120);
                    }
                    beginPitch();
                } else if (key->code == K::LBracket) {
                    if (!practiceMode) {
                        pitchMph = std::max(60.0f, pitchMph - 2.0f);
                        normalPitchMph = pitchMph;
                    }
                } else if (key->code == K::RBracket) {
                    if (!practiceMode) {
                        pitchMph = std::min(105.0f, pitchMph + 2.0f);
                        normalPitchMph = pitchMph;
                    }
                } else if (key->code == K::Q) {
                    if (!bat.swinging() && !followBallCam) {
                        reticle.plateAngle -= 0.10f;
                    }
                } else if (key->code == K::E) {
                    if (!bat.swinging() && !followBallCam) {
                        reticle.plateAngle += 0.10f;
                    }
                } else if (key->code == K::Space) {
                    // Swing only on press — reticle stays put; 3D bat animates.
                    if (!bat.swinging() && !pitchResolved && ballReleased) {
                        orientBatFromReticle(bat, reticle, batCfg);
                        startSwing(bat);
                        swungThisPitch = true;
                        hasHit = false;
                        status = std::string(prof.name) + " swing!  ·  " + countString();
                        statusCol = prof.color;
                    }
                }
            }
            if (const auto* m = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (m->button == sf::Mouse::Button::Left && !bat.swinging() && !pitchResolved &&
                    ballReleased) {
                    orientBatFromReticle(bat, reticle, batCfg);
                    startSwing(bat);
                    swungThisPitch = true;
                    hasHit = false;
                    status = std::string(prof.name) + " swing!  ·  " + countString();
                    statusCol = prof.color;
                }
            }
            if (const auto* wh = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (!bat.swinging() && !followBallCam) {
                    reticle.plateAngle += wh->delta * 0.08f;
                }
            }
        }

        poseClock += dt;
        if (hrBannerTimer > 0.0f) {
            hrBannerTimer = std::max(0.0f, hrBannerTimer - dt);
        }

        // Pitcher animation + ball glue / release
        if (deliveryAge >= 0.0f) {
            deliveryAge += dt;
            float t01 = clampf(deliveryAge / deliveryDuration, 0.0f, 1.0f);
            pitcherAnim.applyClipNormalized(deliveryClip, t01);
            if (!ballReleased) {
                baseball.position = pitcherAnim.throwHandWorld(pitcherWorldTransform());
                baseball.velocity = Vector3();
                if (t01 >= releaseNorm) {
                    releaseBall();
                }
            }
            if (deliveryAge > deliveryDuration + 2.5f && !bat.swinging()) {
                // Hold last pose briefly then idle
                pitcherAnim.applyClip(idleClip, poseClock, true);
            }
        } else {
            pitcherAnim.applyClip(idleClip, poseClock, true);
        }

        // Reticle: mouse aims the yellow silhouette (never swings).
        // 3D bat: only updates while swinging (load → contact → finish).
        if (!bat.swinging() && !followBallCam) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            reticle.pci = mouseToPci(
                camera,
                static_cast<float>(mp.x),
                static_cast<float>(mp.y),
                static_cast<float>(window.getSize().x),
                static_cast<float>(window.getSize().y)
            );
            // Keep contact pose ready so swing keys match reticle.
            orientBatFromReticle(bat, reticle, batCfg);
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
                    baseball.position.y = baseball.radius + 0.01f;
                } else {
                    world.step(fixedStep);
                }

                HitInfo hit = tryHit(baseball, bat, batCfg, prof, hasHit, fixedStep, practiceMode);
                if (hit.hit) {
                    lastHit = hit;
                    followBallCam = true;
                    // Flight: drag on, no bounce when it eventually lands.
                    world.airResistanceEnabled = true;
                    world.setAtmosphere(0.07f);
                    baseball.dragCoefficient = 0.35f;
                    baseball.airResistanceScale = 0.95f;
                    baseball.restitution = 0.0f;
                    baseball.magnusScale = 0.0f; // no weird post-contact rise from residual spin
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
                        << (lastHit.fair ? "FAIR" : "FOUL")
                        << "  " << timing
                        << "  spray " << lastHit.sprayDeg << "°  ·  " << countString();
                    status = oss.str();
                    if (!lastHit.fair) {
                        statusCol = sf::Color(255, 190, 120);
                    } else if (std::string(lastHit.quality) == "Home Run") {
                        statusCol = sf::Color(255, 220, 80);
                    } else {
                        statusCol = lastHit.sweet > 0.85f ? sf::Color(120, 255, 160)
                            : (lastHit.sweet > 0.5f ? sf::Color(255, 230, 120) : sf::Color(255, 150, 100));
                    }
                }

                // Stick the ball on first ground contact — no roll, no bounce.
                if (hasHit && !ballSettled) {
                    const float groundY = baseball.radius + 0.01f;
                    bool nearGround = baseball.position.y <= groundY + 0.04f;
                    bool falling = baseball.velocity.y <= 0.8f;
                    if (nearGround && falling) {
                        baseball.position.y = groundY;
                        baseball.velocity = Vector3();
                        baseball.angularVelocity = Vector3();
                        baseball.acceleration = Vector3();
                        baseball.restitution = 0.0f;
                        ballSettled = true;

                        if (!landingLogged) {
                            landingLogged = true;
                            float dx = baseball.position.x;
                            float dz = baseball.position.z - plateZ;
                            lastHit.distanceFeet = std::sqrt(dx * dx + dz * dz) * feetPerWorldUnit;
                            if (lastHit.fair && lastHit.distanceFeet >= 340.0f &&
                                lastHit.exitMph >= 95.0f && lastHit.launchDeg >= 20.0f &&
                                lastHit.launchDeg <= 38.0f) {
                                lastHit.quality = "Home Run";
                            }
                            std::ostringstream oss;
                            oss << std::fixed << std::setprecision(0)
                                << lastHit.quality << " lands  " << lastHit.distanceFeet << " ft  "
                                << lastHit.exitMph << " mph  LA " << lastHit.launchDeg << "°  "
                                << (lastHit.fair ? "FAIR" : "FOUL") << "  ·  " << countString();
                            status = oss.str();
                        }
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
            if (trail.empty() || (baseball.position - trail.back()).magnitude() > 0.20f) {
                trail.push_back(baseball.position);
                if (trail.size() > 220) {
                    trail.erase(trail.begin());
                }
            }
        }
        // Auto next pitch after result delay.
        if (practiceRepitchTimer >= 0.0f) {
            practiceRepitchTimer -= dt;
            if (practiceRepitchTimer <= 0.0f && !bat.swinging()) {
                beginPitch();
            }
        }

        // Camera: catcher for pitch/swing, chase the ball after contact.
        if (followBallCam && hasHit) {
            applyBallFollowCamera(camera, baseball.position, baseball.velocity);
        }

        // Skin pitcher
        rebuildTimer += dt;
        if (rebuildTimer >= 1.0f / 60.0f) {
            rebuildTimer = 0.0f;
            pitcherModel.skinInto(pitcherAnim.skinMatrices(), pitcherMesh);
            if (useGL) {
                glPitcher.updatePositionsNormals(pitcherMesh);
            }
        }

        Matrix4 pitcherXform = pitcherWorldTransform();
        Matrix4 ballXform =
            Matrix4::translation(baseball.position) *
            Matrix4::rotationY(spinY) *
            Matrix4::scale(Vector3(baseballRadius, baseballRadius, baseballRadius));
        // 3D bat mesh only while swinging; yellow silhouette is the aim reticle.
        const bool drawBatMesh = bat.swinging();
        Matrix4 batXform = batModelMatrix(bat);

        Matrix4 stadiumXform = Matrix4::identity();
        if (useGL) {
            gl.beginFrame(window, camera, sf::Color(5, 8, 14));
            const float gr = stadiumLayout.wallR() + 80.0f;
            gl.drawGround(gr, plateZ - gr, plateZ + 40.0f, sf::Color(18, 32, 22));
            gl.drawMesh(glStadiumField, stadiumXform);
            gl.drawMesh(glStadiumWalls, stadiumXform);
            gl.drawMesh(glStadiumStands, stadiumXform);
            gl.drawMesh(glStadiumLines, stadiumXform);
            if (!followBallCam) {
                gl.drawMesh(glPitcher, pitcherXform);
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
            drawStrikeZone(window, camera, sf::Color(200, 215, 220, 180));
            // Yellow silhouette reticle — stays put even while the 3D bat swings.
            drawBatReticle(window, camera, reticle, batCfg);
        }
        for (size_t i = 1; i < trail.size(); i++) {
            float a = static_cast<float>(i) / static_cast<float>(trail.size());
            sf::Color c(255, 240, 180, static_cast<std::uint8_t>(40 + a * 170));
            drawThickProjectedLine(window, camera, trail[i - 1], trail[i], 2.0f, c);
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

        // Big HOME RUN banner when you go yard.
        if (hrBannerTimer > 0.0f && fontOk) {
            float w = static_cast<float>(window.getSize().x);
            float h = static_cast<float>(window.getSize().y);
            float pulse = 0.55f + 0.45f * std::sin(poseClock * 10.0f);
            sf::Color banner(255, 220, 60, static_cast<std::uint8_t>(200 + pulse * 55));
            drawText(window, font, "HOME RUN", 48, {w * 0.5f - 140.0f, h * 0.18f}, banner);
            std::ostringstream d;
            d << std::fixed << std::setprecision(0) << lastHit.distanceFeet << " ft   "
              << lastHit.exitMph << " mph";
            drawText(window, font, d.str(), 22, {w * 0.5f - 90.0f, h * 0.18f + 56.0f}, sf::Color(255, 240, 180));
        }

        if (fontOk) {
            drawText(
                window,
                font,
                practiceMode ? "Bat Physics 3D  ·  PRACTICE" : "Bat Physics 3D",
                20,
                {22, 14},
                practiceMode ? sf::Color(120, 255, 160) : sf::Color(240, 245, 240)
            );
            drawText(window, font, status, 15, {22, 42}, statusCol);
            std::ostringstream modes;
            modes << "Z Power  X Contact  C Regular   [" << prof.name << "  pwr "
                  << std::fixed << std::setprecision(2) << prof.power << "]   "
                  << countString();
            drawText(window, font, modes.str(), 14, {22, 68}, prof.color);
            std::ostringstream hud;
            hud << std::fixed << std::setprecision(0)
                << "Pitch " << pitchMph << " mph   "
                << (practiceMode ? "P exit practice" : "P practice")
                << "   R next pitch   N new at-bat   Space/LMB swing";
            if (!practiceMode) {
                hud << "   [ ] speed";
            }
            drawText(window, font, hud.str(), 12, {22, 92}, sf::Color(160, 190, 180));
            drawText(
                window,
                font,
                followBallCam
                    ? "Camera following ball  ·  R new pitch"
                    : "Yellow outline = aim reticle  ·  Small circle = sweet spot  ·  3D bat swings on Space",
                12,
                {22, 112},
                batOutlineCol
            );
            if (lastHit.hit) {
                std::ostringstream hit;
                hit << std::fixed << std::setprecision(0)
                    << "Last: " << lastHit.quality << "  " << lastHit.exitMph << " mph @ "
                    << lastHit.launchDeg << "°  " << lastHit.distanceFeet << " ft  "
                    << (lastHit.fair ? "FAIR" : "FOUL")
                    << "  spray " << lastHit.sprayDeg << "°  sweet "
                    << (lastHit.sweet * 100.0f) << "%";
                drawText(window, font, hit.str(), 13, {22, 132}, sf::Color(255, 220, 140));
            }
        }

        fps.frame(window);
        window.display();
    }

    return 0;
}
