#pragma once

#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"
#include "../src/rendering/Camera3D.h"
#include "../src/rendering/FrameBuffer.h"
#include "../src/rendering/Mesh3D.h"
#include "../src/rendering/Rasterizer3D.h"
#include <SFML/System/Vector2.hpp>
#include <algorithm>
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
    std::vector<Vector3> screenVertices;
    std::vector<bool> visibleVertices;

    void reserveFor(const Mesh3D& mesh) {
        worldVertices.clear();
        screenVertices.clear();
        visibleVertices.clear();

        worldVertices.reserve(mesh.vertices.size());
        screenVertices.reserve(mesh.vertices.size());
        visibleVertices.reserve(mesh.vertices.size());
    }
};

inline void rasterizeMeshTriangles(
    FrameBuffer& frameBuffer,
    const Camera3D& camera,
    const Mesh3D& mesh,
    const Matrix4& transform,
    sf::Color fallbackColor,
    RasterMeshRenderCache& cache,
    bool cullBackFaces = true
) {
    BoundingSphere3D localSphere = mesh.localBoundingSphere();
    Vector3 worldCenter = transform.transformPoint(localSphere.center);
    float worldRadius = std::max({
        transform.transformDirection(Vector3(localSphere.radius, 0.0f, 0.0f)).magnitude(),
        transform.transformDirection(Vector3(0.0f, localSphere.radius, 0.0f)).magnitude(),
        transform.transformDirection(Vector3(0.0f, 0.0f, localSphere.radius)).magnitude()
    });

    if (
        !camera.canSeeSphere(
            worldCenter,
            worldRadius,
            frameBuffer.getWidth(),
            frameBuffer.getHeight()
        )
    ) {
        return;
    }

    cache.reserveFor(mesh);

    for (const Vector3& vertex : mesh.vertices) {
        Vector3 world = transform.transformPoint(vertex);
        ProjectedPoint3D projected = camera.projectPoint(
            world,
            frameBuffer.getWidth(),
            frameBuffer.getHeight()
        );
        Vector3 cameraSpace = world - camera.position;

        cache.worldVertices.push_back(world);
        cache.screenVertices.push_back(Vector3(
            projected.position.x,
            projected.position.y,
            cameraSpace.z
        ));
        cache.visibleVertices.push_back(projected.visible);
    }

    for (int i = 0; i < mesh.triangles.size(); i++) {
        const Triangle3D& triangle = mesh.triangles[i];

        if (
            !cache.visibleVertices[triangle.a] ||
            !cache.visibleVertices[triangle.b] ||
            !cache.visibleVertices[triangle.c]
        ) {
            continue;
        }

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
                continue;
            }
        }

        sf::Color color = fallbackColor;
        if (i < mesh.triangleColors.size()) {
            color = mesh.triangleColors[i];
        }

        Rasterizer3D::drawTriangle(
            frameBuffer,
            cache.screenVertices[triangle.a],
            cache.screenVertices[triangle.b],
            cache.screenVertices[triangle.c],
            color
        );
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
