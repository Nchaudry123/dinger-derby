// Stadium Demo — blank play-space scaffold (stadium model removed).
// Same world frame as pitching + bat demos: plate at plateZ, mound at 0, CF −Z.
// Rebuild the park from scratch when ready.
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

#include "DemoFpsCounter.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "rendering/Camera3D.h"
#include "rendering/GlRenderer.h"
#include "rendering/Stadium3D.h"

namespace {

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

enum class CamPreset { Free = 0, Batter, Mound, CenterField, PressBox, Overview };

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
    cam.farPlane = Stadium3D::recommendedFarPlane();
}

void applyPreset(Camera3D& cam, CamPreset p, const Stadium3D::Layout& L) {
    configureStadiumCamera(cam);
    const float plateZ = L.plateZ();
    const float wallR = L.maxWallR();
    const float wallH = L.wallHeightAtAngle(0.0f);
    switch (p) {
        case CamPreset::Batter:
            lookAt(
                cam,
                Vector3(0.0f, 1.55f, plateZ + 2.2f),
                Vector3(0.0f, 1.2f, plateZ - wallR * 0.45f)
            );
            cam.fieldOfView = 780.0f;
            break;
        case CamPreset::Mound:
            lookAt(cam, Vector3(0.0f, 2.0f, 1.5f), Vector3(0.0f, 1.0f, plateZ));
            cam.fieldOfView = 820.0f;
            break;
        case CamPreset::CenterField:
            lookAt(
                cam,
                L.fromHome(L.wallRAtAngle(0.0f) - 8.0f, 0.0f, wallH + 4.0f),
                Vector3(0.0f, 1.0f, plateZ - 10.0f)
            );
            cam.fieldOfView = 900.0f;
            break;
        case CamPreset::PressBox:
            lookAt(
                cam,
                Vector3(95.0f, 72.0f, plateZ + 40.0f),
                L.parkCenter()
            );
            cam.fieldOfView = 1050.0f;
            break;
        case CamPreset::Overview:
            lookAt(cam, Vector3(0.0f, 145.0f, plateZ + 95.0f), L.parkCenter());
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
    pitch = std::clamp(pitch, 0.05f, 1.45f);
    float h = std::cos(pitch) * distance;
    float elev = std::sin(pitch) * distance;
    cam.position = target + Vector3(std::sin(yaw) * h, elev + 3.0f, -std::cos(yaw) * h);
    lookAt(cam, cam.position, target);
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

    Stadium3D::Layout layout = Stadium3D::defaultPlayLayout();
    // Stadium meshes intentionally empty (build returns blank).

    GlRenderer gl;
    bool useGL = gl.initialize(window);

    Camera3D camera;
    CamPreset preset = CamPreset::Overview;
    float orbitYaw = 0.15f;
    float orbitPitch = 0.72f;
    float orbitDist = 340.0f;
    Vector3 orbitTarget = layout.parkCenter();
    applyPreset(camera, preset, layout);

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
                    applyPreset(camera, preset, layout);
                } else if (key->code == K::Num2) {
                    preset = CamPreset::Mound;
                    applyPreset(camera, preset, layout);
                } else if (key->code == K::Num3) {
                    preset = CamPreset::CenterField;
                    applyPreset(camera, preset, layout);
                } else if (key->code == K::Num4) {
                    preset = CamPreset::PressBox;
                    applyPreset(camera, preset, layout);
                } else if (key->code == K::Num5) {
                    preset = CamPreset::Free;
                    orbitTarget = layout.parkCenter();
                    updateOrbitCamera(camera, orbitYaw, orbitPitch, orbitDist, orbitTarget);
                } else if (key->code == K::Num6) {
                    preset = CamPreset::Overview;
                    applyPreset(camera, preset, layout);
                } else if (key->code == K::R) {
                    orbitYaw = 0.15f;
                    orbitPitch = 0.72f;
                    orbitDist = 340.0f;
                    orbitTarget = layout.parkCenter();
                    preset = CamPreset::Overview;
                    applyPreset(camera, preset, layout);
                } else if (key->code == K::LBracket) {
                    orbitDist = std::min(900.0f, orbitDist + 18.0f);
                    preset = CamPreset::Free;
                    orbitTarget = layout.parkCenter();
                    updateOrbitCamera(camera, orbitYaw, orbitPitch, orbitDist, orbitTarget);
                } else if (key->code == K::RBracket) {
                    orbitDist = std::max(50.0f, orbitDist - 18.0f);
                    preset = CamPreset::Free;
                    orbitTarget = layout.parkCenter();
                    updateOrbitCamera(camera, orbitYaw, orbitPitch, orbitDist, orbitTarget);
                } else if (key->code == K::G) {
                    showLabels = !showLabels;
                }
            }
            if (const auto* m = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (m->button == sf::Mouse::Button::Left) {
                    dragging = true;
                    lastMouse = sf::Mouse::getPosition(window);
                    if (preset != CamPreset::Free) {
                        orbitTarget = layout.parkCenter();
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
                orbitTarget = layout.parkCenter();
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
            orbitTarget = layout.parkCenter();
            updateOrbitCamera(camera, orbitYaw, orbitPitch, orbitDist, orbitTarget);
        }

        if (useGL) {
            // Blank slate: sky + ground plane only. No stadium model.
            gl.beginFrame(window, camera, Stadium3D::skyColor());
            const float gr = layout.maxWallR() + 80.0f;
            const float plateZ = layout.plateZ();
            gl.drawGround(gr, plateZ - gr, plateZ + gr, Stadium3D::concreteFloorColor());
            gl.endFrame(window);
        } else {
            window.clear(Stadium3D::skyColor());
        }

        if (fontOk) {
            drawText(
                window,
                font,
                "Stadium Demo | BLANK — model removed, ready to rebuild",
                20,
                {22, 14},
                sf::Color(240, 245, 250)
            );
            std::ostringstream info;
            info << "View: " << presetName(preset)
                 << "   LF " << static_cast<int>(layout.wallFeetAtAngle(-layout.foulAngleRad()))
                 << " / CF " << static_cast<int>(layout.wallFeetAtAngle(0.0f))
                 << " / RF " << static_cast<int>(layout.wallFeetAtAngle(layout.foulAngleRad()))
                 << " ft   zoom " << static_cast<int>(orbitDist);
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
                projectLabel(layout.home() + Vector3(0, 1.2f, 0), "HOME", sf::Color(255, 240, 200));
                projectLabel(layout.mound() + Vector3(0, 2.0f, 0), "MOUND", sf::Color(255, 220, 160));
                projectLabel(
                    layout.wallPoint(0.0f, 1.0f) + Vector3(0, 2, 0),
                    "CF 401",
                    sf::Color(180, 220, 255)
                );
                projectLabel(
                    layout.wallPoint(-layout.foulAngleRad(), 1.0f) + Vector3(0, 2, 0),
                    "LF 329",
                    sf::Color(255, 230, 100)
                );
                projectLabel(
                    layout.wallPoint(layout.foulAngleRad(), 1.0f) + Vector3(0, 2, 0),
                    "RF 330",
                    sf::Color(255, 230, 100)
                );
                projectLabel(
                    layout.wallPoint(-4.0f * 3.14159265f / 180.0f, 1.0f) + Vector3(0, 2, 0),
                    "404 DEEP",
                    sf::Color(255, 180, 120)
                );
            }
        }

        fps.frame(window);
        window.display();
    }

    return 0;
}
