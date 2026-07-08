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

class Mesh3D {
public:
    std::vector<Vector3> vertices;
    std::vector<Edge3D> edges;
    std::vector<Triangle3D> triangles;
    std::vector<sf::Color> triangleColors;
    std::vector<Vector3> triangleNormals;

    static Mesh3D cube(float size = 2.0f);
    static Mesh3D axes(float length = 1.5f);

private:
    void buildTriangleNormals();
};
