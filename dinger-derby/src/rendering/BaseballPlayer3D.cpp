#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

const sf::Color skin(232, 192, 158);
const sf::Color skinDeep(212, 170, 138);
const sf::Color jersey(252, 253, 255);
const sf::Color jerseyDeep(234, 238, 244);
const sf::Color pants(64, 72, 90);
const sf::Color pantsDeep(54, 60, 76);
const sf::Color belt(50, 52, 60);
const sf::Color cleat(46, 46, 54);
const sf::Color cleatSole(58, 58, 66);
const sf::Color cap(44, 64, 114);
const sf::Color capDeep(36, 52, 96);
const sf::Color accent(212, 56, 64);
const sf::Color gear(54, 70, 98);
const sf::Color gearDeep(46, 58, 82);
const sf::Color gearLight(74, 94, 124);
const sf::Color mitt(174, 122, 78);
const sf::Color mittDeep(148, 100, 62);
const sf::Color sock(252, 252, 255);
const sf::Color ballWhite(242, 236, 224);
const sf::Color ballRed(190, 48, 52);

int ringsFor(int detail) {
    return detail >= 2 ? 14 : (detail >= 1 ? 11 : 9);
}

int segsFor(int detail) {
    return detail >= 2 ? 22 : (detail >= 1 ? 16 : 12);
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

// Smooth capsule limb: long shaft + fused joint balls (same color = no hard creases).
void limb(
    Mesh3D& dest,
    const Vector3& from,
    const Vector3& to,
    float radiusFrom,
    float radiusTo,
    sf::Color color,
    int detail
) {
    Vector3 delta = to - from;
    float length = delta.magnitude();
    if (length < 0.001f) {
        ball(dest, from, radiusFrom, radiusFrom, radiusFrom, color, detail);
        return;
    }

    Vector3 dir = delta * (1.0f / length);
    float yaw = std::atan2(dir.x, dir.z);
    float pitch = std::acos(std::clamp(dir.y, -1.0f, 1.0f));
    float midR = (radiusFrom + radiusTo) * 0.5f;
    Vector3 mid = (from + to) * 0.5f;

    Mesh3D sphere = Mesh3D::sphere(1.0f, ringsFor(detail), segsFor(detail));
    Matrix4 shaft =
        Matrix4::translation(mid) *
        Matrix4::rotationY(yaw) *
        Matrix4::rotationX(pitch) *
        Matrix4::scale(Vector3(midR * 0.94f, length * 0.56f + midR * 0.42f, midR * 0.94f));
    appendTransformed(dest, sphere, shaft, color);

    ball(dest, from, radiusFrom * 1.2f, radiusFrom * 1.2f, radiusFrom * 1.2f, color, detail);
    ball(dest, to, radiusTo * 1.18f, radiusTo * 1.18f, radiusTo * 1.18f, color, detail);
}

void addCleat(Mesh3D& dest, const Vector3& ankle, float yaw, int detail) {
    ball(dest, ankle + Vector3(0.0f, -0.006f, 0.04f), 0.06f, 0.036f, 0.10f, cleat, detail, yaw);
    ball(dest, ankle + Vector3(0.0f, 0.0f, -0.015f), 0.052f, 0.038f, 0.055f, cleat, detail, yaw);
    ball(dest, ankle + Vector3(0.0f, -0.018f, 0.025f), 0.068f, 0.018f, 0.112f, cleatSole, detail, yaw);
    // Laces area
    if (detail >= 1) {
        ball(dest, ankle + Vector3(0.0f, 0.01f, 0.05f), 0.035f, 0.015f, 0.04f, sock, detail, yaw);
    }
    ball(dest, ankle + Vector3(0.0f, 0.03f, 0.0f), 0.046f, 0.036f, 0.046f, sock, detail);
}

void addDetailedMitt(
    Mesh3D& dest,
    const Vector3& wrist,
    const Vector3& forward,
    float open,
    int detail
) {
    Vector3 f = forward.magnitude() > 0.001f ? forward.normalized() : Vector3(0.0f, 0.0f, 1.0f);
    Vector3 side = f.cross(Vector3(0.0f, 1.0f, 0.0f));
    if (side.magnitude() < 0.001f) {
        side = Vector3(1.0f, 0.0f, 0.0f);
    } else {
        side = side.normalized();
    }
    Vector3 up = side.cross(f).normalized();
    Vector3 palm = wrist + f * (0.05f + open * 0.02f);
    float s = 1.0f + open * 0.14f;

    // Palm body
    ball(dest, palm, 0.078f * s, 0.088f * s, 0.052f * s, mitt, detail);
    // Pocket
    ball(dest, palm + f * 0.02f + up * 0.02f, 0.06f * s, 0.05f * s, 0.04f * s, mittDeep, detail);
    // Finger ridge (one mass, not sticks)
    ball(dest, palm + f * 0.03f + up * 0.05f, 0.07f * s, 0.045f * s, 0.04f * s, mitt, detail);
    // Thumb
    ball(dest, palm - side * (0.05f + open * 0.02f) + up * 0.01f, 0.038f, 0.05f, 0.035f, mittDeep, detail);
    // Wrist cuff
    ball(dest, wrist + f * 0.01f, 0.04f, 0.035f, 0.04f, mittDeep, detail);
    if (detail >= 1) {
        ball(dest, palm + up * 0.01f - f * 0.01f, 0.045f, 0.02f, 0.03f, mittDeep, detail);
    }
}

void addCap(Mesh3D& dest, const Vector3& head, float r, int detail) {
    // Crown only above skull midline — never cuts the face.
    ball(dest, head + Vector3(0.0f, r * 0.58f, -r * 0.04f), r * 0.98f, r * 0.4f, r * 0.98f, cap, detail);
    ball(dest, head + Vector3(0.0f, r * 0.35f, -r * 0.02f), r * 1.0f, r * 0.22f, r * 1.0f, capDeep, detail);
    // Bill forward of forehead only
    ball(dest, head + Vector3(0.0f, r * 0.32f, r * 0.88f), r * 0.58f, r * 0.07f, r * 0.3f, capDeep, detail);
    if (detail >= 1) {
        ball(dest, head + Vector3(0.0f, r * 0.55f, r * 0.5f), r * 0.09f, r * 0.07f, r * 0.05f, accent, detail);
    }
}

void addHelmet(Mesh3D& dest, const Vector3& head, float r, int detail) {
    // Shell on top/back
    ball(dest, head + Vector3(0.0f, r * 0.4f, -r * 0.1f), r * 1.08f, r * 0.72f, r * 1.08f, gear, detail);
    ball(dest, head + Vector3(0.0f, r * 0.55f, 0.0f), r * 1.02f, r * 0.45f, r * 1.02f, gearDeep, detail);
    // Ears
    ball(dest, head + Vector3(-r * 0.88f, 0.0f, 0.0f), r * 0.26f, r * 0.38f, r * 0.3f, gearDeep, detail);
    ball(dest, head + Vector3(r * 0.88f, 0.0f, 0.0f), r * 0.26f, r * 0.38f, r * 0.3f, gearDeep, detail);
    // Face mask only in front of face (z+)
    ball(dest, head + Vector3(0.0f, -r * 0.02f, r * 1.08f), r * 0.72f, r * 0.68f, r * 0.16f, gearDeep, detail);
    if (detail >= 1) {
        // Horizontal bar suggestion on mask (front only)
        ball(dest, head + Vector3(0.0f, r * 0.12f, r * 1.15f), r * 0.55f, r * 0.04f, r * 0.05f, gear, detail);
        ball(dest, head + Vector3(0.0f, -r * 0.12f, r * 1.15f), r * 0.55f, r * 0.04f, r * 0.05f, gear, detail);
    }
    ball(dest, head + Vector3(0.0f, -r * 0.72f, r * 0.7f), r * 0.42f, r * 0.22f, r * 0.3f, gearDeep, detail);
}

void addBaseball(Mesh3D& dest, const Vector3& center, float radius, int detail) {
    ball(dest, center, radius, radius, radius, ballWhite, detail);
    if (detail >= 1) {
        // Seam hints
        ball(dest, center + Vector3(radius * 0.35f, 0.0f, 0.0f), radius * 0.08f, radius * 0.55f, radius * 0.08f, ballRed, detail);
        ball(dest, center + Vector3(-radius * 0.2f, radius * 0.15f, radius * 0.2f), radius * 0.5f, radius * 0.08f, radius * 0.08f, ballRed, detail);
    }
}

}

