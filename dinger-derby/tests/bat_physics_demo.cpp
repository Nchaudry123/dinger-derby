// Bat Physics Demo — same world as pitching_simulator_demo.
// CharacterModel3D pitcher, BaseballVisual3D ball, same plate distance / ground.
//
// Mouse aims PCI on the plate. Z/X/C = Power / Contact / Regular.
// Space / LMB swings. Pitcher throws with throw_preview; ball leaves at release.
//
// Controls:
//   Mouse              aim PCI
//   Q / E / wheel      bat roll
//   Z / X / C          Power / Contact / Regular
//   Space / LMB        swing
//   R                  new pitch
//   [ ]                pitch speed
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
const Vector3 boundsMinimum(-3.2f, -40.0f, -2.0f);
const Vector3 boundsMaximum(3.2f, 3.6f, plateZ + 4.0f);

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

Matrix4 pitcherWorldTransform() {
    return Matrix4::translation(Vector3(0.0f, 0.0f, moundZ + 0.15f));
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
    float minCor = 0.34f;
    float maxCor = 0.56f;
};

struct BatPose {
    // RHB at plate, facing pitcher (toward −Z / mound)
    Vector3 hands{0.42f, 1.05f, plateZ - 0.35f};
    Vector3 axis{-0.35f, 0.05f, -0.93f};
    float roll = 0.0f;
    float swingT = -1.0f;
    float omega = 0.0f;
    Vector3 swingAxis{0, 1, 0};
    SwingType type = SwingType::Regular;
    Vector3 pci = strikeZoneCenter;
    bool locked = false;

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

void orientBat(BatPose& bat, const Vector3& pci) {
    bat.pci = pci;
    Vector3 to = pci - bat.hands;
    if (to.magnitude() < 0.08f) {
        to = Vector3(-0.2f, 0.0f, -0.8f);
    }
    bat.axis = safeNorm(to);
    Vector3 up(0, 1, 0);
    Vector3 side = safeNorm(up.cross(bat.axis), Vector3(1, 0, 0));
    bat.swingAxis = safeNorm(bat.axis.cross(side), up);
}

Vector3 batPointVelocity(const BatPose& bat, float s) {
    return (bat.swingAxis * bat.omega).cross(bat.axis * s);
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

void startSwing(BatPose& bat) {
    if (bat.swinging()) {
        return;
    }
    bat.swingT = 0.0f;
    bat.omega = 0.0f;
    bat.locked = true;
}

void updateSwing(BatPose& bat, const SwingProfile& prof, float dt) {
    if (bat.swingT < 0.0f) {
        bat.omega = 0.0f;
        return;
    }
    float prev = bat.swingT;
    bat.swingT = std::min(1.0f, bat.swingT + dt / prof.duration);
    auto speedCurve = [](float t) {
        float u = clampf(t, 0.0f, 1.0f);
        return 6.0f * u * (1.0f - u);
    };
    float peak = 15.0f * prof.power;
    float w0 = peak * speedCurve(prev);
    float w1 = peak * speedCurve(bat.swingT);
    bat.omega = w1;
    float dTheta = 0.5f * (w0 + w1) * dt;
    if (std::abs(dTheta) > 1e-7f) {
        Vector3 k = bat.swingAxis;
        Vector3 v = bat.axis;
        float c = std::cos(dTheta);
        float s = std::sin(dTheta);
        bat.axis = safeNorm(v * c + k.cross(v) * s + k * (k.dot(v) * (1.0f - c)));
    }
    if (bat.swingT >= 1.0f) {
        bat.omega = 0.0f;
        bat.swingT = -1.0f;
        bat.locked = false;
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
};

HitInfo tryHit(
    Body3D& ball,
    const BatPose& bat,
    const BatConfig& cfg,
    const SwingProfile& prof,
    bool& hasHit
) {
    HitInfo h;
    if (hasHit || !bat.swinging()) {
        return h;
    }
    float s = 0.0f;
    Vector3 closest;
    closestOnBat(bat, cfg, ball.position, s, closest);
    float rBat = batRadius(cfg, s) * 1.3f;
    Vector3 delta = ball.position - closest;
    float dist = delta.magnitude();
    float minD = ball.radius + rBat;
    if (dist > minD || dist < 1e-5f) {
        return h;
    }
    Vector3 n = delta * (1.0f / dist);
    Vector3 vBat = batPointVelocity(bat, s);
    Vector3 vRel = ball.velocity - vBat;
    float approach = vRel.dot(n);
    if (approach >= 0.0f) {
        return h;
    }
    float sweet = sweetFactor(cfg, s, prof.sweetScale);
    float cor = clampf(lerp(cfg.minCor, cfg.maxCor + prof.corBonus, sweet), 0.28f, 0.58f);
    float tip = clampf(s / cfg.length, 0.0f, 1.0f);
    float mEff =
        cfg.mass * prof.massScale * lerp(1.3f, 0.55f, tip) * lerp(0.75f, 1.15f, sweet);
    float mBall = std::max(ball.mass, 0.05f);
    float j = -(1.0f + cor) * approach / (1.0f / mBall + 1.0f / mEff);
    j *= lerp(0.9f, 1.12f, sweet) * lerp(0.95f, 1.1f, (prof.power - 0.7f) / 0.55f);
    ball.velocity = ball.velocity + n * (j / mBall);
    Vector3 tan = safeNorm(n.cross(Vector3(0, 1, 0)), Vector3(1, 0, 0));
    float vt = vRel.dot(tan);
    float jt = clampf(-vt * mBall * 0.3f, -std::abs(j) * 0.15f * sweet, std::abs(j) * 0.15f * sweet);
    ball.velocity = ball.velocity + tan * (jt / mBall);
    ball.position = ball.position + n * (minD - dist + 0.006f);

    hasHit = true;
    h.hit = true;
    h.s = s;
    h.sweet = sweet;
    h.point = closest;
    h.batMph = vBat.magnitude() * 2.236936f;
    h.exitMph = ball.velocity.magnitude() * 2.236936f;
    float horiz = std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.z * ball.velocity.z);
    h.launchDeg = std::atan2(ball.velocity.y, horiz) * kDeg;
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
            sf::Color col = barrel ? sf::Color(175, 118, 58) : sf::Color(145, 95, 48);
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
    float tanHalf = std::tan(0.45f);
    Vector3 dir = safeNorm(forward + right * (nx * tanHalf * aspect) + up * (ny * tanHalf));
    // Plate plane
    float planeZ = plateZ + 0.05f;
    if (std::abs(dir.z) < 1e-5f) {
        return strikeZoneCenter;
    }
    float t = (planeZ - cam.position.z) / dir.z;
    if (t < 0.1f) {
        t = 5.0f;
    }
    Vector3 hit = cam.position + dir * t;
    hit.x = clampf(hit.x, -strikeZoneHalfWidth * 1.2f, strikeZoneHalfWidth * 1.2f);
    hit.y = clampf(
        hit.y,
        strikeZoneCenter.y - strikeZoneHalfHeight - 0.08f,
        strikeZoneCenter.y + strikeZoneHalfHeight + 0.08f
    );
    hit.z = planeZ;
    return hit;
}

void drawPci(
    sf::RenderWindow& window,
    const Camera3D& cam,
    const Vector3& pci,
    float radiusM,
    sf::Color col
) {
    const int n = 22;
    Vector3 prev;
    for (int i = 0; i <= n; i++) {
        float a = (static_cast<float>(i) / n) * pi * 2.0f;
        Vector3 p = pci + Vector3(std::cos(a) * radiusM, std::sin(a) * radiusM, 0);
        if (i > 0) {
            drawThickProjectedLine(window, cam, prev, p, 2.2f, col);
        }
        prev = p;
    }
    drawThickProjectedLine(
        window, cam, pci + Vector3(-radiusM * 0.55f, 0, 0), pci + Vector3(radiusM * 0.55f, 0, 0), 1.6f, col
    );
    drawThickProjectedLine(
        window, cam, pci + Vector3(0, -radiusM * 0.55f, 0), pci + Vector3(0, radiusM * 0.55f, 0), 1.6f, col
    );
}

void drawBatSilhouette2D(
    sf::RenderWindow& window,
    const Camera3D& cam,
    const BatPose& bat,
    const BatConfig& cfg,
    sf::Color col
) {
    const int segs = 14;
    float sw = static_cast<float>(window.getSize().x);
    float sh = static_cast<float>(window.getSize().y);
    Vector3 toCam = safeNorm(cam.position - bat.hands);
    Vector3 side = safeNorm(bat.axis.cross(toCam), Vector3(1, 0, 0));
    std::vector<sf::Vector2f> L, R;
    for (int i = 0; i <= segs; i++) {
        float t = static_cast<float>(i) / segs;
        float s = t * cfg.length;
        float r = batRadius(cfg, s) * (t > 0.4f ? 1.4f : 1.15f);
        Vector3 c = batPoint(bat, s);
        ProjectedPoint3D pl = cam.projectPoint(c + side * r, sw, sh);
        ProjectedPoint3D pr = cam.projectPoint(c - side * r, sw, sh);
        if (pl.visible) {
            L.push_back({pl.position.x, pl.position.y});
        }
        if (pr.visible) {
            R.push_back({pr.position.x, pr.position.y});
        }
    }
    auto poly = [&](const std::vector<sf::Vector2f>& pts) {
        for (size_t i = 1; i < pts.size(); i++) {
            sf::Vertex line[] = {sf::Vertex{pts[i - 1], col}, sf::Vertex{pts[i], col}};
            window.draw(line, 2, sf::PrimitiveType::Lines);
        }
    };
    poly(L);
    poly(R);
    if (!L.empty() && !R.empty()) {
        sf::Vertex a[] = {sf::Vertex{L.back(), col}, sf::Vertex{R.back(), col}};
        sf::Vertex b[] = {sf::Vertex{L.front(), col}, sf::Vertex{R.front(), col}};
        window.draw(a, 2, sf::PrimitiveType::Lines);
        window.draw(b, 2, sf::PrimitiveType::Lines);
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

    // Batter-side camera looking toward pitcher on the mound
    Camera3D camera;
    lookAt(
        camera,
        Vector3(1.1f, 1.65f, plateZ + 2.4f),
        Vector3(0.0f, 1.35f, moundZ + 2.0f)
    );
    camera.fieldOfView = 780.0f;
    camera.nearPlane = 0.08f;

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
    BatPose bat;
    orientBat(bat, strikeZoneCenter);
    Mesh3D batMesh = makeBatMesh(batCfg);

    GlRenderer gl;
    bool useGL = gl.initialize(window);
    GlMesh glPitcher;
    GlMesh glBall;
    GlMesh glBat;
    if (useGL) {
        glPitcher.upload(pitcherMesh);
        glBall.upload(baseballMesh);
        glBat.upload(batMesh);
    }

    FrameBuffer frameBuffer(window.getSize().x, window.getSize().y);
    RasterMeshRenderCache pitcherCache;
    RasterMeshRenderCache ballCache;
    RasterMeshRenderCache batCache;

    PhysicsWorld3D world;
    Body3D baseball;
    float pitchMph = 90.0f;
    float deliveryAge = -1.0f;
    float deliveryDuration = deliveryClip.duration > 0.1f ? deliveryClip.duration : 2.2f;
    float releaseNorm = (deliveryClip.name == "throw_preview") ? (1.18f / deliveryDuration) : 0.66f;
    bool ballReleased = false;
    bool hasHit = false;
    HitInfo lastHit;
    float poseClock = 0.0f;
    float rebuildTimer = 0.0f;
    float batRoll = 0.0f;
    std::string status = "R pitch · Mouse aim · Z/X/C swing type · Space swing";
    sf::Color statusCol(220, 230, 220);
    std::vector<Vector3> trail;
    float spinX = 0, spinY = 0, spinZ = 0;

    auto beginPitch = [&]() {
        world = PhysicsWorld3D();
        world.gravity = Vector3(0, -9.8f, 0);
        world.setBounds(boundsMinimum, boundsMaximum);
        world.airResistanceEnabled = false;
        baseball = Body3D(Vector3(0, 1.5f, moundZ), 0.145f);
        baseball.setRadius(baseballRadius);
        baseball.restitution = 0.4f;
        baseball.velocity = Vector3();
        world.addBody(&baseball);
        deliveryAge = 0.0f;
        ballReleased = false;
        hasHit = false;
        trail.clear();
        spinX = spinY = spinZ = 0;
        bat.swingT = -1.0f;
        bat.locked = false;
        bat.omega = 0;
        status = "Pitcher winding up…";
        statusCol = sf::Color(255, 230, 140);
    };

    auto releaseBall = [&]() {
        Vector3 hand = pitcherAnim.throwHandWorld(pitcherWorldTransform());
        baseball.position = hand;
        float speed = pitchMph * 0.44704f;
        // Toward center of zone (simple four-seam path)
        Vector3 target = strikeZoneCenter + Vector3(0.0f, 0.0f, 0.0f);
        Vector3 dir = safeNorm(target - hand, Vector3(0, 0, 1));
        baseball.velocity = dir * speed;
        ballReleased = true;
        status = "Ball in flight — swing!";
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
                } else if (key->code == K::Space) {
                    startSwing(bat);
                    hasHit = false;
                    status = std::string(prof.name) + " swing";
                    statusCol = prof.color;
                } else if (key->code == K::R) {
                    beginPitch();
                } else if (key->code == K::LBracket) {
                    pitchMph = std::max(60.0f, pitchMph - 2.0f);
                } else if (key->code == K::RBracket) {
                    pitchMph = std::min(105.0f, pitchMph + 2.0f);
                } else if (key->code == K::Q) {
                    batRoll -= 0.08f;
                } else if (key->code == K::E) {
                    batRoll += 0.08f;
                }
            }
            if (const auto* m = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (m->button == sf::Mouse::Button::Left) {
                    startSwing(bat);
                    hasHit = false;
                    status = std::string(prof.name) + " swing";
                    statusCol = prof.color;
                }
            }
            if (const auto* wh = event->getIf<sf::Event::MouseWheelScrolled>()) {
                batRoll += wh->delta * 0.06f;
            }
        }

        poseClock += dt;

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

        // Mouse aim
        if (!bat.locked) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            Vector3 pci = mouseToPci(
                camera,
                static_cast<float>(mp.x),
                static_cast<float>(mp.y),
                static_cast<float>(window.getSize().x),
                static_cast<float>(window.getSize().y)
            );
            bat.roll = batRoll;
            orientBat(bat, pci);
        }
        updateSwing(bat, prof, dt);

