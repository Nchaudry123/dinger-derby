#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

// Soft, cohesive kit palette — fewer harsh material jumps.
const sf::Color skin(212, 166, 130);
const sf::Color skinDeep(188, 140, 108);
const sf::Color jersey(234, 236, 240);
const sf::Color jerseyShade(210, 214, 222);
const sf::Color pants(52, 58, 72);
const sf::Color pantsShade(42, 48, 60);
const sf::Color belt(32, 34, 40);
const sf::Color cleats(28, 28, 32);
const sf::Color capNavy(28, 42, 78);
const sf::Color logoRed(188, 48, 54);
const sf::Color gear(36, 48, 68);
const sf::Color gearShade(28, 38, 54);
const sf::Color mitt(150, 102, 62);
const sf::Color mittShade(122, 80, 48);
const sf::Color sock(240, 240, 244);

int ringsFor(int detail) {
    return detail >= 2 ? 9 : (detail >= 1 ? 7 : 6);
}

int segsFor(int detail) {
    return detail >= 2 ? 14 : (detail >= 1 ? 11 : 9);
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

// Ellipsoid blob — primary building block for a fused body.
void addBlob(
    Mesh3D& dest,
    const Vector3& center,
    float rx,
    float ry,
    float rz,
    sf::Color color,
    int detail,
    float yaw = 0.0f,
    float pitch = 0.0f,
    float roll = 0.0f
) {
    Mesh3D sphere = Mesh3D::sphere(1.0f, ringsFor(detail), segsFor(detail));
    Matrix4 transform =
        Matrix4::translation(center) *
        Matrix4::rotationY(yaw) *
        Matrix4::rotationX(pitch) *
        Matrix4::rotationZ(roll) *
        Matrix4::scale(Vector3(rx, ry, rz));
    appendTransformed(dest, sphere, transform, color);
}

// Limb as overlapping spheres along a segment (reads as one smooth piece).
void addLimbChain(
    Mesh3D& dest,
    const Vector3& from,
    const Vector3& to,
    float radius,
    sf::Color color,
    int detail,
    int beads = 4
) {
    beads = std::max(beads, 2);
    for (int i = 0; i < beads; i++) {
        float t = static_cast<float>(i) / static_cast<float>(beads - 1);
        // Slight taper toward ends.
        float flare = 1.0f - 0.12f * std::abs(t - 0.5f) * 2.0f;
        Vector3 p = from + (to - from) * t;
        addBlob(dest, p, radius * flare, radius * flare, radius * flare, color, detail);
    }
    // Joint caps for fusion with torso/feet
    addBlob(dest, from, radius * 1.08f, radius * 1.08f, radius * 1.08f, color, detail);
    addBlob(dest, to, radius * 1.05f, radius * 1.05f, radius * 1.05f, color, detail);
}

void addSoftMitt(
    Mesh3D& dest,
    const Vector3& center,
    float open,
    int detail,
    float yaw = 0.0f,
    float pitch = 0.0f
) {
    float o = 1.0f + open * 0.15f;
    addBlob(dest, center, 0.09f * o, 0.10f * o, 0.06f * o, mitt, detail, yaw, pitch);
    addBlob(
        dest,
        center + Vector3(-0.03f, 0.04f, 0.03f),
        0.055f * o,
        0.06f * o,
        0.04f * o,
        mittShade,
        detail,
        yaw,
        pitch
    );
    addBlob(
        dest,
        center + Vector3(0.04f, 0.05f, 0.02f),
        0.05f * o,
        0.07f * o,
        0.035f * o,
        mitt,
        detail,
        yaw + 0.15f,
        pitch
    );
    addBlob(
        dest,
        center + Vector3(-0.06f, 0.0f, 0.02f),
        0.04f,
        0.05f,
        0.035f,
        mittShade,
        detail,
        yaw - 0.4f,
        pitch
    );
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

    const float footY = 0.04f;
    const float shinLen = 0.38f;
    const float thighLen = 0.36f;
    const float torsoH = 0.48f;
    const float headR = 0.11f;

    const float hipY = footY + shinLen + thighLen * 0.88f;
    const float shoulderY = hipY + torsoH * 0.92f;
    const float headY = shoulderY + 0.14f + headR;
    const float stride = pose.stride;
    const float legLift = pose.frontLegLift;

    // Feet / lower legs — plant (right) and lead (left)
    Vector3 plantAnkle(0.10f, footY + 0.04f, 0.02f + stride * 0.1f);
    Vector3 plantKnee(0.10f, footY + shinLen, 0.0f + stride * 0.08f);
    Vector3 plantHip(0.09f, hipY - 0.02f, stride * 0.12f);

    Vector3 leadAnkle(-0.10f, footY + 0.04f + legLift * 0.32f, 0.06f + stride * 0.85f);
    Vector3 leadKnee(-0.10f, footY + shinLen + legLift * 0.2f, 0.04f + stride * 0.45f);
    Vector3 leadHip(-0.09f, hipY - 0.02f, 0.02f + stride * 0.25f);

    addLimbChain(mesh, plantAnkle, plantKnee, 0.055f, pantsShade, detail, 5);
    addLimbChain(mesh, plantKnee, plantHip, 0.065f, pants, detail, 5);
    addLimbChain(mesh, leadAnkle, leadKnee, 0.055f, pantsShade, detail, 5);
    addLimbChain(mesh, leadKnee, leadHip, 0.065f, pants, detail, 5);

    // Cleats as soft ovals fused to ankles
    addBlob(mesh, plantAnkle + Vector3(0.0f, -0.02f, 0.03f), 0.07f, 0.035f, 0.11f, cleats, detail);
    addBlob(mesh, leadAnkle + Vector3(0.0f, -0.02f, 0.03f), 0.07f, 0.035f, 0.11f, cleats, detail);
    addBlob(mesh, plantAnkle + Vector3(0.0f, 0.04f, 0.0f), 0.05f, 0.06f, 0.05f, sock, detail);
    addBlob(mesh, leadAnkle + Vector3(0.0f, 0.04f, 0.0f), 0.05f, 0.06f, 0.05f, sock, detail);

    // Hips as one fused mass
    addBlob(mesh, Vector3(0.0f, hipY, stride * 0.15f), 0.17f, 0.11f, 0.12f, pants, detail);
    addBlob(mesh, Vector3(0.0f, hipY + 0.05f, stride * 0.15f), 0.16f, 0.04f, 0.11f, belt, detail);

    Matrix4 upper =
        Matrix4::translation(Vector3(0.0f, hipY, stride * 0.2f)) *
        Matrix4::rotationY(pose.torsoTwist) *
        Matrix4::rotationX(pose.torsoLean) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));
    auto up = [&](const Vector3& p) { return upper.transformPoint(p); };

    // Torso: overlapping ellipsoids so chest/abdomen read as one body
    addBlob(mesh, up(Vector3(0.0f, hipY + torsoH * 0.32f, 0.0f)), 0.15f, 0.16f, 0.11f, jerseyShade, detail);
    addBlob(mesh, up(Vector3(0.0f, hipY + torsoH * 0.58f, 0.01f)), 0.17f, 0.18f, 0.12f, jersey, detail);
    addBlob(mesh, up(Vector3(0.0f, hipY + torsoH * 0.78f, 0.0f)), 0.16f, 0.12f, 0.12f, jersey, detail);
    // Soft stripe (not a floating plate)
    addBlob(mesh, up(Vector3(0.0f, hipY + torsoH * 0.55f, 0.10f)), 0.04f, 0.14f, 0.02f, logoRed, detail);

    // Shoulders fused into torso
    Vector3 shL = up(Vector3(-0.18f, shoulderY - 0.02f, 0.0f));
    Vector3 shR = up(Vector3(0.18f, shoulderY - 0.02f, 0.0f));
    addBlob(mesh, shL, 0.09f, 0.08f, 0.09f, jersey, detail);
    addBlob(mesh, shR, 0.09f, 0.08f, 0.09f, jersey, detail);
    addBlob(mesh, up(Vector3(0.0f, shoulderY - 0.06f, 0.0f)), 0.14f, 0.08f, 0.10f, jerseyShade, detail);

    // Neck + head
    addBlob(mesh, up(Vector3(0.0f, shoulderY + 0.06f, 0.01f)), 0.05f, 0.06f, 0.05f, skin, detail);
    addBlob(mesh, up(Vector3(0.0f, headY, 0.02f)), headR, headR * 1.05f, headR * 0.95f, skin, detail);
    // Cap as soft dome + small brim blob
    addBlob(mesh, up(Vector3(0.0f, headY + 0.04f, -0.01f)), headR * 1.02f, headR * 0.55f, headR * 1.0f, capNavy, detail);
    addBlob(mesh, up(Vector3(0.0f, headY - 0.01f, 0.11f)), 0.08f, 0.02f, 0.06f, capNavy, detail);

    // Glove arm chain
    Vector3 gShoulder = up(Vector3(-0.22f, shoulderY - 0.04f, 0.04f));
    Vector3 gElbow = up(Vector3(-0.28f, shoulderY - 0.22f, 0.12f));
    Vector3 gWrist = up(Vector3(-0.18f, shoulderY - 0.40f, 0.24f));
    addLimbChain(mesh, gShoulder, gElbow, 0.048f, jerseyShade, detail, 4);
    addLimbChain(mesh, gElbow, gWrist, 0.042f, skin, detail, 4);
    addSoftMitt(mesh, gWrist + Vector3(0.02f, -0.02f, 0.04f), 0.2f + pose.gloveShoulderRoll * 0.1f, detail, 0.2f, 0.3f);

    // Throwing arm chain
    Vector3 tShoulder = up(Vector3(0.22f, shoulderY - 0.04f, 0.02f));
    Vector3 tElbow = up(Vector3(
        0.30f + pose.throwShoulderYaw * 0.06f,
        shoulderY - 0.24f + pose.throwShoulderPitch * 0.04f,
        0.04f + pose.throwShoulderPitch * 0.1f
    ));
    Vector3 tWrist = up(Vector3(
        0.32f + pose.throwShoulderYaw * 0.1f,
        shoulderY - 0.48f + pose.throwElbow * 0.05f,
        0.06f + pose.throwShoulderPitch * 0.16f
    ));
    addLimbChain(mesh, tShoulder, tElbow, 0.048f, jerseyShade, detail, 4);
    addLimbChain(mesh, tElbow, tWrist, 0.042f, skin, detail, 4);
    addBlob(mesh, tWrist, 0.04f, 0.04f, 0.05f, skinDeep, detail);

    mesh.rebuildNormals();
    return mesh;
}

