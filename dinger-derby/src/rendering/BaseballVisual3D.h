#pragma once

#include <vector>

#include "Mesh3D.h"
#include "../math/Vector3.h"

struct SeamPoint3D {
    Vector3 position;
    Vector3 side;
};

// Unit-sphere baseball mesh + seam polylines shared by demos.
// Albedo only — lighting is applied at draw time (Gouraud).
class BaseballVisual3D {
public:
    static Mesh3D makeMesh(int rings = 36, int segments = 72);
    static std::vector<SeamPoint3D> makeSeamLoop(bool mirrored, int pointCount = 180);
};
