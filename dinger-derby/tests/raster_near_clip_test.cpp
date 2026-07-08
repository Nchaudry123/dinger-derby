#include <cassert>

#include "RasterDemo3D.h"
#include "../src/rendering/Camera3D.h"
#include "../src/rendering/FrameBuffer.h"
#include "../src/rendering/Mesh3D.h"

namespace {

void testTriangleCrossingNearPlaneStillRenders() {
    FrameBuffer buffer(120, 90);
    buffer.clear(sf::Color::Black);
    buffer.clearDepth(100.0f);

    Camera3D camera;
    Mesh3D mesh;
    mesh.vertices = {
        Vector3(-0.25f, -0.18f, -5.82f),
        Vector3(0.25f, -0.18f, -5.82f),
        Vector3(0.0f, 0.22f, -5.96f)
    };
    mesh.triangles = {{0, 1, 2}};
    mesh.triangleColors = {sf::Color::Red};

    RasterMeshRenderCache cache;
    RasterRenderStats stats;

    rasterizeMeshTriangles(
        buffer,
        camera,
        mesh,
        Matrix4::identity(),
        sf::Color::Red,
        cache,
        false,
        false,
        &stats
    );

    assert(stats.trianglesSubmitted == 1);
    assert(stats.trianglesDrawn == 1);

    bool foundRedPixel = false;
    for (int y = 0; y < buffer.getHeight(); y++) {
        for (int x = 0; x < buffer.getWidth(); x++) {
            sf::Color color = buffer.getPixelColor(x, y);
            if (color.r > 0) {
                foundRedPixel = true;
            }
        }
    }

    assert(foundRedPixel);
}

void testTriangleBehindNearPlaneIsSkipped() {
    FrameBuffer buffer(120, 90);
    buffer.clear(sf::Color::Black);
    buffer.clearDepth(100.0f);

    Camera3D camera;
    Mesh3D mesh;
    mesh.vertices = {
        Vector3(-0.25f, -0.18f, -5.96f),
        Vector3(0.25f, -0.18f, -5.96f),
        Vector3(0.0f, 0.22f, -5.94f)
    };
    mesh.triangles = {{0, 1, 2}};

    RasterMeshRenderCache cache;
    RasterRenderStats stats;

    rasterizeMeshTriangles(
        buffer,
        camera,
        mesh,
        Matrix4::identity(),
        sf::Color::Green,
        cache,
        false,
        false,
        &stats
    );

    assert(stats.trianglesSubmitted == 1);
    assert(stats.trianglesDrawn == 0);
    assert(stats.trianglesSkipped == 1);
}

void testTriangleCrossingScreenEdgeStillRenders() {
    FrameBuffer buffer(120, 90);
    buffer.clear(sf::Color::Black);
    buffer.clearDepth(100.0f);

    Camera3D camera;
    Mesh3D mesh;
    mesh.vertices = {
        Vector3(-3.0f, -0.25f, -3.0f),
        Vector3(0.2f, -0.25f, -3.0f),
        Vector3(0.2f, 0.25f, -3.0f)
    };
    mesh.triangles = {{0, 1, 2}};

    RasterMeshRenderCache cache;
    RasterRenderStats stats;

    rasterizeMeshTriangles(
        buffer,
        camera,
        mesh,
        Matrix4::identity(),
        sf::Color::Blue,
        cache,
        false,
        false,
        &stats
    );

    assert(stats.trianglesSubmitted == 1);
    assert(stats.trianglesDrawn >= 1);
}

void testTriangleOutsideScreenIsSkippedAfterClipping() {
    FrameBuffer buffer(120, 90);
    buffer.clear(sf::Color::Black);
    buffer.clearDepth(100.0f);

    Camera3D camera;
    Mesh3D mesh;
    mesh.vertices = {
        Vector3(-8.0f, -0.25f, -3.0f),
        Vector3(-7.0f, -0.25f, -3.0f),
        Vector3(-7.0f, 0.25f, -3.0f)
    };
    mesh.triangles = {{0, 1, 2}};

    RasterMeshRenderCache cache;
    RasterRenderStats stats;

    rasterizeMeshTriangles(
        buffer,
        camera,
        mesh,
        Matrix4::identity(),
        sf::Color::Blue,
        cache,
        false,
        false,
        &stats
    );

    assert(stats.trianglesSubmitted == 1);
    assert(stats.trianglesDrawn == 0);
    assert(stats.trianglesSkipped == 1);
}

}

int main() {
    testTriangleCrossingNearPlaneStillRenders();
    testTriangleBehindNearPlaneIsSkipped();
    testTriangleCrossingScreenEdgeStillRenders();
    testTriangleOutsideScreenIsSkippedAfterClipping();

    return 0;
}
