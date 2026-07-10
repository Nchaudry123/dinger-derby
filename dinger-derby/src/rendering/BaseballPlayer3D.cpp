#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

// PS1 sports-game palette: limited, slightly crushed values for that era look.
const sf::Color skin(206, 154, 118);
const sf::Color skinShadow(176, 122, 92);
const sf::Color jersey(228, 230, 236);
const sf::Color jerseyShade(198, 202, 214);
const sf::Color pants(42, 48, 62);
const sf::Color pantsLight(58, 66, 84);
const sf::Color belt(22, 22, 26);
const sf::Color cleats(18, 18, 22);
const sf::Color cleatSole(36, 36, 40);
const sf::Color capNavy(18, 32, 68);
const sf::Color capBill(14, 24, 52);
const sf::Color logoRed(196, 36, 44);
const sf::Color underShirt(28, 34, 48);
const sf::Color gear(24, 34, 52);
const sf::Color gearMid(38, 52, 74);
const sf::Color gearLight(56, 74, 98);
const sf::Color gearEdge(16, 22, 34);
const sf::Color mitt(156, 98, 54);
const sf::Color mittMid(132, 80, 44);
const sf::Color mittDark(98, 58, 32);
const sf::Color sock(236, 236, 240);
const sf::Color stitch(210, 200, 190);

int headSeg(int detail) {
    // Low, chunky spheres = PS1 head/shoulder caps.
    return detail >= 2 ? 8 : 6;
}

int headRings(int detail) {
    return detail >= 2 ? 6 : 5;
}

float smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
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

    for (const Triangle3D& triangle : source.triangles) {
        dest.triangles.push_back({
            triangle.a + base,
            triangle.b + base,
            triangle.c + base
        });
        dest.triangleColors.push_back(color);
    }
}

Matrix4 trs(
    const Vector3& center,
    float yaw = 0.0f,
    float pitch = 0.0f,
    float roll = 0.0f,
    const Vector3& scale = Vector3(1.0f, 1.0f, 1.0f)
) {
    return Matrix4::translation(center) *
        Matrix4::rotationY(yaw) *
        Matrix4::rotationX(pitch) *
        Matrix4::rotationZ(roll) *
        Matrix4::scale(scale);
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
    appendTransformed(
        dest,
        Mesh3D::box(width, height, depth),
        trs(center, yaw, pitch, roll),
        color
    );
}

// Tapered limb segment (PS1 often used non-uniform scaled boxes).
void addBeam(
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
    addBox(dest, center, width, height, depth, color, yaw, pitch, roll);
}

void addLowSphere(
    Mesh3D& dest,
    const Vector3& center,
    float radius,
    sf::Color color,
    int detail,
    const Vector3& squash = Vector3(1.0f, 1.0f, 1.0f)
) {
    Mesh3D sphere = Mesh3D::sphere(1.0f, headRings(detail), headSeg(detail));
    appendTransformed(
        dest,
        sphere,
        trs(center, 0.0f, 0.0f, 0.0f, Vector3(radius * squash.x, radius * squash.y, radius * squash.z)),
        color
    );
}

