// Bat Physics Demo — side-view swing lab (separate from pitching simulator).
//
// Models a kinematic bat (pivot at the hands) swinging through a pitched ball.
// Contact uses segment-vs-circle, sweet-spot efficiency, COR, and bat point
// velocity (ω × r) to produce exit speed, launch angle, and back/topspin.
//
// Controls:
//   Space / LMB        swing
//   R                  reset pitch + bat
//   [ ]                pitch speed −/+
//   - =                swing power −/+
//   1 / 2 / 3          pitch height low / middle / high
//   A / D              pitch timing early / late (ball start X offset)
//   Esc                quit

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
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

// Screen: origin top-left, +Y down (SFML). Physics use the same coords.
// Pitcher is LEFT, field is RIGHT, batter faces left (RHB from 1B view-ish).

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

// ── Bat model ───────────────────────────────────────────────────────────

struct BatConfig {
    float length = 0.86f;          // meters (~34")
    float barrelRadius = 0.033f;   // ~2.6" diameter / 2
    float handleRadius = 0.014f;
    float massKg = 0.90f;          // ~32 oz
    float sweetSpotFromKnob = 0.58f; // meters along bat
    float sweetSpotWidth = 0.12f;
    float baseCor = 0.50f;         // bat-ball COR (BBCOR-ish)
    float maxCor = 0.55f;
    float minCor = 0.38f;          // mishit
};

struct BatState {
    Vector2 pivot;          // hands (screen px)
    float angle = 0.0f;     // rad; 0 = horizontal pointing left (−X), + = CCW
    float omega = 0.0f;     // rad/s (positive = CCW, toward pitch)
    float swingT = -1.0f;   // <0 idle; 0..1 active swing
    float swingDuration = 0.28f;
    float power = 1.0f;     // 0.6..1.2
    float pixelsPerMeter = 220.0f;

    // Rest / swing angles (screen: +Y down, so "up" is negative Y)
    float cockedAngle = 1.05f;   // bat up behind head
    float contactAngle = 0.05f;  // near level through zone
    float finishAngle = -1.15f;  // wrap around

    bool swinging() const { return swingT >= 0.0f && swingT < 1.0f; }
};

Vector2 batDirection(float angle) {
    // angle 0 → pointing left (−X); positive rotates CCW in screen space
    // (x,y) with y down: CCW from −X is toward −Y (up on screen) first...
    // Standard math CCW with y-down is clockwise on screen. We want the
    // visual swing to go "down through the zone" from cocked (bat up).
    // cockedAngle > 0 means bat tip above/behind. As swingT increases, angle
    // decreases through contact toward finish (negative).
    return Vector2(std::cos(angle + kPi), std::sin(angle + kPi));
}

Vector2 batTip(const BatState& bat, const BatConfig& cfg) {
    return bat.pivot + batDirection(bat.angle) * (cfg.length * bat.pixelsPerMeter);
}

// Point along bat: s = 0 knob, 1 tip (in meters along axis).
Vector2 batPoint(const BatState& bat, const BatConfig& cfg, float sMeters) {
    return bat.pivot + batDirection(bat.angle) * (sMeters * bat.pixelsPerMeter);
}

// Linear velocity of a point on the bat (px/s), from angular vel about pivot.
Vector2 batPointVelocity(const BatState& bat, const BatConfig& cfg, float sMeters) {
    // v = ω × r ; 2D: ω z-axis, r = (rx, ry) → v = ω * (-ry, rx) for CCW
    // With our angle convention, positive omega increases angle...
    // During swing omega is negative (angle decreasing). Use:
    // screen y-down: ω positive CCW in math with y-up maps to...
    Vector2 r = batDirection(bat.angle) * (sMeters * bat.pixelsPerMeter);
    // ω about +Z out of screen, y-down: v = ω * (ry, -rx) for positive ω
    // If angle increases (dθ>0), tip should move in direction of d(dir)/dθ.
    // dir = (cos(θ+π), sin(θ+π)) = (-cos θ, -sin θ)
    // d dir / dθ = (sin θ, -cos θ)
    // v = omega * d(dir)/dθ * length
    float c = std::cos(bat.angle);
    float s = std::sin(bat.angle);
    Vector2 dDir(s, -c); // d/dθ of (-cos θ, -sin θ)
    return dDir * (sMeters * bat.pixelsPerMeter * bat.omega);
}

