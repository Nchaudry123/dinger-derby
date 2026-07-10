#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

// Soft sports kit — close values so folds don't create harsh black creases.
const sf::Color skin(228, 188, 154);
const sf::Color skinDeep(210, 168, 136);
const sf::Color jersey(248, 250, 252);
const sf::Color jerseyDeep(232, 236, 242);
const sf::Color pants(58, 66, 84);
const sf::Color pantsDeep(50, 56, 72);
const sf::Color belt(46, 48, 56);
const sf::Color cleat(42, 42, 50);
const sf::Color cleatSole(54, 54, 62);
const sf::Color cap(40, 60, 110);
const sf::Color capDeep(32, 48, 92);
const sf::Color accent(208, 52, 60);
const sf::Color gear(50, 66, 94);
const sf::Color gearDeep(42, 54, 78);
const sf::Color gearLight(70, 90, 120);
const sf::Color mitt(168, 116, 72);
const sf::Color mittDeep(140, 92, 56);
const sf::Color sock(250, 250, 252);

// Smooth enough for Gouraud without looking faceted.
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

// Single continuous limb: one shaft ellipsoid between joints + large end balls
// that swallow the joint so you never see a bead chain.
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
    // Shaft long enough to bury ends inside joint spheres.
    Matrix4 shaft =
        Matrix4::translation(mid) *
        Matrix4::rotationY(yaw) *
        Matrix4::rotationX(pitch) *
        Matrix4::scale(Vector3(radius * 0.95f, length * 0.55f + radius * 0.35f, radius * 0.95f));
    appendTransformed(dest, sphere, shaft, color);

    // Large joint balls — key to "one mesh" look (no snowman segments).
    ball(dest, from, radius * 1.18f, radius * 1.18f, radius * 1.18f, color, detail);
    ball(dest, to, radius * 1.15f, radius * 1.15f, radius * 1.15f, color, detail);
}

void addCleat(Mesh3D& dest, const Vector3& ankle, float yaw, int detail) {
    // One solid shoe mass, deeply overlapped with ankle joint.
    ball(dest, ankle + Vector3(0.0f, -0.005f, 0.03f), 0.065f, 0.04f, 0.10f, cleat, detail, yaw);
    ball(dest, ankle + Vector3(0.0f, -0.02f, 0.02f), 0.07f, 0.022f, 0.115f, cleatSole, detail, yaw);
    ball(dest, ankle + Vector3(0.0f, 0.03f, 0.0f), 0.05f, 0.04f, 0.05f, sock, detail);
}

void addMitt(Mesh3D& dest, const Vector3& wrist, const Vector3& forward, float open, int detail) {
    Vector3 f = forward.magnitude() > 0.001f ? forward.normalized() : Vector3(0.0f, 0.0f, 1.0f);
    Vector3 palm = wrist + f * 0.05f;
    float s = 1.0f + open * 0.12f;
    // Palm + pocket as two heavily overlapped volumes (not finger sticks).
    ball(dest, palm, 0.08f * s, 0.09f * s, 0.055f * s, mitt, detail);
    ball(dest, palm + f * 0.03f + Vector3(0.0f, 0.04f, 0.0f), 0.07f * s, 0.055f * s, 0.05f * s, mitt, detail);
    ball(dest, palm + Vector3(-0.045f, 0.015f, 0.0f), 0.04f, 0.055f, 0.04f, mittDeep, detail);
}

void addCap(Mesh3D& dest, const Vector3& head, float r, int detail) {
    // Crown engulfs the top of the skull so it doesn't float.
    ball(dest, head + Vector3(0.0f, r * 0.15f, -r * 0.02f), r * 1.12f, r * 0.62f, r * 1.12f, cap, detail);
    ball(dest, head + Vector3(0.0f, r * 0.0f, 0.0f), r * 1.05f, r * 0.28f, r * 1.05f, capDeep, detail);
    ball(dest, head + Vector3(0.0f, -r * 0.02f, r * 0.7f), r * 0.72f, r * 0.1f, r * 0.4f, capDeep, detail);
    ball(dest, head + Vector3(0.0f, r * 0.22f, r * 0.55f), r * 0.11f, r * 0.09f, r * 0.07f, accent, detail);
}