// Angular baseball mitt: palm plate + finger slabs + thumb (classic PS1 glove read).
void addMitt(
    Mesh3D& dest,
    const Vector3& palmCenter,
    float yaw,
    float pitch,
    float roll,
    float open,
    int detail
) {
    Matrix4 base = trs(palmCenter, yaw, pitch, roll);
    float spread = 0.018f + open * 0.03f;

    appendTransformed(dest, Mesh3D::box(0.12f, 0.14f, 0.05f), base, mitt);
    appendTransformed(
        dest,
        Mesh3D::box(0.10f, 0.08f, 0.04f),
        base * Matrix4::translation(Vector3(0.0f, 0.09f, 0.01f)),
        mittMid
    );

    // Four finger plates
    for (int i = 0; i < 4; i++) {
        float x = -0.045f + static_cast<float>(i) * (0.03f + spread);
        appendTransformed(
            dest,
            Mesh3D::box(0.026f, 0.09f + open * 0.02f, 0.03f),
            base * Matrix4::translation(Vector3(x, 0.16f + open * 0.02f, 0.015f)) *
                Matrix4::rotationZ((x) * 1.2f),
            (i % 2 == 0) ? mitt : mittMid
        );
    }

    // Thumb
    appendTransformed(
        dest,
        Mesh3D::box(0.035f, 0.08f, 0.032f),
        base * Matrix4::translation(Vector3(-0.08f - open * 0.02f, 0.06f, 0.02f)) *
            Matrix4::rotationZ(-0.7f - open * 0.2f) *
            Matrix4::rotationY(-0.25f),
        mittDark
    );

    // Webbing
    appendTransformed(
        dest,
        Mesh3D::box(0.05f, 0.06f, 0.02f),
        base * Matrix4::translation(Vector3(-0.04f, 0.12f, 0.03f)) *
            Matrix4::rotationZ(-0.35f),
        mittDark
    );

    if (detail >= 1) {
        appendTransformed(
            dest,
            Mesh3D::box(0.08f, 0.015f, 0.015f),
            base * Matrix4::translation(Vector3(0.0f, 0.02f, 0.03f)),
            stitch
        );
    }
}

// Catcher mask cage — orthogonal bars, very PS1.
void addCatcherMask(Mesh3D& dest, const Vector3& headCenter, float sway) {
    Vector3 c = headCenter + Vector3(sway * 0.05f, 0.0f, 0.0f);
    addBox(dest, c + Vector3(0.0f, 0.02f, 0.13f), 0.20f, 0.16f, 0.06f, gearMid);

    // Horizontal bars
    for (int i = 0; i < 4; i++) {
        float y = -0.05f + static_cast<float>(i) * 0.04f;
        addBox(dest, c + Vector3(0.0f, y, 0.165f), 0.16f, 0.012f, 0.012f, gearEdge);
    }
    // Vertical bars
    for (int i = 0; i < 3; i++) {
        float x = -0.05f + static_cast<float>(i) * 0.05f;
        addBox(dest, c + Vector3(x, 0.0f, 0.165f), 0.012f, 0.14f, 0.012f, gearEdge);
    }
    // Chin / throat guard
    addBox(dest, c + Vector3(0.0f, -0.10f, 0.10f), 0.14f, 0.05f, 0.10f, gear);
    // Ear flaps
    addBox(dest, c + Vector3(-0.11f, 0.0f, 0.02f), 0.04f, 0.10f, 0.10f, gear);
    addBox(dest, c + Vector3(0.11f, 0.0f, 0.02f), 0.04f, 0.10f, 0.10f, gear);
}

void addCleat(Mesh3D& dest, const Vector3& center, float yaw) {
    addBox(dest, center, 0.11f, 0.05f, 0.22f, cleats, yaw);
    addBox(dest, center + Vector3(0.0f, -0.02f, 0.01f), 0.12f, 0.025f, 0.24f, cleatSole, yaw);
    // Heel block
    addBox(dest, center + Vector3(0.0f, 0.01f, -0.07f), 0.10f, 0.06f, 0.08f, cleats, yaw);
}

}

PitcherPose BaseballPlayer3D::pitcherIdlePose(float timeSeconds) {
    PitcherPose pose;
    pose.torsoTwist = std::sin(timeSeconds * 1.3f) * 0.04f;
    pose.torsoLean = std::sin(timeSeconds * 0.9f) * 0.02f;
    pose.throwShoulderPitch = std::sin(timeSeconds * 1.1f) * 0.05f;
    pose.gloveShoulderPitch = std::sin(timeSeconds * 1.1f + 0.4f) * 0.04f;
    pose.frontLegLift = std::max(0.0f, std::sin(timeSeconds * 0.7f)) * 0.03f;
    return pose;
}

