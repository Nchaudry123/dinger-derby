// Stadium Demo — procedural ballpark showcase for Home Run Derby.
//
// Builds a full diamond + outfield wall + stands in world units
// (1 unit ≈ 2 ft, matching bat/pitch demos). Orbit the park and
// jump between preset views (batter, mound, CF, press box).
//
// Controls:
//   Mouse drag        orbit
//   Scroll / [ ]      zoom
//   1 Batter view
//   2 Mound view
//   3 CF wall view
//   4 Press box
//   5 Free orbit
//   6 Full park overview (default)
//   R                 reset to overview
//   G                 toggle labels
//   Esc               quit

#include <SFML/Graphics.hpp>
#include <SFML/Window/ContextSettings.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "DemoFpsCounter.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "rendering/Camera3D.h"
#include "rendering/GlRenderer.h"
#include "rendering/Mesh3D.h"

namespace {

constexpr float pi = 3.1415926535f;
constexpr float feetPerWorldUnit = 2.0f;

// Home at origin. +Z = center field. Foul poles ~±45° from CF.
// Short-porch showcase dimensions (real-ish, compact enough to read).
constexpr float wallFeet = 380.0f;
constexpr float wallR = wallFeet / feetPerWorldUnit;       // ~190
constexpr float wallH = 18.0f / feetPerWorldUnit;          // taller wall so it reads from orbit
constexpr float foulAngle = 45.0f * pi / 180.0f;
constexpr float infieldR = 95.0f / feetPerWorldUnit;       // dirt arc
constexpr float moundZ = 60.5f / feetPerWorldUnit;         // 60.5 ft from home toward CF
constexpr float basePath = 90.0f / feetPerWorldUnit;

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

void lookAt(Camera3D& cam, const Vector3& pos, const Vector3& target) {
    cam.position = pos;
    Vector3 to = target - pos;
    float h = std::sqrt(to.x * to.x + to.z * to.z);
    cam.rotation.y = std::atan2(to.x, to.z);
    cam.rotation.x = -std::atan2(to.y, h);
    cam.rotation.z = 0.0f;
}

// ── Mesh builders ───────────────────────────────────────────────────────

void addTri(Mesh3D& m, const Vector3& a, const Vector3& b, const Vector3& c, sf::Color col) {
    int i = static_cast<int>(m.vertices.size());
    m.vertices.push_back(a);
    m.vertices.push_back(b);
    m.vertices.push_back(c);
    m.triangles.push_back({i, i + 1, i + 2});
    m.triangleColors.push_back(col);
}

void addQuad(
    Mesh3D& m,
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    const Vector3& d,
    sf::Color col
) {
    // a--b
    // |  |
    // d--c
    addTri(m, a, b, c, col);
    addTri(m, a, c, d, col);
}

void addBox(
    Mesh3D& m,
    const Vector3& center,
    float w,
    float h,
    float d,
    sf::Color col
) {
    float hw = w * 0.5f;
    float hh = h * 0.5f;
    float hd = d * 0.5f;
    Vector3 p[8] = {
        center + Vector3(-hw, -hh, -hd),
        center + Vector3(hw, -hh, -hd),
        center + Vector3(hw, -hh, hd),
        center + Vector3(-hw, -hh, hd),
        center + Vector3(-hw, hh, -hd),
        center + Vector3(hw, hh, -hd),
        center + Vector3(hw, hh, hd),
        center + Vector3(-hw, hh, hd),
    };
    // Bottom / top
    addQuad(m, p[0], p[1], p[2], p[3], col);
    addQuad(m, p[4], p[7], p[6], p[5], col);
    // Sides
    addQuad(m, p[0], p[4], p[5], p[1], col);
    addQuad(m, p[1], p[5], p[6], p[2], col);
    addQuad(m, p[2], p[6], p[7], p[3], col);
    addQuad(m, p[3], p[7], p[4], p[0], col);
}

Vector3 polar(float r, float angle, float y = 0.0f) {
    return Vector3(std::sin(angle) * r, y, std::cos(angle) * r);
}

// Grass outfield disc sector from r0..r1, a0..a1
void addAnnulusSector(
    Mesh3D& m,
    float r0,
    float r1,
    float a0,
    float a1,
    float y,
    int segs,
    sf::Color col
) {
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = a0 + (a1 - a0) * t0;
        float ang1 = a0 + (a1 - a0) * t1;
        Vector3 a = polar(r0, ang0, y);
        Vector3 b = polar(r0, ang1, y);
        Vector3 c = polar(r1, ang1, y);
        Vector3 d = polar(r1, ang0, y);
        addQuad(m, a, b, c, d, col);
    }
}

