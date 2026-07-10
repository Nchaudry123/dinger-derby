#include "BaseballVisual3D.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

constexpr float pi = 3.1415926535f;

sf::Color leatherAlbedo(const Vector3& normal) {
    float warmPanel = 0.5f + 0.5f * std::sin(normal.x * 11.0f + normal.y * 7.0f);
    int red = static_cast<int>(224.0f + warmPanel * 14.0f);
    int green = static_cast<int>(216.0f + warmPanel * 12.0f);
    int blue = static_cast<int>(200.0f + warmPanel * 10.0f);

    return sf::Color(
        static_cast<std::uint8_t>(std::clamp(red, 0, 255)),
        static_cast<std::uint8_t>(std::clamp(green, 0, 255)),
        static_cast<std::uint8_t>(std::clamp(blue, 0, 255))
    );
}

}

Mesh3D BaseballVisual3D::makeMesh(int rings, int segments) {
    Mesh3D mesh = Mesh3D::sphere(1.0f, rings, segments);
    mesh.triangleColors.clear();
    mesh.triangleColors.reserve(mesh.triangles.size());

    // Per-triangle albedo from face normal; smooth lighting uses vertexNormals at draw.
    for (int i = 0; i < mesh.triangles.size(); i++) {
        Vector3 normal = i < mesh.triangleNormals.size()
            ? mesh.triangleNormals[i]
            : Vector3(0.0f, 1.0f, 0.0f);
        mesh.triangleColors.push_back(leatherAlbedo(normal.normalized()));
    }

    return mesh;
}

std::vector<SeamPoint3D> BaseballVisual3D::makeSeamLoop(bool mirrored, int pointCount) {
    pointCount = std::max(pointCount, 8);
    std::vector<Vector3> positions;
    positions.reserve(pointCount);
    std::vector<SeamPoint3D> points;
    points.reserve(pointCount);

    for (int i = 0; i < pointCount; i++) {
        float t = static_cast<float>(i) / static_cast<float>(pointCount) * pi * 2.0f;
        float wave = 0.42f * std::sin(t * 2.0f);
        if (mirrored) {
            wave = -wave;
        }

        positions.push_back(Vector3(std::cos(t), wave, std::sin(t)).normalized());
    }

    for (int i = 0; i < pointCount; i++) {
        Vector3 previous = positions[(i + pointCount - 1) % pointCount];
        Vector3 next = positions[(i + 1) % pointCount];
        Vector3 tangent = (next - previous).normalized();
        Vector3 normal = positions[i].normalized();
        Vector3 side = normal.cross(tangent).normalized();
        points.push_back(SeamPoint3D{normal * 1.018f, side});
    }

    return points;
}