PitcherPose BaseballPlayer3D::blend(const PitcherPose& a, const PitcherPose& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    PitcherPose p;
    p.torsoTwist = lerp(a.torsoTwist, b.torsoTwist, t);
    p.torsoLean = lerp(a.torsoLean, b.torsoLean, t);
    p.torsoSide = lerp(a.torsoSide, b.torsoSide, t);
    p.headTurn = lerp(a.headTurn, b.headTurn, t);
    p.headNod = lerp(a.headNod, b.headNod, t);
    p.throwShoulderPitch = lerp(a.throwShoulderPitch, b.throwShoulderPitch, t);
    p.throwShoulderYaw = lerp(a.throwShoulderYaw, b.throwShoulderYaw, t);
    p.throwShoulderRoll = lerp(a.throwShoulderRoll, b.throwShoulderRoll, t);
    p.throwElbow = lerp(a.throwElbow, b.throwElbow, t);
    p.throwWrist = lerp(a.throwWrist, b.throwWrist, t);
    p.gloveShoulderPitch = lerp(a.gloveShoulderPitch, b.gloveShoulderPitch, t);
    p.gloveShoulderYaw = lerp(a.gloveShoulderYaw, b.gloveShoulderYaw, t);
    p.gloveShoulderRoll = lerp(a.gloveShoulderRoll, b.gloveShoulderRoll, t);
    p.gloveElbow = lerp(a.gloveElbow, b.gloveElbow, t);
    p.frontLegLift = lerp(a.frontLegLift, b.frontLegLift, t);
    p.frontKneeBend = lerp(a.frontKneeBend, b.frontKneeBend, t);
    p.plantKneeBend = lerp(a.plantKneeBend, b.plantKneeBend, t);
    p.hipOpen = lerp(a.hipOpen, b.hipOpen, t);
    p.stride = lerp(a.stride, b.stride, t);
    p.ballInHand = lerp(a.ballInHand, b.ballInHand, t);
    return p;
}

