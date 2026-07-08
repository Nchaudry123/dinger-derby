#pragma once

#include <SFML/Graphics.hpp>

#include "../math/Matrix4.h"
#include "Camera3D.h"
#include "Mesh3D.h"

class SoftwareRenderer3D {
public:
    explicit SoftwareRenderer3D(sf::RenderWindow& window);

    void drawLine(const Vector3& a, const Vector3& b, sf::Color color);
    void drawPoint(const Vector3& point, float radius, sf::Color color);
    void drawTriangle(
        const Vector3& a,
        const Vector3& b,
        const Vector3& c,
        sf::Color color
    );
    void drawMeshEdges(
        const Mesh3D& mesh,
        const Matrix4& transform,
        sf::Color color
    );
    void drawMeshTriangles(
        const Mesh3D& mesh,
        const Matrix4& transform,
        sf::Color fallbackColor
    );

    Camera3D camera;

private:
    sf::RenderWindow& window;
};