void addHelmet(Mesh3D& dest, const Vector3& head, float r, int detail) {
    // Full wrap around the head — one continuous helmet form.
    ball(dest, head + Vector3(0.0f, r * 0.08f, -r * 0.03f), r * 1.22f, r * 0.95f, r * 1.22f, gear, detail);
    ball(dest, head + Vector3(0.0f, r * 0.18f, 0.0f), r * 1.15f, r * 0.6f, r * 1.15f, gearDeep, detail);
    ball(dest, head + Vector3(0.0f, -r * 0.08f, r * 0.72f), r * 0.95f, r * 0.85f, r * 0.48f, gearDeep, detail);
    ball(dest, head + Vector3(0.0f, -r * 0.55f, r * 0.4f), r * 0.55f, r * 0.3f, r * 0.42f, gearDeep, detail);
    ball(dest, head + Vector3(-r * 0.9f, 0.0f, 0.0f), r * 0.32f, r * 0.42f, r * 0.38f, gear, detail);
    ball(dest, head + Vector3(r * 0.9f, 0.0f, 0.0f), r * 0.32f, r * 0.42f, r * 0.38f, gear, detail);
}

}

PitcherPose BaseballPlayer3D::pitcherIdlePose(float timeSeconds) {
    PitcherPose pose;
    pose.torsoTwist = std::sin(timeSeconds * 1.2f) * 0.03f;
    pose.torsoLean = std::sin(timeSeconds * 0.8f) * 0.015f;
    pose.throwShoulderPitch = std::sin(timeSeconds * 1.0f) * 0.035f;
    pose.gloveShoulderPitch = std::sin(timeSeconds * 1.0f + 0.5f) * 0.03f;
    pose.frontLegLift = std::max(0.0f, std::sin(timeSeconds * 0.6f)) * 0.02f;
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
    pose.torsoSway = std::sin(timeSeconds * 0.95f) * 0.028f;
    pose.crouchBob = std::sin(timeSeconds * 1.55f) * 0.01f;
    pose.mittSide = std::sin(timeSeconds * 0.7f) * 0.015f;
    pose.mittHeight = std::sin(timeSeconds * 1.1f) * 0.01f;
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

    const float stride = pose.stride;
    const float legLift = pose.frontLegLift;

    // Landmark heights for a ~1.72 unit athlete.
    const float hipY = 0.88f;
    const float shoulderY = 1.36f;
    const float headY = 1.60f;
    const float headR = 0.12f;

    // ---- Legs: two segments each, large joint radii ----
    Vector3 plantHip(0.09f, hipY, stride * 0.06f);
    Vector3 plantKnee(0.095f, 0.45f, stride * 0.04f);
    Vector3 plantAnkle(0.095f, 0.07f, 0.02f + stride * 0.05f);

    Vector3 leadHip(-0.09f, hipY, 0.02f + stride * 0.18f);
    Vector3 leadKnee(-0.095f, 0.47f + legLift * 0.18f, 0.05f + stride * 0.38f);
    Vector3 leadAnkle(-0.095f, 0.07f + legLift * 0.25f, 0.08f + stride * 0.82f);

    limb(mesh, plantHip, plantKnee, 0.08f, pants, detail);
    limb(mesh, plantKnee, plantAnkle, 0.065f, pantsDeep, detail);
    limb(mesh, leadHip, leadKnee, 0.08f, pants, detail);
    limb(mesh, leadKnee, leadAnkle, 0.065f, pantsDeep, detail);
    addCleat(mesh, plantAnkle, 0.04f, detail);
    addCleat(mesh, leadAnkle, -0.05f, detail);

    // ---- Pelvis: ONE mass connecting both hips ----
    ball(mesh, Vector3(0.0f, hipY, stride * 0.1f), 0.16f, 0.12f, 0.12f, pants, detail);
    ball(mesh, Vector3(0.0f, hipY + 0.05f, stride * 0.1f), 0.145f, 0.035f, 0.11f, belt, detail);

    // Upper body transform
    Matrix4 upper =
        Matrix4::translation(Vector3(0.0f, hipY, stride * 0.12f)) *
        Matrix4::rotationY(pose.torsoTwist) *
        Matrix4::rotationX(pose.torsoLean) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));
    auto U = [&](const Vector3& p) { return upper.transformPoint(p); };

    // ---- Torso: ONE primary jersey volume (not stacked rings) ----
    // Tall ellipsoid from belt to collar — the body is a single shape.
    Vector3 torsoCenter = U(Vector3(0.0f, (hipY + shoulderY) * 0.5f + 0.02f, 0.01f));
    float torsoHalfH = (shoulderY - hipY) * 0.58f;
    ball(mesh, torsoCenter, 0.15f, torsoHalfH, 0.115f, jersey, detail);

    // Upper chest fill — same jersey color, deep overlap (no ring seam).
    ball(mesh, U(Vector3(0.0f, shoulderY - 0.08f, 0.01f)), 0.145f, 0.12f, 0.11f, jersey, detail);

    // ---- Shoulders: sunk deep into torso ----
    Vector3 shL = U(Vector3(-0.155f, shoulderY - 0.02f, 0.0f));
    Vector3 shR = U(Vector3(0.155f, shoulderY - 0.02f, 0.0f));
    ball(mesh, shL, 0.09f, 0.08f, 0.09f, jersey, detail);
    ball(mesh, shR, 0.09f, 0.08f, 0.09f, jersey, detail);
    ball(mesh, U(Vector3(0.0f, shoulderY + 0.02f, 0.01f)), 0.08f, 0.06f, 0.07f, jerseyDeep, detail);

    // Soft jersey stripe painted on the torso surface
    ball(mesh, U(Vector3(0.0f, (hipY + shoulderY) * 0.52f, 0.10f)), 0.028f, 0.08f, 0.016f, accent, detail);

    // ---- Neck + head: short thick limb buried in collar and skull ----
    Vector3 neckBase = U(Vector3(0.0f, shoulderY + 0.04f, 0.015f));
    Vector3 neckTop = U(Vector3(0.0f, headY - headR * 0.55f, 0.02f));
    limb(mesh, neckBase, neckTop, 0.048f, skin, detail);

    Vector3 head = U(Vector3(0.0f, headY, 0.02f));
    ball(mesh, head, headR, headR * 1.05f, headR * 0.97f, skin, detail);
    addCap(mesh, head, headR, detail);

    // ---- Arms: upper + forearm only ----
    Vector3 gElbow = U(Vector3(-0.24f, shoulderY - 0.18f, 0.08f));
    Vector3 gWrist = U(Vector3(-0.14f, shoulderY - 0.36f, 0.18f));
    limb(mesh, shL, gElbow, 0.058f, jerseyDeep, detail);
    limb(mesh, gElbow, gWrist, 0.048f, skin, detail);
    addMitt(mesh, gWrist, Vector3(0.2f, -0.15f, 0.85f), 0.25f + pose.gloveShoulderRoll * 0.1f, detail);

    Vector3 tElbow = U(Vector3(
        0.24f + pose.throwShoulderYaw * 0.06f,
        shoulderY - 0.20f + pose.throwShoulderPitch * 0.04f,
        0.03f + pose.throwShoulderPitch * 0.09f
    ));
    Vector3 tWrist = U(Vector3(
        0.26f + pose.throwShoulderYaw * 0.09f,
        shoulderY - 0.42f + pose.throwElbow * 0.04f,
        0.05f + pose.throwShoulderPitch * 0.15f
    ));
    limb(mesh, shR, tElbow, 0.058f, jerseyDeep, detail);
    limb(mesh, tElbow, tWrist, 0.048f, skin, detail);
    ball(mesh, tWrist + Vector3(0.01f, -0.01f, 0.02f), 0.042f, 0.042f, 0.048f, skinDeep, detail);

    mesh.rebuildNormals();
    return mesh;
}

