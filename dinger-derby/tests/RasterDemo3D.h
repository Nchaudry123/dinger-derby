#pragma once

#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "rendering/Camera3D.h"
#include "rendering/FrameBuffer.h"
#include "rendering/Mesh3D.h"
#include "rendering/Rasterizer3D.h"
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <array>
#include <vector>

// renderScale: 1.0 = native window resolution. Prefer >= 1.0 when AA is on;
// values below 1.0 upscale and reintroduce stair-stepping that undoes coverage AA.
inline sf::Vector2u rasterSizeForWindow(sf::Vector2u windowSize, float renderScale = 1.0f) {
    renderScale = std::max(renderScale, 0.25f);

    return sf::Vector2u(
        std::max(1u, static_cast<unsigned int>(windowSize.x * renderScale + 0.5f)),
        std::max(1u, static_cast<unsigned int>(windowSize.y * renderScale + 0.5f))
    );
}

// fullQuality: 2x supersample (best edges). Otherwise native 1x, or 1.25x if only AA is on.
inline float rasterScaleForQuality(bool fullQuality, bool antiAliasingEnabled = true) {
    if (fullQuality) {
        return 2.0f;
    }

    return antiAliasingEnabled ? 1.25f : 1.0f;
}

inline float rasterScaleForAntiAliasing(bool antiAliasingEnabled) {
    return rasterScaleForQuality(false, antiAliasingEnabled);
}

struct RasterMeshRenderCache {
    std::vector<Vector3> worldVertices;
    std::vector<Vector3> cameraVertices;

    void reserveFor(const Mesh3D& mesh) {
        worldVertices.clear();
        cameraVertices.clear();

        worldVertices.reserve(mesh.vertices.size());
        cameraVertices.reserve(mesh.vertices.size());
    }
};

struct RasterRenderStats {
    int objectsSubmitted = 0;
    int objectsVisible = 0;
    int objectsCulled = 0;
    int trianglesSubmitted = 0;
    int trianglesDrawn = 0;
    int trianglesSkipped = 0;

    void reset() {
        objectsSubmitted = 0;
        objectsVisible = 0;
        objectsCulled = 0;
        trianglesSubmitted = 0;
        trianglesDrawn = 0;
        trianglesSkipped = 0;
    }
};

inline Vector3 projectCameraPointToScreen(
    const Vector3& cameraPoint,
    const Camera3D& camera,
    const FrameBuffer& frameBuffer
) {
    float scale = camera.fieldOfView / cameraPoint.z;

    return Vector3(
        frameBuffer.getWidth() * 0.5f + cameraPoint.x * scale,
        frameBuffer.getHeight() * 0.5f - cameraPoint.y * scale,
        cameraPoint.z
    );
}

inline Vector3 intersectNearPlane(
    const Vector3& a,
    const Vector3& b,
    float nearPlane
) {
    float t = (nearPlane - a.z) / (b.z - a.z);
    return a + (b - a) * t;
}

inline int clipTriangleAgainstNearPlane(
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    float nearPlane,
    std::array<Vector3, 4>& clipped
) {
    std::array<Vector3, 3> input = {a, b, c};
    int outputCount = 0;

    for (int i = 0; i < 3; i++) {
        const Vector3& current = input[i];
        const Vector3& previous = input[(i + 2) % 3];
        bool currentInside = current.z >= nearPlane;
        bool previousInside = previous.z >= nearPlane;

        if (currentInside != previousInside) {
            clipped[outputCount] = intersectNearPlane(previous, current, nearPlane);
            outputCount++;
        }

        if (currentInside) {
            clipped[outputCount] = current;
            outputCount++;
        }
    }

    return outputCount;
}

inline Vector3 intersectScreenX(const Vector3& a, const Vector3& b, float x) {
    float t = (x - a.x) / (b.x - a.x);
    return a + (b - a) * t;
}

inline Vector3 intersectScreenY(const Vector3& a, const Vector3& b, float y) {
    float t = (y - a.y) / (b.y - a.y);
    return a + (b - a) * t;
}

template <typename IsInside, typename Intersect>
inline int clipScreenPolygonEdge(
    const std::array<Vector3, 12>& input,
    int inputCount,
    std::array<Vector3, 12>& output,
    IsInside isInside,
    Intersect intersect
) {
    if (inputCount == 0) {
        return 0;
    }

    int outputCount = 0;

    for (int i = 0; i < inputCount; i++) {
        const Vector3& current = input[i];
        const Vector3& previous = input[(i + inputCount - 1) % inputCount];
        bool currentInside = isInside(current);
        bool previousInside = isInside(previous);

        if (currentInside != previousInside) {
            output[outputCount] = intersect(previous, current);
            outputCount++;
        }

        if (currentInside) {
            output[outputCount] = current;
            outputCount++;
        }
    }

    return outputCount;
}

