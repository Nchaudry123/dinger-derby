#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

constexpr float pi = 3.1415926535f;

// MLB-ish kit colors (navy home set, tan mitt, skin).
const sf::Color skin(214, 168, 132);
const sf::Color jersey(236, 238, 242);
const sf::Color pants(48, 54, 68);
const sf::Color belt(28, 28, 32);
const sf::Color cleats(24, 24, 28);
const sf::Color capNavy(22, 36, 72);
const sf::Color logoRed(180, 40, 48);
const sf::Color gear(28, 40, 58);
const sf::Color gearHighlight(48, 68, 92);
const sf::Color mitt(148, 96, 58);
const sf::Color mittDark(110, 68, 40);
const sf::Color sock(245, 245, 248);

int sphereRings(int detail) {
    return detail >= 2 ? 8 : (detail >= 1 ? 6 : 5);
}

int sphereSegments(int detail) {
    return detail >= 2 ? 12 : (detail >= 1 ? 10 : 8);
}

void appendTransformed(
    Mesh3D& dest,
    const Mesh3D& source,
    const Matrix4& transform,
    sf::Color color
) {
    int base = static_cast<int>(dest.vertices.size());

    for (const Vector3& vertex : source.vertices) {
        dest.vertices.push_back(transform.transformPoint(vertex));
    }

    for (const Edge3D& edge : source.edges) {
        dest.edges.push_back({edge.start + base, edge.end + base});
    }

    for (int i = 0; i < source.triangles.size(); i++) {
        const Triangle3D& triangle = source.triangles[i];
        dest.triangles.push_back({
            triangle.a + base,
            triangle.b + base,
            triangle.c + base
        });
        dest.triangleColors.push_back(color);
    }
}

void addSphere(
    Mesh3D& dest,
    const Vector3& center,
    float radius,
    sf::Color color,
    int detail
) {
    Mesh3D sphere = Mesh3D::sphere(1.0f, sphereRings(detail), sphereSegments(detail));
    Matrix4 transform =
        Matrix4::translation(center) *
        Matrix4::scale(Vector3(radius, radius, radius));
    appendTransformed(dest, sphere, transform, color);
}

void addBox(
    Mesh3D& dest,
    const Vector3& center,
    float width,
    float height,
    float depth,
    sf::Color color,
    float yaw = 0.0f,
    float pitch = 0.0f,
    float roll = 0.0f
) {
    Mesh3D box = Mesh3D::box(width, height, depth);
    Matrix4 transform =
        Matrix4::translation(center) *
        Matrix4::rotationY(yaw) *
        Matrix4::rotationX(pitch) *
        Matrix4::rotationZ(roll);
    appendTransformed(dest, box, transform, color);
}

// Capsule along local Y from center, total height includes rounded ends.
void addLimb(
    Mesh3D& dest,
    const Vector3& center,
    float radius,
    float length,
    sf::Color color,
    int detail,
    float yaw = 0.0f,
    float pitch = 0.0f,
    float roll = 0.0f
) {
    float shaft = std::max(0.02f, length - radius * 2.0f);
    Matrix4 orient =
        Matrix4::translation(center) *
        Matrix4::rotationY(yaw) *
        Matrix4::rotationX(pitch) *
        Matrix4::rotationZ(roll);

    Mesh3D shaftMesh = Mesh3D::box(radius * 1.7f, shaft, radius * 1.7f);
    appendTransformed(dest, shaftMesh, orient, color);

    Mesh3D tip = Mesh3D::sphere(1.0f, sphereRings(detail), sphereSegments(detail));
    appendTransformed(
        dest,
        tip,
        orient * Matrix4::translation(Vector3(0.0f, shaft * 0.5f + radius * 0.15f, 0.0f)) *
            Matrix4::scale(Vector3(radius, radius, radius)),
        color
    );
    appendTransformed(
        dest,
        tip,
        orient * Matrix4::translation(Vector3(0.0f, -shaft * 0.5f - radius * 0.15f, 0.0f)) *
            Matrix4::scale(Vector3(radius, radius, radius)),
        color
    );
}

}

