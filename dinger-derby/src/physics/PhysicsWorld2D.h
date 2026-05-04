#pragma once
#include <vector>
#include "Body2D.h"
#include "../math/Vector2.h"

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
};