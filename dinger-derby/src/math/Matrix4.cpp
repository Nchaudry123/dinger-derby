#include "Matrix4.h"

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