CatcherPose BaseballPlayer3D::blend(const CatcherPose& a, const CatcherPose& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    CatcherPose p;
    p.torsoSway = lerp(a.torsoSway, b.torsoSway, t);
    p.torsoLean = lerp(a.torsoLean, b.torsoLean, t);
    p.crouchBob = lerp(a.crouchBob, b.crouchBob, t);
    p.headTurn = lerp(a.headTurn, b.headTurn, t);
    p.mittSide = lerp(a.mittSide, b.mittSide, t);
    p.mittHeight = lerp(a.mittHeight, b.mittHeight, t);
    p.mittReach = lerp(a.mittReach, b.mittReach, t);
    p.gloveOpen = lerp(a.gloveOpen, b.gloveOpen, t);
    p.freeArmBrace = lerp(a.freeArmBrace, b.freeArmBrace, t);
    return p;
}

PitcherPose BaseballPlayer3D::pitcherIdlePose(float timeSeconds) {
    PitcherPose pose;
    pose.torsoTwist = std::sin(timeSeconds * 1.1f) * 0.04f;
    pose.torsoLean = std::sin(timeSeconds * 0.7f) * 0.02f + 0.04f;
    pose.torsoSide = std::sin(timeSeconds * 0.55f) * 0.015f;
    pose.headTurn = std::sin(timeSeconds * 0.45f) * 0.08f;
    pose.headNod = std::sin(timeSeconds * 0.9f) * 0.03f;
    pose.throwShoulderPitch = 0.15f + std::sin(timeSeconds * 0.85f) * 0.04f;
    pose.throwElbow = 0.25f + std::sin(timeSeconds * 0.85f + 0.3f) * 0.05f;
    pose.gloveShoulderPitch = 0.45f + std::sin(timeSeconds * 0.85f + 0.6f) * 0.04f;
    pose.gloveElbow = 0.55f;
    pose.frontLegLift = std::max(0.0f, std::sin(timeSeconds * 0.5f)) * 0.03f;
    pose.plantKneeBend = 0.12f + std::sin(timeSeconds * 0.5f) * 0.03f;
    pose.ballInHand = 1.0f;
    return pose;
}

