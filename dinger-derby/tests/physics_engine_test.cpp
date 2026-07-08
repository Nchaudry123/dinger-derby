#include <cassert>
#include <cmath>

#include "../src/collision/Collision2D.h"
#include "../src/physics/Body2D.h"
#include "../src/physics/PhysicsWorld2D.h"

namespace {

bool nearlyEqual(float a, float b, float tolerance = 0.001f) {
    return std::abs(a - b) <= tolerance;
}

void testGravityIsMassIndependent() {
    PhysicsWorld2D world;
    world.gravity = Vector2(0.0f, 100.0f);
    world.setBounds(1000.0f, 1000.0f);

    Body2D light(Vector2(100.0f, 100.0f), 1.0f);
    Body2D heavy(Vector2(200.0f, 100.0f), 10.0f);

    world.addBody(&light);
    world.addBody(&heavy);
    world.step(0.1f);

    assert(nearlyEqual(light.velocity.y, heavy.velocity.y));
}

void testOverlappingCirclesSeparate() {
    Body2D a(Vector2(100.0f, 100.0f), 1.0f);
    Body2D b(Vector2(100.0f, 100.0f), 1.0f);

    a.setRadius(10.0f);
    b.setRadius(10.0f);

    assert(circleCircleCollision(a, b));
    resolveCircleCollision(a, b);

    assert((b.position - a.position).magnitude() > 0.0f);
}

void testCircleRectangleBounce() {
    Body2D ball(Vector2(50.0f, 45.0f), 1.0f);
    ball.setRadius(10.0f);
    ball.velocity = Vector2(0.0f, 100.0f);
    ball.restitution = 0.5f;

    resolveCircleRectangleCollision(
        ball,
        Vector2(50.0f, 50.0f),
        100.0f,
        10.0f
    );

    assert(ball.position.y < 45.0f);
    assert(ball.velocity.y < 0.0f);
}

}

int main() {
    testGravityIsMassIndependent();
    testOverlappingCirclesSeparate();
    testCircleRectangleBounce();

    return 0;
}
