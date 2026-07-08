#include "SoftwareRenderer3D.h"

SoftwareRenderer3D::SoftwareRenderer3D(sf::RenderWindow& window)
    : window(window) {}

void SoftwareRenderer3D::drawLine(
    const Vector3& a,
    const Vector3& b,
    sf::Color color
) {
    ProjectedPoint3D projectedA =
        camera.projectPoint(a, window.getSize().x, window.getSize().y);
    ProjectedPoint3D projectedB =
        camera.projectPoint(b, window.getSize().x, window.getSize().y);

    if (!projectedA.visible || !projectedB.visible) {
        return;
    }

    sf::VertexArray line(sf::PrimitiveType::Lines, 2);
    line[0].position = sf::Vector2f(
        projectedA.position.x,
        projectedA.position.y
    );
    line[0].color = color;
    line[1].position = sf::Vector2f(
        projectedB.position.x,
        projectedB.position.y
    );
    line[1].color = color;

    window.draw(line);
}

void SoftwareRenderer3D::drawPoint(
    const Vector3& point,
    float radius,
    sf::Color color
) {
    ProjectedPoint3D projected =
        camera.projectPoint(point, window.getSize().x, window.getSize().y);

    if (!projected.visible) {
        return;
    }

    sf::CircleShape shape(radius);
    shape.setOrigin(sf::Vector2f(radius, radius));
    shape.setPosition(sf::Vector2f(projected.position.x, projected.position.y));
    shape.setFillColor(color);

    window.draw(shape);
}

void SoftwareRenderer3D::drawMeshEdges(
    const Mesh3D& mesh,
    const Matrix4& transform,
    sf::Color color
) {
    for (const Edge3D& edge : mesh.edges) {
        Vector3 a = transform.transformPoint(mesh.vertices[edge.start]);
        Vector3 b = transform.transformPoint(mesh.vertices[edge.end]);
        drawLine(a, b, color);
    }
}
