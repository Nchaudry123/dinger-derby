#include "Mesh3D.h"

#include <algorithm>

Mesh3D Mesh3D::cube(float size) {
    float halfSize = size * 0.5f;

    Mesh3D mesh;
    mesh.vertices = {
        Vector3(-halfSize, -halfSize, -halfSize),
        Vector3(halfSize, -halfSize, -halfSize),
        Vector3(halfSize, halfSize, -halfSize),
        Vector3(-halfSize, halfSize, -halfSize),
        Vector3(-halfSize, -halfSize, halfSize),
        Vector3(halfSize, -halfSize, halfSize),
        Vector3(halfSize, halfSize, halfSize),
        Vector3(-halfSize, halfSize, halfSize)
    };

    mesh.edges = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    mesh.triangles = {
        {0, 2, 1}, {0, 3, 2},
        {4, 5, 6}, {4, 6, 7},
        {0, 1, 5}, {0, 5, 4},
        {3, 6, 2}, {3, 7, 6},
        {1, 2, 6}, {1, 6, 5},
        {0, 4, 7}, {0, 7, 3}
    };

    mesh.triangleColors = {
        sf::Color(190, 70, 70), sf::Color(190, 70, 70),
        sf::Color(70, 190, 120), sf::Color(70, 190, 120),
        sf::Color(70, 120, 220), sf::Color(70, 120, 220),
        sf::Color(220, 190, 70), sf::Color(220, 190, 70),
        sf::Color(200, 90, 210), sf::Color(200, 90, 210),
        sf::Color(70, 200, 210), sf::Color(70, 200, 210)
    };

    mesh.buildTriangleNormals();

    return mesh;
}

Mesh3D Mesh3D::axes(float length) {
    Mesh3D mesh;
    mesh.vertices = {
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(length, 0.0f, 0.0f),
        Vector3(0.0f, length, 0.0f),
        Vector3(0.0f, 0.0f, length)
    };

    mesh.edges = {
        {0, 1},
        {0, 2},
        {0, 3}
    };

    return mesh;
}

void Mesh3D::buildTriangleNormals() {
    triangleNormals.clear();
    triangleNormals.reserve(triangles.size());

    for (const Triangle3D& triangle : triangles) {
        Vector3 a = vertices[triangle.a];
        Vector3 b = vertices[triangle.b];
        Vector3 c = vertices[triangle.c];
        triangleNormals.push_back((b - a).cross(c - a).normalized());
    }
}

BoundingSphere3D Mesh3D::localBoundingSphere() const {
    BoundingSphere3D sphere;

    if (vertices.empty()) {
        return sphere;
    }

    Vector3 minimum = vertices[0];
    Vector3 maximum = vertices[0];

    for (const Vector3& vertex : vertices) {
        minimum.x = std::min(minimum.x, vertex.x);
        minimum.y = std::min(minimum.y, vertex.y);
        minimum.z = std::min(minimum.z, vertex.z);
        maximum.x = std::max(maximum.x, vertex.x);
        maximum.y = std::max(maximum.y, vertex.y);
        maximum.z = std::max(maximum.z, vertex.z);
    }

    sphere.center = (minimum + maximum) * 0.5f;

    for (const Vector3& vertex : vertices) {
        sphere.radius = std::max(sphere.radius, (vertex - sphere.center).magnitude());
    }

    return sphere;
}