Mesh3D BaseballPlayer3D::catcher(int detail, const CatcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float bob = pose.crouchBob;
    const float sway = pose.torsoSway;
    const float footY = 0.05f + bob * 0.35f;
    const float hipY = 0.52f + bob;
    const float shoulderY = 1.20f + bob;
    const float headY = 1.48f + bob;

    // Legs / shinguard mass as smooth volumes (not separate plates)
    for (int side = -1; side <= 1; side += 2) {
        float s = static_cast<float>(side);
        Vector3 ankle(0.16f * s, footY + 0.03f, 0.10f);
        Vector3 knee(0.17f * s, 0.32f + bob, 0.06f);
        Vector3 hip(0.14f * s, hipY - 0.04f, 0.0f);
        addLimbChain(mesh, ankle, knee, 0.07f, gearShade, detail, 5);
        addLimbChain(mesh, knee, hip, 0.075f, pants, detail, 4);
        // Guard bulge fused on shin
        addBlob(mesh, (ankle + knee) * 0.5f + Vector3(0.0f, 0.0f, 0.04f), 0.08f, 0.12f, 0.06f, gear, detail);
        addBlob(mesh, ankle + Vector3(0.0f, -0.02f, 0.03f), 0.07f, 0.03f, 0.10f, cleats, detail);
    }

    // Hips / seat — one mass
    addBlob(mesh, Vector3(sway * 0.04f, hipY, -0.02f), 0.20f, 0.12f, 0.14f, pants, detail);
    addBlob(mesh, Vector3(sway * 0.04f, hipY + 0.05f, -0.02f), 0.18f, 0.04f, 0.12f, belt, detail);

    // Chest protector as rounded shell fused to torso
    float chestY = (hipY + shoulderY) * 0.52f;
    addBlob(mesh, Vector3(sway * 0.06f, chestY, 0.02f), 0.18f, 0.24f, 0.14f, jerseyShade, detail);
    addBlob(mesh, Vector3(sway * 0.06f, chestY + 0.02f, 0.08f), 0.17f, 0.22f, 0.08f, gear, detail);
    addBlob(mesh, Vector3(sway * 0.06f, chestY + 0.08f, 0.10f), 0.15f, 0.14f, 0.06f, gearShade, detail);

    // Shoulders
    Vector3 shL(-0.20f + sway * 0.04f, shoulderY, 0.02f);
    Vector3 shR(0.20f + sway * 0.04f, shoulderY, 0.02f);
    addBlob(mesh, shL, 0.09f, 0.08f, 0.09f, gear, detail);
    addBlob(mesh, shR, 0.09f, 0.08f, 0.09f, gear, detail);
    addBlob(mesh, Vector3(sway * 0.04f, shoulderY - 0.04f, 0.0f), 0.16f, 0.08f, 0.10f, gearShade, detail);

    // Free arm resting
    Vector3 freeElbow(0.30f, shoulderY - 0.18f, 0.06f);
    Vector3 freeWrist(0.34f, shoulderY - 0.38f, 0.08f);
    addLimbChain(mesh, shR, freeElbow, 0.05f, gearShade, detail, 4);
    addLimbChain(mesh, freeElbow, freeWrist, 0.042f, skin, detail, 4);
    addBlob(mesh, freeWrist, 0.04f, 0.04f, 0.04f, skinDeep, detail);

    // Glove arm + mitt
    Vector3 mittPos(
        -0.46f + pose.mittSide,
        shoulderY - 0.26f + pose.mittHeight,
        0.34f + pose.mittReach
    );
    Vector3 gElbow(
        -0.36f + pose.mittSide * 0.65f,
        shoulderY - 0.18f + pose.mittHeight * 0.55f,
        0.18f + pose.mittReach * 0.45f
    );
    Vector3 gShoulder = shL + Vector3(pose.mittSide * 0.15f, pose.mittHeight * 0.1f, pose.mittReach * 0.1f);
    addLimbChain(mesh, gShoulder, gElbow, 0.052f, gearShade, detail, 4);
    addLimbChain(mesh, gElbow, mittPos, 0.044f, skin, detail, 4);
    addSoftMitt(mesh, mittPos, pose.gloveOpen, detail, 0.25f + pose.mittSide, 0.2f);

    // Head + helmet as fused forms (no cage grid)
    Vector3 head(sway * 0.04f, headY, 0.02f);
    addBlob(mesh, head, 0.115f, 0.12f, 0.11f, skin, detail);
    addBlob(mesh, head + Vector3(0.0f, 0.04f, -0.01f), 0.125f, 0.08f, 0.125f, gear, detail);
    addBlob(mesh, head + Vector3(0.0f, 0.0f, 0.08f), 0.10f, 0.09f, 0.06f, gearShade, detail); // mask bulk
    addBlob(mesh, head + Vector3(0.0f, -0.06f, 0.06f), 0.07f, 0.04f, 0.06f, gearShade, detail); // chin
    addBlob(mesh, Vector3(sway * 0.03f, shoulderY + 0.08f, 0.02f), 0.05f, 0.06f, 0.05f, skin, detail);

    mesh.rebuildNormals();
    return mesh;
}
