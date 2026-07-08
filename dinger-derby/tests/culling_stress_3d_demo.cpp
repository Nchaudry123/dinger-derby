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
#include "../src/rendering/SpatialGrid3D.h"

namespace {

struct CubeInstance {
    Vector3 position;
    float scale = 1.0f;
    float spin = 0.0f;
};

struct StressDemoStats {
    RasterRenderStats render;
    int gridCandidates = 0;
    int gridSkipped = 0;
    int gridCells = 0;
    int gridReferences = 0;

    void resetFrame() {
        render.reset();
        gridCandidates = 0;
        gridSkipped = 0;
    }
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

void rebuildSpatialGrid(
    SpatialGrid3D& grid,
    const std::vector<CubeInstance>& cubes,
    float meshRadius
) {
    grid.clear();

    for (int i = 0; i < cubes.size(); i++) {
        const CubeInstance& cube = cubes[i];
        grid.insertSphere(i, cube.position, meshRadius * cube.scale);
    }
}

float maxCubeRadius(const std::vector<CubeInstance>& cubes, float meshRadius) {
    float radius = 0.0f;

    for (const CubeInstance& cube : cubes) {
        radius = std::max(radius, meshRadius * cube.scale);
    }

    return radius;
}

void queryGridForCameraRegion(
    SpatialGrid3D& grid,
    const Camera3D& camera,
    const FrameBuffer& frameBuffer,
    const Vector3& fieldOffset,
    float maxRadius,
    std::vector<int>& candidateIds
) {
    const float maxGridDistance = 42.0f;
    float farWorldZ = camera.position.z + maxGridDistance + maxRadius;
    float farDepth = farWorldZ - camera.position.z;
    float halfWidth = frameBuffer.getWidth() * 0.5f * farDepth / camera.fieldOfView + maxRadius;
    float halfHeight = frameBuffer.getHeight() * 0.5f * farDepth / camera.fieldOfView + maxRadius;

    Vector3 worldMinimum(
        camera.position.x - halfWidth,
        camera.position.y - halfHeight,
        camera.position.z + camera.nearPlane - maxRadius
    );
    Vector3 worldMaximum(
        camera.position.x + halfWidth,
        camera.position.y + halfHeight,
        farWorldZ
    );

    grid.queryAabb(worldMinimum - fieldOffset, worldMaximum - fieldOffset, candidateIds);
}

std::string titleFor(
    const StressDemoStats& stats,
    bool gridEnabled,
    bool cullingEnabled,
    bool antiAliasingEnabled,
    int cubeCount
) {
    std::ostringstream stream;
    stream
        << "3D Culling Stress"
        << " | cubes: " << cubeCount
        << " | grid: " << (gridEnabled ? "on" : "off")
        << " " << stats.gridCandidates << "/" << cubeCount
        << " | skipped: " << stats.gridSkipped
        << " | cells: " << stats.gridCells
        << " | visible: " << stats.render.objectsVisible
        << " | culled: " << stats.render.objectsCulled
        << " | tris: " << stats.render.trianglesDrawn << "/" << stats.render.trianglesSubmitted
        << " | cull: " << (cullingEnabled ? "on" : "off")
        << " | AA: " << (antiAliasingEnabled ? "on" : "off");
    return stream.str();
}

}

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1280, 720)),
        "3D Culling Stress | G: grid | C: cull | A: AA | +/-: cube count"
    );
    window.setFramerateLimit(60);

    bool antiAliasingEnabled = false;
    bool gridEnabled = true;
    bool cullingEnabled = true;
    int gridRadius = 9;
    std::vector<CubeInstance> cubes = makeCubeField(gridRadius);
    Rasterizer3D::setAntiAliasingEnabled(antiAliasingEnabled);

    StressDemoStats stats;
    DemoFpsCounter fpsCounter(titleFor(stats, gridEnabled, cullingEnabled, antiAliasingEnabled, cubes.size()));

    sf::Vector2u rasterSize = rasterSizeForWindow(window.getSize());
    FrameBuffer frameBuffer(rasterSize.x, rasterSize.y);
    Camera3D camera;
    Mesh3D cubeMesh = Mesh3D::cube();
    BoundingSphere3D cubeSphere = cubeMesh.localBoundingSphere();
    SpatialGrid3D spatialGrid(8.0f);
    std::vector<int> candidateIds;
    float maxFieldCubeRadius = maxCubeRadius(cubes, cubeSphere.radius);
    rebuildSpatialGrid(spatialGrid, cubes, cubeSphere.radius);
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

                if (key->code == sf::Keyboard::Key::G) {
                    gridEnabled = !gridEnabled;
                }

                if (key->code == sf::Keyboard::Key::C) {
                    cullingEnabled = !cullingEnabled;
                }

                if (key->code == sf::Keyboard::Key::Equal) {
                    gridRadius = std::min(gridRadius + 2, 19);
                    cubes = makeCubeField(gridRadius);
                    maxFieldCubeRadius = maxCubeRadius(cubes, cubeSphere.radius);
                    rebuildSpatialGrid(spatialGrid, cubes, cubeSphere.radius);
                }

                if (key->code == sf::Keyboard::Key::Hyphen) {
                    gridRadius = std::max(gridRadius - 2, 3);
                    cubes = makeCubeField(gridRadius);
                    maxFieldCubeRadius = maxCubeRadius(cubes, cubeSphere.radius);
                    rebuildSpatialGrid(spatialGrid, cubes, cubeSphere.radius);
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
        stats.resetFrame();
        stats.gridCells = spatialGrid.getCellCount();
        stats.gridReferences = spatialGrid.getObjectReferenceCount();

        frameBuffer.clear(sf::Color(5, 7, 12));
        frameBuffer.clearDepth(std::numeric_limits<float>::infinity());

        Vector3 fieldOffset(fieldOffsetX, 0.0f, fieldOffsetZ);

        if (gridEnabled) {
            queryGridForCameraRegion(
                spatialGrid,
                camera,
                frameBuffer,
                fieldOffset,
                maxFieldCubeRadius,
                candidateIds
            );
            stats.gridCandidates = static_cast<int>(candidateIds.size());
            stats.gridSkipped = static_cast<int>(cubes.size()) - stats.gridCandidates;
        } else {
            candidateIds.clear();
            candidateIds.reserve(cubes.size());

            for (int i = 0; i < cubes.size(); i++) {
                candidateIds.push_back(i);
            }

            stats.gridCandidates = static_cast<int>(candidateIds.size());
            stats.gridSkipped = 0;
        }

        for (int cubeId : candidateIds) {
            const CubeInstance& cube = cubes[cubeId];
            Matrix4 transform =
                Matrix4::translation(cube.position + fieldOffset) *
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
                &stats.render
            );
        }

        fpsCounter.setTitle(titleFor(stats, gridEnabled, cullingEnabled, antiAliasingEnabled, cubes.size()));

        window.clear();
        frameBuffer.present(window);
        fpsCounter.frame(window);
        window.display();
    }

    return 0;
}
