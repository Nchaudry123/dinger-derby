#include "Camera3D.h"

#include <algorithm>
#include <cmath>

Camera3D::Camera3D() {
    position = Vector3(0.0f, 0.0f, -6.0f);
    rotation = Vector3(0.0f, 0.0f, 0.0f);
    fieldOfView = 420.0f;
    nearPlane = 0.1f;
}

ProjectedPoint3D Camera3D::projectPoint(
    const Vector3& worldPoint,
    float screenWidth,
    float screenHeight
) const {
    Vector3 cameraPoint = worldPoint - position;

    if (cameraPoint.z <= nearPlane) {
        return ProjectedPoint3D{};
    }

    float scale = fieldOfView / cameraPoint.z;

    ProjectedPoint3D projected;
    projected.position = Vector2(
        screenWidth * 0.5f + cameraPoint.x * scale,
        screenHeight * 0.5f - cameraPoint.y * scale
    );
    projected.visible = true;

    return projected;
}

bool Camera3D::canSeeSphere(
    const Vector3& center,
    float radius,
    float screenWidth,
    float screenHeight
) const {
    Vector3 cameraPoint = center - position;

    if (cameraPoint.z + radius <= nearPlane) {
        return false;
    }

    float safeDepth = std::max(cameraPoint.z, nearPlane);
    float projectedRadius = radius * fieldOfView / safeDepth;
    float projectedX = screenWidth * 0.5f + cameraPoint.x * fieldOfView / safeDepth;
    float projectedY = screenHeight * 0.5f - cameraPoint.y * fieldOfView / safeDepth;

    if (projectedX + projectedRadius < 0.0f) {
        return false;
    }

    if (projectedX - projectedRadius > screenWidth) {
        return false;
    }

    if (projectedY + projectedRadius < 0.0f) {
        return false;
    }

    if (projectedY - projectedRadius > screenHeight) {
        return false;
    }

    return true;
}