PitcherPose BaseballPlayer3D::pitcherDeliveryPose(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    PitcherPose pose;

    float gather = smoothstep(0.0f, 0.22f, t);
    float legLift = smoothstep(0.12f, 0.42f, t) * (1.0f - smoothstep(0.45f, 0.62f, t));
    float drive = smoothstep(0.40f, 0.55f, t);
    float release = smoothstep(0.50f, 0.58f, t);
    float follow = smoothstep(0.55f, 1.0f, t);

    pose.torsoTwist = lerp(0.0f, -0.55f, gather) + lerp(0.0f, 0.95f, drive) + lerp(0.0f, 0.25f, follow);
    pose.torsoLean = lerp(0.0f, 0.12f, gather) + lerp(0.0f, 0.35f, drive) + lerp(0.0f, 0.2f, follow);
    pose.frontLegLift = legLift * 0.85f;
    pose.stride = drive * 0.22f + follow * 0.08f;

    pose.throwShoulderPitch =
        lerp(0.25f, -1.1f, gather) +
        lerp(0.0f, 1.8f, release) +
        lerp(0.0f, 0.9f, follow);
    pose.throwShoulderYaw =
        lerp(0.0f, -0.7f, gather) +
        lerp(0.0f, 0.5f, release);
    pose.throwElbow =
        lerp(0.2f, 1.1f, gather) +
        lerp(0.0f, -0.9f, release) +
        lerp(0.0f, -0.3f, follow);

    pose.gloveShoulderPitch = lerp(0.55f, 0.2f, drive) + follow * 0.15f;
    pose.gloveShoulderRoll = lerp(0.35f, -0.1f, drive);

    return pose;
}

CatcherPose BaseballPlayer3D::catcherIdlePose(float timeSeconds) {
    CatcherPose pose;
    pose.torsoSway = std::sin(timeSeconds * 1.05f) * 0.035f;
    pose.crouchBob = std::sin(timeSeconds * 1.7f) * 0.012f;
    pose.mittSide = std::sin(timeSeconds * 0.8f) * 0.02f;
    pose.mittHeight = std::sin(timeSeconds * 1.2f) * 0.015f;
    return pose;
}

CatcherPose BaseballPlayer3D::catcherReceivePose(
    float timeSeconds,
    float aimX,
    float aimY,
    float catcherWorldX,
    float catcherWorldY
) {
    CatcherPose pose = catcherIdlePose(timeSeconds);

    float dx = aimX - catcherWorldX;
    float dy = aimY - catcherWorldY;
    pose.mittSide = std::clamp(-dx * 0.55f, -0.22f, 0.22f);
    pose.mittHeight = std::clamp((dy - 0.15f) * 0.35f, -0.18f, 0.22f);
    pose.mittReach = std::clamp(0.06f + std::abs(dx) * 0.08f, 0.04f, 0.16f);
    pose.gloveOpen = 0.35f;
    pose.torsoSway = std::clamp(-dx * 0.12f, -0.12f, 0.12f);
    return pose;
}