// Vertical wall ribbon along an arc (double-sided so it reads from any orbit).
void addArcWall(
    Mesh3D& m,
    float r,
    float a0,
    float a1,
    float y0,
    float y1,
    int segs,
    sf::Color face,
    sf::Color top
) {
    for (int i = 0; i < segs; i++) {
        float t0 = static_cast<float>(i) / segs;
        float t1 = static_cast<float>(i + 1) / segs;
        float ang0 = a0 + (a1 - a0) * t0;
        float ang1 = a0 + (a1 - a0) * t1;
        Vector3 b0 = polar(r, ang0, y0);
        Vector3 b1 = polar(r, ang1, y0);
        Vector3 tA = polar(r, ang0, y1);
        Vector3 tB = polar(r, ang1, y1);
        // Inward face (toward home) + outward face for exterior orbit views
        addQuad(m, b0, b1, tB, tA, face);
        addQuad(m, b1, b0, tA, tB, face);
        // Cap on top of wall
        Vector3 outer0 = polar(r + 1.8f, ang0, y1);
        Vector3 outer1 = polar(r + 1.8f, ang1, y1);
        addQuad(m, tA, tB, outer1, outer0, top);
        addQuad(m, tB, tA, outer0, outer1, top);
    }
}

// Stepped seating bowl outside the wall
void addStands(
    Mesh3D& m,
    float rInner,
    float a0,
    float a1,
    int rows,
    int segs,
    float rowDepth,
    float rowRise,
    sf::Color seat,
    sf::Color riser
) {
    for (int row = 0; row < rows; row++) {
        float r0 = rInner + row * rowDepth;
        float r1 = r0 + rowDepth;
        float y0 = wallH + 0.4f + row * rowRise;
        float y1 = y0 + rowRise;
        for (int i = 0; i < segs; i++) {
            float t0 = static_cast<float>(i) / segs;
            float t1 = static_cast<float>(i + 1) / segs;
            float ang0 = a0 + (a1 - a0) * t0;
            float ang1 = a0 + (a1 - a0) * t1;
            // Seat tread
            addQuad(
                m,
                polar(r0, ang0, y1),
                polar(r0, ang1, y1),
                polar(r1, ang1, y1),
                polar(r1, ang0, y1),
                seat
            );
            // Riser face
            addQuad(
                m,
                polar(r0, ang0, y0),
                polar(r0, ang1, y0),
                polar(r0, ang1, y1),
                polar(r0, ang0, y1),
                riser
            );
        }
    }
}

