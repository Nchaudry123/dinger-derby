#pragma once

#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"
#include "../src/rendering/Camera3D.h"
#include "../src/rendering/FrameBuffer.h"
#include "../src/rendering/Mesh3D.h"
#include "../src/rendering/Rasterizer3D.h"

inline void rasterizeMeshTriangles(
    FrameBuffer& frameBuffer,
    const Camera3D& camera,
    const Mesh3D& mesh,
    const Matrix4& transform,
    sf::Color fallbackColor
) {
    for (int i = 0; i < mesh.triangles.size(); i++) {
        const Triangle3D& triangle = mesh.triangles[i];

        Vector3 worldA = transform.transformPoint(mesh.vertices[triangle.a]);
        Vector3 worldB = transform.transformPoint(mesh.vertices[triangle.b]);
        Vector3 worldC = transform.transformPoint(mesh.vertices[triangle.c]);

        ProjectedPoint3D projectedA = camera.projectPoint(
            worldA,
            frameBuffer.getWidth(),
            frameBuffer.getHeight()
        );
        ProjectedPoint3D projectedB = camera.projectPoint(
            worldB,
            frameBuffer.getWidth(),
            frameBuffer.getHeight()
        );
        ProjectedPoint3D projectedC = camera.projectPoint(
            worldC,
            frameBuffer.getWidth(),
            frameBuffer.getHeight()
        );

        if (!projectedA.visible || !projectedB.visible || !projectedC.visible) {
            continue;
        }

        Vector3 cameraA = worldA - camera.position;
        Vector3 cameraB = worldB - camera.position;
        Vector3 cameraC = worldC - camera.position;

        sf::Color color = fallbackColor;
        if (i < mesh.triangleColors.size()) {
            color = mesh.triangleColors[i];
        }

        Rasterizer3D::drawTriangle(
            frameBuffer,
            Vector3(projectedA.position.x, projectedA.position.y, cameraA.z),
            Vector3(projectedB.position.x, projectedB.position.y, cameraB.z),
            Vector3(projectedC.position.x, projectedC.position.y, cameraC.z),
            color
        );
    }
}
