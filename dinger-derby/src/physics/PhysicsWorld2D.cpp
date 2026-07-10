#include "PhysicsWorld2D.h"
#include "../collision/Collision2D.h"
// Handles friction + spin transfer when a body hits a surface
void applySurfaceResponse(Body2D* body, const Vector2& normal) {
    Vector2 tangent(-normal.y, normal.x);

    float tangentVelocity = body->velocity.dot(tangent);
    float targetAngularVelocity = tangentVelocity / body->radius;

    body->angularVelocity +=
        (targetAngularVelocity - body->angularVelocity) * 0.1f;

    body->velocity -= tangent * tangentVelocity * 0.02f;
}

PhysicsWorld2D::PhysicsWorld2D() {
    gravity = Vector2(0, 9.8f);

    leftWall = 0.0f;
    ceilingY = 0.0f;
    rightWall = 800.0f;
    groundY = 600.0f;
}

void PhysicsWorld2D::addBody(Body2D* body) {
    bodies.push_back(body);
}

void PhysicsWorld2D::step(float dt) {
    // Apply forces, update movement, and handle wall collisions
    for (Body2D* body : bodies) {
        // Treat world gravity as acceleration so all bodies fall consistently.
        body->applyForce(gravity * body->mass);
        body->update(dt);

        // Ground
        if (body->position.y + body->radius > groundY) {
            body->position.y = groundY - body->radius;
            body->velocity.y *= -body->restitution;

            applySurfaceResponse(body, Vector2(0, -1));
        }

        // Ceiling
        if (body->position.y - body->radius < ceilingY) {
            body->position.y = ceilingY + body->radius;
            body->velocity.y *= -body->restitution;

            applySurfaceResponse(body, Vector2(0, 1));
        }

        //Left wall
        if (body->position.x - body->radius < leftWall) {
            body->position.x = leftWall + body->radius;
            body->velocity.x *= -body->restitution;

            applySurfaceResponse(body, Vector2(1, 0));
        }

        //Right wall
        if (body->position.x + body->radius > rightWall) {
            body->position.x = rightWall - body->radius;
            body->velocity.x *= -body->restitution;

            applySurfaceResponse(body, Vector2(-1, 0));
        }
    }

    // Resolve body-to-body collisions multiple times for stability
    int iterations = 5;

    for (int k = 0; k < iterations; k++) {
        collectContacts();
        resolveContacts();
    }
}

void PhysicsWorld2D::setBounds(float width, float height) {
    leftWall = 0.0f;
    ceilingY = 0.0f;
    rightWall = width;
    groundY = height;
}

void PhysicsWorld2D::collectContacts() {
    contacts.clear();

    for (int i = 0; i < bodies.size(); i++) {
        for (int j = i + 1; j < bodies.size(); j++) {
            CollisionManifold2D manifold =
                findCircleCircleCollision(*bodies[i], *bodies[j]);

            if (manifold.colliding) {
                contacts.push_back(Contact2D{bodies[i], bodies[j], manifold});
            }
        }
    }
}

void PhysicsWorld2D::resolveContacts() {
    for (Contact2D& contact : contacts) {
        resolveCircleCollision(*contact.a, *contact.b, contact.manifold);
    }
}
