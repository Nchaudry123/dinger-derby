#include "PhysicsWorld3D.h"

#include <algorithm>

namespace {

void resolveAxisMinimum(float& position, float& velocity, float radius, float minimum, float restitution) {
    if (position - radius >= minimum) {
        return;
    }

    position = minimum + radius;
    if (velocity < 0.0f) {
        velocity = -velocity * restitution;
    }
}

void resolveAxisMaximum(float& position, float& velocity, float radius, float maximum, float restitution) {
    if (position + radius <= maximum) {
        return;
    }

    position = maximum - radius;
    if (velocity > 0.0f) {
        velocity = -velocity * restitution;
    }
}

}

PhysicsWorld3D::PhysicsWorld3D() {
    gravity = Vector3(0.0f, -9.8f, 0.0f);
    minimumBounds = Vector3(-8.0f, -4.0f, -2.0f);
    maximumBounds = Vector3(8.0f, 5.0f, 14.0f);
    airVelocity = Vector3();
}

void PhysicsWorld3D::setBounds(const Vector3& minimum, const Vector3& maximum) {
    minimumBounds = minimum;
    maximumBounds = maximum;
}

void PhysicsWorld3D::setAtmosphere(float density, const Vector3& windVelocity) {
    airDensity = std::max(density, 0.0f);
    airVelocity = windVelocity;
}

void PhysicsWorld3D::addBody(Body3D* body) {
    bodies.push_back(body);
}

void PhysicsWorld3D::step(float dt) {
    for (Body3D* body : bodies) {
        body->applyForce(gravity * body->mass);

        if (airResistanceEnabled) {
            body->applyForce(AirResistance3D::calculateDragForce(*body, airVelocity, airDensity));
        }

        body->update(dt);
        resolveBounds(*body);
    }

    const int iterations = 6;
    for (int i = 0; i < iterations; i++) {
        collectContacts();
        resolveContacts();
    }
}

int PhysicsWorld3D::getContactCount() const {
    return static_cast<int>(contacts.size());
}

void PhysicsWorld3D::resolveBounds(Body3D& body) {
    if (body.isStatic()) {
        return;
    }

    resolveAxisMinimum(body.position.x, body.velocity.x, body.radius, minimumBounds.x, body.restitution);
    resolveAxisMaximum(body.position.x, body.velocity.x, body.radius, maximumBounds.x, body.restitution);
    resolveAxisMinimum(body.position.y, body.velocity.y, body.radius, minimumBounds.y, body.restitution);
    resolveAxisMaximum(body.position.y, body.velocity.y, body.radius, maximumBounds.y, body.restitution);
    resolveAxisMinimum(body.position.z, body.velocity.z, body.radius, minimumBounds.z, body.restitution);
    resolveAxisMaximum(body.position.z, body.velocity.z, body.radius, maximumBounds.z, body.restitution);
}

void PhysicsWorld3D::collectContacts() {
    contacts.clear();

    for (int i = 0; i < bodies.size(); i++) {
        for (int j = i + 1; j < bodies.size(); j++) {
            CollisionManifold3D manifold = findSphereSphereCollision(*bodies[i], *bodies[j]);

            if (manifold.colliding) {
                contacts.push_back(Contact3D{bodies[i], bodies[j], manifold});
            }
        }
    }
}

void PhysicsWorld3D::resolveContacts() {
    for (Contact3D& contact : contacts) {
        resolveSphereCollision(*contact.a, *contact.b, contact.manifold);
    }
}