Mesh3D BaseballPlayer3D::catcher(int detail, const CatcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float bob = pose.crouchBob;
    const float sway = pose.torsoSway;

    const float hipY = 0.48f + bob;
    const float shoulderY = 1.12f + bob;
    const float headY = 1.36f + bob;
    const float headR = 0.115f;

    // Legs
    for (int side = -1; side <= 1; side += 2) {
        float s = static_cast<float>(side);
        Vector3 hipJ(0.11f * s + sway * 0.02f, hipY, 0.0f);
        Vector3 knee(0.14f * s, 0.32f + bob, 0.05f);
        Vector3 ankle(0.13f * s, 0.07f + bob * 0.25f, 0.10f);

        limb(mesh, hipJ, knee, 0.082f, pants, detail);
        limb(mesh, knee, ankle, 0.065f, pantsDeep, detail);
        // Shin guard: single soft bulge on shin, same family as gear (deeply into limb)
        Vector3 shin = (knee + ankle) * 0.5f + Vector3(0.0f, 0.0f, 0.025f);
        ball(mesh, shin, 0.07f, 0.10f, 0.055f, gear, detail);
        addCleat(mesh, ankle, -0.08f * s, detail);
    }

    // Pelvis
    ball(mesh, Vector3(sway * 0.03f, hipY, -0.02f), 0.17f, 0.12f, 0.13f, pants, detail);
    ball(mesh, Vector3(sway * 0.03f, hipY + 0.04f, -0.02f), 0.155f, 0.03f, 0.115f, belt, detail);

    // ---- ONE torso mass + ONE protector shell (heavy overlap, similar values) ----
    Vector3 torso(sway * 0.04f, (hipY + shoulderY) * 0.52f, 0.02f);
    float torsoH = (shoulderY - hipY) * 0.55f;
    ball(mesh, torso, 0.15f, torsoH, 0.12f, jerseyDeep, detail);

    // Protector is almost the same center/size — reads as padding, not stacked rings
    ball(mesh, torso + Vector3(0.0f, 0.02f, 0.045f), 0.155f, torsoH * 0.95f, 0.095f, gear, detail);

    Vector3 shL(-0.15f + sway * 0.03f, shoulderY, 0.02f);
    Vector3 shR(0.15f + sway * 0.03f, shoulderY, 0.02f);
    ball(mesh, shL, 0.09f, 0.08f, 0.09f, gearLight, detail);
    ball(mesh, shR, 0.09f, 0.08f, 0.09f, gearLight, detail);
    ball(mesh, Vector3(sway * 0.03f, shoulderY - 0.03f, 0.0f), 0.12f, 0.07f, 0.09f, gearDeep, detail);

    // Neck + head + helmet
    Vector3 neckBase(sway * 0.03f, shoulderY + 0.03f, 0.02f);
    Vector3 neckTop(sway * 0.03f, headY - headR * 0.5f, 0.02f);
    limb(mesh, neckBase, neckTop, 0.048f, skin, detail);

    Vector3 head(sway * 0.03f, headY, 0.02f);
    ball(mesh, head, headR, headR * 1.04f, headR * 0.97f, skin, detail);
    addHelmet(mesh, head, headR, detail);

    // Free arm
    Vector3 freeElbow(0.25f, shoulderY - 0.15f, 0.05f);
    Vector3 freeWrist(0.29f, shoulderY - 0.34f, 0.06f);
    limb(mesh, shR, freeElbow, 0.055f, gearDeep, detail);
    limb(mesh, freeElbow, freeWrist, 0.046f, skin, detail);
    ball(mesh, freeWrist, 0.04f, 0.04f, 0.045f, skinDeep, detail);

    // Glove arm
    Vector3 mittPos(
        -0.40f + pose.mittSide,
        shoulderY - 0.22f + pose.mittHeight,
        0.28f + pose.mittReach
    );
    Vector3 gElbow(
        -0.30f + pose.mittSide * 0.5f,
        shoulderY - 0.14f + pose.mittHeight * 0.4f,
        0.13f + pose.mittReach * 0.3f
    );
    Vector3 gShoulder = shL + Vector3(pose.mittSide * 0.08f, pose.mittHeight * 0.05f, pose.mittReach * 0.05f);
    limb(mesh, gShoulder, gElbow, 0.056f, gearDeep, detail);
    limb(mesh, gElbow, mittPos, 0.048f, skin, detail);
    addMitt(mesh, mittPos, Vector3(-0.15f + pose.mittSide, pose.mittHeight * 0.2f, 0.9f), pose.gloveOpen, detail);

    mesh.rebuildNormals();
    return mesh;
}