        // Flight + bat contact
        if (ballReleased) {
            float acc = dt;
            while (acc >= fixedStep) {
                world.step(fixedStep);
                HitInfo hit = tryHit(baseball, bat, batCfg, prof, hasHit);
                if (hit.hit) {
                    lastHit = hit;
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(0)
                        << prof.name << " HIT  " << lastHit.exitMph << " mph exit  LA "
                        << lastHit.launchDeg << "°  bat " << lastHit.batMph << " mph  sweet "
                        << (lastHit.sweet * 100.0f) << "%  ·  pitch " << pitchMph << " mph";
                    status = oss.str();
                    statusCol = lastHit.sweet > 0.85f ? sf::Color(120, 255, 160)
                        : (lastHit.sweet > 0.5f ? sf::Color(255, 230, 120) : sf::Color(255, 150, 100));
                }
                spinY += 8.0f * fixedStep;
                acc -= fixedStep;
            }
            if (trail.empty() || (baseball.position - trail.back()).magnitude() > 0.15f) {
                trail.push_back(baseball.position);
                if (trail.size() > 120) {
                    trail.erase(trail.begin());
                }
            }
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
        Matrix4 batXform = batModelMatrix(bat);

        if (useGL) {
            gl.beginFrame(window, camera, sf::Color(5, 8, 14));
            gl.drawGround(4.0f, -2.0f, plateZ + 4.0f, sf::Color(20, 28, 24));
            gl.drawMesh(glPitcher, pitcherXform);
            gl.drawMesh(glBat, batXform);
            gl.drawMesh(glBall, ballXform);
            gl.endFrame(window);
        } else {
            frameBuffer.clear(sf::Color(5, 8, 14));
            frameBuffer.clearDepth(std::numeric_limits<float>::infinity());
            rasterizeMeshTriangles(
                frameBuffer, camera, pitcherMesh, pitcherXform, sf::Color(230, 230, 235), pitcherCache
            );
            rasterizeMeshTriangles(
                frameBuffer, camera, batMesh, batXform, sf::Color(170, 115, 55), batCache
            );
            rasterizeMeshTrianglesSupersampled(
                frameBuffer, camera, baseballMesh, ballXform, sf::Color(230, 220, 205), ballCache, 2
            );
            window.clear();
            frameBuffer.present(window);
        }

        drawFieldGuide(window, camera);
        drawHomePlate(window, camera);
        drawStrikeZone(window, camera, sf::Color(200, 215, 220, 180));
        for (size_t i = 1; i < trail.size(); i++) {
            float a = static_cast<float>(i) / static_cast<float>(trail.size());
            sf::Color c(255, 240, 180, static_cast<std::uint8_t>(40 + a * 170));
            drawThickProjectedLine(window, camera, trail[i - 1], trail[i], 2.0f, c);
        }
        drawPci(window, camera, bat.pci, prof.pciR, prof.color);
        drawBatSilhouette2D(window, camera, bat, batCfg, prof.color);

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

        if (fontOk) {
            drawText(window, font, "Bat Physics 3D", 20, {22, 14}, sf::Color(240, 245, 240));
            drawText(window, font, status, 15, {22, 42}, statusCol);
            std::ostringstream modes;
            modes << "Z Power  X Contact  C Regular   [" << prof.name << "  pwr "
                  << std::fixed << std::setprecision(2) << prof.power << "]";
            drawText(window, font, modes.str(), 14, {22, 68}, prof.color);
            std::ostringstream hud;
            hud << std::fixed << std::setprecision(0)
                << "Pitch " << pitchMph
                << " mph   R new pitch   Mouse PCI   Q/E roll   Space/LMB swing   [ ] speed";
            drawText(window, font, hud.str(), 12, {22, 92}, sf::Color(160, 190, 180));
            if (lastHit.hit) {
                std::ostringstream hit;
                hit << std::fixed << std::setprecision(0)
                    << "Last: exit " << lastHit.exitMph << " mph @ " << lastHit.launchDeg
                    << "° · bat " << lastHit.batMph << " · sweet " << (lastHit.sweet * 100.0f)
                    << "% · pitch was " << pitchMph << " mph";
                drawText(window, font, hit.str(), 13, {22, 116}, sf::Color(255, 220, 140));
            }
        }

        fps.frame(window);
        window.display();
    }

    return 0;
}
