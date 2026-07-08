#pragma once

#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"
#include "../src/rendering/Camera3D.h"
#include "../src/rendering/FrameBuffer.h"
#include "../src/rendering/Mesh3D.h"
#include "../src/rendering/Rasterizer3D.h"
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <array>
#include <vector>

inline sf::Vector2u rasterSizeForWindow(sf::Vector2u windowSize) {
    const float renderScale = 0.75f;

    return sf::Vector2u(
        std::max(1u, static_cast<unsigned int>(windowSize.x * renderScale)),
        std::max(1u, static_cast<unsigned int>(windowSize.y * renderScale))
    );
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
        Vector3 cameraSpace = world - camera.position;

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

        Vector3 screenA = projectCameraPointToScreen(clippedCameraPoints[0], camera, frameBuffer);
        Vector3 screenB = projectCameraPointToScreen(clippedCameraPoints[1], camera, frameBuffer);
        Vector3 screenC = projectCameraPointToScreen(clippedCameraPoints[2], camera, frameBuffer);

        Rasterizer3D::drawTriangle(
            frameBuffer,
            screenA,
            screenB,
            screenC,
            color
        );

        if (stats) {
            stats->trianglesDrawn++;
        }

        if (clippedCount == 4) {
            Vector3 screenD = projectCameraPointToScreen(clippedCameraPoints[3], camera, frameBuffer);

            Rasterizer3D::drawTriangle(
                frameBuffer,
                screenA,
                screenC,
                screenD,
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
