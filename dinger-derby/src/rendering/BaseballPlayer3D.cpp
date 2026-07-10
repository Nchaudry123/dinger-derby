#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

// Soft baseball kit — close values reduce crease shading between overlaps.
const sf::Color skin(230, 190, 156);
const sf::Color skinDeep(214, 172, 140);
const sf::Color jersey(250, 252, 255);
const sf::Color jerseyDeep(236, 240, 246);
const sf::Color pants(62, 70, 88);
const sf::Color pantsDeep(52, 58, 74);
const sf::Color belt(48, 50, 58);
const sf::Color cleat(44, 44, 52);
const sf::Color cleatSole(56, 56, 64);
const sf::Color cap(42, 62, 112);
const sf::Color capDeep(34, 50, 94);
const sf::Color accent(210, 54, 62);
const sf::Color gear(52, 68, 96);
const sf::Color gearDeep(44, 56, 80);
const sf::Color gearLight(72, 92, 122);
const sf::Color mitt(172, 120, 76);
const sf::Color mittDeep(146, 98, 60);
const sf::Color sock(252, 252, 255);

int ringsFor(int detail) {
    return detail >= 2 ? 12 : (detail >= 1 ? 10 : 9);
}

int segsFor(int detail) {
    return detail >= 2 ? 20 : (detail >= 1 ? 16 : 14);
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

void ball(
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

// One continuous limb: long shaft + oversized joint balls (no bead chain).
void limb(
    Mesh3D& dest,
    const Vector3& from,
    const Vector3& to,
    float radius,
    sf::Color color,
    int detail
) {
    Vector3 delta = to - from;
    float length = delta.magnitude();
    if (length < 0.001f) {
        ball(dest, from, radius, radius, radius, color, detail);
        return;
    }

    Vector3 dir = delta * (1.0f / length);
    float yaw = std::atan2(dir.x, dir.z);
    float pitch = std::acos(std::clamp(dir.y, -1.0f, 1.0f));
    Vector3 mid = (from + to) * 0.5f;

    Mesh3D sphere = Mesh3D::sphere(1.0f, ringsFor(detail), segsFor(detail));
    Matrix4 shaft =
        Matrix4::translation(mid) *
        Matrix4::rotationY(yaw) *
        Matrix4::rotationX(pitch) *
        Matrix4::scale(Vector3(radius * 0.92f, length * 0.58f + radius * 0.4f, radius * 0.92f));
    appendTransformed(dest, sphere, shaft, color);

    ball(dest, from, radius * 1.22f, radius * 1.22f, radius * 1.22f, color, detail);
    ball(dest, to, radius * 1.18f, radius * 1.18f, radius * 1.18f, color, detail);
}

void addCleat(Mesh3D& dest, const Vector3& ankle, float yaw, int detail) {
    ball(dest, ankle + Vector3(0.0f, -0.008f, 0.04f), 0.062f, 0.038f, 0.10f, cleat, detail, yaw);
    ball(dest, ankle + Vector3(0.0f, -0.02f, 0.03f), 0.068f, 0.02f, 0.11f, cleatSole, detail, yaw);
    ball(dest, ankle + Vector3(0.0f, 0.028f, 0.0f), 0.048f, 0.038f, 0.048f, sock, detail);
}

void addMitt(Mesh3D& dest, const Vector3& wrist, const Vector3& forward, float open, int detail) {
    Vector3 f = forward.magnitude() > 0.001f ? forward.normalized() : Vector3(0.0f, 0.0f, 1.0f);
    Vector3 palm = wrist + f * 0.055f;
    float s = 1.0f + open * 0.1f;
    ball(dest, palm, 0.082f * s, 0.09f * s, 0.055f * s, mitt, detail);
    ball(dest, palm + f * 0.025f + Vector3(0.0f, 0.035f, 0.0f), 0.065f * s, 0.05f * s, 0.045f * s, mitt, detail);
    ball(dest, palm + Vector3(-0.04f, 0.01f, 0.0f), 0.038f, 0.05f, 0.035f, mittDeep, detail);
}

// Cap sits ABOVE the skull only — never a horizontal band through the face.
void addCap(Mesh3D& dest, const Vector3& headCenter, float headR, int detail) {
    // Crown on top of head (center well above head center).
    ball(
        dest,
        headCenter + Vector3(0.0f, headR * 0.55f, -headR * 0.05f),
        headR * 0.95f,
        headR * 0.42f,
        headR * 0.95f,
        cap,
        detail
    );
    // Small front bill, only in front of forehead (z+), not wrapping through head.
    ball(
        dest,
        headCenter + Vector3(0.0f, headR * 0.28f, headR * 0.85f),
        headR * 0.55f,
        headR * 0.08f,
        headR * 0.28f,
        capDeep,
        detail
    );
    // Tiny logo on crown front — stays on cap surface.
    ball(
        dest,
        headCenter + Vector3(0.0f, headR * 0.62f, headR * 0.55f),
        headR * 0.1f,
        headR * 0.08f,
        headR * 0.06f,
        accent,
        detail
    );
}

// Catcher helmet: top shell + face mask placed IN FRONT of face only.
void addHelmet(Mesh3D& dest, const Vector3& headCenter, float headR, int detail) {
    // Top/back shell — high on the head.
    ball(
        dest,
        headCenter + Vector3(0.0f, headR * 0.35f, -headR * 0.08f),
        headR * 1.05f,
        headR * 0.7f,
        headR * 1.05f,
        gear,
        detail
    );
    // Ear guards at the sides of the head (not across face).
    ball(dest, headCenter + Vector3(-headR * 0.85f, 0.0f, 0.0f), headR * 0.28f, headR * 0.4f, headR * 0.32f, gearDeep, detail);
    ball(dest, headCenter + Vector3(headR * 0.85f, 0.0f, 0.0f), headR * 0.28f, headR * 0.4f, headR * 0.32f, gearDeep, detail);
    // Mask: thin plate in FRONT of face (center well forward of head).
    ball(
        dest,
        headCenter + Vector3(0.0f, -headR * 0.05f, headR * 1.05f),
        headR * 0.75f,
        headR * 0.7f,
        headR * 0.18f,
        gearDeep,
        detail
    );
    // Chin guard, also in front/below.
    ball(
        dest,
        headCenter + Vector3(0.0f, -headR * 0.7f, headR * 0.65f),
        headR * 0.4f,
        headR * 0.22f,
        headR * 0.28f,
        gearDeep,
        detail
    );
}

}

PitcherPose BaseballPlayer3D::pitcherIdlePose(float timeSeconds) {
    PitcherPose pose;
    pose.torsoTwist = std::sin(timeSeconds * 1.15f) * 0.028f;
    pose.torsoLean = std::sin(timeSeconds * 0.75f) * 0.012f;
    pose.throwShoulderPitch = std::sin(timeSeconds * 0.95f) * 0.03f;
    pose.gloveShoulderPitch = std::sin(timeSeconds * 0.95f + 0.5f) * 0.025f;
    pose.frontLegLift = std::max(0.0f, std::sin(timeSeconds * 0.55f)) * 0.018f;
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
    pose.torsoSway = std::sin(timeSeconds * 0.9f) * 0.025f;
    pose.crouchBob = std::sin(timeSeconds * 1.5f) * 0.009f;
    pose.mittSide = std::sin(timeSeconds * 0.65f) * 0.012f;
    pose.mittHeight = std::sin(timeSeconds * 1.05f) * 0.01f;
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

// ---------------------------------------------------------------------------
// PITCHER — single clean body volumes, cap only on top of head
// ---------------------------------------------------------------------------
Mesh3D BaseballPlayer3D::pitcher(int detail, const PitcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float stride = pose.stride;
    const float legLift = pose.frontLegLift;

    const float hipY = 0.86f;
    const float shoulderY = 1.34f;
    const float headY = 1.58f;
    const float headR = 0.118f;

    // Legs
    Vector3 plantHip(0.085f, hipY, stride * 0.05f);
    Vector3 plantKnee(0.09f, 0.44f, stride * 0.03f);
    Vector3 plantAnkle(0.09f, 0.07f, 0.02f + stride * 0.04f);
    Vector3 leadHip(-0.085f, hipY, 0.02f + stride * 0.16f);
    Vector3 leadKnee(-0.09f, 0.46f + legLift * 0.16f, 0.05f + stride * 0.36f);
    Vector3 leadAnkle(-0.09f, 0.07f + legLift * 0.22f, 0.08f + stride * 0.8f);

    limb(mesh, plantHip, plantKnee, 0.078f, pants, detail);
    limb(mesh, plantKnee, plantAnkle, 0.062f, pantsDeep, detail);
    limb(mesh, leadHip, leadKnee, 0.078f, pants, detail);
    limb(mesh, leadKnee, leadAnkle, 0.062f, pantsDeep, detail);
    addCleat(mesh, plantAnkle, 0.03f, detail);
    addCleat(mesh, leadAnkle, -0.04f, detail);

    // Hips — one mass
    ball(mesh, Vector3(0.0f, hipY, stride * 0.08f), 0.155f, 0.11f, 0.115f, pants, detail);
    ball(mesh, Vector3(0.0f, hipY + 0.045f, stride * 0.08f), 0.14f, 0.03f, 0.105f, belt, detail);

    Matrix4 upper =
        Matrix4::translation(Vector3(0.0f, hipY, stride * 0.1f)) *
        Matrix4::rotationY(pose.torsoTwist) *
        Matrix4::rotationX(pose.torsoLean) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));
    auto U = [&](const Vector3& p) { return upper.transformPoint(p); };

    // ONE torso volume from belt line to shoulders (same color throughout).
    Vector3 torsoC = U(Vector3(0.0f, (hipY + shoulderY) * 0.52f, 0.01f));
    float torsoH = (shoulderY - hipY) * 0.62f;
    ball(mesh, torsoC, 0.145f, torsoH, 0.11f, jersey, detail);

    // Shoulders slightly inside torso radius so they fuse.
    Vector3 shL = U(Vector3(-0.14f, shoulderY - 0.02f, 0.0f));
    Vector3 shR = U(Vector3(0.14f, shoulderY - 0.02f, 0.0f));
    ball(mesh, shL, 0.08f, 0.07f, 0.08f, jersey, detail);
    ball(mesh, shR, 0.08f, 0.07f, 0.08f, jersey, detail);

    // Neck: very short, thick, buried in torso and chin.
    Vector3 neckA = U(Vector3(0.0f, shoulderY + 0.02f, 0.012f));
    Vector3 neckB = U(Vector3(0.0f, headY - headR * 0.65f, 0.018f));
    limb(mesh, neckA, neckB, 0.045f, skin, detail);

    // Head — clean sphere, then cap only on top.
    Vector3 head = U(Vector3(0.0f, headY, 0.02f));
    ball(mesh, head, headR, headR * 1.04f, headR * 0.97f, skin, detail);
    addCap(mesh, head, headR, detail);

    // Soft jersey mark on chest surface only.
    ball(mesh, U(Vector3(0.0f, (hipY + shoulderY) * 0.52f, 0.10f)), 0.025f, 0.06f, 0.014f, accent, detail);

    // Arms
    Vector3 gElbow = U(Vector3(-0.22f, shoulderY - 0.17f, 0.07f));
    Vector3 gWrist = U(Vector3(-0.13f, shoulderY - 0.34f, 0.16f));
    limb(mesh, shL, gElbow, 0.055f, jerseyDeep, detail);
    limb(mesh, gElbow, gWrist, 0.046f, skin, detail);
    addMitt(mesh, gWrist, Vector3(0.15f, -0.1f, 0.9f), 0.2f + pose.gloveShoulderRoll * 0.1f, detail);

    Vector3 tElbow = U(Vector3(
        0.22f + pose.throwShoulderYaw * 0.05f,
        shoulderY - 0.19f + pose.throwShoulderPitch * 0.035f,
        0.03f + pose.throwShoulderPitch * 0.08f
    ));
    Vector3 tWrist = U(Vector3(
        0.24f + pose.throwShoulderYaw * 0.08f,
        shoulderY - 0.40f + pose.throwElbow * 0.035f,
        0.05f + pose.throwShoulderPitch * 0.14f
    ));
    limb(mesh, shR, tElbow, 0.055f, jerseyDeep, detail);
    limb(mesh, tElbow, tWrist, 0.046f, skin, detail);
    ball(mesh, tWrist + Vector3(0.01f, -0.01f, 0.015f), 0.04f, 0.04f, 0.045f, skinDeep, detail);

    mesh.rebuildNormals();
    return mesh;
}