Mesh3D BaseballPlayer3D::pitcher(int detail) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    // Proportions ~6'0" athlete scaled to field units (~1.88 tall).
    const float footY = 0.04f;
    const float shin = 0.42f;
    const float thigh = 0.40f;
    const float torso = 0.52f;
    const float headR = 0.11f;

    // Stance: slight open set, facing plate (+Z).
    const float hipY = footY + shin + thigh * 0.92f;
    const float shoulderY = hipY + torso;
    const float headY = shoulderY + 0.16f + headR;

    // Legs
    addLimb(mesh, Vector3(-0.11f, footY + shin * 0.5f, 0.04f), 0.055f, shin, pants, detail, 0.0f, 0.08f, 0.0f);
    addLimb(mesh, Vector3(0.11f, footY + shin * 0.5f, -0.02f), 0.055f, shin, pants, detail, 0.0f, -0.05f, 0.0f);
    addLimb(mesh, Vector3(-0.10f, footY + shin + thigh * 0.45f, 0.02f), 0.065f, thigh, pants, detail, 0.0f, 0.12f, 0.05f);
    addLimb(mesh, Vector3(0.10f, footY + shin + thigh * 0.45f, -0.01f), 0.065f, thigh, pants, detail, 0.0f, -0.06f, -0.04f);

    // Cleats
    addBox(mesh, Vector3(-0.11f, footY * 0.5f, 0.06f), 0.10f, 0.05f, 0.18f, cleats);
    addBox(mesh, Vector3(0.11f, footY * 0.5f, 0.02f), 0.10f, 0.05f, 0.18f, cleats);

    // Socks peek
    addBox(mesh, Vector3(-0.11f, footY + 0.08f, 0.04f), 0.07f, 0.10f, 0.07f, sock);
    addBox(mesh, Vector3(0.11f, footY + 0.08f, 0.0f), 0.07f, 0.10f, 0.07f, sock);

    // Hips / belt
    addBox(mesh, Vector3(0.0f, hipY, 0.0f), 0.34f, 0.14f, 0.18f, pants);
    addBox(mesh, Vector3(0.0f, hipY + 0.06f, 0.0f), 0.36f, 0.04f, 0.19f, belt);

    // Torso (jersey)
    addBox(mesh, Vector3(0.0f, hipY + torso * 0.48f, 0.0f), 0.38f, torso * 0.9f, 0.20f, jersey);
    // Chest seam stripe
    addBox(mesh, Vector3(0.0f, hipY + torso * 0.55f, 0.105f), 0.06f, torso * 0.55f, 0.02f, logoRed);

    // Shoulders
    addSphere(mesh, Vector3(-0.20f, shoulderY - 0.02f, 0.0f), 0.07f, jersey, detail);
    addSphere(mesh, Vector3(0.20f, shoulderY - 0.02f, 0.0f), 0.07f, jersey, detail);

    // Head + cap
    addSphere(mesh, Vector3(0.0f, headY, 0.02f), headR, skin, detail);
    addSphere(mesh, Vector3(0.0f, headY + 0.04f, 0.0f), headR * 0.92f, capNavy, detail);
    addBox(mesh, Vector3(0.0f, headY - 0.02f, 0.12f), 0.16f, 0.03f, 0.10f, capNavy); // brim

    // Neck
    addBox(mesh, Vector3(0.0f, shoulderY + 0.05f, 0.01f), 0.08f, 0.08f, 0.08f, skin);

    // Glove arm (left) held near chest
    addLimb(
        mesh,
        Vector3(-0.28f, shoulderY - 0.18f, 0.10f),
        0.045f,
        0.30f,
        jersey,
        detail,
        0.0f,
        0.55f,
        0.35f
    );
    addLimb(
        mesh,
        Vector3(-0.22f, shoulderY - 0.38f, 0.18f),
        0.04f,
        0.26f,
        skin,
        detail,
        0.0f,
        0.85f,
        0.15f
    );
    // Mitt
    addSphere(mesh, Vector3(-0.16f, shoulderY - 0.48f, 0.28f), 0.09f, mitt, detail);
    addSphere(mesh, Vector3(-0.12f, shoulderY - 0.46f, 0.34f), 0.055f, mittDark, detail);

    // Throwing arm (right) relaxed at side, slight bend
    addLimb(
        mesh,
        Vector3(0.28f, shoulderY - 0.16f, 0.02f),
        0.045f,
        0.30f,
        jersey,
        detail,
        0.0f,
        0.25f,
        -0.2f
    );
    addLimb(
        mesh,
        Vector3(0.32f, shoulderY - 0.42f, 0.04f),
        0.04f,
        0.28f,
        skin,
        detail,
        0.0f,
        0.15f,
        -0.1f
    );
    addSphere(mesh, Vector3(0.34f, shoulderY - 0.58f, 0.05f), 0.045f, skin, detail);

    mesh.rebuildNormals();
    return mesh;
}

