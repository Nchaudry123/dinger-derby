#pragma once

#include "../math/Vector3.h"
#include "Body3D.h"

class AirResistance3D {
public:
    static float crossSectionArea(const Body3D& body);
    static float effectiveDragCoefficient(const Body3D& body, float speed);
    // Spin parameter S = r |ω_active| / |v| (dimensionless).
    static float spinParameter(const Body3D& body, float relativeSpeed);
    // Lift coefficient from spin parameter (Nathan-style baseball curve).
    static float liftCoefficient(float spinParameter);
    static Vector3 calculateDragForce(
        const Body3D& body,
        const Vector3& airVelocity,
        float airDensity
    );
    // Magnus force: ½ ρ A Cl v² (ω_active × v) / |ω_active × v| direction.
    // Spin axis is conserved over a short pitch; only the component of ω
    // perpendicular to velocity produces movement.
    static Vector3 calculateMagnusForce(
        const Body3D& body,
        const Vector3& airVelocity,
        float airDensity
    );
};
