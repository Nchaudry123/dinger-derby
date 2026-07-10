#include "Camera3D.h"

#include <algorithm>
#include <cmath>

namespace {

Vector3 rotateX(const Vector3& point, float radians) {
    float cosine = std::cos(radians);
    float sine = std::sin(radians);

    return Vector3(
        point.x,
        point.y * cosine - point.z * sine,
        point.y * sine + point.z * cosine
    );
}

Vector3 rotateY(const Vector3& point, float radians) {
    float cosine = std::cos(radians);
    float sine = std::sin(radians);

    return Vector3(
        point.x * cosine + point.z * sine,
        point.y,
        -point.x * sine + point.z * cosine
    );
}

Vector3 rotateZ(const Vector3& point, float radians) {
    float cosine = std::cos(radians);
    float sine = std::sin(radians);

    return Vector3(
        point.x * cosine - point.y * sine,
        point.x * sine + point.y * cosine,
        point.z
    );
}

}

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
    Vector3 cameraPoint = worldToCameraPoint(worldPoint);

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

Vector3 Camera3D::worldToCameraPoint(const Vector3& worldPoint) const {
    Vector3 cameraPoint = worldPoint - position;
    cameraPoint = rotateY(cameraPoint, -rotation.y);
    cameraPoint = rotateX(cameraPoint, -rotation.x);
    cameraPoint = rotateZ(cameraPoint, -rotation.z);
    return cameraPoint;
}

bool Camera3D::canSeeSphere(
    const Vector3& center,
    float radius,
    float screenWidth,
    float screenHeight
) const {
    Vector3 cameraPoint = worldToCameraPoint(center);

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