Mesh3D buildStadiumField() {
    Mesh3D m;
    const float aL = -foulAngle;
    const float aR = foulAngle;
    const int segs = 48;

    // Outfield grass
    sf::Color grass(34, 110, 48);
    sf::Color grassDark(28, 92, 40);
    addAnnulusSector(m, 0.5f, wallR - 4.0f, aL, aR, 0.0f, segs, grass);

    // Checker-ish darker bands (every other sector strip via second pass)
    for (int band = 0; band < 6; band++) {
        float r0 = 20.0f + band * 28.0f;
        float r1 = r0 + 10.0f;
        if (r1 > wallR - 4.0f) {
            break;
        }
        addAnnulusSector(m, r0, r1, aL, aR, 0.01f, segs, grassDark);
    }

    // Warning track
    sf::Color track(150, 120, 70);
    addAnnulusSector(m, wallR - 4.0f, wallR - 0.3f, aL, aR, 0.02f, segs, track);

    // Infield dirt diamond-ish disc + base paths
    sf::Color dirt(168, 120, 70);
    sf::Color dirtDark(140, 98, 55);
    addAnnulusSector(m, 0.0f, infieldR, aL * 1.15f, aR * 1.15f, 0.03f, 36, dirt);

    // Pitcher's mound circle
    addAnnulusSector(m, 0.0f, 4.5f, 0.0f, pi * 2.0f, 0.08f, 24, dirtDark);
    // Shift mound geometry by translating verts is hard — build mound at correct place:
    // Rebuild mound at (0, y, moundZ) as a small cone-ish disc via quads around center.
    {
        Vector3 c(0.0f, 0.12f, moundZ);
        const int n = 20;
        float mr = 4.2f;
        for (int i = 0; i < n; i++) {
            float a0 = (static_cast<float>(i) / n) * pi * 2.0f;
            float a1 = (static_cast<float>(i + 1) / n) * pi * 2.0f;
            Vector3 p0 = c + Vector3(std::cos(a0) * mr, -0.08f, std::sin(a0) * mr * 0.55f);
            Vector3 p1 = c + Vector3(std::cos(a1) * mr, -0.08f, std::sin(a1) * mr * 0.55f);
            addTri(m, c, p0, p1, dirtDark);
        }
        // Rubber
        addBox(m, Vector3(0.0f, 0.18f, moundZ), 1.0f, 0.06f, 0.3f, sf::Color(230, 225, 210));
    }

    // Home plate (point toward catcher = −Z)
    {
        Vector3 tip(0.0f, 0.06f, -0.55f);
        Vector3 bl(-0.55f, 0.06f, 0.15f);
        Vector3 br(0.55f, 0.06f, 0.15f);
        Vector3 fl(-0.35f, 0.06f, 0.55f);
        Vector3 fr(0.35f, 0.06f, 0.55f);
        sf::Color plate(240, 235, 220);
        addTri(m, tip, bl, br, plate);
        addQuad(m, bl, br, fr, fl, plate);
    }

    // Bases: 1B (+X), 2B (+Z), 3B (−X) at 90 ft
    auto base = [&](float x, float z) {
        addBox(m, Vector3(x, 0.08f, z), 1.1f, 0.08f, 1.1f, sf::Color(245, 240, 225));
    };
    base(basePath, basePath * 0.15f);           // rough 1B
    base(0.0f, basePath);                       // 2B toward CF a bit
    base(-basePath, basePath * 0.15f);          // 3B

    // Batter's boxes
    addBox(m, Vector3(-1.5f, 0.04f, 0.2f), 1.4f, 0.04f, 2.2f, dirtDark);
    addBox(m, Vector3(1.5f, 0.04f, 0.2f), 1.4f, 0.04f, 2.2f, dirtDark);

    m.rebuildNormals();
    return m;
}

Mesh3D buildStadiumWalls() {
    Mesh3D m;
    const float aL = -foulAngle;
    const float aR = foulAngle;
    const int segs = 56;

    sf::Color wall(55, 70, 95);
    sf::Color wallTop(70, 88, 115);
    sf::Color pad(40, 55, 75);
    addArcWall(m, wallR, aL, aR, 0.0f, wallH, segs, wall, wallTop);

    // Inner pad strip
    addArcWall(m, wallR - 0.35f, aL, aR, 0.0f, wallH * 0.55f, segs, pad, pad);

    // Foul poles
    sf::Color pole(240, 230, 80);
    Vector3 left = polar(wallR, aL, 0.0f);
    Vector3 right = polar(wallR, aR, 0.0f);
    addBox(m, left + Vector3(0, wallH * 1.6f, 0), 0.55f, wallH * 3.2f, 0.55f, pole);
    addBox(m, right + Vector3(0, wallH * 1.6f, 0), 0.55f, wallH * 3.2f, 0.55f, pole);
    // Pole screens (thin tall)
    addBox(m, left + Vector3(0, wallH * 2.8f, 0), 0.12f, wallH * 2.0f, 2.5f, sf::Color(200, 200, 190, 180));
    addBox(m, right + Vector3(0, wallH * 2.8f, 0), 0.12f, wallH * 2.0f, 2.5f, sf::Color(200, 200, 190, 180));

    // CF scoreboard
    Vector3 cf = polar(wallR + 6.0f, 0.0f, wallH + 8.0f);
    addBox(m, cf, 28.0f, 14.0f, 2.5f, sf::Color(30, 35, 45));
    addBox(m, cf + Vector3(0, 0, -1.4f), 24.0f, 10.0f, 0.4f, sf::Color(20, 90, 50)); // green screen

    // Dugouts
    addBox(m, Vector3(-18.0f, 1.0f, 12.0f), 14.0f, 2.0f, 4.0f, sf::Color(50, 60, 70));
    addBox(m, Vector3(18.0f, 1.0f, 12.0f), 14.0f, 2.0f, 4.0f, sf::Color(50, 60, 70));

    m.rebuildNormals();
    return m;
}

