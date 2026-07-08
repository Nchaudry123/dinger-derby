#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <vector>

#include "DemoFpsCounter.h"
#include "RasterDemo3D.h"
#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"
#include "../src/rendering/Camera3D.h"
#include "../src/rendering/FrameBuffer.h"
#include "../src/rendering/Mesh3D.h"
#include "../src/rendering/Rasterizer3D.h"

namespace {

struct CubeInstance {
    Vector3 position;
    float scale = 1.0f;
    float spin = 0.0f;
};

std::vector<CubeInstance> makeCubeField(int gridRadius) {
    std::vector<CubeInstance> cubes;
    cubes.reserve((gridRadius * 2 + 1) * (gridRadius * 2 + 1) * 3);

    for (int z = 0; z < gridRadius * 2 + 1; z++) {
        for (int x = -gridRadius; x <= gridRadius; x++) {
            for (int y = -1; y <= 1; y++) {
                CubeInstance cube;
                cube.position = Vector3(
                    x * 2.25f,
                    y * 2.1f + std::sin((x + z) * 0.6f) * 0.45f,
                    z * 2.35f
                );
                cube.scale = 0.42f + ((x + y + z) & 3) * 0.035f;
                cube.spin = (x * 0.17f + y * 0.31f + z * 0.11f);
                cubes.push_back(cube);
            }
        }
    }

    return cubes;
}

std::string titleFor(
    const RasterRenderStats& stats,
    bool cullingEnabled,
    bool antiAliasingEnabled,
    int cubeCount
) {
    std::ostringstream stream;
    stream
        << "3D Culling Stress"
        << " | cubes: " << cubeCount
        << " | visible: " << stats.objectsVisible
        << " | culled: " << stats.objectsCulled
        << " | tris: " << stats.trianglesDrawn << "/" << stats.trianglesSubmitted
        << " | cull: " << (cullingEnabled ? "on" : "off")
        << " | AA: " << (antiAliasingEnabled ? "on" : "off");
    return stream.str();
}

}

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Culling Stress | C: cull | A: AA | +/-: cube count"
    );
    window.setFramerateLimit(60);

    bool antiAliasingEnabled = false;
    bool cullingEnabled = true;
    int gridRadius = 9;
    std::vector<CubeInstance> cubes = makeCubeField(gridRadius);
    Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);

    RasterRenderStats stats;
    DemoFpsCounter fpsCounter(titleFor(stats, cullingEnabled, antiAliasingEnabled, cubes.size()));

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    Mesh3D cubeMesh = Mesh3D::cube();
    RasterMeshRenderCache renderCache;
    sf::Clock clock;
    float fieldOffsetX = 0.0f;
    float fieldOffsetZ = 2.5f;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::A) {
                    antiAliasingEnabled = !antiAliasingEnabled;
                    Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);
                }

                if (key->code == sf::Keyboard::Key::C) {
                    cullingEnabled = !cullingEnabled;
                }

                if (key->code == sf::Keyboard::Key::Equal) {
                    gridRadius = std::min(gridRadius + 2, 19);
                    cubes = makeCubeField(gridRadius);
                }

                if (key->code == sf::Keyboard::Key::Hyphen) {
                    gridRadius = std::max(gridRadius - 2, 3);
                    cubes = makeCubeField(gridRadius);
                }
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect(
                    sf::Vector2f(0.0f, 0.0f),
                    sf::Vector2f(resized->size.x, resized->size.y)
                )));
                rasterSize = rasterSizeForWindow(resized->size);
                frameBuffer.resize(rasterSize.x, rasterSize.y);
            }
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) {
            fieldOffsetX += 0.12f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) {
            fieldOffsetX -= 0.12f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) {
            fieldOffsetZ -= 0.12f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) {
            fieldOffsetZ += 0.12f;
        }

        float time = clock.getElapsedTime().asSeconds();
        stats.reset();

        frameBuffer.clear(sf::Color(5, 7, 12));
        frameBuffer.clearDepth(std::numeric_limits<float>::infinity());

        for (const CubeInstance& cube : cubes) {
            Matrix4 transform =
                Matrix4::translation(cube.position + Vector3(fieldOffsetX, 0.0f, fieldOffsetZ)) *
                Matrix4::rotationY(time * 0.55f + cube.spin) *
                Matrix4::rotationX(time * 0.25f + cube.spin * 0.5f) *
                Matrix4::scale(Vector3(cube.scale, cube.scale, cube.scale));

            rasterizeMeshTriangles(
                frameBuffer,
                camera,
                cubeMesh,
                transform,
                sf::Color(110, 190, 230),
                renderCache,
                true,
                cullingEnabled,
                &stats
            );
        }

        fpsCounter.setTitle(titleFor(stats, cullingEnabled, antiAliasingEnabled, cubes.size()));

        window.clear();
        frameBuffer.present(window);
        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