PitcherPose BaseballPlayer3D::pitcherDeliveryPose(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    PitcherPose pose;

    // Phases (normalized):
    // 0.00–0.18 load / rocker
    // 0.15–0.40 leg lift / balance
    // 0.38–0.58 stride / drive
    // 0.55–0.62 release
    // 0.60–1.00 follow-through
    float load = smoothstep(0.0f, 0.18f, t);
    float lift = smoothstep(0.12f, 0.38f, t) * (1.0f - smoothstep(0.42f, 0.58f, t));
    float drive = smoothstep(0.38f, 0.58f, t);
    float release = smoothstep(0.54f, 0.62f, t);
    float follow = smoothstep(0.58f, 1.0f, t);

    pose.torsoTwist =
        lerp(0.0f, -0.65f, load) +
        lerp(0.0f, 1.15f, drive) +
        lerp(0.0f, 0.35f, follow);
    pose.torsoLean =
        lerp(0.05f, 0.18f, load) +
        lerp(0.0f, 0.42f, drive) +
        lerp(0.0f, 0.28f, follow);
    pose.torsoSide = lerp(0.0f, -0.12f, load) + lerp(0.0f, 0.08f, drive);

    pose.headTurn = lerp(0.0f, 0.15f, load) + lerp(0.0f, -0.25f, drive) + lerp(0.0f, -0.1f, follow);
    pose.headNod = lerp(0.0f, 0.1f, load) + lerp(0.0f, 0.2f, drive);

    pose.frontLegLift = lift * 0.95f;
    pose.frontKneeBend = lift * 0.9f + drive * 0.25f;
    pose.plantKneeBend = lerp(0.15f, 0.55f, drive) + follow * 0.2f;
    pose.hipOpen = lerp(0.0f, 0.35f, load) + lerp(0.0f, 0.55f, drive);
    pose.stride = drive * 0.28f + follow * 0.12f;

    // Throw arm path: back → high → snap
    pose.throwShoulderPitch =
        lerp(0.2f, -1.25f, load) +
        lerp(0.0f, 2.05f, release) +
        lerp(0.0f, 1.15f, follow);
    pose.throwShoulderYaw =
        lerp(0.0f, -0.85f, load) +
        lerp(0.0f, 0.65f, release) +
        lerp(0.0f, 0.2f, follow);
    pose.throwShoulderRoll = lerp(0.0f, 0.4f, load) + lerp(0.0f, -0.5f, release);
    pose.throwElbow =
        lerp(0.3f, 1.35f, load) +
        lerp(0.0f, -1.15f, release) +
        lerp(0.0f, -0.45f, follow);
    pose.throwWrist =
        lerp(0.0f, 0.35f, load) +
        lerp(0.0f, -0.6f, release) +
        lerp(0.0f, -0.25f, follow);

    pose.gloveShoulderPitch = lerp(0.45f, 0.15f, drive) + follow * 0.2f;
    pose.gloveShoulderYaw = lerp(0.1f, -0.15f, drive);
    pose.gloveShoulderRoll = lerp(0.3f, -0.05f, drive);
    pose.gloveElbow = lerp(0.55f, 0.35f, drive) + follow * 0.1f;

    pose.ballInHand = 1.0f - release; // disappears at release
    return pose;
}

CatcherPose BaseballPlayer3D::catcherIdlePose(float timeSeconds) {
    CatcherPose pose;
    pose.torsoSway = std::sin(timeSeconds * 0.85f) * 0.03f;
    pose.torsoLean = 0.08f + std::sin(timeSeconds * 0.7f) * 0.02f;
    pose.crouchBob = std::sin(timeSeconds * 1.45f) * 0.012f;
    pose.headTurn = std::sin(timeSeconds * 0.4f) * 0.06f;
    pose.mittSide = std::sin(timeSeconds * 0.6f) * 0.015f;
    pose.mittHeight = std::sin(timeSeconds * 1.0f) * 0.012f;
    pose.mittReach = 0.04f;
    pose.gloveOpen = 0.2f + std::sin(timeSeconds * 0.8f) * 0.05f;
    pose.freeArmBrace = 0.85f;
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
    pose.mittSide = std::clamp(-dx * 0.6f, -0.24f, 0.24f);
    pose.mittHeight = std::clamp((dy - 0.12f) * 0.4f, -0.2f, 0.24f);
    pose.mittReach = std::clamp(0.05f + std::abs(dx) * 0.1f + std::abs(dy - 1.2f) * 0.05f, 0.04f, 0.18f);
    pose.gloveOpen = 0.45f;
    pose.torsoSway = std::clamp(-dx * 0.14f, -0.14f, 0.14f);
    pose.torsoLean = 0.12f + std::clamp((1.4f - dy) * 0.08f, -0.08f, 0.12f);
    pose.headTurn = std::clamp(-dx * 0.25f, -0.35f, 0.35f);
    pose.freeArmBrace = 0.7f;
    return pose;
}

