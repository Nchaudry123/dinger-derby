#pragma once

#include "../math/Vector2.h"
#include "../math/Vector3.h"

struct ProjectedPoint3D {
    Vector2 position;
    bool visible = false;
};

class Camera3D {
public:
    Vector3 position;
    Vector3 rotation;
    float fieldOfView;
    float nearPlane;

    Camera3D();

    ProjectedPoint3D projectPoint(
        const Vector3& worldPoint,
        float screenWidth,
        float screenHeight
    ) const;

    bool canSeeSphere(
        const Vector3& center,
        float radius,
        float screenWidth,
        float screenHeight
    ) const;
};
