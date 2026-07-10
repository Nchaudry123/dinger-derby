#pragma once

#include "Vector3.h"

class Quaternion;

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
    // T * R * S composition for joint local transforms / glTF nodes.
    static Matrix4 fromTrs(const Vector3& translation, const Quaternion& rotation, const Vector3& scale);

    Matrix4 operator*(const Matrix4& other) const;
    Matrix4 inverse() const;
    Vector3 transformPoint(const Vector3& point) const;
    Vector3 transformDirection(const Vector3& direction) const;
    // Upper-left 3x3 only (for skinning normals).
    Vector3 transformDirection3x3(const Vector3& direction) const;
};
