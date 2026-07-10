// Character model workshop — orbit, scrub, and compare poses of the new
// CharacterModel3D athlete. Tune the model here before wiring it into the
// pitching simulator.
//
// Controls:
//   Mouse drag        orbit camera
//   Scroll / [ ]      zoom
//   A / D             yaw model
//   W / S             pitch model (tilt)
//   Left / Right      previous / next clip
//   1..8              jump to clip
//   Space             play / pause
//   , / .             scrub −/+ 0.05
//   R                 reset camera + model orientation
//   G                 toggle skeleton overlay
//   B                 toggle ground
//   P / C / T         Pitcher / Catcher / aThlete role
//   Q                 cycle detail Low→Med→High (rebuilds mesh)
//   Tab               auto-rotate model
//   Esc               quit

#include <SFML/Graphics.hpp>
#include <SFML/Window/ContextSettings.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "rendering/Camera3D.h"
#include "rendering/CharacterModel3D.h"
#include "rendering/FrameBuffer.h"
#include "rendering/GlRenderer.h"
#include "rendering/SkeletonAnimator.h"
#include "rendering/SkinnedModel3D.h"
#include "DemoFpsCounter.h"
#include "RasterDemo3D.h"

#include <limits>
#include <optional>

namespace {

constexpr float kPi = 3.14159265f;

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

void lookAt(Camera3D& camera, const Vector3& position, const Vector3& target) {
    camera.position = position;
    Vector3 toTarget = target - position;
    float horizontal = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    camera.rotation.y = std::atan2(toTarget.x, toTarget.z);
    camera.rotation.x = -std::atan2(toTarget.y, horizontal);
    camera.rotation.z = 0.0f;
}

struct OrbitCam {
    // Model faces +Z; closer 3/4 front view for fine-tuning.
    float yaw = 0.40f;
    float pitch = 0.12f;
    float distance = 2.6f;
    Vector3 target{0.0f, 0.95f, 0.05f};