Mesh3D buildStadiumStands() {
    Mesh3D m;
    const float aL = -foulAngle - 0.12f;
    const float aR = foulAngle + 0.12f;
    sf::Color seat(70, 95, 140);
    sf::Color riser(50, 68, 100);
    sf::Color upper(90, 70, 75);

    addStands(m, wallR + 1.5f, aL, aR, 12, 40, 3.2f, 1.15f, seat, riser);
    // Upper deck band
    addStands(m, wallR + 1.5f + 12 * 3.2f, aL * 0.85f, aR * 0.85f, 6, 32, 3.6f, 1.35f, upper, sf::Color(60, 48, 52));

    // Light towers
    sf::Color steel(180, 185, 190);
    auto tower = [&](float ang) {
        Vector3 base = polar(wallR + 18.0f, ang, 0.0f);
        addBox(m, base + Vector3(0, 22.0f, 0), 1.2f, 44.0f, 1.2f, steel);
        addBox(m, base + Vector3(0, 44.0f, 0), 8.0f, 2.0f, 3.0f, sf::Color(240, 240, 220));
    };
    tower(-0.55f);
    tower(0.0f);
    tower(0.55f);

    m.rebuildNormals();
    return m;
}

Mesh3D buildFoulLinesOverlay() {
    // Slightly raised white chalk lines for readability
    Mesh3D m;
    sf::Color chalk(245, 245, 235);
    float len = wallR + 2.0f;
    auto line = [&](float ang) {
        Vector3 a(0.0f, 0.07f, 0.0f);
        Vector3 b = polar(len, ang, 0.07f);
        // Fat ribbon along the line
        Vector3 dir = b - a;
        float dlen = dir.magnitude();
        if (dlen < 1e-4f) {
            return;
        }
        dir = dir * (1.0f / dlen);
        Vector3 side(-dir.z, 0, dir.x);
        side = side * 0.18f;
        addQuad(m, a + side, a - side, b - side, b + side, chalk);
    };
    line(-foulAngle);
    line(foulAngle);
    // Batter's box outline already dirt; add CF hash at wall
    m.rebuildNormals();
    return m;
}

enum class CamPreset { Free = 0, Batter, Mound, CenterField, PressBox, Overview };

// Park center for framing the whole bowl.
Vector3 parkCenter() {
    return Vector3(0.0f, 4.0f, wallR * 0.42f);
}

const char* presetName(CamPreset p) {
    switch (p) {
        case CamPreset::Batter:
            return "Batter";
        case CamPreset::Mound:
            return "Mound";
        case CamPreset::CenterField:
            return "CF Wall";
        case CamPreset::PressBox:
            return "Press Box";
        case CamPreset::Overview:
            return "Full Park";
        default:
            return "Free Orbit";
    }
}

void configureStadiumCamera(Camera3D& cam) {
    cam.nearPlane = 0.5f;
    // Wall ~190, stands out to ~250, orbit up to ~800 — need a deep far plane.
    cam.farPlane = 2500.0f;
}

void applyPreset(Camera3D& cam, CamPreset p) {
    configureStadiumCamera(cam);
    switch (p) {
        case CamPreset::Batter:
            lookAt(cam, Vector3(0.0f, 1.55f, -2.2f), Vector3(0.0f, 1.2f, wallR * 0.45f));
            cam.fieldOfView = 780.0f;
            break;
        case CamPreset::Mound:
            lookAt(cam, Vector3(0.0f, 2.0f, moundZ - 1.5f), Vector3(0.0f, 1.0f, 0.0f));
            cam.fieldOfView = 820.0f;
            break;
        case CamPreset::CenterField:
            lookAt(
                cam,
                polar(wallR - 8.0f, 0.0f, wallH + 4.0f),
                Vector3(0.0f, 1.0f, 10.0f)
            );
            cam.fieldOfView = 900.0f;
            break;
        case CamPreset::PressBox:
            // High 1B-side press box looking across the whole diamond + wall.
            lookAt(
                cam,
                Vector3(95.0f, 72.0f, -40.0f),
                Vector3(0.0f, 6.0f, wallR * 0.40f)
            );
            cam.fieldOfView = 1050.0f;
            break;
        case CamPreset::Overview:
            // Elevated behind home, entire bowl + wall in frame.
            lookAt(
                cam,
                Vector3(0.0f, 145.0f, -95.0f),
                parkCenter()
            );
            cam.fieldOfView = 720.0f;
            break;
        default:
            break;
    }
}

