#include <cassert>
#include <cmath>

#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"

namespace {

bool nearlyEqual(float a, float b, float tolerance = 0.001f) {
    return std::abs(a - b) <= tolerance;
}

void assertVectorNear(const Vector3& value, const Vector3& expected) {
    assert(nearlyEqual(value.x, expected.x));
    assert(nearlyEqual(value.y, expected.y));
    assert(nearlyEqual(value.z, expected.z));
}

void testVector3DotAndCross() {
    Vector3 right(1.0f, 0.0f, 0.0f);
    Vector3 up(0.0f, 1.0f, 0.0f);
    Vector3 forward = right.cross(up);

    assert(nearlyEqual(right.dot(up), 0.0f));
    assertVectorNear(forward, Vector3(0.0f, 0.0f, 1.0f));
}

void testVector3Normalize() {
    Vector3 direction(3.0f, 4.0f, 0.0f);
    Vector3 normalized = direction.normalized();

    assert(nearlyEqual(normalized.magnitude(), 1.0f));
    assertVectorNear(normalized, Vector3(0.6f, 0.8f, 0.0f));
}

void testMatrixTranslationAndScale() {
    Matrix4 transform =
        Matrix4::translation(Vector3(10.0f, 0.0f, -2.0f)) *
        Matrix4::scale(Vector3(2.0f, 3.0f, 4.0f));

    Vector3 result = transform.transformPoint(Vector3(1.0f, 2.0f, 3.0f));

    assertVectorNear(result, Vector3(12.0f, 6.0f, 10.0f));
}

void testMatrixRotationZ() {
    Matrix4 rotation = Matrix4::rotationZ(3.1415926535f / 2.0f);
    Vector3 result = rotation.transformDirection(Vector3(1.0f, 0.0f, 0.0f));

    assertVectorNear(result, Vector3(0.0f, 1.0f, 0.0f));
}

}

int main() {
    testVector3DotAndCross();
    testVector3Normalize();
    testMatrixTranslationAndScale();
    testMatrixRotationZ();

    return 0;
}
