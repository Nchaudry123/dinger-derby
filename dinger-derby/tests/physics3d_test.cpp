#include <cassert>
#include <cmath>

#include "collision/Collision3D.h"
#include "physics/AirResistance3D.h"
#include "physics/PhysicsWorld3D.h"

namespace {

bool nearlyEqual(float a, float b, float tolerance = 0.001f) {
    return std::abs(a - b) <= tolerance;
}

void testBody3DIntegratesForces() {
    Body3D body(Vector3(0.0f, 0.0f, 0.0f), 2.0f);
    body.applyForce(Vector3(4.0f, 0.0f, 0.0f));
    body.update(1.0f);

    assert(body.position.x > 1.5f);
    assert(body.velocity.x > 1.5f);
}

void testSphereSphereCollisionFindsNormalAndPenetration() {
    Body3D a(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
    Body3D b(Vector3(1.5f, 0.0f, 0.0f), 1.0f);
    a.radius = 1.0f;
    b.radius = 1.0f;

    CollisionManifold3D manifold = findSphereSphereCollision(a, b);

    assert(manifold.colliding);
    assert(nearlyEqual(manifold.normal.x, 1.0f));
    assert(nearlyEqual(manifold.penetration, 0.5f));
}

void testSphereResolutionSeparatesBodies() {
    Body3D a(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
    Body3D b(Vector3(1.5f, 0.0f, 0.0f), 1.0f);
    a.radius = 1.0f;
    b.radius = 1.0f;

    resolveSphereCollision(a, b);

    assert((b.position - a.position).magnitude() > 1.9f);
}

void testPhysicsWorld3DGravityAndBounds() {
    PhysicsWorld3D world;
    world.setBounds(Vector3(-2.0f, -1.0f, -2.0f), Vector3(2.0f, 3.0f, 2.0f));

    Body3D body(Vector3(0.0f, -0.8f, 0.0f), 1.0f);
    body.radius = 0.5f;
    body.velocity = Vector3(0.0f, -5.0f, 0.0f);
    world.addBody(&body);
    world.step(0.5f);

    assert(body.position.y >= -0.5f);
    assert(body.velocity.y >= 0.0f);
}

void testAirResistanceCalculatesQuadraticDragAgainstMotion() {
    Body3D body(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
    body.setRadius(0.5f);
    body.dragCoefficient = 1.0f;
    body.velocity = Vector3(10.0f, 0.0f, 0.0f);

    Vector3 dragForce = AirResistance3D::calculateDragForce(
        body,
        Vector3(),
        1.0f
    );

    assert(dragForce.x < 0.0f);
    assert(nearlyEqual(dragForce.y, 0.0f));
    assert(nearlyEqual(dragForce.z, 0.0f));
    assert(std::abs(dragForce.x) > 35.0f);
    assert(std::abs(dragForce.x) < 40.0f);
}

void testAirResistanceCoefficientChangesWithSpeed() {
    Body3D body(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
    body.setRadius(0.5f);
    body.dragCoefficient = 1.0f;

    float lowSpeedCoefficient = AirResistance3D::effectiveDragCoefficient(body, 10.0f);
    float highSpeedCoefficient = AirResistance3D::effectiveDragCoefficient(body, 80.0f);

    assert(nearlyEqual(lowSpeedCoefficient, 1.0f));
    assert(highSpeedCoefficient < lowSpeedCoefficient);
    assert(highSpeedCoefficient > 0.75f);
}

void testPhysicsWorld3DAirResistanceSlowsBody() {
    PhysicsWorld3D world;
    world.gravity = Vector3();
    world.setAtmosphere(0.35f);
    world.setBounds(Vector3(-100.0f, -100.0f, -100.0f), Vector3(100.0f, 100.0f, 100.0f));

    Body3D body(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
    body.setRadius(0.5f);
    body.dragCoefficient = 1.0f;
    body.velocity = Vector3(10.0f, 0.0f, 0.0f);
    world.addBody(&body);

    world.step(0.1f);

    assert(body.velocity.x > 0.0f);
    assert(body.velocity.x < 10.0f);
}

void testPhysicsWorld3DWindUsesRelativeVelocity() {
    PhysicsWorld3D world;
    world.gravity = Vector3();
    world.setAtmosphere(0.35f, Vector3(10.0f, 0.0f, 0.0f));
    world.setBounds(Vector3(-100.0f, -100.0f, -100.0f), Vector3(100.0f, 100.0f, 100.0f));

    Body3D body(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
    body.setRadius(0.5f);
    body.dragCoefficient = 1.0f;
    body.velocity = Vector3();
    world.addBody(&body);

    world.step(0.1f);

    assert(body.velocity.x > 0.0f);
}

void testPhysicsWorld3DAirResistanceCanBeDisabled() {
    PhysicsWorld3D world;
    world.gravity = Vector3();
    world.airResistanceEnabled = false;
    world.setAtmosphere(1.0f);
    world.setBounds(Vector3(-100.0f, -100.0f, -100.0f), Vector3(100.0f, 100.0f, 100.0f));

    Body3D body(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
    body.setRadius(0.5f);
    body.velocity = Vector3(10.0f, 0.0f, 0.0f);
    world.addBody(&body);

    world.step(0.1f);

    assert(nearlyEqual(body.velocity.x, 10.0f));
}

void testMagnusForceFromBackspinLifts() {
    // ω = −X, v = +Z → ω × v = +Y (four-seam ride).
    Body3D body(Vector3(0.0f, 0.0f, 0.0f), 0.145f);
    body.setRadius(0.065f);
    body.velocity = Vector3(0.0f, 0.0f, 70.0f);
    body.angularVelocity = Vector3(-250.0f, 0.0f, 0.0f);
    body.spinEfficiency = 1.0f;
    body.magnusScale = 1.0f;

    Vector3 force = AirResistance3D::calculateMagnusForce(body, Vector3(), 0.075f);
    assert(force.y > 0.0f);
    assert(std::abs(force.x) < force.y * 0.25f);
    assert(std::abs(force.z) < force.y * 0.25f);
}

void testMagnusForceFromSidespinBreaksGloveSide() {
    // ω = −Y, v = +Z → ω × v = −X (RHP glove-side break).
    Body3D body(Vector3(0.0f, 0.0f, 0.0f), 0.145f);
    body.setRadius(0.065f);
    body.velocity = Vector3(0.0f, 0.0f, 60.0f);
    body.angularVelocity = Vector3(0.0f, -250.0f, 0.0f);
    body.spinEfficiency = 1.0f;
    body.magnusScale = 1.0f;

    Vector3 force = AirResistance3D::calculateMagnusForce(body, Vector3(), 0.075f);
    assert(force.x < 0.0f);
}

void testGyrospinProducesNoMagnus() {
    // ω parallel to v → no active spin → zero Magnus.
    Body3D body(Vector3(0.0f, 0.0f, 0.0f), 0.145f);
    body.setRadius(0.065f);
    body.velocity = Vector3(0.0f, 0.0f, 60.0f);
    body.angularVelocity = Vector3(0.0f, 0.0f, 300.0f);
    body.spinEfficiency = 1.0f;
    body.magnusScale = 1.0f;

    Vector3 force = AirResistance3D::calculateMagnusForce(body, Vector3(), 0.075f);
    assert(std::abs(force.x) < 1e-4f);
    assert(std::abs(force.y) < 1e-4f);
    assert(std::abs(force.z) < 1e-4f);
}

void testPhysicsWorldAppliesMagnus() {
    PhysicsWorld3D world;
    world.gravity = Vector3();
    world.setAtmosphere(0.075f);
    world.setBounds(Vector3(-100.0f, -100.0f, -100.0f), Vector3(100.0f, 100.0f, 100.0f));

    Body3D body(Vector3(0.0f, 1.5f, 0.0f), 0.145f);
    body.setRadius(0.065f);
    body.velocity = Vector3(0.0f, 0.0f, 70.0f);
    body.angularVelocity = Vector3(-250.0f, 0.0f, 0.0f);
    body.spinEfficiency = 1.0f;
    body.magnusScale = 1.2f;
    body.dragCoefficient = 0.05f; // low drag so lift is clear
    body.airResistanceScale = 0.2f;
    world.addBody(&body);

    for (int i = 0; i < 30; i++) {
        world.step(1.0f / 180.0f);
    }

    assert(body.velocity.y > 0.0f); // lifted by backspin
}

}

int main() {
    testBody3DIntegratesForces();
    testSphereSphereCollisionFindsNormalAndPenetration();
    testSphereResolutionSeparatesBodies();
    testPhysicsWorld3DGravityAndBounds();
    testAirResistanceCalculatesQuadraticDragAgainstMotion();
    testAirResistanceCoefficientChangesWithSpeed();
    testPhysicsWorld3DAirResistanceSlowsBody();
    testPhysicsWorld3DWindUsesRelativeVelocity();
    testPhysicsWorld3DAirResistanceCanBeDisabled();
    testMagnusForceFromBackspinLifts();
    testMagnusForceFromSidespinBreaksGloveSide();
    testGyrospinProducesNoMagnus();
    testPhysicsWorldAppliesMagnus();

    return 0;
}
