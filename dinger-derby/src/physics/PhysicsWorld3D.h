#pragma once

#include <vector>

#include "../collision/Collision3D.h"
#include "../math/Vector3.h"
#include "Body3D.h"

struct Contact3D {
    Body3D* a = nullptr;
    Body3D* b = nullptr;
    CollisionManifold3D manifold;
};

class PhysicsWorld3D {
public:
    Vector3 gravity;
    Vector3 minimumBounds;
    Vector3 maximumBounds;

    PhysicsWorld3D();

    void setBounds(const Vector3& minimum, const Vector3& maximum);
    void addBody(Body3D* body);
    void step(float dt);
    int getContactCount() const;

private:
    std::vector<Body3D*> bodies;
    std::vector<Contact3D> contacts;

    void resolveBounds(Body3D& body);
    void collectContacts();
    void resolveContacts();
};
