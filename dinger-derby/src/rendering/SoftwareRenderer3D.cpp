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

void SoftwareRenderer3D::drawTriangle(
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    sf::Color color
) {
    ProjectedPoint3D projectedA =
        camera.projectPoint(a, window.getSize().x, window.getSize().y);
    ProjectedPoint3D projectedB =
        camera.projectPoint(b, window.getSize().x, window.getSize().y);
    ProjectedPoint3D projectedC =
        camera.projectPoint(c, window.getSize().x, window.getSize().y);

    if (!projectedA.visible || !projectedB.visible || !projectedC.visible) {
        return;
    }

    sf::ConvexShape triangle(3);
    triangle.setPoint(
        0,
        sf::Vector2f(projectedA.position.x, projectedA.position.y)
    );
    triangle.setPoint(
        1,
        sf::Vector2f(projectedB.position.x, projectedB.position.y)
    );
    triangle.setPoint(
        2,
        sf::Vector2f(projectedC.position.x, projectedC.position.y)
    );
    triangle.setFillColor(color);

    window.draw(triangle);
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

void SoftwareRenderer3D::drawMeshTriangles(
    const Mesh3D& mesh,
    const Matrix4& transform,
    sf::Color fallbackColor
) {
    for (int i = 0; i < mesh.triangles.size(); i++) {
        const Triangle3D& triangle = mesh.triangles[i];
        sf::Color color = fallbackColor;

        if (i < mesh.triangleColors.size()) {
            color = mesh.triangleColors[i];
        }

        Vector3 a = transform.transformPoint(mesh.vertices[triangle.a]);
        Vector3 b = transform.transformPoint(mesh.vertices[triangle.b]);
        Vector3 c = transform.transformPoint(mesh.vertices[triangle.c]);

        drawTriangle(a, b, c, color);
    }
}
