// Bat Physics Demo — 3D batting cage (separate from pitching simulator).
//
// Mouse aims PCI on the plate plane; 3D bat orients to the aim point.
// Z/X/C = Power / Contact / Regular. Space or LMB swings.
//
// World: +Y up · plate near z=0 · pitcher at −Z throws toward +Z.
//
// Controls:
//   Mouse              aim PCI (plate X / height)
//   Q / E / wheel      bat roll
//   Z / X / C          Power / Contact / Regular
//   Space / LMB        swing
//   R                  reset
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
#include "rendering/BaseballVisual3D.h"
#include "rendering/Camera3D.h"
#include "rendering/FrameBuffer.h"
#include "rendering/GlRenderer.h"
#include "rendering/Mesh3D.h"
#include "rendering/Rasterizer3D.h"

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kDeg = 180.0f / kPi;
constexpr float kFixed = 1.0f / 180.0f;
constexpr float kBallR = 0.037f;
constexpr float kPlateZ = 0.0f;
constexpr float kMoundZ = -16.0f;
constexpr float kZoneHalfW = 0.45f;
constexpr float kZoneBot = 0.46f;
constexpr float kZoneTop = 1.15f;

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
        {"POWER", 1.25f, 0.20f, 0.68f, 0.05f, 1.12f, 0.05f, sf::Color(255, 130, 90)},
        {"CONTACT", 0.70f, 0.30f, 1.55f, -0.01f, 0.92f, 0.095f, sf::Color(110, 210, 255)},
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
    float baseCor = 0.50f;
    float maxCor = 0.56f;
    float minCor = 0.34f;
};

struct BatPose {
    Vector3 hands{0.35f, 0.95f, 0.15f}; // RHB hands near plate
    Vector3 axis{-0.55f, 0.05f, 0.83f};  // knob → tip
    float roll = 0.0f;
    float swingT = -1.0f;
    float omega = 0.0f;
    Vector3 swingAxis{0, 1, 0};
    SwingType type = SwingType::Regular;
    Vector3 pci{0.0f, 0.85f, kPlateZ + 0.12f};
    bool locked = false; // freeze aim while swinging

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
        to = Vector3(-0.5f, 0.0f, 0.5f);
    }
    bat.axis = safeNorm(to);
    Vector3 up(0, 1, 0);
    Vector3 side = safeNorm(up.cross(bat.axis), Vector3(1, 0, 0));
    bat.swingAxis = safeNorm(bat.axis.cross(side), up);
}