void updateOrbitCamera(
    Camera3D& cam,
    float yaw,
    float pitch,
    float distance,
    const Vector3& target
) {
    configureStadiumCamera(cam);
    // Allow steep top-down and wide side orbits for full-park viewing.
    pitch = std::clamp(pitch, 0.05f, 1.45f);
    float h = std::cos(pitch) * distance;
    float elev = std::sin(pitch) * distance;
    cam.position = target + Vector3(std::sin(yaw) * h, elev + 3.0f, -std::cos(yaw) * h);
    lookAt(cam, cam.position, target);
    // Wider FOV when zoomed far so the bowl fits.
    cam.fieldOfView = std::clamp(900.0f + distance * 0.35f, 900.0f, 1400.0f);
}

} // namespace

int main() {
    sf::ContextSettings glSettings;
    glSettings.depthBits = 24;
    glSettings.stencilBits = 8;
    glSettings.antiAliasingLevel = 4;

    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1400, 820)),
        "Stadium Demo | Home Run Derby ballpark",
        sf::Style::Default,
        sf::State::Windowed,
        glSettings
    );
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    DemoFpsCounter fps("Stadium Demo");
    sf::Font font;
    bool fontOk = loadUiFont(font);

    Mesh3D fieldMesh = buildStadiumField();
    Mesh3D wallMesh = buildStadiumWalls();
    Mesh3D standsMesh = buildStadiumStands();
    Mesh3D linesMesh = buildFoulLinesOverlay();

    GlRenderer gl;
    bool useGL = gl.initialize(window);
    GlMesh glField;
    GlMesh glWalls;
    GlMesh glStands;
    GlMesh glLines;
    if (useGL) {
        glField.upload(fieldMesh);
        glWalls.upload(wallMesh);
        glStands.upload(standsMesh);
        glLines.upload(linesMesh);
    }

    Camera3D camera;
    CamPreset preset = CamPreset::Overview;
    float orbitYaw = 0.15f;
    float orbitPitch = 0.72f;
    float orbitDist = 340.0f;
    Vector3 orbitTarget = parkCenter();
    applyPreset(camera, preset);

    bool dragging = false;
    sf::Vector2i lastMouse;
    bool showLabels = true;

    sf::Clock clock;
    while (window.isOpen()) {
        float dt = std::min(clock.restart().asSeconds(), 0.05f);
        (void)dt;

        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                using K = sf::Keyboard::Key;
                if (key->code == K::Escape) {
                    window.close();
                } else if (key->code == K::Num1) {
                    preset = CamPreset::Batter;
                    applyPreset(camera, preset);
                } else if (key->code == K::Num2) {
                    preset = CamPreset::Mound;
                    applyPreset(camera, preset);
                } else if (key->code == K::Num3) {
                    preset = CamPreset::CenterField;
                    applyPreset(camera, preset);
                } else if (key->code == K::Num4) {
                    preset = CamPreset::PressBox;
                    applyPreset(camera, preset);
                } else if (key->code == K::Num5) {
                    preset = CamPreset::Free;
                    orbitTarget = parkCenter();
                    updateOrbitCamera(camera, orbitYaw, orbitPitch, orbitDist, orbitTarget);
                } else if (key->code == K::Num6) {
                    preset = CamPreset::Overview;
                    applyPreset(camera, preset);
                } else if (key->code == K::R) {
                    orbitYaw = 0.15f;
                    orbitPitch = 0.72f;
                    orbitDist = 340.0f;
                    orbitTarget = parkCenter();
                    preset = CamPreset::Overview;
                    applyPreset(camera, preset);
                } else if (key->code == K::LBracket) {
                    orbitDist = std::min(900.0f, orbitDist + 18.0f);
                    preset = CamPreset::Free;
                    orbitTarget = parkCenter();
                    updateOrbitCamera(camera, orbitYaw, orbitPitch, orbitDist, orbitTarget);
                } else if (key->code == K::RBracket) {
                    orbitDist = std::max(50.0f, orbitDist - 18.0f);
                    preset = CamPreset::Free;
                    orbitTarget = parkCenter();
                    updateOrbitCamera(camera, orbitYaw, orbitPitch, orbitDist, orbitTarget);
                } else if (key->code == K::G) {
                    showLabels = !showLabels;
                }
            }
            if (const auto* m = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (m->button == sf::Mouse::Button::Left) {
                    dragging = true;
                    lastMouse = sf::Mouse::getPosition(window);
                    // Enter free orbit from whatever preset, framed on the park.
                    if (preset != CamPreset::Free) {
                        orbitTarget = parkCenter();
                        orbitDist = std::max(orbitDist, 300.0f);
                    }
                    preset = CamPreset::Free;
                }
            }
            if (const auto* m = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (m->button == sf::Mouse::Button::Left) {
                    dragging = false;
                }
            }
            if (const auto* wh = event->getIf<sf::Event::MouseWheelScrolled>()) {
                orbitDist = std::clamp(orbitDist - wh->delta * 16.0f, 50.0f, 900.0f);
                orbitTarget = parkCenter();
                preset = CamPreset::Free;
                updateOrbitCamera(camera, orbitYaw, orbitPitch, orbitDist, orbitTarget);
            }
        }

        if (dragging && preset == CamPreset::Free) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            sf::Vector2i d = mp - lastMouse;
            lastMouse = mp;
            orbitYaw += d.x * 0.005f;
            orbitPitch += d.y * 0.004f;
            orbitTarget = parkCenter();
            updateOrbitCamera(camera, orbitYaw, orbitPitch, orbitDist, orbitTarget);
        }

        Matrix4 id = Matrix4::identity();
        if (useGL) {
            gl.beginFrame(window, camera, sf::Color(8, 12, 22));
            // Large ground so exterior views still have a floor.
            gl.drawGround(wallR + 120.0f, -80.0f, wallR + 120.0f, sf::Color(18, 32, 22));
            gl.drawMesh(glField, id);
            gl.drawMesh(glWalls, id);
            gl.drawMesh(glStands, id);
            gl.drawMesh(glLines, id);
            gl.endFrame(window);
        } else {
            window.clear(sf::Color(8, 12, 22));
        }

        if (fontOk) {
            drawText(window, font, "Stadium Demo | Home Run Derby ballpark", 22, {22, 14}, sf::Color(240, 245, 250));
            std::ostringstream info;
            info << "View: " << presetName(preset)
                 << "   Wall " << static_cast<int>(wallFeet) << " ft CF"
                 << "   Foul poles +/-45 deg"
                 << "   1 unit = " << feetPerWorldUnit << " ft"
                 << "   zoom " << static_cast<int>(orbitDist);
            drawText(window, font, info.str(), 14, {22, 46}, sf::Color(160, 200, 180));
            drawText(
                window,
                font,
                "Drag orbit  Scroll/[ ] zoom  1 Batter  2 Mound  3 CF  4 Press  5 Free  6 Full park  R reset",
                13,
                {22, 70},
                sf::Color(140, 170, 190)
            );

            if (showLabels) {
                auto projectLabel = [&](const Vector3& w, const std::string& text, sf::Color c) {
                    ProjectedPoint3D p = camera.projectPoint(
                        w,
                        static_cast<float>(window.getSize().x),
                        static_cast<float>(window.getSize().y)
                    );
                    if (!p.visible) {
                        return;
                    }
                    drawText(window, font, text, 12, {p.position.x + 6.0f, p.position.y - 8.0f}, c);
                };
                projectLabel(Vector3(0, 1.2f, 0), "HOME", sf::Color(255, 240, 200));
                projectLabel(Vector3(0, 2.0f, moundZ), "MOUND", sf::Color(255, 220, 160));
                projectLabel(polar(wallR, 0.0f, wallH + 2.0f), "CF WALL", sf::Color(180, 220, 255));
                projectLabel(polar(wallR, -foulAngle, wallH * 2.0f), "LF POLE", sf::Color(255, 230, 100));
                projectLabel(polar(wallR, foulAngle, wallH * 2.0f), "RF POLE", sf::Color(255, 230, 100));
                projectLabel(polar(wallR + 6.0f, 0.0f, wallH + 14.0f), "SCOREBOARD", sf::Color(160, 255, 180));
            }
        }

        fps.frame(window);
        window.display();
    }

    return 0;
}
