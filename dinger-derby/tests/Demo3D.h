#pragma once

#include <SFML/Graphics.hpp>

#include "../src/math/Matrix4.h"
#include "../src/math/Vector3.h"

struct ProjectedPoint {
    sf::Vector2f position;
    bool visible = false;
};

inline ProjectedPoint projectPoint(
    const Vector3& point,
    sf::Vector2u screenSize,
    float cameraDistance = 6.0f,
    float focalLength = 420.0f
) {
    float depth = point.z + cameraDistance;

    if (depth <= 0.1f) {
        return ProjectedPoint{};
    }

    float scale = focalLength / depth;
    float centerX = screenSize.x * 0.5f;
    float centerY = screenSize.y * 0.5f;

    ProjectedPoint projected;
    projected.position = sf::Vector2f(
        centerX + point.x * scale,
        centerY - point.y * scale
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
    Vector3 origin = transform.transformPoint(Vector3(0.0f, 0.0f, 0.0f));
    Vector3 x = transform.transformPoint(Vector3(1.5f, 0.0f, 0.0f));
    Vector3 y = transform.transformPoint(Vector3(0.0f, 1.5f, 0.0f));
    Vector3 z = transform.transformPoint(Vector3(0.0f, 0.0f, 1.5f));

    drawLine3D(window, origin, x, sf::Color(240, 80, 80));
    drawLine3D(window, origin, y, sf::Color(80, 220, 120));
    drawLine3D(window, origin, z, sf::Color(90, 150, 255));
}

inline void drawWireCube(
    sf::RenderWindow& window,
    const Matrix4& transform,
    sf::Color color
) {
    Vector3 vertices[8] = {
        Vector3(-1.0f, -1.0f, -1.0f),
        Vector3(1.0f, -1.0f, -1.0f),
        Vector3(1.0f, 1.0f, -1.0f),
        Vector3(-1.0f, 1.0f, -1.0f),
        Vector3(-1.0f, -1.0f, 1.0f),
        Vector3(1.0f, -1.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f),
        Vector3(-1.0f, 1.0f, 1.0f)
    };

    int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    for (int i = 0; i < 12; i++) {
        Vector3 a = transform.transformPoint(vertices[edges[i][0]]);
        Vector3 b = transform.transformPoint(vertices[edges[i][1]]);
        drawLine3D(window, a, b, color);
    }
}