float batRadiusAt(const BatConfig& cfg, float sMeters) {
    float t = clampf(sMeters / cfg.length, 0.0f, 1.0f);
    // Handle → taper → barrel
    if (t < 0.45f) {
        return lerp(cfg.handleRadius, cfg.barrelRadius * 0.7f, t / 0.45f);
    }
    return lerp(cfg.barrelRadius * 0.7f, cfg.barrelRadius, (t - 0.45f) / 0.55f);
}

float sweetSpotFactor(const BatConfig& cfg, float sMeters) {
    float d = std::abs(sMeters - cfg.sweetSpotFromKnob);
    float half = cfg.sweetSpotWidth * 0.5f;
    if (d >= half) {
        // Falloff outside sweet spot
        float extra = (d - half) / std::max(cfg.length * 0.35f, 0.01f);
        return clampf(1.0f - extra * 0.85f, 0.15f, 1.0f);
    }
    return 1.0f;
}

void updateSwing(BatState& bat, float dt) {
    if (bat.swingT < 0.0f) {
        bat.angle = bat.cockedAngle;
        bat.omega = 0.0f;
        return;
    }

    // Higher power → shorter swing (faster bat head).
    float duration = bat.swingDuration / std::max(bat.power, 0.5f);
    float prevT = bat.swingT;
    bat.swingT = std::min(1.0f, bat.swingT + dt / duration);

    // Ease: accelerate into contact, decelerate after.
    auto angleAt = [&](float t) {
        float u = smooth01(t);
        // Spend more time accelerating to contact (~0.45), then finish.
        if (u < 0.45f) {
            float k = u / 0.45f;
            k = k * k; // ease-in
            return lerp(bat.cockedAngle, bat.contactAngle, k);
        }
        float k = (u - 0.45f) / 0.55f;
        k = smooth01(k);
        return lerp(bat.contactAngle, bat.finishAngle, k);
    };

    float a0 = angleAt(prevT);
    float a1 = angleAt(bat.swingT);
    bat.angle = a1;
    if (dt > 1e-6f) {
        bat.omega = (a1 - a0) / dt;
    }

    if (bat.swingT >= 1.0f) {
        bat.omega = 0.0f;
    }
}

void startSwing(BatState& bat) {
    if (bat.swinging()) {
        return;
    }
    bat.swingT = 0.0f;
    bat.angle = bat.cockedAngle;
    bat.omega = 0.0f;
}

// Closest point on bat segment to a world point; returns s in meters along bat.
void closestOnBat(
    const BatState& bat,
    const BatConfig& cfg,
    const Vector2& point,
    float& outSMeters,
    Vector2& outPoint
) {
    Vector2 a = bat.pivot;
    Vector2 b = batTip(bat, cfg);
    Vector2 ab = b - a;
    float ab2 = ab.dot(ab);
    float t = 0.0f;
    if (ab2 > 1e-8f) {
        t = clampf((point - a).dot(ab) / ab2, 0.0f, 1.0f);
    }
    outPoint = a + ab * t;
    outSMeters = t * cfg.length;
}

// ── Contact impulse ─────────────────────────────────────────────────────

struct ContactResult {
    bool hit = false;
    float sMeters = 0.0f;
    float exitMph = 0.0f;
    float launchDeg = 0.0f; // screen: up is negative Y, report math launch
    float sweet = 0.0f;
    float batMph = 0.0f;
    Vector2 contactPoint;
};

