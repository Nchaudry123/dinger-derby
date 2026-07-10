#include "Matrix4.h"
#include "Quaternion.h"

#include <cmath>

Matrix4::Matrix4() {
    for (int i = 0; i < 16; i++) {
        values[i] = 0.0f;
    }
}

Matrix4 Matrix4::identity() {
    Matrix4 matrix;

    matrix.values[0] = 1.0f;
    matrix.values[5] = 1.0f;
    matrix.values[10] = 1.0f;
    matrix.values[15] = 1.0f;

    return matrix;
}

Matrix4 Matrix4::translation(const Vector3& offset) {
    Matrix4 matrix = Matrix4::identity();

    matrix.values[3] = offset.x;
    matrix.values[7] = offset.y;
    matrix.values[11] = offset.z;

    return matrix;
}

Matrix4 Matrix4::scale(const Vector3& amount) {
    Matrix4 matrix = Matrix4::identity();

    matrix.values[0] = amount.x;
    matrix.values[5] = amount.y;
    matrix.values[10] = amount.z;

    return matrix;
}

Matrix4 Matrix4::rotationX(float radians) {
    Matrix4 matrix = Matrix4::identity();
    float cosine = std::cos(radians);
    float sine = std::sin(radians);

    matrix.values[5] = cosine;
    matrix.values[6] = -sine;
    matrix.values[9] = sine;
    matrix.values[10] = cosine;

    return matrix;
}

Matrix4 Matrix4::rotationY(float radians) {
    Matrix4 matrix = Matrix4::identity();
    float cosine = std::cos(radians);
    float sine = std::sin(radians);

    matrix.values[0] = cosine;
    matrix.values[2] = sine;
    matrix.values[8] = -sine;
    matrix.values[10] = cosine;

    return matrix;
}

Matrix4 Matrix4::rotationZ(float radians) {
    Matrix4 matrix = Matrix4::identity();
    float cosine = std::cos(radians);
    float sine = std::sin(radians);

    matrix.values[0] = cosine;
    matrix.values[1] = -sine;
    matrix.values[4] = sine;
    matrix.values[5] = cosine;

    return matrix;
}

Matrix4 Matrix4::operator*(const Matrix4& other) const {
    Matrix4 result;

    for (int row = 0; row < 4; row++) {
        for (int column = 0; column < 4; column++) {
            float sum = 0.0f;

            for (int i = 0; i < 4; i++) {
                sum += values[row * 4 + i] * other.values[i * 4 + column];
            }

            result.values[row * 4 + column] = sum;
        }
    }

    return result;
}

Vector3 Matrix4::transformPoint(const Vector3& point) const {
    float x =
        values[0] * point.x +
        values[1] * point.y +
        values[2] * point.z +
        values[3];

    float y =
        values[4] * point.x +
        values[5] * point.y +
        values[6] * point.z +
        values[7];

    float z =
        values[8] * point.x +
        values[9] * point.y +
        values[10] * point.z +
        values[11];

    float w =
        values[12] * point.x +
        values[13] * point.y +
        values[14] * point.z +
        values[15];

    if (w != 0.0f && w != 1.0f) {
        return Vector3(x / w, y / w, z / w);
    }

    return Vector3(x, y, z);
}

Vector3 Matrix4::transformDirection(const Vector3& direction) const {
    return transformDirection3x3(direction);
}

Vector3 Matrix4::transformDirection3x3(const Vector3& direction) const {
    float x =
        values[0] * direction.x +
        values[1] * direction.y +
        values[2] * direction.z;

    float y =
        values[4] * direction.x +
        values[5] * direction.y +
        values[6] * direction.z;

    float z =
        values[8] * direction.x +
        values[9] * direction.y +
        values[10] * direction.z;

    return Vector3(x, y, z);
}

Matrix4 Matrix4::fromTrs(
    const Vector3& translation,
    const Quaternion& rotation,
    const Vector3& scaleAmount
) {
    return Matrix4::translation(translation) *
        rotation.toMatrix4() *
        Matrix4::scale(scaleAmount);
}

Matrix4 Matrix4::inverse() const {
    // General 4x4 inverse via adjugate (sufficient for skinning TRS matrices).
    const float* m = values;
    float inv[16];

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
        m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
        m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
        m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
        m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
        m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
        m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
        m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
        m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
        m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
        m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
        m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
        m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
        m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
        m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
        m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
        m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    Matrix4 result = Matrix4::identity();
    if (std::fabs(det) < 1e-12f) {
        return result;
    }
    float invDet = 1.0f / det;
    for (int i = 0; i < 16; i++) {
        result.values[i] = inv[i] * invDet;
    }
    return result;
}
