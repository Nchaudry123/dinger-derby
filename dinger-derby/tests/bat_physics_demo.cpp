// Bat Physics Demo — plate-view aim + three swing types (The Show–style PCI).
//
// Mouse aims a 2D bat silhouette over the plate. Z/X/C select swing type
// (power / contact / regular). Space or LMB starts the swing; contact uses
// sweet-spot COR + bat point velocity for exit speed.
//
// Controls:
//   Mouse              aim bat / PCI over the plate
//   Z                  Power swing
//   X                  Contact swing
//   C                  Regular swing
//   Space / LMB        swing
//   R                  reset
//   [ ]                pitch speed −/+
//   Esc                quit

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "DemoFpsCounter.h"
#include "math/Vector2.h"
#include "physics/Body2D.h"
#include "physics/PhysicsWorld2D.h"

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kDeg = 180.0f / kPi;

bool loadUiFont(sf::Font& font) {
    const std::vector<std::filesystem::path> candidates = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/SFNS.ttf"
    };
    for (const auto& c : candidates) {
        if (font.openFromFile(c)) {
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
    text.setFillColor(color);
    text.setPosition(position);
    window.draw(text);
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
float deg2rad(float d) {
    return d * kPi / 180.0f;
}

// ── Swing types ─────────────────────────────────────────────────────────

enum class SwingType {
    Power = 0,    // Z
    Contact = 1,  // X
    Regular = 2   // C
};

struct SwingProfile {
    const char* name;
    char hotkey;
    float power;           // multiplies bat head speed
    float swingDuration;   // seconds (lower = quicker)
    float sweetWidthScale; // >1 more forgiving
    float corBonus;        // added to base COR at sweet spot
    float massScale;       // effective bat mass
    float pciRadiusPx;     // visual PCI size
    sf::Color accent;
};

const SwingProfile& profileFor(SwingType t) {
    static const SwingProfile kProfiles[] = {
        // Power: hard, small sweet spot, fast through zone
        {"POWER", 'Z', 1.22f, 0.22f, 0.72f, 0.04f, 1.10f, 14.0f, sf::Color(255, 120, 90)},
        // Contact: softer, big sweet spot, slower, more COR forgiveness on mishit
        {"CONTACT", 'X', 0.72f, 0.32f, 1.45f, -0.02f, 0.95f, 22.0f, sf::Color(120, 220, 255)},
        // Regular: balanced
        {"REGULAR", 'C', 1.00f, 0.26f, 1.00f, 0.00f, 1.00f, 17.0f, sf::Color(255, 230, 80)},
    };
    return kProfiles[static_cast<int>(t)];
}

// ── Bat geometry (plate-view silhouette) ────────────────────────────────

struct BatConfig {
    float lengthM = 0.86f;
    float barrelRadiusM = 0.033f;
    float handleRadiusM = 0.014f;
    float massKg = 0.90f;
    float sweetSpotFromKnobM = 0.56f;
    float sweetSpotWidthM = 0.11f;
    float baseCor = 0.50f;
    float maxCor = 0.55f;
    float minCor = 0.36f;
    float pixelsPerMeter = 280.0f;
};

struct AimState {
    // PCI center in screen space (mouse, clamped to zone)
    Vector2 pci;
    // Bat angle in screen rad: 0 = pointing right (+X), CCW positive with y-down
    // RHB default ~ 150–200° style diagonal across plate
    float batAngle = deg2rad(-35.0f);
    // Hands / knob position derived from PCI + angle so PCI sits on barrel
    Vector2 knob;
    float swingT = -1.0f; // <0 ready; 0..1 swinging
    float omega = 0.0f;   // rad/s during swing
    float displayAngle = 0.0f; // may lag during swing animation
    SwingType type = SwingType::Regular;

    bool swinging() const {
        return swingT >= 0.0f && swingT < 1.0f;
    }
};

// Unit direction along bat from knob → tip
Vector2 batAxis(float angle) {
    return Vector2(std::cos(angle), std::sin(angle));
}

Vector2 batPointFromKnob(const Vector2& knob, float angle, float sMeters, float ppm) {
    return knob + batAxis(angle) * (sMeters * ppm);
}

// Place knob so the sweet-spot / PCI lies under the mouse along the bat axis.
void updateKnobFromPci(AimState& aim, const BatConfig& cfg) {
    float s = cfg.sweetSpotFromKnobM;
    Vector2 axis = batAxis(aim.batAngle);
    aim.knob = aim.pci - axis * (s * cfg.pixelsPerMeter);
}

float batRadiusM(const BatConfig& cfg, float sMeters) {
    float t = clampf(sMeters / cfg.lengthM, 0.0f, 1.0f);
    if (t < 0.42f) {
        return lerp(cfg.handleRadiusM, cfg.barrelRadiusM * 0.75f, t / 0.42f);
    }
    return lerp(cfg.barrelRadiusM * 0.75f, cfg.barrelRadiusM, (t - 0.42f) / 0.58f);
}

float sweetFactor(const BatConfig& cfg, float sMeters, float widthScale) {
    float width = cfg.sweetSpotWidthM * widthScale;
    float d = std::abs(sMeters - cfg.sweetSpotFromKnobM);
    float half = width * 0.5f;
    if (d <= half) {
        return 1.0f;
    }
    float extra = (d - half) / std::max(cfg.lengthM * 0.35f, 0.01f);
    return clampf(1.0f - extra * 0.9f, 0.12f, 1.0f);
}

// Closest point on bat segment to world point
void closestOnBat(
    const Vector2& knob,
    float angle,
    const BatConfig& cfg,
    const Vector2& point,
    float& outS,
    Vector2& outPt
) {
    Vector2 tip = batPointFromKnob(knob, angle, cfg.lengthM, cfg.pixelsPerMeter);
    Vector2 ab = tip - knob;
    float ab2 = ab.dot(ab);
    float t = 0.0f;
    if (ab2 > 1e-8f) {
        t = clampf((point - knob).dot(ab) / ab2, 0.0f, 1.0f);
    }
    outPt = knob + ab * t;
    outS = t * cfg.lengthM;
}

// Bat point velocity during swing: rotate about knob with omega
Vector2 batPointVelocity(const Vector2& knob, float angle, float omega, float sMeters, float ppm) {
    // Screen y-down: d(axis)/dθ = (-sin θ, cos θ)
    float c = std::cos(angle);
    float s = std::sin(angle);
    Vector2 dAxis(-s, c);
    return dAxis * (sMeters * ppm * omega);
}

// ── Swing animation ─────────────────────────────────────────────────────

void startSwing(AimState& aim) {
    if (aim.swinging()) {
        return;
    }
    aim.swingT = 0.0f;
    aim.displayAngle = aim.batAngle;
    aim.omega = 0.0f;
}

float swingAngleOffset(float t01) {
    float u = smooth01(t01);
    // From +18° before contact to −22° after (RHB sweep through the ball).
    return deg2rad(lerp(18.0f, -22.0f, u));
}

void updateSwing(AimState& aim, const SwingProfile& prof, float dt) {
    if (aim.swingT < 0.0f) {
        aim.displayAngle = aim.batAngle;
        aim.omega = 0.0f;
        return;
    }

    float dur = prof.swingDuration;
    float prev = aim.swingT;
    aim.swingT = std::min(1.0f, aim.swingT + dt / dur);

    float a0 = aim.batAngle + swingAngleOffset(prev);
    float a1 = aim.batAngle + swingAngleOffset(aim.swingT);
    aim.displayAngle = a1;
    if (dt > 1e-6f) {
        aim.omega = (a1 - a0) / dt * prof.power;
    }
    if (aim.swingT >= 1.0f) {
        aim.omega = 0.0f;
    }
}

// ── Contact ─────────────────────────────────────────────────────────────

struct ContactResult {
    bool hit = false;
    float exitMph = 0.0f;
    float launchDeg = 0.0f;
    float batMph = 0.0f;
    float sweet = 0.0f;
    float sMeters = 0.0f;
    Vector2 point;
};

ContactResult tryContact(
    Body2D& ball,
    const AimState& aim,
    const BatConfig& cfg,
    const SwingProfile& prof,
    bool& hasHit
) {
    ContactResult r;
    if (hasHit || !aim.swinging()) {
        return r;
    }

    float s = 0.0f;
    Vector2 closest;
    closestOnBat(aim.knob, aim.displayAngle, cfg, ball.position, s, closest);

    float rBat = batRadiusM(cfg, s) * cfg.pixelsPerMeter * 1.15f; // slight forgiveness
    Vector2 delta = ball.position - closest;
    float dist = delta.magnitude();
    float minDist = ball.radius + rBat;
    if (dist > minDist || dist < 1e-5f) {
        return r;
    }

    Vector2 n = delta * (1.0f / dist);
    Vector2 vBat = batPointVelocity(aim.knob, aim.displayAngle, aim.omega, s, cfg.pixelsPerMeter);
    Vector2 vRel = ball.velocity - vBat;
    float approach = vRel.dot(n);
    if (approach >= 0.0f) {
        return r;
    }

    float sweet = sweetFactor(cfg, s, prof.sweetWidthScale);
    float cor = lerp(cfg.minCor, cfg.maxCor + prof.corBonus, sweet);
    cor = clampf(cor, 0.30f, 0.58f);

    float tip = clampf(s / cfg.lengthM, 0.0f, 1.0f);
    float mEff = cfg.massKg * prof.massScale * lerp(1.3f, 0.55f, tip) * lerp(0.75f, 1.15f, sweet);
    float mBall = std::max(ball.mass, 0.05f);
    float invM = 1.0f / mBall + 1.0f / mEff;
    float j = -(1.0f + cor) * approach / invM;
    j *= lerp(0.90f, 1.12f, sweet) * lerp(0.95f, 1.08f, (prof.power - 0.7f) / 0.55f);

    ball.velocity += n * (j / mBall);

    Vector2 tangent(-n.y, n.x);
    float vT = vRel.dot(tangent);
    float mu = 0.16f * sweet;
    float jt = clampf(-vT * mBall * 0.32f, -std::abs(j) * mu, std::abs(j) * mu);
    ball.velocity += tangent * (jt / mBall);

    float pen = minDist - dist;
    ball.position += n * (pen + 0.5f);

    hasHit = true;
    r.hit = true;
    r.sMeters = s;
    r.sweet = sweet;
    r.point = closest;
    float ppm = cfg.pixelsPerMeter;
    r.batMph = (vBat.magnitude() / ppm) * 2.236936f;
    r.exitMph = (ball.velocity.magnitude() / ppm) * 2.236936f;
    // Launch: 0 = toward top of screen (pitcher / OF in this layout uses −Y as “out”)
    // Report angle: 0 horizontal to right, + up field (negative Y)
    r.launchDeg = std::atan2(-ball.velocity.y, ball.velocity.x) * kDeg;
    return r;
}

// ── Drawing helpers ─────────────────────────────────────────────────────

void drawLine(
    sf::RenderWindow& window,
    sf::Vector2f a,
    sf::Vector2f b,
    sf::Color color,
    float /*thickness*/
) {
    sf::Vertex line[] = {
        sf::Vertex{a, color},
        sf::Vertex{b, color}
    };
    window.draw(line, 2, sf::PrimitiveType::Lines);
}

// Rounded bat silhouette (The Show style outline)
void drawBatSilhouette(
    sf::RenderWindow& window,
    const Vector2& knob,
    float angle,
    const BatConfig& cfg,
    sf::Color outline,
    float outlineScale = 1.0f
) {
    const int segs = 18;
    std::vector<sf::Vector2f> left;
    std::vector<sf::Vector2f> right;
    left.reserve(segs + 1);
    right.reserve(segs + 1);

    Vector2 axis = batAxis(angle);
    Vector2 perp(-axis.y, axis.x);
    float ppm = cfg.pixelsPerMeter;

    for (int i = 0; i <= segs; i++) {
        float t = static_cast<float>(i) / segs;
        float s = t * cfg.lengthM;
        float r = batRadiusM(cfg, s) * ppm * outlineScale;
        // Fatter visual barrel for readable silhouette
        if (t > 0.4f) {
            r *= 1.25f;
        }
        Vector2 c = knob + axis * (s * ppm);
        left.push_back(sf::Vector2f(c.x + perp.x * r, c.y + perp.y * r));
        right.push_back(sf::Vector2f(c.x - perp.x * r, c.y - perp.y * r));
    }

    // Outline path: left tip→knob, arc at tip, right tip→knob
    for (int i = 0; i < segs; i++) {
        drawLine(window, left[i], left[i + 1], outline, 2.0f);
        drawLine(window, right[i], right[i + 1], outline, 2.0f);
    }
    // Tip cap
    drawLine(window, left.back(), right.back(), outline, 2.0f);
    // Knob cap
    drawLine(window, left.front(), right.front(), outline, 2.0f);

    // Soft fill (semi-transparent quads)
    for (int i = 0; i < segs; i++) {
        sf::Color fill = outline;
        fill.a = 28;
        sf::Vertex quad[] = {
            sf::Vertex{left[i], fill},
            sf::Vertex{left[i + 1], fill},
            sf::Vertex{right[i + 1], fill},
            sf::Vertex{right[i], fill}
        };
        window.draw(quad, 4, sf::PrimitiveType::TriangleFan);
    }
}

void drawPci(sf::RenderWindow& window, Vector2 center, float radius, sf::Color color) {
    sf::CircleShape ring(radius);
    ring.setOrigin(sf::Vector2f(radius, radius));
    ring.setPosition(sf::Vector2f(center.x, center.y));
    ring.setFillColor(sf::Color(color.r, color.g, color.b, 200));
    ring.setOutlineThickness(2.5f);
    ring.setOutlineColor(sf::Color(color.r, color.g, color.b, 255));
    window.draw(ring);

    // Inner core
    float inner = radius * 0.35f;
    sf::CircleShape core(inner);
    core.setOrigin(sf::Vector2f(inner, inner));
    core.setPosition(sf::Vector2f(center.x, center.y));
    core.setFillColor(sf::Color(255, 255, 255, 220));
    window.draw(core);
}

struct ZoneRect {
    float left = 0;
    float top = 0;
    float width = 0;
    float height = 0;
    float right() const { return left + width; }
    float bottom() const { return top + height; }
    sf::Vector2f center() const {
        return sf::Vector2f(left + width * 0.5f, top + height * 0.5f);
    }
};

Vector2 clampToZone(Vector2 p, const ZoneRect& z, float margin) {
    return Vector2(
        clampf(p.x, z.left + margin, z.right() - margin),
        clampf(p.y, z.top + margin, z.bottom() - margin)
    );
}

// ── Pitch ───────────────────────────────────────────────────────────────

struct PitchState {
    float speedMph = 90.0f;
};

void resetPitch(Body2D& ball, const PitchState& pitch, const ZoneRect& zone, float ppm) {
    // Ball starts above zone (pitcher side = top of screen) and flies down toward plate
    float cx = zone.left + zone.width * 0.5f;
    ball.position = Vector2(cx + 10.0f, zone.top - 80.0f);
    float speedMs = pitch.speedMph * 0.44704f;
    float speedPx = speedMs * ppm * 0.55f; // scale for readable timing
    // Toward plate center with slight side noise-free aim
    Vector2 target(cx, zone.top + zone.height * 0.62f);
    Vector2 dir = target - ball.position;
    float m = dir.magnitude();
    if (m > 1e-4f) {
        dir = dir * (1.0f / m);
    }
    ball.velocity = dir * speedPx;
    ball.acceleration = Vector2();
}

} // namespace

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "Bat Physics | Z power  X contact  C regular  |  Mouse aim  Space swing"
    );
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);
    window.setMouseCursorVisible(false);

    DemoFpsCounter fpsCounter("Bat Physics | Z/X/C swing types | Mouse aim");
    sf::Font font;
    bool fontOk = loadUiFont(font);

    // Plate aim zone (center of window) — The Show–style frame
    ZoneRect zone;
    zone.width = 420.0f;
    zone.height = 480.0f;
    zone.left = (1280.0f - zone.width) * 0.5f;
    zone.top = (720.0f - zone.height) * 0.42f;

    BatConfig batCfg;
    AimState aim;
    aim.pci = Vector2(zone.center().x, zone.center().y + 40.0f);
    aim.batAngle = deg2rad(-38.0f);
    aim.type = SwingType::Regular;
    updateKnobFromPci(aim, batCfg);

    // Freeze aim at swing start so mouse motion mid-swing doesn't teleport bat
    AimState frozenAim = aim;
    bool aimFrozen = false;

    PhysicsWorld2D world;
    world.gravity = Vector2(0.0f, 80.0f);
    world.setBounds(1280.0f, 900.0f);

    Body2D ball(Vector2(100, 100), 0.145f);
    ball.setRadius(0.037f * batCfg.pixelsPerMeter * 0.85f);
    ball.restitution = 0.4f;
    world.addBody(&ball);

    PitchState pitch;
    resetPitch(ball, pitch, zone, batCfg.pixelsPerMeter);

    bool hasHit = false;
    ContactResult lastHit;
    std::string status = "Aim with mouse · Z Power · X Contact · C Regular · Space swing";
    sf::Color statusColor(220, 230, 220);
    std::vector<Vector2> trail;

    // Optional: rotate bat with mouse wheel or Q/E
    float batAngleAdjust = aim.batAngle;

    sf::Clock clock;
    while (window.isOpen()) {
        float dt = std::min(clock.restart().asSeconds(), 0.05f);
        const SwingProfile& prof = profileFor(aim.type);

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect(
                    sf::Vector2f(0, 0),
                    sf::Vector2f(static_cast<float>(resized->size.x), static_cast<float>(resized->size.y))
                )));
                float w = static_cast<float>(resized->size.x);
                float h = static_cast<float>(resized->size.y);
                zone.left = (w - zone.width) * 0.5f;
                zone.top = (h - zone.height) * 0.42f;
            }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                using K = sf::Keyboard::Key;
                if (key->code == K::Escape) {
                    window.close();
                } else if (key->code == K::Z) {
                    aim.type = SwingType::Power;
                    status = "POWER swing selected";
                    statusColor = profileFor(SwingType::Power).accent;
                } else if (key->code == K::X) {
                    aim.type = SwingType::Contact;
                    status = "CONTACT swing selected";
                    statusColor = profileFor(SwingType::Contact).accent;
                } else if (key->code == K::C) {
                    aim.type = SwingType::Regular;
                    status = "REGULAR swing selected";
                    statusColor = profileFor(SwingType::Regular).accent;
                } else if (key->code == K::Space) {
                    if (!aim.swinging()) {
                        frozenAim = aim;
                        aimFrozen = true;
                        startSwing(aim);
                        hasHit = false;
                        status = std::string(prof.name) + " swing!";
                        statusColor = prof.accent;
                    }
                } else if (key->code == K::R) {
                    aim.swingT = -1.0f;
                    aim.omega = 0.0f;
                    aimFrozen = false;
                    hasHit = false;
                    lastHit = {};
                    trail.clear();
                    resetPitch(ball, pitch, zone, batCfg.pixelsPerMeter);
                    status = "Reset";
                    statusColor = sf::Color(180, 210, 200);
                } else if (key->code == K::LBracket) {
                    pitch.speedMph = std::max(60.0f, pitch.speedMph - 2.0f);
                } else if (key->code == K::RBracket) {
                    pitch.speedMph = std::min(105.0f, pitch.speedMph + 2.0f);
                } else if (key->code == K::Q) {
                    batAngleAdjust -= deg2rad(4.0f);
                } else if (key->code == K::E) {
                    batAngleAdjust += deg2rad(4.0f);
                }
            }
            if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left && !aim.swinging()) {
                    frozenAim = aim;
                    aimFrozen = true;
                    startSwing(aim);
                    hasHit = false;
                    status = std::string(prof.name) + " swing!";
                    statusColor = prof.accent;
                }
            }
            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                batAngleAdjust += deg2rad(wheel->delta * 3.0f);
            }
        }

        // Mouse aim only when not swinging; freeze geometry at swing start.
        if (!aim.swinging()) {
            aimFrozen = false;
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            Vector2 mouse(static_cast<float>(mp.x), static_cast<float>(mp.y));
            aim.pci = clampToZone(mouse, zone, 28.0f);
            aim.batAngle = batAngleAdjust;
            updateKnobFromPci(aim, batCfg);
            aim.displayAngle = aim.batAngle;
        }

        updateSwing(aim, prof, dt);

        // Contact uses frozen aim pose + current swing angle/omega.
        AimState contactAim = aimFrozen ? frozenAim : aim;
        contactAim.swingT = aim.swingT;
        contactAim.omega = aim.omega;
        contactAim.displayAngle = aim.displayAngle;
        contactAim.type = aim.type;
        if (aim.swinging()) {
            // Recompute knob so PCI stays fixed while barrel rotates through.
            Vector2 axis = batAxis(contactAim.displayAngle);
            contactAim.knob =
                contactAim.pci - axis * (batCfg.sweetSpotFromKnobM * batCfg.pixelsPerMeter);
        }

        world.step(dt);
        ContactResult hit = tryContact(ball, contactAim, batCfg, prof, hasHit);
        if (hit.hit) {
            lastHit = hit;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0)
                << prof.name << " HIT  exit " << lastHit.exitMph << " mph  "
                << "LA " << lastHit.launchDeg << "°  bat " << lastHit.batMph
                << " mph  sweet " << (lastHit.sweet * 100.0f) << "%";
            status = oss.str();
            statusColor = lastHit.sweet > 0.85f ? sf::Color(120, 255, 160)
                : (lastHit.sweet > 0.5f ? sf::Color(255, 230, 120) : sf::Color(255, 150, 100));
        }

        // Auto re-pitch
        if (ball.position.y > zone.bottom() + 200.0f ||
            ball.position.x < -100.0f ||
            ball.position.x > 1400.0f ||
            (hasHit && ball.position.y < zone.top - 250.0f)) {
            if (aim.swingT < 0.0f || aim.swingT >= 1.0f) {
                aim.swingT = -1.0f;
                aimFrozen = false;
                hasHit = false;
                trail.clear();
                resetPitch(ball, pitch, zone, batCfg.pixelsPerMeter);
            }
        }

        if (trail.empty() || (ball.position - trail.back()).magnitude() > 10.0f) {
            trail.push_back(ball.position);
            if (trail.size() > 70) {
                trail.erase(trail.begin());
            }
        }

        // ── Draw ────────────────────────────────────────────────────────
        window.clear(sf::Color(28, 55, 42)); // grass

        // Dirt oval under zone
        {
            sf::CircleShape dirt(zone.width * 0.62f);
            dirt.setScale(sf::Vector2f(1.0f, 0.72f));
            dirt.setOrigin(sf::Vector2f(zone.width * 0.62f, zone.width * 0.62f));
            dirt.setPosition(sf::Vector2f(zone.center().x, zone.bottom() - 40.0f));
            dirt.setFillColor(sf::Color(150, 105, 70));
            window.draw(dirt);
        }

        // Pitcher marker (top of zone)
        {
            sf::CircleShape mound(28.0f);
            mound.setOrigin(sf::Vector2f(28.0f, 28.0f));
            mound.setPosition(sf::Vector2f(zone.center().x, zone.top - 50.0f));
            mound.setFillColor(sf::Color(160, 115, 80));
            window.draw(mound);
            sf::CircleShape pitcher(10.0f);
            pitcher.setOrigin(sf::Vector2f(10.0f, 10.0f));
            pitcher.setPosition(sf::Vector2f(zone.center().x, zone.top - 55.0f));
            pitcher.setFillColor(sf::Color(200, 200, 210));
            window.draw(pitcher);
        }

        // Zone frame (white-ish)
        {
            sf::RectangleShape frame(sf::Vector2f(zone.width, zone.height));
            frame.setPosition(sf::Vector2f(zone.left, zone.top));
            frame.setFillColor(sf::Color(0, 0, 0, 0));
            frame.setOutlineThickness(4.0f);
            frame.setOutlineColor(sf::Color(210, 215, 220, 200));
            window.draw(frame);
            // Inner dirt fill slightly
            sf::RectangleShape fill(sf::Vector2f(zone.width - 8.0f, zone.height - 8.0f));
            fill.setPosition(sf::Vector2f(zone.left + 4.0f, zone.top + 4.0f));
            fill.setFillColor(sf::Color(140, 95, 62, 50));
            window.draw(fill);
        }

        // Home plate
        {
            float px = zone.center().x;
            float py = zone.bottom() - 70.0f;
            sf::ConvexShape plate;
            plate.setPointCount(5);
            plate.setPoint(0, sf::Vector2f(px - 22.0f, py));
            plate.setPoint(1, sf::Vector2f(px + 22.0f, py));
            plate.setPoint(2, sf::Vector2f(px + 22.0f, py + 18.0f));
            plate.setPoint(3, sf::Vector2f(px, py + 36.0f));
            plate.setPoint(4, sf::Vector2f(px - 22.0f, py + 18.0f));
            plate.setFillColor(sf::Color(235, 230, 215));
            window.draw(plate);
        }

        // Ball trail
        for (size_t i = 1; i < trail.size(); i++) {
            float a = static_cast<float>(i) / static_cast<float>(trail.size());
            sf::Vertex line[] = {
                sf::Vertex{
                    sf::Vector2f(trail[i - 1].x, trail[i - 1].y),
                    sf::Color(255, 255, 240, static_cast<std::uint8_t>(30 + a * 150))
                },
                sf::Vertex{
                    sf::Vector2f(trail[i].x, trail[i].y),
                    sf::Color(255, 255, 240, static_cast<std::uint8_t>(30 + a * 150))
                }
            };
            window.draw(line, 2, sf::PrimitiveType::Lines);
        }

        // Ball
        {
            sf::CircleShape b(ball.radius);
            b.setOrigin(sf::Vector2f(ball.radius, ball.radius));
            b.setPosition(sf::Vector2f(ball.position.x, ball.position.y));
            b.setFillColor(sf::Color(250, 250, 245));
            b.setOutlineThickness(1.5f);
            b.setOutlineColor(sf::Color(180, 40, 45));
            window.draw(b);
        }

        // Bat silhouette + PCI (The Show–style yellow outline)
        {
            float drawAngle = aim.swinging() ? contactAim.displayAngle : aim.batAngle;
            Vector2 pciPos = aimFrozen ? frozenAim.pci : aim.pci;
            Vector2 drawKnob = aim.knob;
            if (aim.swinging()) {
                Vector2 axis = batAxis(drawAngle);
                drawKnob = pciPos - axis * (batCfg.sweetSpotFromKnobM * batCfg.pixelsPerMeter);
            } else {
                drawKnob = aim.knob;
            }

            sf::Color batCol = prof.accent;
            batCol.a = 255;
            drawBatSilhouette(window, drawKnob, drawAngle, batCfg, batCol, 1.08f);
            drawPci(window, pciPos, prof.pciRadiusPx, batCol);

            if (lastHit.hit && hasHit) {
                sf::CircleShape flash(12.0f);
                flash.setOrigin(sf::Vector2f(12.0f, 12.0f));
                flash.setPosition(sf::Vector2f(lastHit.point.x, lastHit.point.y));
                flash.setFillColor(sf::Color(255, 255, 150, 140));
                window.draw(flash);
            }
        }

        // HUD (text only)
        if (fontOk) {
            const SwingProfile& p = profileFor(aim.type);
            drawText(window, font, "Bat Physics", 20, {24, 16}, sf::Color(240, 245, 240));
            drawText(window, font, status, 15, {24, 46}, statusColor);

            std::ostringstream modes;
            modes << "Z Power  X Contact  C Regular   [" << p.name << "  pwr "
                  << std::fixed << std::setprecision(2) << p.power << "]";
            drawText(window, font, modes.str(), 14, {24, 72}, p.accent);

            std::ostringstream hud;
            hud << std::fixed << std::setprecision(0)
                << "Pitch " << pitch.speedMph << " mph   "
                << "Mouse aim · Q/E or wheel bat angle   Space/LMB swing   R reset   [ ] speed";
            drawText(window, font, hud.str(), 12, {24, 96}, sf::Color(170, 195, 185));

            if (lastHit.hit) {
                std::ostringstream hit;
                hit << std::fixed << std::setprecision(0)
                    << "Last: exit " << lastHit.exitMph << " mph @ "
                    << lastHit.launchDeg << "° · bat " << lastHit.batMph
                    << " mph · sweet " << (lastHit.sweet * 100.0f) << "% · s="
                    << std::setprecision(2) << lastHit.sMeters << " m";
                drawText(window, font, hit.str(), 13, {24, 120}, sf::Color(255, 220, 140));
            }

            // Legend bottom
            drawText(
                window, font,
                "Yellow bat = silhouette / PCI (aim). Power = small PCI + high exit. Contact = big PCI + soft.",
                11, {24, 688}, sf::Color(140, 170, 160)
            );
        }

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