// ---------------------------------------------------------------------------
// CATCHER — helmet not cutting face, protector as one soft shell
// ---------------------------------------------------------------------------
Mesh3D BaseballPlayer3D::catcher(int detail, const CatcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float bob = pose.crouchBob;
    const float sway = pose.torsoSway;

    const float hipY = 0.46f + bob;
    const float shoulderY = 1.08f + bob;
    const float headY = 1.30f + bob;
    const float headR = 0.112f;

    for (int side = -1; side <= 1; side += 2) {
        float s = static_cast<float>(side);
        Vector3 hipJ(0.10f * s + sway * 0.015f, hipY, 0.0f);
        Vector3 knee(0.13f * s, 0.30f + bob, 0.05f);
        Vector3 ankle(0.12f * s, 0.07f + bob * 0.2f, 0.10f);
        limb(mesh, hipJ, knee, 0.08f, pants, detail);
        limb(mesh, knee, ankle, 0.062f, pantsDeep, detail);
        // Shin pad: single soft bulge on shin front.
        ball(mesh, (knee + ankle) * 0.5f + Vector3(0.0f, 0.0f, 0.028f), 0.065f, 0.095f, 0.05f, gear, detail);
        addCleat(mesh, ankle, -0.07f * s, detail);
    }

    ball(mesh, Vector3(sway * 0.025f, hipY, -0.02f), 0.16f, 0.11f, 0.12f, pants, detail);
    ball(mesh, Vector3(sway * 0.025f, hipY + 0.04f, -0.02f), 0.145f, 0.028f, 0.11f, belt, detail);

    // One torso + one gear shell (same center family).
    Vector3 torso(sway * 0.03f, (hipY + shoulderY) * 0.52f, 0.02f);
    float torsoH = (shoulderY - hipY) * 0.58f;
    ball(mesh, torso, 0.145f, torsoH, 0.115f, jerseyDeep, detail);
    ball(mesh, torso + Vector3(0.0f, 0.015f, 0.04f), 0.15f, torsoH * 0.9f, 0.09f, gear, detail);

    Vector3 shL(-0.135f + sway * 0.02f, shoulderY, 0.02f);
    Vector3 shR(0.135f + sway * 0.02f, shoulderY, 0.02f);
    ball(mesh, shL, 0.08f, 0.07f, 0.08f, gearLight, detail);
    ball(mesh, shR, 0.08f, 0.07f, 0.08f, gearLight, detail);

    Vector3 neckA(sway * 0.02f, shoulderY + 0.02f, 0.02f);
    Vector3 neckB(sway * 0.02f, headY - headR * 0.6f, 0.02f);
    limb(mesh, neckA, neckB, 0.044f, skin, detail);

    Vector3 head(sway * 0.02f, headY, 0.02f);
    ball(mesh, head, headR, headR * 1.03f, headR * 0.97f, skin, detail);
    addHelmet(mesh, head, headR, detail);

    Vector3 freeElbow(0.24f, shoulderY - 0.14f, 0.05f);
    Vector3 freeWrist(0.27f, shoulderY - 0.32f, 0.06f);
    limb(mesh, shR, freeElbow, 0.052f, gearDeep, detail);
    limb(mesh, freeElbow, freeWrist, 0.044f, skin, detail);
    ball(mesh, freeWrist, 0.038f, 0.038f, 0.042f, skinDeep, detail);

    Vector3 mittPos(
        -0.38f + pose.mittSide,
        shoulderY - 0.20f + pose.mittHeight,
        0.26f + pose.mittReach
    );
    Vector3 gElbow(
        -0.28f + pose.mittSide * 0.45f,
        shoulderY - 0.13f + pose.mittHeight * 0.35f,
        0.12f + pose.mittReach * 0.28f
    );
    Vector3 gShoulder = shL + Vector3(pose.mittSide * 0.06f, pose.mittHeight * 0.04f, pose.mittReach * 0.04f);
    limb(mesh, gShoulder, gElbow, 0.054f, gearDeep, detail);
    limb(mesh, gElbow, mittPos, 0.046f, skin, detail);
    addMitt(mesh, mittPos, Vector3(-0.1f + pose.mittSide, pose.mittHeight * 0.15f, 0.95f), pose.gloveOpen, detail);

    mesh.rebuildNormals();
    return mesh;
}
