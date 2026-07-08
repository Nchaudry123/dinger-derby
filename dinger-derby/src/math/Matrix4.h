#pragma once

#include "Vector3.h"

class Matrix4 {
public:
    float values[16];

    Matrix4();

    static Matrix4 identity();
    static Matrix4 translation(const Vector3& offset);
    static Matrix4 scale(const Vector3& amount);
    static Matrix4 rotationX(float radians);
    static Matrix4 rotationY(float radians);
    static Matrix4 rotationZ(float radians);

    Matrix4 operator*(const Matrix4& other) const;
    Vector3 transformPoint(const Vector3& point) const;
    Vector3 transformDirection(const Vector3& direction) const;
};
