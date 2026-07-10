#pragma once
#include <vector>
#include "../collision/Collision2D.h"
#include "Body2D.h"
#include "../math/Vector2.h"

struct Contact2D {
    Body2D* a = nullptr;
    Body2D* b = nullptr;
    CollisionManifold2D manifold;
};

class PhysicsWorld2D {
public:
    Vector2 gravity;

    float groundY;
    float leftWall;
    float rightWall;
    float ceilingY;

    PhysicsWorld2D();

    void setBounds(float width, float height);

    void addBody(Body2D* body);
    void step(float dt);

private:
    std::vector<Body2D*> bodies;
    std::vector<Contact2D> contacts;

    void collectContacts();
    void resolveContacts();
};