Mesh3D BaseballPlayer3D::pitcher(int detail, const PitcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float stride = pose.stride;
    const float lift = pose.frontLegLift;
    const float hipY = 0.86f;
    const float shoulderY = 1.35f;
    const float headY = 1.60f;
    const float headR = 0.115f;

    // Legs with knee bend
    float plantKneeY = 0.44f - pose.plantKneeBend * 0.06f;
    float leadKneeY = 0.46f + lift * 0.2f - pose.frontKneeBend * 0.05f;

    Vector3 plantHip(0.09f + pose.hipOpen * 0.02f, hipY, stride * 0.05f);
    Vector3 plantKnee(0.095f, plantKneeY, stride * 0.03f);
    Vector3 plantAnkle(0.095f, 0.07f, 0.02f + stride * 0.04f);

    Vector3 leadHip(-0.09f - pose.hipOpen * 0.02f, hipY, 0.02f + stride * 0.18f);
    Vector3 leadKnee(-0.095f, leadKneeY, 0.05f + stride * 0.38f);
    Vector3 leadAnkle(-0.095f, 0.07f + lift * 0.28f, 0.08f + stride * 0.82f);

    limb(mesh, plantHip, plantKnee, 0.08f, 0.068f, pants, detail);
    limb(mesh, plantKnee, plantAnkle, 0.065f, 0.055f, pantsDeep, detail);
    limb(mesh, leadHip, leadKnee, 0.08f, 0.068f, pants, detail);
    limb(mesh, leadKnee, leadAnkle, 0.065f, 0.055f, pantsDeep, detail);
    addCleat(mesh, plantAnkle, 0.03f, detail);
    addCleat(mesh, leadAnkle, -0.04f, detail);

    // Hips
    ball(mesh, Vector3(0.0f, hipY, stride * 0.08f), 0.158f, 0.115f, 0.12f, pants, detail);
    ball(mesh, Vector3(0.0f, hipY + 0.045f, stride * 0.08f), 0.142f, 0.032f, 0.108f, belt, detail);
    if (detail >= 1) {
        ball(mesh, Vector3(0.0f, hipY + 0.045f, stride * 0.08f + 0.1f), 0.035f, 0.025f, 0.015f, accent, detail);
    }

    Matrix4 upper =
        Matrix4::translation(Vector3(0.0f, hipY, stride * 0.1f)) *
        Matrix4::rotationY(pose.torsoTwist) *
        Matrix4::rotationZ(pose.torsoSide) *
        Matrix4::rotationX(pose.torsoLean) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));
    auto U = [&](const Vector3& p) { return upper.transformPoint(p); };

    // Single torso + subtle chest volume (same color)
    Vector3 torsoC = U(Vector3(0.0f, (hipY + shoulderY) * 0.52f, 0.012f));
    float torsoH = (shoulderY - hipY) * 0.6f;
    ball(mesh, torsoC, 0.148f, torsoH, 0.112f, jersey, detail);
    ball(mesh, U(Vector3(0.0f, shoulderY - 0.1f, 0.015f)), 0.14f, 0.11f, 0.108f, jersey, detail);
    // Sleeves root / collar
    ball(mesh, U(Vector3(0.0f, shoulderY + 0.0f, 0.01f)), 0.09f, 0.055f, 0.08f, jerseyDeep, detail);
    ball(mesh, U(Vector3(0.0f, (hipY + shoulderY) * 0.52f, 0.1f)), 0.026f, 0.075f, 0.014f, accent, detail);
    if (detail >= 2) {
        // Sleeve hems
        ball(mesh, U(Vector3(-0.12f, shoulderY - 0.08f, 0.0f)), 0.055f, 0.04f, 0.055f, jerseyDeep, detail);
        ball(mesh, U(Vector3(0.12f, shoulderY - 0.08f, 0.0f)), 0.055f, 0.04f, 0.055f, jerseyDeep, detail);
    }

    Vector3 shL = U(Vector3(-0.145f, shoulderY - 0.015f, 0.0f));
    Vector3 shR = U(Vector3(0.145f, shoulderY - 0.015f, 0.0f));
    ball(mesh, shL, 0.085f, 0.075f, 0.085f, jersey, detail);
    ball(mesh, shR, 0.085f, 0.075f, 0.085f, jersey, detail);

    // Neck + head
    Vector3 neckA = U(Vector3(0.0f, shoulderY + 0.025f, 0.012f));
    Vector3 neckB = U(Vector3(0.0f, headY - headR * 0.62f, 0.018f));
    limb(mesh, neckA, neckB, 0.046f, 0.044f, skin, detail);

    Vector3 head = U(Vector3(
        std::sin(pose.headTurn) * 0.02f,
        headY + pose.headNod * 0.015f,
        0.02f + std::cos(pose.headTurn) * 0.005f
    ));
    ball(mesh, head, headR, headR * 1.04f, headR * 0.97f, skin, detail);
    if (detail >= 1) {
        // Ears
        ball(mesh, head + Vector3(-headR * 0.85f, 0.0f, 0.0f), headR * 0.14f, headR * 0.2f, headR * 0.12f, skinDeep, detail);
        ball(mesh, head + Vector3(headR * 0.85f, 0.0f, 0.0f), headR * 0.14f, headR * 0.2f, headR * 0.12f, skinDeep, detail);
    }
    addCap(mesh, head + Vector3(std::sin(pose.headTurn) * 0.01f, 0.0f, 0.0f), headR, detail);

    // Glove arm
    Vector3 gElbow = U(Vector3(
        -0.24f + pose.gloveShoulderYaw * 0.05f,
        shoulderY - 0.18f - pose.gloveElbow * 0.02f,
        0.08f + pose.gloveShoulderPitch * 0.04f
    ));
    Vector3 gWrist = U(Vector3(
        -0.14f + pose.gloveShoulderYaw * 0.08f,
        shoulderY - 0.35f - pose.gloveElbow * 0.04f,
        0.17f + pose.gloveShoulderPitch * 0.08f
    ));
    limb(mesh, shL, gElbow, 0.058f, 0.05f, jerseyDeep, detail);
    limb(mesh, gElbow, gWrist, 0.048f, 0.042f, skin, detail);
    addDetailedMitt(mesh, gWrist, Vector3(0.15f, -0.1f, 0.9f), 0.25f + pose.gloveShoulderRoll * 0.15f, detail);

    // Throw arm
    Vector3 tElbow = U(Vector3(
        0.24f + pose.throwShoulderYaw * 0.07f,
        shoulderY - 0.2f + pose.throwShoulderPitch * 0.04f,
        0.03f + pose.throwShoulderPitch * 0.1f
    ));
    Vector3 tWrist = U(Vector3(
        0.26f + pose.throwShoulderYaw * 0.1f + pose.throwWrist * 0.02f,
        shoulderY - 0.42f + pose.throwElbow * 0.05f,
        0.05f + pose.throwShoulderPitch * 0.16f + pose.throwWrist * 0.03f
    ));
    limb(mesh, shR, tElbow, 0.058f, 0.05f, jerseyDeep, detail);
    limb(mesh, tElbow, tWrist, 0.048f, 0.042f, skin, detail);
    ball(mesh, tWrist + Vector3(0.01f, -0.01f, 0.02f), 0.04f, 0.04f, 0.046f, skinDeep, detail);

    if (pose.ballInHand > 0.15f) {
        Vector3 ballPos = tWrist + Vector3(0.03f, 0.0f, 0.04f);
        addBaseball(mesh, ballPos, 0.032f * pose.ballInHand + 0.02f, detail);
    }

    mesh.rebuildNormals();
    return mesh;
}

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
        limb(mesh, hipJ, knee, 0.082f, 0.07f, pants, detail);
        limb(mesh, knee, ankle, 0.065f, 0.055f, pantsDeep, detail);
        ball(mesh, (knee + ankle) * 0.5f + Vector3(0.0f, 0.0f, 0.03f), 0.068f, 0.1f, 0.052f, gear, detail);
        if (detail >= 1) {
            ball(mesh, (knee + ankle) * 0.5f + Vector3(0.0f, 0.02f, 0.045f), 0.05f, 0.07f, 0.03f, gearDeep, detail);
        }
        addCleat(mesh, ankle, -0.07f * s, detail);
    }

    ball(mesh, Vector3(sway * 0.02f, hipY, -0.02f), 0.162f, 0.112f, 0.122f, pants, detail);
    ball(mesh, Vector3(sway * 0.02f, hipY + 0.04f, -0.02f), 0.148f, 0.03f, 0.112f, belt, detail);

    Vector3 torso(sway * 0.03f, (hipY + shoulderY) * 0.52f + pose.torsoLean * 0.02f, 0.02f);
    float torsoH = (shoulderY - hipY) * 0.58f;
    ball(mesh, torso, 0.148f, torsoH, 0.115f, jerseyDeep, detail);
    // Protector shell — heavy overlap, same centerline
    ball(mesh, torso + Vector3(0.0f, 0.02f, 0.045f), 0.155f, torsoH * 0.92f, 0.09f, gear, detail);
    if (detail >= 1) {
        ball(mesh, torso + Vector3(0.0f, 0.06f, 0.07f), 0.12f, torsoH * 0.45f, 0.05f, gearDeep, detail);
        // Harness straps
        ball(mesh, torso + Vector3(-0.08f, 0.08f, -0.02f), 0.025f, 0.12f, 0.04f, gearDeep, detail);
        ball(mesh, torso + Vector3(0.08f, 0.08f, -0.02f), 0.025f, 0.12f, 0.04f, gearDeep, detail);
    }

    Vector3 shL(-0.14f + sway * 0.02f, shoulderY, 0.02f);
    Vector3 shR(0.14f + sway * 0.02f, shoulderY, 0.02f);
    ball(mesh, shL, 0.085f, 0.075f, 0.085f, gearLight, detail);
    ball(mesh, shR, 0.085f, 0.075f, 0.085f, gearLight, detail);

    Vector3 neckA(sway * 0.02f, shoulderY + 0.02f, 0.02f);
    Vector3 neckB(sway * 0.02f, headY - headR * 0.6f, 0.02f);
    limb(mesh, neckA, neckB, 0.045f, 0.043f, skin, detail);

    Vector3 head(sway * 0.02f + pose.headTurn * 0.02f, headY, 0.02f);
    ball(mesh, head, headR, headR * 1.03f, headR * 0.97f, skin, detail);
    if (detail >= 1) {
        ball(mesh, head + Vector3(-headR * 0.82f, 0.0f, 0.0f), headR * 0.13f, headR * 0.18f, headR * 0.11f, skinDeep, detail);
        ball(mesh, head + Vector3(headR * 0.82f, 0.0f, 0.0f), headR * 0.13f, headR * 0.18f, headR * 0.11f, skinDeep, detail);
    }
    addHelmet(mesh, head, headR, detail);

    // Free arm braced
    float brace = pose.freeArmBrace;
    Vector3 freeElbow(0.24f, shoulderY - 0.14f - brace * 0.02f, 0.05f);
    Vector3 freeWrist(0.26f + brace * 0.02f, shoulderY - 0.32f - brace * 0.08f, 0.06f + brace * 0.04f);
    limb(mesh, shR, freeElbow, 0.054f, 0.048f, gearDeep, detail);
    limb(mesh, freeElbow, freeWrist, 0.046f, 0.04f, skin, detail);
    ball(mesh, freeWrist, 0.038f, 0.038f, 0.042f, skinDeep, detail);

    Vector3 mittPos(
        -0.38f + pose.mittSide,
        shoulderY - 0.2f + pose.mittHeight,
        0.26f + pose.mittReach
    );
    Vector3 gElbow(
        -0.28f + pose.mittSide * 0.45f,
        shoulderY - 0.13f + pose.mittHeight * 0.35f,
        0.12f + pose.mittReach * 0.28f
    );
    Vector3 gShoulder = shL + Vector3(pose.mittSide * 0.06f, pose.mittHeight * 0.04f, pose.mittReach * 0.04f);
    limb(mesh, gShoulder, gElbow, 0.056f, 0.05f, gearDeep, detail);
    limb(mesh, gElbow, mittPos, 0.048f, 0.042f, skin, detail);
    addDetailedMitt(
        mesh,
        mittPos,
        Vector3(-0.1f + pose.mittSide, pose.mittHeight * 0.15f, 0.95f),
        pose.gloveOpen,
        detail
    );

    mesh.rebuildNormals();
    return mesh;
}