ContactResult tryBatBallContact(
    Body2D& ball,
    const BatState& bat,
    const BatConfig& cfg,
    float pixelsPerMeter,
    bool& hasHitThisSwing
) {
    ContactResult result;
    if (hasHitThisSwing || !bat.swinging()) {
        return result;
    }

    float s = 0.0f;
    Vector2 closest;
    closestOnBat(bat, cfg, ball.position, s, closest);

    float rBat = batRadiusAt(cfg, s) * pixelsPerMeter;
    Vector2 delta = ball.position - closest;
    float dist = delta.magnitude();
    float minDist = ball.radius + rBat;
    if (dist > minDist || dist < 1e-5f) {
        return result;
    }

    Vector2 n = delta * (1.0f / dist); // bat surface → ball center
    Vector2 vBat = batPointVelocity(bat, cfg, s);
    Vector2 vRel = ball.velocity - vBat;
    float approach = vRel.dot(n);
    // Only resolve if closing (ball moving into bat surface)
    if (approach >= 0.0f) {
        return result;
    }

    float sweet = sweetSpotFactor(cfg, s);
    float cor = lerp(cfg.minCor, cfg.maxCor, sweet);
    cor = std::min(cor, cfg.baseCor + 0.05f * sweet);

    // Effective bat mass: higher near sweet spot / handle-weighted barrel feel
    // Tip is lighter effectively (harder to drive).
    float tipFactor = clampf(s / cfg.length, 0.0f, 1.0f);
    float mEff = cfg.massKg * lerp(1.35f, 0.55f, tipFactor) * lerp(0.7f, 1.15f, sweet);
    float mBall = std::max(ball.mass, 0.05f);

    // Impulse magnitude (kinematic bat ≈ infinite mass along known v_bat):
    // Using finite mEff for more realistic energy transfer.
    float invM = 1.0f / mBall + 1.0f / mEff;
    float j = -(1.0f + cor) * approach / invM;
    // Boost slightly with bat speed (tramline through the ball)
    float batSpeed = vBat.magnitude();
    j *= lerp(0.92f, 1.08f, sweet);

    ball.velocity += n * (j / mBall);

    // Friction → topspin / backspin proxy: tangential relative velocity
    // stored as Body2D has no spin; apply small tangential impulse.
    Vector2 tangent(-n.y, n.x);
    float vT = vRel.dot(tangent);
    float mu = 0.18f * sweet;
    float jt = clampf(-vT * mBall * 0.35f, -std::abs(j) * mu, std::abs(j) * mu);
    ball.velocity += tangent * (jt / mBall);

    // Separate so we don't stick
    float pen = minDist - dist;
    ball.position += n * (pen + 0.5f);

    hasHitThisSwing = true;
    result.hit = true;
    result.sMeters = s;
    result.sweet = sweet;
    result.contactPoint = closest;
    result.batMph = (batSpeed / pixelsPerMeter) * 2.236936f; // m/s → mph
    float exitMs = ball.velocity.magnitude() / pixelsPerMeter;
    result.exitMph = exitMs * 2.236936f;
    // Launch angle: 0 = horizontal +X (to field), positive = up (screen −Y)
    result.launchDeg = std::atan2(-ball.velocity.y, ball.velocity.x) * kDeg;
    return result;
}

// ── Pitch ───────────────────────────────────────────────────────────────

struct PitchState {
    float speedMph = 92.0f;
    float heightMode = 1.0f; // 0 low 1 mid 2 high
    float startXOffset = 0.0f; // timing: early (−) / late (+)
};

