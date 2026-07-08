#pragma once

#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"
#include "../src/rendering/Camera3D.h"
#include "../src/rendering/FrameBuffer.h"
#include "../src/rendering/Mesh3D.h"
#include "../src/rendering/Rasterizer3D.h"
#include <vector>

inline void rasterizeMeshTriangles(
    FrameBuffer& frameBuffer,
    const Camera3D& camera,
    const Mesh3D& mesh,
    const Matrix4& transform,
    sf::Color fallbackColor,
    bool cullBackFaces = true
) {
    std::vector<Vector3> worldVertices;
    std::vector<Vector3> screenVertices;
    std::vector<bool> visibleVertices;

    worldVertices.reserve(mesh.vertices.size());
    screenVertices.reserve(mesh.vertices.size());
    visibleVertices.reserve(mesh.vertices.size());

    for (const Vector3& vertex : mesh.vertices) {
        Vector3 world = transform.transformPoint(vertex);
        ProjectedPoint3D projected = camera.projectPoint(
            world,
            frameBuffer.getWidth(),
            frameBuffer.getHeight()
        );
        Vector3 cameraSpace = world - camera.position;

        worldVertices.push_back(world);
        screenVertices.push_back(Vector3(
            projected.position.x,
            projected.position.y,
            cameraSpace.z
        ));
        visibleVertices.push_back(projected.visible);
    }

    for (int i = 0; i < mesh.triangles.size(); i++) {
        const Triangle3D& triangle = mesh.triangles[i];

        if (
            !visibleVertices[triangle.a] ||
            !visibleVertices[triangle.b] ||
            !visibleVertices[triangle.c]
        ) {
            continue;
        }

        Vector3 worldA = worldVertices[triangle.a];
        Vector3 worldB = worldVertices[triangle.b];
        Vector3 worldC = worldVertices[triangle.c];

        if (cullBackFaces) {
            Vector3 normal = (worldB - worldA).cross(worldC - worldA);
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
            screenVertices[triangle.a],
            screenVertices[triangle.b],
            screenVertices[triangle.c],
            color
        );
    }
}