    void apply(Camera3D& camera) const {
        float cp = std::cos(pitch);
        Vector3 pos(
            target.x + std::sin(yaw) * cp * distance,
            target.y + std::sin(pitch) * distance,
            target.z + std::cos(yaw) * cp * distance
        );
        lookAt(camera, pos, target);
        camera.fieldOfView = 720.0f;
        camera.nearPlane = 0.08f;
    }
};

void drawSkeletonOverlay(
    sf::RenderWindow& window,
    const Camera3D& camera,
    const SkinnedModel3D& model,
    const SkeletonAnimator& anim,
    const Matrix4& world
) {
    auto project = [&](const Vector3& worldPt) -> sf::Vector2f {
        ProjectedPoint3D p = camera.projectPoint(
            worldPt,
            static_cast<float>(window.getSize().x),
            static_cast<float>(window.getSize().y)
        );
        return sf::Vector2f(p.position.x, p.position.y);
    };

    const auto& globals = anim.globalPose();
    for (int i = 0; i < static_cast<int>(model.joints.size()); i++) {
        Vector3 a = world.transformPoint(globals[i].transformPoint(Vector3()));
        int parent = model.joints[i].parent;
        if (parent >= 0 && parent < static_cast<int>(model.joints.size())) {
            Vector3 b = world.transformPoint(globals[parent].transformPoint(Vector3()));
            sf::Vertex line[] = {
                sf::Vertex{project(a), sf::Color(80, 220, 255, 200)},
                sf::Vertex{project(b), sf::Color(80, 220, 255, 120)}
            };
            window.draw(line, 2, sf::PrimitiveType::Lines);
        }
        sf::CircleShape dot(3.0f);
        dot.setOrigin(sf::Vector2f(3.0f, 3.0f));
        sf::Vector2f sp = project(a);
        dot.setPosition(sp);
        const std::string& jn = model.joints[i].name;
        bool isArm = jn.find("Shoulder") != std::string::npos ||
                     jn.find("UpperArm") != std::string::npos ||
                     jn.find("HumTwist") != std::string::npos ||
                     jn.find("Elbow") != std::string::npos ||
                     jn.find("Forearm") != std::string::npos ||
                     jn.find("ProTwist") != std::string::npos ||
                     jn.find("Wrist") != std::string::npos ||
                     jn.find("Clavicle") != std::string::npos ||
                     jn.find("Palm") != std::string::npos ||
                     jn.find("Index") != std::string::npos ||
                     jn.find("Middle") != std::string::npos ||
                     jn.find("Ring") != std::string::npos ||
                     jn.find("Pinky") != std::string::npos ||
                     jn.find("Thumb") != std::string::npos;
        dot.setFillColor(isArm ? sf::Color(255, 180, 60) : sf::Color(120, 255, 180));
        window.draw(dot);
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string startClip;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if ((a == "--clip" || a == "-c") && i + 1 < argc) {
            startClip = argv[++i];
        }
    }

    sf::ContextSettings glSettings;
    glSettings.depthBits = 24;
    glSettings.stencilBits = 8;
    glSettings.antiAliasingLevel = 4;

    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 800)),
        "Character Viewer | new CharacterModel3D",
        sf::Style::Default,
        sf::State::Windowed,
        glSettings
    );
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    DemoFpsCounter fps("Character Viewer");
    sf::Font font;
    bool fontLoaded = loadUiFont(font);

    CharacterModel3D::Role role = CharacterModel3D::Role::Pitcher;
    CharacterModel3D::Detail detail = CharacterModel3D::Detail::High;

    SkinnedModel3D model = CharacterModel3D::build(role, detail);
    CharacterModel3D::BuildInfo info = CharacterModel3D::inspect(model);
    std::vector<std::string> clips = CharacterModel3D::clipNames(model);
    int clipIndex = 0;
    auto pickClip = [&](const std::string& name) {
        for (int i = 0; i < static_cast<int>(clips.size()); i++) {
            if (clips[static_cast<size_t>(i)] == name) {
                clipIndex = i;
                return true;
            }
        }
        return false;
    };
    if (startClip.empty() || !pickClip(startClip)) {
        // Prefer natural standing rest (straight arms) for first look.
        if (!pickClip("rest")) {
            clipIndex = 0;
        }
    }

    SkeletonAnimator anim;
    anim.setModel(model);
    anim.resetToRest();

    Mesh3D posed = model.skinToMesh(anim.skinMatrices());
    GlRenderer gl;
    bool useOpenGL = gl.initialize(window);
    GlMesh glMesh;
    if (useOpenGL) {
        glMesh.upload(posed);
        std::cerr << "Viewer: OpenGL path\n";
    } else {
        std::cerr << "Viewer: software raster path\n";
    }

    FrameBuffer frameBuffer(window.getSize().x, window.getSize().y);
    Camera3D camera;
    OrbitCam orbit;
    orbit.apply(camera);

    float modelYaw = 0.0f;
    float modelPitch = 0.0f;
    bool playing = true;
    bool showSkeleton = false; // mesh first; press G for bones
    bool showGround = true;
    bool autoRotate = false;
    float animTime = 0.0f;
    float animSpeed = 1.0f;

    bool dragging = false;
    sf::Vector2i lastMouse;

    auto rebuildModel = [&]() {
        model = CharacterModel3D::build(role, detail);
        info = CharacterModel3D::inspect(model);
        clips = CharacterModel3D::clipNames(model);
        clipIndex = std::clamp(clipIndex, 0, std::max(0, static_cast<int>(clips.size()) - 1));
        anim.setModel(model);
        anim.resetToRest();
        animTime = 0.0f;
        if (!clips.empty()) {
            if (const AnimationClip* c = model.findClip(clips[static_cast<size_t>(clipIndex)])) {
                anim.applyClip(*c, 0.0f, true);
            }
        }
        posed = model.skinToMesh(anim.skinMatrices());
        if (useOpenGL) {
            glMesh.upload(posed);
        }
        std::cerr << "Rebuilt " << CharacterModel3D::roleName(role)
                  << " detail=" << CharacterModel3D::detailName(detail)
                  << " | " << info.summary << "\n";
    };

    auto applyCurrentPose = [&]() {
        if (clips.empty()) {
            anim.resetToRest();
        } else if (const AnimationClip* c = model.findClip(clips[static_cast<size_t>(clipIndex)])) {
            float t = animTime;
            if (c->duration > 1e-4f) {
                t = std::fmod(std::max(animTime, 0.0f), c->duration);
                if (t < 0.0f) {
                    t += c->duration;
                }
            }
            anim.applyClip(*c, t, true);
        }
        model.skinInto(anim.skinMatrices(), posed);
        if (useOpenGL) {
            glMesh.updatePositionsNormals(posed);
        }
    };

    applyCurrentPose();
    std::cerr << "Character ready: " << info.summary << "\n";
    std::cerr << "Clips:";
    for (const auto& n : clips) {
        std::cerr << " " << n;
    }
    std::cerr << "\n";

    sf::Clock clock;
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        dt = std::min(dt, 0.05f);

        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect(
                    sf::Vector2f(0.0f, 0.0f),
                    sf::Vector2f(static_cast<float>(resized->size.x), static_cast<float>(resized->size.y))
                )));
                frameBuffer.resize(static_cast<int>(resized->size.x), static_cast<int>(resized->size.y));
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                using K = sf::Keyboard::Key;
                if (key->code == K::Escape) {
                    window.close();
                } else if (key->code == K::Space) {
                    playing = !playing;
                } else if (key->code == K::G) {
                    showSkeleton = !showSkeleton;
                } else if (key->code == K::B) {
                    showGround = !showGround;
                } else if (key->code == K::Tab) {
                    autoRotate = !autoRotate;
                } else if (key->code == K::R) {
                    orbit = OrbitCam{};
                    modelYaw = 0.0f;
                    modelPitch = 0.0f;
                    orbit.apply(camera);
                } else if (key->code == K::P) {
                    role = CharacterModel3D::Role::Pitcher;
                    rebuildModel();
                } else if (key->code == K::C) {
                    role = CharacterModel3D::Role::Catcher;
                    rebuildModel();
                } else if (key->code == K::T) {
                    role = CharacterModel3D::Role::Athlete;
                    rebuildModel();
                } else if (key->code == K::Q) {
                    int d = (static_cast<int>(detail) + 1) % 3;
                    detail = static_cast<CharacterModel3D::Detail>(d);
                    rebuildModel();
                } else if (key->code == K::Left) {
                    if (!clips.empty()) {
                        clipIndex = (clipIndex + static_cast<int>(clips.size()) - 1) %
                                    static_cast<int>(clips.size());
                        animTime = 0.0f;
                        applyCurrentPose();
                    }
                } else if (key->code == K::Right) {
                    if (!clips.empty()) {
                        clipIndex = (clipIndex + 1) % static_cast<int>(clips.size());
                        animTime = 0.0f;
                        applyCurrentPose();
                    }
                } else if (key->code == K::Comma) {
                    animTime = std::max(0.0f, animTime - 0.05f);
                    playing = false;
                    applyCurrentPose();
                } else if (key->code == K::Period) {
                    animTime += 0.05f;
                    playing = false;
                    applyCurrentPose();
                } else if (key->code >= K::Num1 && key->code <= K::Num9) {
                    int idx = static_cast<int>(key->code) - static_cast<int>(K::Num1);
                    if (idx < static_cast<int>(clips.size())) {
                        clipIndex = idx;
                        animTime = 0.0f;
                        applyCurrentPose();
                    }
                }
            } else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    dragging = true;
                    lastMouse = sf::Mouse::getPosition(window);
                }
            } else if (const auto* mr = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mr->button == sf::Mouse::Button::Left) {
                    dragging = false;
                }
            } else if (const auto* move = event->getIf<sf::Event::MouseMoved>()) {
                if (dragging) {
                    sf::Vector2i pos = move->position;
                    float dx = static_cast<float>(pos.x - lastMouse.x);
                    float dy = static_cast<float>(pos.y - lastMouse.y);
                    orbit.yaw -= dx * 0.007f;
                    orbit.pitch += dy * 0.007f;
                    orbit.pitch = std::clamp(orbit.pitch, -1.2f, 1.35f);
                    lastMouse = pos;
                    orbit.apply(camera);
                }
            } else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                orbit.distance *= (wheel->delta > 0.0f) ? 0.90f : 1.11f;
                orbit.distance = std::clamp(orbit.distance, 1.2f, 12.0f);
                orbit.apply(camera);
            }
        }

        // Held keys for continuous rotate / zoom
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
            modelYaw += dt * 1.4f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
            modelYaw -= dt * 1.4f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
            modelPitch += dt * 1.0f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
            modelPitch -= dt * 1.0f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LBracket)) {
            orbit.distance = std::min(12.0f, orbit.distance * (1.0f + dt * 0.8f));
            orbit.apply(camera);
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RBracket)) {
            orbit.distance = std::max(1.2f, orbit.distance * (1.0f - dt * 0.8f));
            orbit.apply(camera);
        }

        if (autoRotate) {
            modelYaw += dt * 0.55f;
        }

        if (playing && !clips.empty()) {
            if (const AnimationClip* c = model.findClip(clips[static_cast<size_t>(clipIndex)])) {
                animTime += dt * animSpeed;
                if (c->duration > 1e-4f && animTime > c->duration) {
                    animTime = std::fmod(animTime, c->duration);
                }
                applyCurrentPose();
            }
        }

        Matrix4 modelXform =
            Matrix4::rotationY(modelYaw) *
            Matrix4::rotationX(modelPitch);

        // ── render ──────────────────────────────────────────────────────
        if (useOpenGL) {
            gl.beginFrame(window, camera, sf::Color(12, 16, 24));
            if (showGround) {
                gl.drawGround(3.5f, -2.0f, 4.0f, sf::Color(28, 36, 32));
            }
            gl.drawMesh(glMesh, modelXform);
            gl.endFrame(window);
        } else {
            frameBuffer.clear(sf::Color(12, 16, 24));
            frameBuffer.clearDepth(std::numeric_limits<float>::infinity());
            RasterMeshRenderCache cache;
            cache.reserveFor(posed);
            rasterizeMeshTriangles(
                frameBuffer, camera, posed, modelXform,
                sf::Color(230, 230, 235), cache
            );
            window.clear();
            frameBuffer.present(window);
        }

        if (showSkeleton) {
            drawSkeletonOverlay(window, camera, model, anim, modelXform);
        }

        // ── HUD ─────────────────────────────────────────────────────────
        if (fontLoaded) {
            sf::RectangleShape panel(sf::Vector2f(430.0f, 250.0f));
            panel.setPosition(sf::Vector2f(16.0f, 16.0f));
            panel.setFillColor(sf::Color(6, 10, 16, 210));
            panel.setOutlineThickness(1.0f);
            panel.setOutlineColor(sf::Color(80, 180, 190, 140));
            window.draw(panel);

            const AnimationClip* cur = clips.empty()
                ? nullptr
                : model.findClip(clips[static_cast<size_t>(clipIndex)]);
            float dur = cur ? cur->duration : 0.0f;
            float tShow = dur > 1e-4f ? std::fmod(animTime, dur) : animTime;

            std::ostringstream title;
            title << "CharacterModel3D  ·  " << CharacterModel3D::roleName(role)
                  << "  ·  " << CharacterModel3D::detailName(detail);
            drawText(window, font, title.str(), 18, {28, 28}, sf::Color(230, 245, 250));
            drawText(window, font, info.summary, 13, {28, 54}, sf::Color(160, 190, 200));

            std::ostringstream clipLine;
            clipLine << "Clip [" << (clipIndex + 1) << "/" << clips.size() << "]  "
                     << (clips.empty() ? "(none)" : clips[static_cast<size_t>(clipIndex)])
                     << (playing ? "  ▶" : "  ❚❚")
                     << "  t=" << std::fixed;
            clipLine.precision(2);
            clipLine << tShow << " / " << dur << "s";
            drawText(window, font, clipLine.str(), 14, {28, 78}, sf::Color(255, 220, 140));

            // Timeline bar
            sf::RectangleShape track(sf::Vector2f(380.0f, 8.0f));
            track.setPosition(sf::Vector2f(28.0f, 104.0f));
            track.setFillColor(sf::Color(40, 55, 70));
            window.draw(track);
            float frac = (dur > 1e-4f) ? std::clamp(tShow / dur, 0.0f, 1.0f) : 0.0f;
            sf::RectangleShape fill(sf::Vector2f(380.0f * frac, 8.0f));
            fill.setPosition(sf::Vector2f(28.0f, 104.0f));
            fill.setFillColor(sf::Color(90, 210, 200));
            window.draw(fill);

            drawText(
                window, font,
                "Drag orbit  Scroll zoom  A/D yaw  W/S tilt  Tab auto-spin",
                12, {28, 124}, sf::Color(150, 170, 185)
            );
            drawText(
                window, font,
                "←/→ clip  1-7 jump  Space play  ,/. scrub  G bones  B ground",
                12, {28, 144}, sf::Color(150, 170, 185)
            );
            drawText(
                window, font,
                "P pitcher  C catcher  T athlete  Q detail  R reset cam",
                12, {28, 164}, sf::Color(150, 170, 185)
            );

            std::ostringstream flags;
            flags << "skeleton " << (showSkeleton ? "ON" : "off")
                  << "   ground " << (showGround ? "ON" : "off")
                  << "   auto " << (autoRotate ? "ON" : "off")
                  << "   " << (useOpenGL ? "OpenGL" : "software");
            drawText(window, font, flags.str(), 12, {28, 190}, sf::Color(120, 200, 160));

            // Clip list
            std::ostringstream list;
            list << "Clips: ";
            for (int i = 0; i < static_cast<int>(clips.size()); i++) {
                if (i == clipIndex) {
                    list << "[" << clips[static_cast<size_t>(i)] << "] ";
                } else {
                    list << clips[static_cast<size_t>(i)] << " ";
                }
            }
            drawText(window, font, list.str(), 11, {28, 214}, sf::Color(170, 180, 200));

            // Help corner
            drawText(
                window, font,
                "Fine-tune CharacterModel3D.cpp then press Q/P/C/T to rebuild.",
                12,
                {16.0f, static_cast<float>(window.getSize().y) - 28.0f},
                sf::Color(130, 150, 165)
            );
        }

        fps.frame(window);
        window.display();
    }

    return 0;
}
