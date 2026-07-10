#pragma once

#include <SFML/Graphics.hpp>

#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "rendering/Mesh3D.h"
#include "rendering/SoftwareRenderer3D.h"

inline void drawLine3D(
    sf::RenderWindow& window,
    const Vector3& a,
    const Vector3& b,
    sf::Color color
) {
    SoftwareRenderer3D renderer(window);
    renderer.drawLine(a, b, color);
}

inline void drawPoint3D(
    sf::RenderWindow& window,
    const Vector3& point,
    float radius,
    sf::Color color
) {
    SoftwareRenderer3D renderer(window);
    renderer.drawPoint(point, radius, color);
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
    SoftwareRenderer3D renderer(window);
    renderer.drawMeshEdges(Mesh3D::cube(), transform, color);
}