Mesh3D BaseballPlayer3D::catcher(int detail) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    // Deep crouch: hips low, knees wide, torso upright — matches plate scale.
    const float footY = 0.05f;
    const float hipY = 0.55f;
    const float shoulderY = 1.28f;
    const float headY = 1.58f;

    // Boots / shin guards
    addBox(mesh, Vector3(-0.18f, 0.22f, 0.06f), 0.14f, 0.38f, 0.16f, gear, 0.15f, 0.35f, 0.0f);
    addBox(mesh, Vector3(0.18f, 0.22f, 0.06f), 0.14f, 0.38f, 0.16f, gear, -0.15f, 0.35f, 0.0f);
    addBox(mesh, Vector3(-0.20f, footY, 0.10f), 0.12f, 0.06f, 0.20f, cleats);
    addBox(mesh, Vector3(0.20f, footY, 0.10f), 0.12f, 0.06f, 0.20f, cleats);

    // Thighs angled out in crouch
    addLimb(mesh, Vector3(-0.16f, 0.42f, 0.02f), 0.07f, 0.34f, pants, detail, 0.2f, 0.9f, 0.15f);
    addLimb(mesh, Vector3(0.16f, 0.42f, 0.02f), 0.07f, 0.34f, pants, detail, -0.2f, 0.9f, -0.15f);

    // Hips / seat
    addBox(mesh, Vector3(0.0f, hipY, -0.02f), 0.40f, 0.18f, 0.24f, pants);
    addBox(mesh, Vector3(0.0f, hipY + 0.08f, -0.02f), 0.42f, 0.05f, 0.25f, belt);

    // Chest protector
    addBox(mesh, Vector3(0.0f, (hipY + shoulderY) * 0.5f, 0.04f), 0.42f, 0.58f, 0.22f, gear);
    addBox(mesh, Vector3(0.0f, (hipY + shoulderY) * 0.52f, 0.14f), 0.36f, 0.48f, 0.06f, gearHighlight);
    // Jersey sleeves under gear
    addBox(mesh, Vector3(0.0f, shoulderY - 0.08f, -0.02f), 0.46f, 0.16f, 0.20f, jersey);

    // Shoulders / arms
    addSphere(mesh, Vector3(-0.24f, shoulderY, 0.02f), 0.075f, gear, detail);
    addSphere(mesh, Vector3(0.24f, shoulderY, 0.02f), 0.075f, gear, detail);

    // Left arm + mitt (target side, slightly forward)
    addLimb(
        mesh,
        Vector3(-0.34f, shoulderY - 0.14f, 0.12f),
        0.05f,
        0.28f,
        gear,
        detail,
        0.1f,
        0.7f,
        0.4f
    );
    addLimb(
        mesh,
        Vector3(-0.42f, shoulderY - 0.28f, 0.22f),
        0.045f,
        0.24f,
        skin,
        detail,
        0.05f,
        1.0f,
        0.2f
    );
    addSphere(mesh, Vector3(-0.48f, shoulderY - 0.32f, 0.34f), 0.11f, mitt, detail);
    addSphere(mesh, Vector3(-0.52f, shoulderY - 0.28f, 0.40f), 0.06f, mittDark, detail);
    addBox(mesh, Vector3(-0.50f, shoulderY - 0.34f, 0.42f), 0.08f, 0.14f, 0.04f, mittDark, 0.4f, 0.3f, 0.0f);

    // Right arm braced on thigh
    addLimb(
        mesh,
        Vector3(0.32f, shoulderY - 0.16f, 0.06f),
        0.05f,
        0.28f,
        gear,
        detail,
        -0.15f,
        0.55f,
        -0.35f
    );
    addLimb(
        mesh,
        Vector3(0.36f, shoulderY - 0.36f, 0.08f),
        0.045f,
        0.24f,
        skin,
        detail,
        -0.1f,
        0.4f,
        -0.2f
    );
    addSphere(mesh, Vector3(0.38f, shoulderY - 0.50f, 0.08f), 0.045f, skin, detail);

    // Helmet + mask
    addSphere(mesh, Vector3(0.0f, headY, 0.02f), 0.12f, skin, detail);
    addSphere(mesh, Vector3(0.0f, headY + 0.02f, 0.0f), 0.125f, gear, detail);
    addBox(mesh, Vector3(0.0f, headY - 0.02f, 0.12f), 0.18f, 0.14f, 0.08f, gearHighlight); // mask cage block
    // Mask bars (simple)
    addBox(mesh, Vector3(0.0f, headY + 0.04f, 0.155f), 0.14f, 0.02f, 0.02f, gear);
    addBox(mesh, Vector3(0.0f, headY - 0.04f, 0.155f), 0.14f, 0.02f, 0.02f, gear);
    addBox(mesh, Vector3(-0.05f, headY, 0.155f), 0.02f, 0.12f, 0.02f, gear);
    addBox(mesh, Vector3(0.05f, headY, 0.155f), 0.02f, 0.12f, 0.02f, gear);

    // Neck
    addBox(mesh, Vector3(0.0f, shoulderY + 0.08f, 0.02f), 0.09f, 0.10f, 0.09f, skin);

    mesh.rebuildNormals();
    return mesh;
}
