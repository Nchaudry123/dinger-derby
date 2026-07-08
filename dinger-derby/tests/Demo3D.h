#pragma once

#include <SFML/Graphics.hpp>

#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"
#include "../src/rendering/Camera3D.h"
#include "../src/rendering/Mesh3D.h"

struct ProjectedPoint {
    sf::Vector2f position;
    bool visible = false;
};

inline ProjectedPoint projectPoint(
    const Vector3& point,
    sf::Vector2u screenSize
) {
    Camera3D camera;
    ProjectedPoint3D projected3D =
        camera.projectPoint(point, screenSize.x, screenSize.y);

    if (!projected3D.visible) {
        return ProjectedPoint{};
    }

    ProjectedPoint projected;
    projected.position = sf::Vector2f(
        projected3D.position.x,
        projected3D.position.y
    );
    projected.visible = true;

    return projected;
}

inline void drawLine3D(
    sf::RenderWindow& window,
    const Vector3& a,
    const Vector3& b,
    sf::Color color
) {
    ProjectedPoint projectedA = projectPoint(a, window.getSize());
    ProjectedPoint projectedB = projectPoint(b, window.getSize());

    if (!projectedA.visible || !projectedB.visible) {
        return;
    }

    sf::VertexArray line(sf::PrimitiveType::Lines, 2);
    line[0].position = projectedA.position;
    line[0].color = color;
    line[1].position = projectedB.position;
    line[1].color = color;

    window.draw(line);
}

inline void drawPoint3D(
    sf::RenderWindow& window,
    const Vector3& point,
    float radius,
    sf::Color color
) {
    ProjectedPoint projected = projectPoint(point, window.getSize());

    if (!projected.visible) {
        return;
    }

    sf::CircleShape shape(radius);
    shape.setOrigin(sf::Vector2f(radius, radius));
    shape.setPosition(projected.position);
    shape.setFillColor(color);

    window.draw(shape);
}

inline void drawAxes(sf::RenderWindow& window, const Matrix4& transform) {
    Mesh3D axes = Mesh3D::axes();
    Vector3 origin = transform.transformPoint(axes.vertices[0]);
    Vector3 x = transform.transformPoint(axes.vertices[1]);
    Vector3 y = transform.transformPoint(axes.vertices[2]);
    Vector3 z = transform.transformPoint(axes.vertices[3]);

    drawLine3D(window, origin, x, sf::Color(240, 80, 80));
    drawLine3D(window, origin, y, sf::Color(80, 220, 120));
    drawLine3D(window, origin, z, sf::Color(90, 150, 255));
}

inline void drawWireCube(
    sf::RenderWindow& window,
    const Matrix4& transform,
    sf::Color color
) {
    Mesh3D cube = Mesh3D::cube();

    for (const Edge3D& edge : cube.edges) {
        Vector3 a = transform.transformPoint(cube.vertices[edge.start]);
        Vector3 b = transform.transformPoint(cube.vertices[edge.end]);
        drawLine3D(window, a, b, color);
    }
}
