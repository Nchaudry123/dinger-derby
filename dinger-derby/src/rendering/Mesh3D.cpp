#include "Mesh3D.h"

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
