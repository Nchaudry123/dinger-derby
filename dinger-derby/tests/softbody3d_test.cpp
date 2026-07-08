#include <cassert>

#include "../src/physics/SoftBodyMesh3D.h"

namespace {

void testCubeCreatesParticlesAndSprings() {
    SoftBodyMesh3D body = SoftBodyMesh3D::cube();

    assert(body.particles.size() == 8);
    assert(!body.springs.empty());
    assert(body.triangles.size() == 12);
}

void testImpulseMovesNearbyParticles() {
    SoftBodyMesh3D body = SoftBodyMesh3D::cube();
    Vector3 before = body.particles[6].position;

    body.applyImpulse(Vector3(1.0f, 1.0f, 1.0f), 1.5f, Vector3(0.0f, 0.0f, 4.0f));
    body.step(1.0f / 60.0f);

    assert(body.particles[6].position.z > before.z);
}

void testShapeMemoryPullsParticleTowardRestPosition() {
    SoftBodyMesh3D body = SoftBodyMesh3D::cube();
    body.particles[0].position += Vector3(-1.0f, 0.0f, 0.0f);
    float displacedX = body.particles[0].position.x;

    body.step(1.0f / 60.0f);

    assert(body.particles[0].position.x > displacedX);
}

}

int main() {
    testCubeCreatesParticlesAndSprings();
    testImpulseMovesNearbyParticles();
    testShapeMemoryPullsParticleTowardRestPosition();

    return 0;
}