Mesh3D BaseballPlayer3D::pitcher(int detail, const PitcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    // Chunky PS1 athlete: bigger head, clear torso block, rectangular limbs.
    const float footY = 0.05f;
    const float shinH = 0.40f;
    const float thighH = 0.38f;
    const float torsoH = 0.50f;
    const float headR = 0.125f;

    const float hipY = footY + shinH + thighH * 0.9f;
    const float shoulderY = hipY + torsoH;
    const float headY = shoulderY + 0.18f + headR * 0.35f;
    const float stride = pose.stride;
    const float legLift = pose.frontLegLift;

    // --- Legs (box beams, slight taper via width) ---
    addBeam(
        mesh,
        Vector3(0.12f, footY + shinH * 0.5f, -0.02f + stride * 0.12f),
        0.10f,
        shinH,
        0.10f,
        pants,
        0.0f,
        -0.04f + pose.torsoLean * 0.1f
    );
    addBeam(
        mesh,
        Vector3(0.11f, footY + shinH + thighH * 0.42f, -0.01f + stride * 0.08f),
        0.12f,
        thighH,
        0.12f,
        pantsLight,
        0.0f,
        -0.05f
    );

    float leadZ = 0.05f + stride * 0.9f;
    float leadY = footY + legLift * 0.38f;
    addBeam(
        mesh,
        Vector3(-0.12f, leadY + shinH * 0.5f, leadZ),
        0.10f,
        shinH,
        0.10f,
        pants,
        0.0f,
        0.1f + legLift * 0.85f - stride * 0.3f
    );
    addBeam(
        mesh,
        Vector3(-0.11f, hipY - thighH * 0.32f + legLift * 0.12f, 0.03f + stride * 0.4f),
        0.12f,
        thighH,
        0.12f,
        pantsLight,
        0.0f,
        0.15f + legLift * 0.65f
    );

    // Knees as small cubes (PS1 joint markers)
    addBox(mesh, Vector3(0.12f, footY + shinH, stride * 0.1f), 0.09f, 0.08f, 0.09f, pantsLight);
    addBox(
        mesh,
        Vector3(-0.12f, leadY + shinH, leadZ),
        0.09f,
        0.08f,
        0.09f,
        pantsLight,
        0.0f,
        legLift * 0.4f
    );

    addCleat(mesh, Vector3(0.12f, footY * 0.45f, 0.02f + stride * 0.1f), 0.05f);
    addCleat(mesh, Vector3(-0.12f, leadY * 0.4f + 0.02f, leadZ + 0.05f), -0.08f);

    addBox(mesh, Vector3(0.12f, footY + 0.1f, stride * 0.08f), 0.08f, 0.12f, 0.08f, sock);
    addBox(mesh, Vector3(-0.12f, leadY + 0.1f, leadZ), 0.08f, 0.12f, 0.08f, sock);

    // Hips / belt / belt buckle
    addBox(mesh, Vector3(0.0f, hipY, stride * 0.18f), 0.38f, 0.16f, 0.20f, pants, pose.torsoTwist * 0.3f);
    addBox(mesh, Vector3(0.0f, hipY + 0.07f, stride * 0.18f), 0.40f, 0.045f, 0.21f, belt, pose.torsoTwist * 0.3f);
    addBox(mesh, Vector3(0.0f, hipY + 0.07f, stride * 0.18f + 0.105f), 0.05f, 0.04f, 0.02f, stitch, pose.torsoTwist * 0.3f);

    Matrix4 upper =
        Matrix4::translation(Vector3(0.0f, hipY, stride * 0.22f)) *
        Matrix4::rotationY(pose.torsoTwist) *
        Matrix4::rotationX(pose.torsoLean) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));

    auto up = [&](const Vector3& p) { return upper.transformPoint(p); };

    // Torso: double-layer jersey block (chest + abdomen) for PS1 volume
    addBox(mesh, up(Vector3(0.0f, hipY + torsoH * 0.28f, 0.0f)), 0.36f, torsoH * 0.42f, 0.20f, jerseyShade, pose.torsoTwist, pose.torsoLean);
    addBox(mesh, up(Vector3(0.0f, hipY + torsoH * 0.62f, 0.01f)), 0.42f, torsoH * 0.55f, 0.22f, jersey, pose.torsoTwist, pose.torsoLean);
    // Sleeves / undershirt
    addBox(mesh, up(Vector3(-0.22f, shoulderY - 0.06f, 0.0f)), 0.12f, 0.14f, 0.14f, underShirt, pose.torsoTwist, pose.torsoLean);
    addBox(mesh, up(Vector3(0.22f, shoulderY - 0.06f, 0.0f)), 0.12f, 0.14f, 0.14f, underShirt, pose.torsoTwist, pose.torsoLean);
    // Number plate / stripe
    addBox(mesh, up(Vector3(0.0f, hipY + torsoH * 0.55f, 0.12f)), 0.08f, torsoH * 0.5f, 0.02f, logoRed, pose.torsoTwist, pose.torsoLean);
    addBox(mesh, up(Vector3(0.0f, hipY + torsoH * 0.72f, 0.12f)), 0.14f, 0.08f, 0.02f, logoRed, pose.torsoTwist, pose.torsoLean);

    // Shoulders as hard pads
    addBox(mesh, up(Vector3(-0.22f, shoulderY, 0.0f)), 0.14f, 0.10f, 0.14f, jersey, pose.torsoTwist, pose.torsoLean);
    addBox(mesh, up(Vector3(0.22f, shoulderY, 0.0f)), 0.14f, 0.10f, 0.14f, jersey, pose.torsoTwist, pose.torsoLean);

    // Neck + head (slightly large, faceted)
    addBox(mesh, up(Vector3(0.0f, shoulderY + 0.08f, 0.01f)), 0.09f, 0.10f, 0.09f, skin, pose.torsoTwist);
    addLowSphere(mesh, up(Vector3(0.0f, headY, 0.02f)), headR, skin, detail, Vector3(1.0f, 1.05f, 0.95f));
    // Ears
    addBox(mesh, up(Vector3(-0.12f, headY, 0.0f)), 0.03f, 0.05f, 0.04f, skinShadow, pose.torsoTwist);
    addBox(mesh, up(Vector3(0.12f, headY, 0.0f)), 0.03f, 0.05f, 0.04f, skinShadow, pose.torsoTwist);

    // Cap crown + bill (angular)
    addLowSphere(mesh, up(Vector3(0.0f, headY + 0.05f, -0.01f)), headR * 0.95f, capNavy, detail, Vector3(1.05f, 0.7f, 1.05f));
    addBox(mesh, up(Vector3(0.0f, headY + 0.02f, 0.0f)), 0.22f, 0.06f, 0.22f, capNavy, pose.torsoTwist);
    addBox(mesh, up(Vector3(0.0f, headY - 0.01f, 0.14f)), 0.18f, 0.025f, 0.12f, capBill, pose.torsoTwist);
    addBox(mesh, up(Vector3(0.0f, headY + 0.06f, 0.08f)), 0.05f, 0.03f, 0.02f, logoRed, pose.torsoTwist);

    // Glove arm
    float gPitch = 0.5f + pose.gloveShoulderPitch;
    float gRoll = 0.3f + pose.gloveShoulderRoll;
    Vector3 gUpper = up(Vector3(-0.30f, shoulderY - 0.16f, 0.08f));
    Vector3 gFore = up(Vector3(-0.24f, shoulderY - 0.36f, 0.16f));
    Vector3 gMitt = up(Vector3(-0.16f, shoulderY - 0.46f, 0.28f));
    addBeam(mesh, gUpper, 0.09f, 0.28f, 0.09f, jerseyShade, pose.torsoTwist, gPitch, gRoll);
    addBeam(mesh, gFore, 0.075f, 0.24f, 0.075f, skin, pose.torsoTwist * 0.7f, gPitch + 0.25f, gRoll * 0.5f);
    addMitt(mesh, gMitt, pose.torsoTwist + 0.2f, gPitch * 0.4f, gRoll, 0.15f, detail);

    // Throwing arm
    float tPitch = 0.2f + pose.throwShoulderPitch;
    float tYaw = -0.15f + pose.throwShoulderYaw;
    float tElbow = 0.12f + pose.throwElbow;
    Vector3 tUpper = up(Vector3(0.30f, shoulderY - 0.14f, 0.02f));
    Vector3 tFore = up(Vector3(
        0.34f + pose.throwShoulderYaw * 0.08f,
        shoulderY - 0.40f + pose.throwShoulderPitch * 0.04f,
        0.04f + pose.throwShoulderPitch * 0.12f
    ));
    Vector3 tHand = up(Vector3(
        0.36f + pose.throwShoulderYaw * 0.1f,
        shoulderY - 0.56f + pose.throwElbow * 0.05f,
        0.06f + pose.throwShoulderPitch * 0.18f
    ));
    addBeam(mesh, tUpper, 0.09f, 0.28f, 0.09f, jerseyShade, tYaw, tPitch, -0.15f);
    addBeam(mesh, tFore, 0.075f, 0.26f, 0.075f, skin, tYaw * 0.7f, tElbow, -0.1f);
    // Fist / hand block
    addBox(mesh, tHand, 0.07f, 0.07f, 0.09f, skin, tYaw, tElbow);
    addBox(mesh, tHand + Vector3(0.0f, 0.0f, 0.04f), 0.06f, 0.04f, 0.04f, skinShadow, tYaw, tElbow);

    mesh.rebuildNormals();
    return mesh;
}