void resetPitch(
    Body2D& ball,
    const PitchState& pitch,
    float groundY,
    float plateX,
    float pixelsPerMeter
) {
    float height = lerp(groundY - 55.0f, groundY - 160.0f, pitch.heightMode / 2.0f);
    ball.position = Vector2(80.0f + pitch.startXOffset, height);
    float speedMs = pitch.speedMph * 0.44704f; // mph → m/s
    float speedPx = speedMs * pixelsPerMeter;
    // Ball comes from left toward plate (right)
    ball.velocity = Vector2(speedPx, 15.0f); // slight drop feel
    ball.acceleration = Vector2(0.0f, 0.0f);
}

} // namespace

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "Bat Physics Demo | Space swing | R reset"
    );
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    DemoFpsCounter fpsCounter("Bat Physics | Space swing | R reset");
    sf::Font font;
    bool fontOk = loadUiFont(font);

    BatConfig batCfg;
    BatState bat;
    bat.pixelsPerMeter = 210.0f;
    bat.pivot = Vector2(780.0f, 430.0f);
    bat.power = 1.0f;
    bat.swingDuration = 0.26f;

    const float groundY = 560.0f;
    const float plateX = 720.0f;

    PhysicsWorld2D world;
    world.gravity = Vector2(0.0f, 420.0f); // mild screen gravity
    world.setBounds(static_cast<float>(window.getSize().x), groundY);

    Body2D ball(Vector2(100.0f, 400.0f), 0.145f);
    ball.setRadius(0.037f * bat.pixelsPerMeter); // ~baseball radius
    ball.restitution = 0.45f;
    world.addBody(&ball);

    PitchState pitch;
    resetPitch(ball, pitch, groundY, plateX, bat.pixelsPerMeter);

    bool hasHit = false;
    ContactResult lastHit;
    std::string status = "Space to swing · ball is pitched automatically";
    sf::Color statusColor(200, 220, 210);

    // Trail
    std::vector<Vector2> trail;
    trail.reserve(128);

    sf::Clock clock;
    while (window.isOpen()) {
        float dt = std::min(clock.restart().asSeconds(), 0.05f);

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect(
                    sf::Vector2f(0, 0),
                    sf::Vector2f(static_cast<float>(resized->size.x), static_cast<float>(resized->size.y))
                )));
                world.setBounds(static_cast<float>(resized->size.x), groundY);
            }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                using K = sf::Keyboard::Key;
                if (key->code == K::Escape) {
                    window.close();
                } else if (key->code == K::Space) {
                    startSwing(bat);
                    hasHit = false;
                    status = "Swinging…";
                    statusColor = sf::Color(255, 230, 150);
                } else if (key->code == K::R) {
                    bat.swingT = -1.0f;
                    bat.angle = bat.cockedAngle;
                    bat.omega = 0.0f;
                    hasHit = false;
                    lastHit = {};
                    trail.clear();
                    resetPitch(ball, pitch, groundY, plateX, bat.pixelsPerMeter);
                    status = "Reset";
                    statusColor = sf::Color(180, 210, 200);
                } else if (key->code == K::LBracket) {
                    pitch.speedMph = std::max(60.0f, pitch.speedMph - 2.0f);
                } else if (key->code == K::RBracket) {
                    pitch.speedMph = std::min(105.0f, pitch.speedMph + 2.0f);
                } else if (key->code == K::Hyphen || key->code == K::Subtract) {
                    bat.power = std::max(0.6f, bat.power - 0.05f);
                } else if (key->code == K::Equal || key->code == K::Add) {
                    bat.power = std::min(1.25f, bat.power + 0.05f);
                } else if (key->code == K::Num1) {
                    pitch.heightMode = 0.0f;
                    resetPitch(ball, pitch, groundY, plateX, bat.pixelsPerMeter);
                    hasHit = false;
                    trail.clear();
                } else if (key->code == K::Num2) {
                    pitch.heightMode = 1.0f;
                    resetPitch(ball, pitch, groundY, plateX, bat.pixelsPerMeter);
                    hasHit = false;
                    trail.clear();
                } else if (key->code == K::Num3) {
                    pitch.heightMode = 2.0f;
                    resetPitch(ball, pitch, groundY, plateX, bat.pixelsPerMeter);
                    hasHit = false;
                    trail.clear();
                } else if (key->code == K::A) {
                    pitch.startXOffset = std::max(-120.0f, pitch.startXOffset - 15.0f);
                    resetPitch(ball, pitch, groundY, plateX, bat.pixelsPerMeter);
                    hasHit = false;
                    trail.clear();
                } else if (key->code == K::D) {
                    pitch.startXOffset = std::min(120.0f, pitch.startXOffset + 15.0f);
                    resetPitch(ball, pitch, groundY, plateX, bat.pixelsPerMeter);
                    hasHit = false;
                    trail.clear();
                }
            }
            if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    startSwing(bat);
                    hasHit = false;
                    status = "Swinging…";
                    statusColor = sf::Color(255, 230, 150);
                }
            }
        }

        updateSwing(bat, dt);

        // Only integrate ball when not stuck in huge velocity
        world.step(dt);

        ContactResult hit = tryBatBallContact(ball, bat, batCfg, bat.pixelsPerMeter, hasHit);
        if (hit.hit) {
            lastHit = hit;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0)
                << "HIT  exit " << lastHit.exitMph << " mph  "
                << "LA " << std::setprecision(0) << lastHit.launchDeg << "°  "
                << "bat " << lastHit.batMph << " mph  "
                << "sweet " << std::setprecision(0) << (lastHit.sweet * 100.0f) << "%  "
                << "s=" << std::setprecision(2) << lastHit.sMeters << " m";
            status = oss.str();
            statusColor = lastHit.sweet > 0.85f
                ? sf::Color(120, 255, 160)
                : (lastHit.sweet > 0.5f ? sf::Color(255, 230, 120) : sf::Color(255, 160, 110));
        }

        // Auto-reset if ball is gone
        if (ball.position.x > static_cast<float>(window.getSize().x) + 80.0f ||
            ball.position.y > groundY + 200.0f ||
            ball.position.x < -200.0f) {
            bat.swingT = -1.0f;
            bat.angle = bat.cockedAngle;
            hasHit = false;
            trail.clear();
            resetPitch(ball, pitch, groundY, plateX, bat.pixelsPerMeter);
            if (!lastHit.hit) {
                status = "Whiff / past — R or auto-reset";
                statusColor = sf::Color(200, 180, 160);
            }
        }

        if (trail.empty() || (ball.position - trail.back()).magnitude() > 8.0f) {
            trail.push_back(ball.position);
            if (trail.size() > 80) {
                trail.erase(trail.begin());
            }
        }

        // ── Draw ────────────────────────────────────────────────────────
        window.clear(sf::Color(12, 16, 22));

        // Ground / plate
        sf::RectangleShape ground(sf::Vector2f(static_cast<float>(window.getSize().x), 8.0f));
        ground.setPosition(sf::Vector2f(0.0f, groundY));
        ground.setFillColor(sf::Color(40, 55, 45));
        window.draw(ground);

        sf::RectangleShape plate(sf::Vector2f(36.0f, 36.0f));
        plate.setOrigin(sf::Vector2f(18.0f, 0.0f));
        plate.setPosition(sf::Vector2f(plateX, groundY - 2.0f));
        plate.setFillColor(sf::Color(230, 225, 205));
        window.draw(plate);

        // Strike zone guide (vertical band)
        sf::Vertex zone[] = {
            sf::Vertex{sf::Vector2f(plateX - 40.0f, groundY - 160.0f), sf::Color(80, 180, 190, 100)},
            sf::Vertex{sf::Vector2f(plateX - 40.0f, groundY - 55.0f), sf::Color(80, 180, 190, 100)},
            sf::Vertex{sf::Vector2f(plateX + 20.0f, groundY - 55.0f), sf::Color(80, 180, 190, 100)},
            sf::Vertex{sf::Vector2f(plateX + 20.0f, groundY - 160.0f), sf::Color(80, 180, 190, 100)},
            sf::Vertex{sf::Vector2f(plateX - 40.0f, groundY - 160.0f), sf::Color(80, 180, 190, 100)}
        };
        window.draw(zone, 5, sf::PrimitiveType::LineStrip);

        // Ball trail
        for (size_t i = 1; i < trail.size(); i++) {
            float a = static_cast<float>(i) / static_cast<float>(trail.size());
            sf::Vertex line[] = {
                sf::Vertex{
                    sf::Vector2f(trail[i - 1].x, trail[i - 1].y),
                    sf::Color(200, 210, 220, static_cast<std::uint8_t>(40 + a * 160))
                },
                sf::Vertex{
                    sf::Vector2f(trail[i].x, trail[i].y),
                    sf::Color(200, 210, 220, static_cast<std::uint8_t>(40 + a * 160))
                }
            };
            window.draw(line, 2, sf::PrimitiveType::Lines);
        }

        // Bat (tapered look as stacked segments)
        {
            const int segs = 14;
            for (int i = 0; i < segs; i++) {
                float t0 = static_cast<float>(i) / segs;
                float t1 = static_cast<float>(i + 1) / segs;
                float s0 = t0 * batCfg.length;
                float s1 = t1 * batCfg.length;
                Vector2 p0 = batPoint(bat, batCfg, s0);
                Vector2 p1 = batPoint(bat, batCfg, s1);
                float r = batRadiusAt(batCfg, 0.5f * (s0 + s1)) * bat.pixelsPerMeter;
                // Sweet spot tint
                float sweet = sweetSpotFactor(batCfg, 0.5f * (s0 + s1));
                sf::Color col(
                    static_cast<std::uint8_t>(140 + 40 * sweet),
                    static_cast<std::uint8_t>(90 + 30 * sweet),
                    static_cast<std::uint8_t>(45 + 10 * (1.0f - sweet))
                );
                sf::RectangleShape seg(sf::Vector2f((p1 - p0).magnitude() + 1.0f, r * 2.0f));
                seg.setOrigin(sf::Vector2f(0.0f, r));
                seg.setPosition(sf::Vector2f(p0.x, p0.y));
                float ang = std::atan2(p1.y - p0.y, p1.x - p0.x) * kDeg;
                seg.setRotation(sf::degrees(ang));
                seg.setFillColor(col);
                window.draw(seg);
            }
            // Knob
            sf::CircleShape knob(6.0f);
            knob.setOrigin(sf::Vector2f(6.0f, 6.0f));
            knob.setPosition(sf::Vector2f(bat.pivot.x, bat.pivot.y));
            knob.setFillColor(sf::Color(90, 70, 50));
            window.draw(knob);

            // Sweet spot marker
            Vector2 ss = batPoint(bat, batCfg, batCfg.sweetSpotFromKnob);
            sf::CircleShape mark(4.0f);
            mark.setOrigin(sf::Vector2f(4.0f, 4.0f));
            mark.setPosition(sf::Vector2f(ss.x, ss.y));
            mark.setFillColor(sf::Color(80, 220, 140, 180));
            window.draw(mark);
        }

        // Ball
        sf::CircleShape ballShape(ball.radius);
        ballShape.setOrigin(sf::Vector2f(ball.radius, ball.radius));
        ballShape.setPosition(sf::Vector2f(ball.position.x, ball.position.y));
        ballShape.setFillColor(sf::Color(245, 245, 240));
        ballShape.setOutlineThickness(1.0f);
        ballShape.setOutlineColor(sf::Color(180, 40, 45));
        window.draw(ballShape);

        // Contact flash
        if (lastHit.hit && hasHit) {
            sf::CircleShape flash(10.0f);
            flash.setOrigin(sf::Vector2f(10.0f, 10.0f));
            flash.setPosition(sf::Vector2f(lastHit.contactPoint.x, lastHit.contactPoint.y));
            flash.setFillColor(sf::Color(255, 255, 120, 120));
            window.draw(flash);
        }

        if (fontOk) {
            drawText(window, font, "Bat Physics Demo", 20, {24, 18}, sf::Color(230, 240, 245));
            drawText(window, font, status, 15, {24, 48}, statusColor);

            std::ostringstream hud;
            hud << std::fixed << std::setprecision(0)
                << "Pitch " << pitch.speedMph << " mph   "
                << "Power " << std::setprecision(2) << bat.power << "   "
                << "Height " << (pitch.heightMode < 0.5f ? "low" : (pitch.heightMode < 1.5f ? "mid" : "high"))
                << "   Timing offset " << std::setprecision(0) << pitch.startXOffset << " px";
            drawText(window, font, hud.str(), 13, {24, 74}, sf::Color(160, 190, 200));

            drawText(
                window, font,
                "Space/LMB swing   R reset   [ ] pitch speed   - = power   1/2/3 height   A/D timing",
                12, {24, 96}, sf::Color(130, 160, 170)
            );

            if (lastHit.hit) {
                std::ostringstream phys;
                phys << std::fixed << std::setprecision(1)
                     << "Last contact: exit " << lastHit.exitMph << " mph @ "
                     << lastHit.launchDeg << "° LA · bat "
                     << lastHit.batMph << " mph · sweet-spot "
                     << std::setprecision(0) << (lastHit.sweet * 100.0f) << "%"
                     << " · along bat " << std::setprecision(2) << lastHit.sMeters << " m";
                drawText(window, font, phys.str(), 13, {24, 122}, sf::Color(255, 220, 140));
            }

            drawText(
                window, font,
                "Green dot = sweet spot · exit vel from COR + bat point speed (ωxr) + sweet-spot mass",
                11, {24, 680}, sf::Color(110, 140, 150)
            );
        }

        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
