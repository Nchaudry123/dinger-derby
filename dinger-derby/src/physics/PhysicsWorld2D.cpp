#include "PhysicsWorld2D.h"
#include "../collision/Collision2D.h" 
PhysicsWorld2D::PhysicsWorld2D() {

    gravity = Vector2(0, 9.8f);

    leftWall = 0;
    ceilingY = 0;
    rightWall = 800;
    groundY = 600;
}

void PhysicsWorld2D::addBody(Body2D* body) {
    bodies.push_back(body);
}

void PhysicsWorld2D::step(float dt) {
    // Apply forces and update movement
    for (Body2D* body : bodies) {
        body->applyForce(gravity);
        body->update(dt);

        // Ground
        if (body->position.y + body->radius > groundY) {
            body->position.y = groundY - body->radius;
            body->velocity.y *= -body->restitution;
        }

        // Ceiling
        if (body->position.y - body->radius < ceilingY) {
            body->position.y = ceilingY + body->radius;
            body->velocity.y *= -body->restitution;
        }

        // Left wall
        if (body->position.x - body->radius < leftWall) {
            body->position.x = leftWall + body->radius;
            body->velocity.x *= -body->restitution;
        }

        // Right wall
        if (body->position.x + body->radius > rightWall) {
            body->position.x = rightWall - body->radius;
            body->velocity.x *= -body->restitution;
        }
    }

    // Resolve body-to-body collisions multiple times for stability
    int iterations = 5;

    for (int k = 0; k < iterations; k++) {
        for (int i = 0; i < bodies.size(); i++) {
            for (int j = i + 1; j < bodies.size(); j++) {
                Body2D* a = bodies[i];
                Body2D* b = bodies[j];

                if (circleCircleCollision(*a, *b)) {
                    resolveCircleCollision(*a, *b);
                }
            }
        }
    }
}
void PhysicsWorld2D::setBounds(float width, float height) {

    leftWall = 0.0f;
    ceilingY = 0.0f;

    rightWall = width;
    groundY = height;
}