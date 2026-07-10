#pragma once

#include "Matrix4.h"
#include "Vector3.h"

// Unit quaternion (x, y, z, w) for joint rotations / glTF.
class Quaternion {
public:
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    Quaternion() = default;
    Quaternion(float x, float y, float z, float w);

    static Quaternion identity();
    static Quaternion fromAxisAngle(const Vector3& axis, float radians);
    static Quaternion fromEulerXYZ(float rx, float ry, float rz);
    // glTF / common convention: xyzw
    static Quaternion fromXyzw(float x, float y, float z, float w);

    Quaternion operator*(const Quaternion& other) const;
    Quaternion normalized() const;
    Quaternion conjugate() const;
    Matrix4 toMatrix4() const;
    Vector3 rotate(const Vector3& v) const;

    static Quaternion slerp(const Quaternion& a, const Quaternion& b, float t);
};