inline int clipScreenPolygonToFrameBuffer(
    const std::array<Vector3, 12>& input,
    int inputCount,
    const FrameBuffer& frameBuffer,
    std::array<Vector3, 12>& clipped
) {
    std::array<Vector3, 12> scratchA = input;
    std::array<Vector3, 12> scratchB;
    int count = inputCount;
    float maxX = static_cast<float>(frameBuffer.getWidth() - 1);
    float maxY = static_cast<float>(frameBuffer.getHeight() - 1);

    count = clipScreenPolygonEdge(
        scratchA,
        count,
        scratchB,
        [](const Vector3& point) { return point.x >= 0.0f; },
        [](const Vector3& a, const Vector3& b) { return intersectScreenX(a, b, 0.0f); }
    );

    count = clipScreenPolygonEdge(
        scratchB,
        count,
        scratchA,
        [maxX](const Vector3& point) { return point.x <= maxX; },
        [maxX](const Vector3& a, const Vector3& b) { return intersectScreenX(a, b, maxX); }
    );

    count = clipScreenPolygonEdge(
        scratchA,
        count,
        scratchB,
        [](const Vector3& point) { return point.y >= 0.0f; },
        [](const Vector3& a, const Vector3& b) { return intersectScreenY(a, b, 0.0f); }
    );

    count = clipScreenPolygonEdge(
        scratchB,
        count,
        clipped,
        [maxY](const Vector3& point) { return point.y <= maxY; },
        [maxY](const Vector3& a, const Vector3& b) { return intersectScreenY(a, b, maxY); }
    );

    return count;
}

inline void rasterizeMeshTriangles(
    FrameBuffer& frameBuffer,
    const Camera3D& camera,
    const Mesh3D& mesh,
    const Matrix4& transform,
    sf::Color fallbackColor,
    RasterMeshRenderCache& cache,
    bool cullBackFaces = true,
    bool cullObjects = true,
    RasterRenderStats* stats = nullptr
) {
    if (stats) {
        stats->objectsSubmitted++;
        stats->trianglesSubmitted += static_cast<int>(mesh.triangles.size());
    }

    BoundingSphere3D localSphere = mesh.localBoundingSphere();
    Vector3 worldCenter = transform.transformPoint(localSphere.center);
    float worldRadius = std::max({
        transform.transformDirection(Vector3(localSphere.radius, 0.0f, 0.0f)).magnitude(),
        transform.transformDirection(Vector3(0.0f, localSphere.radius, 0.0f)).magnitude(),
        transform.transformDirection(Vector3(0.0f, 0.0f, localSphere.radius)).magnitude()
    });

    if (
        cullObjects &&
        !camera.canSeeSphere(
            worldCenter,
            worldRadius,
            frameBuffer.getWidth(),
            frameBuffer.getHeight()
        )
    ) {
        if (stats) {
            stats->objectsCulled++;
            stats->trianglesSkipped += static_cast<int>(mesh.triangles.size());
        }
        return;
    }

    if (stats) {
        stats->objectsVisible++;
    }

    cache.reserveFor(mesh);

    for (const Vector3& vertex : mesh.vertices) {
        Vector3 world = transform.transformPoint(vertex);
        Vector3 cameraSpace = camera.worldToCameraPoint(world);

        cache.worldVertices.push_back(world);
        cache.cameraVertices.push_back(cameraSpace);
    }

    for (int i = 0; i < mesh.triangles.size(); i++) {
        const Triangle3D& triangle = mesh.triangles[i];

        Vector3 worldA = cache.worldVertices[triangle.a];

        if (cullBackFaces) {
            Vector3 normal;

            if (i < mesh.triangleNormals.size()) {
                normal = transform.transformDirection(mesh.triangleNormals[i]);
            } else {
                Vector3 worldB = cache.worldVertices[triangle.b];
                Vector3 worldC = cache.worldVertices[triangle.c];
                normal = (worldB - worldA).cross(worldC - worldA);
            }

            Vector3 cameraToTriangle = worldA - camera.position;

            if (normal.dot(cameraToTriangle) >= 0.0f) {
                if (stats) {
                    stats->trianglesSkipped++;
                }
                continue;
            }
        }

        std::array<Vector3, 4> clippedCameraPoints;
        int clippedCount = clipTriangleAgainstNearPlane(
            cache.cameraVertices[triangle.a],
            cache.cameraVertices[triangle.b],
            cache.cameraVertices[triangle.c],
            camera.nearPlane,
            clippedCameraPoints
        );

        if (clippedCount < 3) {
            if (stats) {
                stats->trianglesSkipped++;
            }
            continue;
        }

        sf::Color color = fallbackColor;
        if (i < mesh.triangleColors.size()) {
            color = mesh.triangleColors[i];
        }

        std::array<Vector3, 12> screenPolygon;
        for (int point = 0; point < clippedCount; point++) {
            screenPolygon[point] = projectCameraPointToScreen(
                clippedCameraPoints[point],
                camera,
                frameBuffer
            );
        }

        std::array<Vector3, 12> clippedScreenPolygon;
        int screenPointCount = clipScreenPolygonToFrameBuffer(
            screenPolygon,
            clippedCount,
            frameBuffer,
            clippedScreenPolygon
        );

        if (screenPointCount < 3) {
            if (stats) {
                stats->trianglesSkipped++;
            }
            continue;
        }

        for (int point = 1; point < screenPointCount - 1; point++) {
            Rasterizer3D::drawTriangle(
                frameBuffer,
                clippedScreenPolygon[0],
                clippedScreenPolygon[point],
                clippedScreenPolygon[point + 1],
                color
            );

            if (stats) {
                stats->trianglesDrawn++;
            }
        }
    }
}

inline void rasterizeMeshTriangles(
    FrameBuffer& frameBuffer,
    const Camera3D& camera,
    const Mesh3D& mesh,
    const Matrix4& transform,
    sf::Color fallbackColor,
    bool cullBackFaces = true
) {
    RasterMeshRenderCache cache;
    rasterizeMeshTriangles(
        frameBuffer,
        camera,
        mesh,
        transform,
        fallbackColor,
        cache,
        cullBackFaces
    );
}
