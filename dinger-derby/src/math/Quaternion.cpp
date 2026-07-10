#include "Quaternion.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

Quaternion::Quaternion(float x, float y, float z, float w)
    : x(x), y(y), z(z), w(w) {}

Quaternion Quaternion::identity() {
    return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
}

Quaternion Quaternion::fromAxisAngle(const Vector3& axis, float radians) {
    float len = axis.magnitude();
    if (len < 1e-8f) {
        return identity();
    }
    Vector3 n = axis * (1.0f / len);
    float half = radians * 0.5f;
    float s = std::sin(half);
    return Quaternion(n.x * s, n.y * s, n.z * s, std::cos(half)).normalized();
}

Quaternion Quaternion::fromEulerXYZ(float rx, float ry, float rz) {
    Quaternion qx = fromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), rx);
    Quaternion qy = fromAxisAngle(Vector3(0.0f, 1.0f, 0.0f), ry);
    Quaternion qz = fromAxisAngle(Vector3(0.0f, 0.0f, 1.0f), rz);
    // Apply X then Y then Z in local terms: q = qz * qy * qx
    return (qz * qy * qx).normalized();
}

Quaternion Quaternion::fromXyzw(float x, float y, float z, float w) {
    return Quaternion(x, y, z, w).normalized();
}

Quaternion Quaternion::operator*(const Quaternion& other) const {
    return Quaternion(
        w * other.x + x * other.w + y * other.z - z * other.y,
        w * other.y - x * other.z + y * other.w + z * other.x,
        w * other.z + x * other.y - y * other.x + z * other.w,
        w * other.w - x * other.x - y * other.y - z * other.z
    );
}

Quaternion Quaternion::normalized() const {
    float m = std::sqrt(x * x + y * y + z * z + w * w);
    if (m < 1e-8f) {
        return identity();
    }
    float inv = 1.0f / m;
    return Quaternion(x * inv, y * inv, z * inv, w * inv);
}

Quaternion Quaternion::conjugate() const {
    return Quaternion(-x, -y, -z, w);
}

Matrix4 Quaternion::toMatrix4() const {
    Quaternion q = normalized();
    float xx = q.x * q.x;
    float yy = q.y * q.y;
    float zz = q.z * q.z;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float yz = q.y * q.z;
    float wx = q.w * q.x;
    float wy = q.w * q.y;
    float wz = q.w * q.z;

    Matrix4 m = Matrix4::identity();
    // Row-major storage matching transformPoint: row0 = [0..3], etc.
    m.values[0] = 1.0f - 2.0f * (yy + zz);
    m.values[1] = 2.0f * (xy - wz);
    m.values[2] = 2.0f * (xz + wy);

    m.values[4] = 2.0f * (xy + wz);
    m.values[5] = 1.0f - 2.0f * (xx + zz);
    m.values[6] = 2.0f * (yz - wx);

    m.values[8] = 2.0f * (xz - wy);
    m.values[9] = 2.0f * (yz + wx);
    m.values[10] = 1.0f - 2.0f * (xx + yy);
    return m;
}

Vector3 Quaternion::rotate(const Vector3& v) const {
    Quaternion qv(v.x, v.y, v.z, 0.0f);
    Quaternion r = (*this) * qv * conjugate();
    return Vector3(r.x, r.y, r.z);
}

Quaternion Quaternion::slerp(const Quaternion& aIn, const Quaternion& bIn, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    Quaternion a = aIn.normalized();
    Quaternion b = bIn.normalized();
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0.0f) {
        b = Quaternion(-b.x, -b.y, -b.z, -b.w);
        dot = -dot;
    }
    if (dot > 0.9995f) {
        return Quaternion(
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t
        ).normalized();
    }
    float theta = std::acos(std::clamp(dot, -1.0f, 1.0f));
    float sinTheta = std::sin(theta);
    float w1 = std::sin((1.0f - t) * theta) / sinTheta;
    float w2 = std::sin(t * theta) / sinTheta;
    return Quaternion(
        a.x * w1 + b.x * w2,
        a.y * w1 + b.y * w2,
        a.z * w1 + b.z * w2,
        a.w * w1 + b.w * w2
    ).normalized();
}
