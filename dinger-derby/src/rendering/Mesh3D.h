#pragma once

#include <vector>

#include "../math/Vector3.h"

struct Edge3D {
    int start = 0;
    int end = 0;
};

class Mesh3D {
public:
    std::vector<Vector3> vertices;
    std::vector<Edge3D> edges;

    static Mesh3D cube(float size = 2.0f);
    static Mesh3D axes(float length = 1.5f);
};