Mesh3D BaseballPlayer3D::catcher(int detail, const CatcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float bob = pose.crouchBob;
    const float sway = pose.torsoSway;
    const float footY = 0.05f + bob * 0.4f;
    const float hipY = 0.52f + bob;
    const float shoulderY = 1.22f + bob;
    const float headY = 1.52f + bob;

    // Shin guards — layered plates (signature catcher silhouette)
    for (int side = -1; side <= 1; side += 2) {
        float sx = 0.20f * static_cast<float>(side);
        float yaw = -0.18f * static_cast<float>(side);
        addBox(mesh, Vector3(sx, 0.18f + bob, 0.08f), 0.13f, 0.20f, 0.12f, gear, yaw, 0.45f);
        addBox(mesh, Vector3(sx, 0.34f + bob, 0.04f), 0.14f, 0.18f, 0.13f, gearMid, yaw, 0.55f);
        addBox(mesh, Vector3(sx * 1.05f, 0.28f + bob, 0.12f), 0.04f, 0.22f, 0.08f, gearLight, yaw, 0.5f);
        addCleat(mesh, Vector3(sx, footY, 0.12f), yaw * 0.5f);
    }

    // Thighs wide crouch
    addBeam(mesh, Vector3(-0.18f, 0.48f + bob, 0.0f), 0.13f, 0.32f, 0.13f, pants, 0.25f, 0.95f, 0.1f);
    addBeam(mesh, Vector3(0.18f, 0.48f + bob, 0.0f), 0.13f, 0.32f, 0.13f, pants, -0.25f, 0.95f, -0.1f);
    addBox(mesh, Vector3(-0.16f, 0.38f + bob, 0.05f), 0.11f, 0.1f, 0.11f, pantsLight, 0.2f, 0.7f);
    addBox(mesh, Vector3(0.16f, 0.38f + bob, 0.05f), 0.11f, 0.1f, 0.11f, pantsLight, -0.2f, 0.7f);

    // Seat / hips
    addBox(mesh, Vector3(sway * 0.05f, hipY, -0.04f), 0.44f, 0.18f, 0.26f, pants, sway);
    addBox(mesh, Vector3(sway * 0.05f, hipY + 0.08f, -0.04f), 0.46f, 0.05f, 0.27f, belt, sway);
    addBox(mesh, Vector3(sway * 0.05f, hipY + 0.08f, 0.1f), 0.06f, 0.04f, 0.02f, stitch, sway);

    // Chest protector — three stacked plates
    float chestY = (hipY + shoulderY) * 0.5f;
    addBox(mesh, Vector3(sway * 0.08f, chestY - 0.08f, 0.04f), 0.40f, 0.22f, 0.20f, gear, sway);
    addBox(mesh, Vector3(sway * 0.08f, chestY + 0.08f, 0.06f), 0.44f, 0.28f, 0.22f, gearMid, sway);
    addBox(mesh, Vector3(sway * 0.08f, chestY + 0.06f, 0.16f), 0.38f, 0.24f, 0.06f, gearLight, sway);
    // Harness straps
    addBox(mesh, Vector3(sway * 0.08f - 0.12f, chestY + 0.16f, -0.02f), 0.05f, 0.22f, 0.08f, gearEdge, sway);
    addBox(mesh, Vector3(sway * 0.08f + 0.12f, chestY + 0.16f, -0.02f), 0.05f, 0.22f, 0.08f, gearEdge, sway);

    // Shoulder pauldrons
    addBox(mesh, Vector3(-0.26f + sway * 0.05f, shoulderY + 0.02f, 0.02f), 0.16f, 0.12f, 0.16f, gearLight, 0.15f + sway);
    addBox(mesh, Vector3(0.26f + sway * 0.05f, shoulderY + 0.02f, 0.02f), 0.16f, 0.12f, 0.16f, gearLight, -0.15f + sway);
    addBox(mesh, Vector3(0.0f, shoulderY - 0.04f, -0.04f), 0.48f, 0.12f, 0.16f, underShirt, sway);

    // Free arm braced on thigh
    addBeam(mesh, Vector3(0.34f, shoulderY - 0.14f, 0.06f), 0.09f, 0.26f, 0.09f, gear, -0.2f, 0.5f, -0.35f);
    addBeam(mesh, Vector3(0.38f, shoulderY - 0.34f, 0.08f), 0.075f, 0.22f, 0.075f, skin, -0.15f, 0.35f, -0.2f);
    addBox(mesh, Vector3(0.40f, shoulderY - 0.48f, 0.08f), 0.07f, 0.07f, 0.08f, skin);

    // Glove arm + mitt (tracked)
    Vector3 mittPos(
        -0.50f + pose.mittSide,
        shoulderY - 0.28f + pose.mittHeight,
        0.36f + pose.mittReach
    );
    Vector3 forearm(
        -0.40f + pose.mittSide * 0.7f,
        shoulderY - 0.24f + pose.mittHeight * 0.7f,
        0.22f + pose.mittReach * 0.55f
    );
    Vector3 upperArm(
        -0.32f + pose.mittSide * 0.35f,
        shoulderY - 0.12f + pose.mittHeight * 0.3f,
        0.10f + pose.mittReach * 0.25f
    );
    addBeam(mesh, upperArm, 0.095f, 0.26f, 0.095f, gear, 0.1f + pose.mittSide, 0.65f - pose.mittHeight, 0.35f);
    addBeam(mesh, forearm, 0.08f, 0.22f, 0.08f, skin, 0.08f + pose.mittSide, 0.95f, 0.15f);
    addMitt(
        mesh,
        mittPos,
        0.35f + pose.mittSide * 0.8f,
        0.25f - pose.mittHeight * 0.5f,
        0.15f,
        pose.gloveOpen,
        detail
    );

    // Head + helmet + mask
    Vector3 head(sway * 0.05f, headY, 0.02f);
    addLowSphere(mesh, head, 0.125f, skin, detail, Vector3(1.0f, 1.02f, 0.95f));
    addBox(mesh, head + Vector3(0.0f, 0.04f, -0.01f), 0.24f, 0.14f, 0.24f, gear); // helmet shell
    addLowSphere(mesh, head + Vector3(0.0f, 0.06f, -0.02f), 0.13f, gearMid, detail, Vector3(1.1f, 0.65f, 1.1f));
    addCatcherMask(mesh, head, sway);
    addBox(mesh, Vector3(sway * 0.04f, shoulderY + 0.1f, 0.02f), 0.10f, 0.1f, 0.1f, skin);

    mesh.rebuildNormals();
    return mesh;
}
