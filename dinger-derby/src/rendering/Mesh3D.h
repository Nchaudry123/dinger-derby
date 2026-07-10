#pragma once

#include <vector>
#include <SFML/Graphics/Color.hpp>

#include "../math/Vector3.h"

struct Edge3D {
    int start = 0;
    int end = 0;
};

struct Triangle3D {
    int a = 0;
    int b = 0;
    int c = 0;
};

struct BoundingSphere3D {
    Vector3 center;
    float radius = 0.0f;
};

class Mesh3D {
public:
    std::vector<Vector3> vertices;
    std::vector<Edge3D> edges;
    std::vector<Triangle3D> triangles;
    std::vector<sf::Color> triangleColors;
    std::vector<Vector3> triangleNormals;
    // Optional smooth shading. Empty => flat triangle colors at draw time.
    std::vector<Vector3> vertexNormals;

    static Mesh3D cube(float size = 2.0f);
    // Axis-aligned box centered at origin with full extents (width, height, depth).
    static Mesh3D box(float width, float height, float depth);
    static Mesh3D sphere(float radius = 1.0f, int rings = 8, int segments = 12);
    static Mesh3D axes(float length = 1.5f);
    BoundingSphere3D localBoundingSphere() const;
    void rebuildNormals();

private:
    void buildTriangleNormals();
    void buildSphereVertexNormals(float radius);
};
