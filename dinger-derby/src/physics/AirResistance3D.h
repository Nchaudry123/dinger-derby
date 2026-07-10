#pragma once

#include "../math/Vector3.h"
#include "Body3D.h"

class AirResistance3D {
public:
    static float crossSectionArea(const Body3D& body);
    static float effectiveDragCoefficient(const Body3D& body, float speed);
    static Vector3 calculateDragForce(
        const Body3D& body,
        const Vector3& airVelocity,
        float airDensity
    );
};
