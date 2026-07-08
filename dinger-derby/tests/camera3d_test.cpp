#include <cassert>
#include <cmath>

#include "../src/rendering/Camera3D.h"

namespace {

bool nearlyEqual(float a, float b, float tolerance = 0.001f) {
    return std::abs(a - b) <= tolerance;
}

void testProjectsCenterPointToScreenCenter() {
    Camera3D camera;

    ProjectedPoint3D point =
        camera.projectPoint(Vector3(0.0f, 0.0f, 0.0f), 1280.0f, 720.0f);

    assert(point.visible);
    assert(nearlyEqual(point.position.x, 640.0f));
    assert(nearlyEqual(point.position.y, 360.0f));
}

void testProjectsOffsetPointWithPerspectiveScale() {
    Camera3D camera;

    ProjectedPoint3D point =
        camera.projectPoint(Vector3(1.0f, 1.0f, 0.0f), 1280.0f, 720.0f);

    assert(point.visible);
    assert(nearlyEqual(point.position.x, 710.0f));
    assert(nearlyEqual(point.position.y, 290.0f));
}

void testRejectsPointsBehindNearPlane() {
    Camera3D camera;

    ProjectedPoint3D point =
        camera.projectPoint(Vector3(0.0f, 0.0f, -6.0f), 1280.0f, 720.0f);

    assert(!point.visible);
}

void testSeesSphereInsideProjection() {
    Camera3D camera;

    assert(camera.canSeeSphere(Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1280.0f, 720.0f));
}

void testRejectsSphereBehindNearPlane() {
    Camera3D camera;

    assert(!camera.canSeeSphere(Vector3(0.0f, 0.0f, -6.5f), 0.1f, 1280.0f, 720.0f));
}

void testRejectsSphereOutsideScreenBounds() {
    Camera3D camera;

    assert(!camera.canSeeSphere(Vector3(20.0f, 0.0f, 0.0f), 1.0f, 1280.0f, 720.0f));
}

}

int main() {
    testProjectsCenterPointToScreenCenter();
    testProjectsOffsetPointWithPerspectiveScale();
    testRejectsPointsBehindNearPlane();
    testSeesSphereInsideProjection();
    testRejectsSphereBehindNearPlane();
    testRejectsSphereOutsideScreenBounds();

    return 0;
}