Vector3 batPointVelocity(const BatPose& bat, float s) {
    Vector3 r = bat.axis * s;
    Vector3 w = bat.swingAxis * bat.omega;
    return w.cross(r);
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
    // Positive omega: rotate through the pitch (RHB feel)
    float peak = 15.0f * prof.power;
    bat.omega = peak * speedCurve(bat.swingT);

    float dTheta = 0.0f;
    if (dt > 1e-6f) {
        // Use average omega over step for rotation
        float prevW = peak * speedCurve(prev);
        dTheta = 0.5f * (prevW + bat.omega) * dt;
    }
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

// ── Hit ─────────────────────────────────────────────────────────────────

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

    float rBat = batRadius(cfg, s) * 1.25f;
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
    float mu = 0.15f * sweet;
    float jt = clampf(-vt * mBall * 0.3f, -std::abs(j) * mu, std::abs(j) * mu);
    ball.velocity = ball.velocity + tan * (jt / mBall);
    ball.position = ball.position + n * (minD - dist + 0.005f);

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

// ── Mesh helpers ────────────────────────────────────────────────────────

Mesh3D makeBatMesh(const BatConfig& cfg) {
    Mesh3D mesh;
    const int slices = 12;
    const int along = 18;
    // Bat along +Y from knob (0) to tip (length)
    for (int i = 0; i <= along; i++) {
        float t = static_cast<float>(i) / along;
        float s = t * cfg.length;
        float r = batRadius(cfg, s);
        for (int j = 0; j < slices; j++) {
            float a = (static_cast<float>(j) / slices) * kPi * 2.0f;
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

// Row-major matrix: columns of basis are (m0,m4,m8), (m1,m5,m9), (m2,m6,m10)
// transform: x' = m0*x + m1*y + m2*z + m3
// Local +Y → bat.axis, local +X → right, local +Z → forward
Matrix4 batModelMatrix(const BatPose& bat) {
    Vector3 y = bat.axis;
    Vector3 x = safeNorm(Vector3(0, 1, 0).cross(y), Vector3(1, 0, 0));
    if (x.magnitude() < 0.2f) {
        x = safeNorm(Vector3(1, 0, 0).cross(y));
    }
    Vector3 z = safeNorm(x.cross(y));
    x = safeNorm(y.cross(z));

    // Roll around bat axis
    float c = std::cos(bat.roll);
    float s = std::sin(bat.roll);
    Vector3 xr = x * c + z * s;
    Vector3 zr = z * c - x * s;

    Matrix4 r = Matrix4::identity();
    // Rows: map local (x,y,z) with basis xr, y, zr as columns in row-major
    // local X → xr: column 0
    r.values[0] = xr.x;
    r.values[4] = xr.y;
    r.values[8] = xr.z;
    // local Y → axis
    r.values[1] = y.x;
    r.values[5] = y.y;
    r.values[9] = y.z;
    // local Z → zr
    r.values[2] = zr.x;
    r.values[6] = zr.y;
    r.values[10] = zr.z;

    return Matrix4::translation(bat.hands) * r;
}

// Map mouse to plate-plane PCI (world X/Y at contact Z).
Vector3 mouseToPci(
    const Camera3D& cam,
    float mouseX,
    float mouseY,
    float screenW,
    float screenH
) {
    // NDC
    float nx = (mouseX / screenW) * 2.0f - 1.0f;
    float ny = 1.0f - (mouseY / screenH) * 2.0f;

    // Camera basis
    float cy = std::cos(cam.rotation.y);
    float sy = std::sin(cam.rotation.y);
    float cx = std::cos(cam.rotation.x);
    float sx = std::sin(cam.rotation.x);
    // Forward in this engine: based on rotation
    Vector3 forward(sy * cx, -sx, cy * cx);
    forward = safeNorm(forward);
    Vector3 right = safeNorm(Vector3(0, 1, 0).cross(forward), Vector3(1, 0, 0));
    Vector3 up = safeNorm(forward.cross(right));

    float aspect = screenW / std::max(screenH, 1.0f);
    // Approximate FOV from camera.fieldOfView (this project uses large values like 700)
    float fovY = 0.9f; // ~50 deg
    float tanHalf = std::tan(fovY * 0.5f);
    Vector3 dir = safeNorm(forward + right * (nx * tanHalf * aspect) + up * (ny * tanHalf));

    // Ray-plane z = kPlateZ + 0.12
    float planeZ = kPlateZ + 0.12f;
    if (std::abs(dir.z) < 1e-5f) {
        return Vector3(0.0f, 0.85f, planeZ);
    }
    float t = (planeZ - cam.position.z) / dir.z;
    if (t < 0.05f) {
        t = 2.0f;
    }
    Vector3 hit = cam.position + dir * t;
    hit.x = clampf(hit.x, -kZoneHalfW * 1.15f, kZoneHalfW * 1.15f);
    hit.y = clampf(hit.y, kZoneBot - 0.05f, kZoneTop + 0.08f);
    hit.z = planeZ;
    return hit;
}

void resetPitch(Body3D& ball, PhysicsWorld3D& world, float speedMph) {
    world = PhysicsWorld3D();
    world.gravity = Vector3(0, -9.8f, 0);
    world.setBounds(Vector3(-6, -1, kMoundZ - 2), Vector3(6, 5, 12));
    world.airResistanceEnabled = false;

    ball = Body3D(Vector3(0.05f, 1.55f, kMoundZ + 1.0f), 0.145f);
    ball.setRadius(kBallR);
    ball.restitution = 0.4f;
    float speed = speedMph * 0.44704f; // m/s
    // Toward plate center-ish
    Vector3 target(0.0f, 0.85f, kPlateZ + 0.1f);
    Vector3 dir = safeNorm(target - ball.position);
    ball.velocity = dir * speed;
    world.addBody(&ball);
}

void drawThickLine(
    sf::RenderWindow& window,
    const Camera3D& cam,
    const Vector3& a,
    const Vector3& b,
    float thickness,
    sf::Color color
) {
    ProjectedPoint3D pa = cam.projectPoint(a, window.getSize().x, window.getSize().y);
    ProjectedPoint3D pb = cam.projectPoint(b, window.getSize().x, window.getSize().y);
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

void drawZoneOverlay(sf::RenderWindow& window, const Camera3D& cam, sf::Color col) {
    Vector3 c00(-kZoneHalfW, kZoneBot, kPlateZ + 0.05f);
    Vector3 c10(kZoneHalfW, kZoneBot, kPlateZ + 0.05f);
    Vector3 c11(kZoneHalfW, kZoneTop, kPlateZ + 0.05f);
    Vector3 c01(-kZoneHalfW, kZoneTop, kPlateZ + 0.05f);
    drawThickLine(window, cam, c00, c10, 2.2f, col);
    drawThickLine(window, cam, c10, c11, 2.2f, col);
    drawThickLine(window, cam, c11, c01, 2.2f, col);
    drawThickLine(window, cam, c01, c00, 2.2f, col);
}

void drawPciOverlay(
    sf::RenderWindow& window,
    const Camera3D& cam,
    const Vector3& pci,
    float radiusM,
    sf::Color col
) {
    // Project a small ring around PCI in the plate plane
    const int n = 20;
    Vector3 prev;
    for (int i = 0; i <= n; i++) {
        float a = (static_cast<float>(i) / n) * kPi * 2.0f;
        Vector3 p = pci + Vector3(std::cos(a) * radiusM, std::sin(a) * radiusM, 0.0f);
        if (i > 0) {
            drawThickLine(window, cam, prev, p, 2.0f, col);
        }
        prev = p;
    }
    // Cross
    drawThickLine(
        window, cam, pci + Vector3(-radiusM * 0.5f, 0, 0), pci + Vector3(radiusM * 0.5f, 0, 0), 1.5f, col
    );
    drawThickLine(
        window, cam, pci + Vector3(0, -radiusM * 0.5f, 0), pci + Vector3(0, radiusM * 0.5f, 0), 1.5f, col
    );
}

// Screen-space bat silhouette (The Show–style) from projected bat edges
void drawBatSilhouette2D(
    sf::RenderWindow& window,
    const Camera3D& cam,
    const BatPose& bat,
    const BatConfig& cfg,
    sf::Color col
) {
    const int segs = 12;
    std::vector<sf::Vector2f> left;
    std::vector<sf::Vector2f> right;
    float sw = static_cast<float>(window.getSize().x);
    float sh = static_cast<float>(window.getSize().y);

    Vector3 side = safeNorm(bat.axis.cross(Vector3(0, 1, 0)), Vector3(1, 0, 0));
    // Prefer side perpendicular to view for readable silhouette
    Vector3 toCam = safeNorm(cam.position - bat.hands);
    side = safeNorm(bat.axis.cross(toCam), side);

    for (int i = 0; i <= segs; i++) {
        float t = static_cast<float>(i) / segs;
        float s = t * cfg.length;
        float r = batRadius(cfg, s) * (t > 0.4f ? 1.35f : 1.1f);
        Vector3 c = batPoint(bat, s);
        Vector3 pl = c + side * r;
        Vector3 pr = c - side * r;
        ProjectedPoint3D al = cam.projectPoint(pl, sw, sh);
        ProjectedPoint3D ar = cam.projectPoint(pr, sw, sh);
        if (al.visible) {
            left.push_back(sf::Vector2f(al.position.x, al.position.y));
        }
        if (ar.visible) {
            right.push_back(sf::Vector2f(ar.position.x, ar.position.y));
        }
    }
    auto drawPoly = [&](const std::vector<sf::Vector2f>& pts) {
        for (size_t i = 1; i < pts.size(); i++) {
            sf::Vertex line[] = {
                sf::Vertex{pts[i - 1], col},
                sf::Vertex{pts[i], col}
            };
            window.draw(line, 2, sf::PrimitiveType::Lines);
        }
    };
    drawPoly(left);
    drawPoly(right);
    if (!left.empty() && !right.empty()) {
        sf::Vertex tip[] = {
            sf::Vertex{left.back(), col},
            sf::Vertex{right.back(), col}
        };
        sf::Vertex kn[] = {
            sf::Vertex{left.front(), col},
            sf::Vertex{right.front(), col}
        };
        window.draw(tip, 2, sf::PrimitiveType::Lines);
        window.draw(kn, 2, sf::PrimitiveType::Lines);
    }
}

} // namespace

int main() {
    sf::ContextSettings glSettings;
    glSettings.depthBits = 24;
    glSettings.antiAliasingLevel = 4;

    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "Bat Physics 3D | Z power  X contact  C regular  |  Mouse aim",
        sf::Style::Default,
        sf::State::Windowed,
        glSettings
    );
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    DemoFpsCounter fps("Bat Physics 3D | Z/X/C | Mouse PCI");
    sf::Font font;
    bool fontOk = loadUiFont(font);

    Camera3D camera;
    // Catcher / umpire-ish view looking toward pitcher (−Z)
    lookAt(camera, Vector3(0.15f, 1.55f, 3.8f), Vector3(0.0f, 0.95f, kPlateZ - 2.0f));
    camera.fieldOfView = 780.0f;
    camera.nearPlane = 0.08f;

    BatConfig batCfg;
    BatPose bat;
    orientBat(bat, Vector3(0.0f, 0.85f, kPlateZ + 0.12f));

    Mesh3D batMesh = makeBatMesh(batCfg);
    Mesh3D ballMesh = BaseballVisual3D::makeMesh(24, 36);
    Mesh3D plateMesh = Mesh3D::box(0.43f, 0.03f, 0.43f);

    GlRenderer gl;
    bool useGL = gl.initialize(window);
    GlMesh glBat;
    GlMesh glBall;
    GlMesh glPlate;
    if (useGL) {
        glBat.upload(batMesh);
        glBall.upload(ballMesh);
        glPlate.upload(plateMesh);
    }

    FrameBuffer fb(window.getSize().x, window.getSize().y);
    RasterMeshRenderCache cache;

    PhysicsWorld3D world;
    Body3D ball;
    float pitchMph = 90.0f;
    resetPitch(ball, world, pitchMph);

    bool hasHit = false;
    HitInfo lastHit;
    std::string status = "Mouse aim · Z Power · X Contact · C Regular · Space swing";
    sf::Color statusCol(220, 230, 220);
    std::vector<Vector3> trail;

    float batRoll = 0.0f;
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
                fb.resize(static_cast<int>(r->size.x), static_cast<int>(r->size.y));
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
                    bat.swingT = -1.0f;
                    bat.locked = false;
                    bat.omega = 0.0f;
                    hasHit = false;
                    lastHit = {};
                    trail.clear();
                    resetPitch(ball, world, pitchMph);
                    orientBat(bat, bat.pci);
                    status = "Reset";
                    statusCol = sf::Color(180, 210, 200);
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

        // Aim
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

        // Physics
        float acc = 0.0f;
        acc += dt;
        while (acc >= kFixed) {
            world.step(kFixed);
            HitInfo hit = tryHit(ball, bat, batCfg, prof, hasHit);
            if (hit.hit) {
                lastHit = hit;
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(0)
                    << prof.name << " HIT  exit " << lastHit.exitMph << " mph  LA "
                    << lastHit.launchDeg << "°  bat " << lastHit.batMph << " mph  sweet "
                    << (lastHit.sweet * 100.0f) << "%";
                status = oss.str();
                statusCol = lastHit.sweet > 0.85f ? sf::Color(120, 255, 160)
                    : (lastHit.sweet > 0.5f ? sf::Color(255, 230, 120) : sf::Color(255, 150, 100));
            }
            acc -= kFixed;
        }

        // Re-pitch when ball is gone
        if (ball.position.z > 8.0f || ball.position.y < -0.5f || ball.position.z < kMoundZ - 1.0f) {
            if (!bat.swinging()) {
                hasHit = false;
                trail.clear();
                resetPitch(ball, world, pitchMph);
            }
        }

        if (trail.empty() || (ball.position - trail.back()).magnitude() > 0.12f) {
            trail.push_back(ball.position);
            if (trail.size() > 100) {
                trail.erase(trail.begin());
            }
        }

        Matrix4 batXform = batModelMatrix(bat);
        Matrix4 ballXform =
            Matrix4::translation(ball.position) *
            Matrix4::scale(Vector3(kBallR, kBallR, kBallR));
        Matrix4 plateXform = Matrix4::translation(Vector3(0.0f, 0.02f, kPlateZ));

        // ── 3D ──────────────────────────────────────────────────────────
        if (useGL) {
            gl.beginFrame(window, camera, sf::Color(18, 40, 32));
            gl.drawGround(8.0f, kMoundZ - 1.0f, 10.0f, sf::Color(34, 70, 48));
            gl.drawMesh(glPlate, plateXform);
            gl.drawMesh(glBat, batXform);
            gl.drawMesh(glBall, ballXform);
            gl.endFrame(window);
        } else {
            fb.clear(sf::Color(18, 40, 32));
            fb.clearDepth(std::numeric_limits<float>::infinity());
            rasterizeMeshTriangles(fb, camera, plateMesh, plateXform, sf::Color(230, 225, 210), cache);
            rasterizeMeshTriangles(fb, camera, batMesh, batXform, sf::Color(170, 115, 55), cache);
            rasterizeMeshTriangles(fb, camera, ballMesh, ballXform, sf::Color(245, 245, 240), cache);
            window.clear();
            fb.present(window);
        }

        // Trail + zone + PCI + silhouette overlay
        for (size_t i = 1; i < trail.size(); i++) {
            float a = static_cast<float>(i) / static_cast<float>(trail.size());
            sf::Color c(255, 255, 230, static_cast<std::uint8_t>(40 + a * 160));
            drawThickLine(window, camera, trail[i - 1], trail[i], 2.0f, c);
        }
        drawZoneOverlay(window, camera, sf::Color(200, 210, 220, 180));
        drawPciOverlay(window, camera, bat.pci, prof.pciR, prof.color);
        drawBatSilhouette2D(window, camera, bat, batCfg, prof.color);

        // Contact flash
        if (lastHit.hit && hasHit) {
            ProjectedPoint3D p = camera.projectPoint(
                lastHit.point,
                static_cast<float>(window.getSize().x),
                static_cast<float>(window.getSize().y)
            );
            if (p.visible) {
                sf::CircleShape flash(8.0f);
                flash.setOrigin({8, 8});
                flash.setPosition({p.position.x, p.position.y});
                flash.setFillColor(sf::Color(255, 255, 120, 160));
                window.draw(flash);
            }
        }

        if (fontOk) {
            drawText(window, font, "Bat Physics 3D", 20, {22, 14}, sf::Color(240, 245, 240));
            drawText(window, font, status, 15, {22, 42}, statusCol);

            std::ostringstream modes;
            modes << "Z Power   X Contact   C Regular    [" << prof.name << "  pwr "
                  << std::fixed << std::setprecision(2) << prof.power << "]";
            drawText(window, font, modes.str(), 14, {22, 68}, prof.color);

            std::ostringstream hud;
            hud << std::fixed << std::setprecision(0)
                << "Pitch " << pitchMph
                << " mph   Mouse aim PCI   Q/E roll   Space/LMB swing   R reset   [ ] speed";
            drawText(window, font, hud.str(), 12, {22, 92}, sf::Color(160, 190, 180));

            if (lastHit.hit) {
                std::ostringstream hit;
                hit << std::fixed << std::setprecision(0)
                    << "Last: exit " << lastHit.exitMph << " mph @ " << lastHit.launchDeg
                    << "° LA · bat " << lastHit.batMph << " mph · sweet "
                    << (lastHit.sweet * 100.0f) << "% · along bat "
                    << std::setprecision(2) << lastHit.s << " m";
                drawText(window, font, hit.str(), 13, {22, 116}, sf::Color(255, 220, 140));
            }
        }

        fps.frame(window);
        window.display();
    }

    return 0;
}
