#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

const sf::Color skin(228, 188, 154);
const sf::Color skinDeep(208, 168, 136);
const sf::Color jersey(250, 252, 255);
const sf::Color jerseyDeep(232, 236, 244);
const sf::Color pants(60, 68, 86);
const sf::Color pantsDeep(50, 56, 72);
const sf::Color belt(48, 50, 58);
const sf::Color cleat(42, 42, 50);
const sf::Color cleatSole(54, 54, 62);
const sf::Color cap(40, 58, 108);
const sf::Color capDeep(32, 48, 92);
const sf::Color accent(208, 52, 60);
const sf::Color gear(50, 66, 94);
const sf::Color gearDeep(40, 54, 76);
const sf::Color gearLight(70, 90, 120);
const sf::Color mitt(168, 116, 74);
const sf::Color mittDeep(140, 94, 58);
const sf::Color sock(250, 250, 252);

// Landmark heights shared by mesh + hand anchor (model space).
constexpr float kHipY = 0.88f;
constexpr float kShoulderY = 1.36f;
constexpr float kHeadY = 1.58f;
constexpr float kHeadR = 0.112f;

int ringsFor(int detail) {
    return detail >= 2 ? 13 : (detail >= 1 ? 10 : 8);
}

int segsFor(int detail) {
    return detail >= 2 ? 20 : (detail >= 1 ? 14 : 12);
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
        Matrix4::scale(Vector3(midR * 0.93f, length * 0.55f + midR * 0.4f, midR * 0.93f));
    appendTransformed(dest, sphere, shaft, color);

    ball(dest, from, radiusFrom * 1.15f, radiusFrom * 1.15f, radiusFrom * 1.15f, color, detail);
    ball(dest, to, radiusTo * 1.12f, radiusTo * 1.12f, radiusTo * 1.12f, color, detail);
}

void addCleat(Mesh3D& dest, const Vector3& ankle, float yaw, int detail) {
    ball(dest, ankle + Vector3(0.0f, -0.005f, 0.035f), 0.058f, 0.034f, 0.095f, cleat, detail, yaw);
    ball(dest, ankle + Vector3(0.0f, -0.016f, 0.02f), 0.064f, 0.016f, 0.105f, cleatSole, detail, yaw);
    ball(dest, ankle + Vector3(0.0f, 0.028f, 0.0f), 0.044f, 0.034f, 0.044f, sock, detail);
}

void addMitt(Mesh3D& dest, const Vector3& wrist, const Vector3& forward, float open, int detail) {
    Vector3 f = forward.magnitude() > 0.001f ? forward.normalized() : Vector3(0.0f, 0.0f, 1.0f);
    Vector3 palm = wrist + f * 0.05f;
    float s = 1.0f + open * 0.12f;
    ball(dest, palm, 0.075f * s, 0.085f * s, 0.05f * s, mitt, detail);
    ball(dest, palm + f * 0.025f + Vector3(0.0f, 0.035f, 0.0f), 0.06f * s, 0.048f * s, 0.04f * s, mitt, detail);
    ball(dest, palm + Vector3(-0.04f, 0.012f, 0.0f), 0.036f, 0.048f, 0.032f, mittDeep, detail);
    ball(dest, wrist + f * 0.01f, 0.038f, 0.032f, 0.038f, mittDeep, detail);
}

// Cap sits only on top of the skull — never a band through the face.
void addCap(Mesh3D& dest, const Vector3& head, float r, int detail) {
    ball(dest, head + Vector3(0.0f, r * 0.62f, -r * 0.05f), r * 0.92f, r * 0.38f, r * 0.92f, cap, detail);
    ball(dest, head + Vector3(0.0f, r * 0.42f, -r * 0.02f), r * 0.96f, r * 0.18f, r * 0.96f, capDeep, detail);
    ball(dest, head + Vector3(0.0f, r * 0.35f, r * 0.9f), r * 0.52f, r * 0.065f, r * 0.28f, capDeep, detail);
    if (detail >= 1) {
        ball(dest, head + Vector3(0.0f, r * 0.58f, r * 0.48f), r * 0.08f, r * 0.06f, r * 0.05f, accent, detail);
    }
}

void addHelmet(Mesh3D& dest, const Vector3& head, float r, int detail) {
    ball(dest, head + Vector3(0.0f, r * 0.42f, -r * 0.12f), r * 1.05f, r * 0.68f, r * 1.05f, gear, detail);
    ball(dest, head + Vector3(-r * 0.9f, 0.0f, 0.0f), r * 0.25f, r * 0.36f, r * 0.28f, gearDeep, detail);
    ball(dest, head + Vector3(r * 0.9f, 0.0f, 0.0f), r * 0.25f, r * 0.36f, r * 0.28f, gearDeep, detail);
    // Mask plate strictly in front of face.
    ball(dest, head + Vector3(0.0f, -r * 0.02f, r * 1.12f), r * 0.7f, r * 0.65f, r * 0.14f, gearDeep, detail);
    ball(dest, head + Vector3(0.0f, -r * 0.7f, r * 0.72f), r * 0.4f, r * 0.2f, r * 0.28f, gearDeep, detail);
    if (detail >= 1) {
        ball(dest, head + Vector3(0.0f, r * 0.1f, r * 1.18f), r * 0.5f, r * 0.035f, r * 0.04f, gear, detail);
        ball(dest, head + Vector3(0.0f, -r * 0.1f, r * 1.18f), r * 0.5f, r * 0.035f, r * 0.04f, gear, detail);
    }
}

// Shared skeleton math for mesh + hand anchor.
struct PitcherSkeleton {
    Matrix4 upper;
    Vector3 plantHip;
    Vector3 plantKnee;
    Vector3 plantAnkle;
    Vector3 leadHip;
    Vector3 leadKnee;
    Vector3 leadAnkle;
    Vector3 shL;
    Vector3 shR;
    Vector3 gElbow;
    Vector3 gWrist;
    Vector3 tElbow;
    Vector3 tWrist;
    Vector3 head;
    Vector3 torsoC;
    float torsoH = 0.0f;
};

PitcherSkeleton buildPitcherSkeleton(const PitcherPose& pose) {
    PitcherSkeleton s;
    const float stride = pose.stride;
    const float lift = pose.frontLegLift;
    const float hipY = kHipY;
    const float shoulderY = kShoulderY;

    float plantKneeY = 0.45f - pose.plantKneeBend * 0.05f;
    float leadKneeY = 0.47f + lift * 0.18f - pose.frontKneeBend * 0.04f;

    s.plantHip = Vector3(0.09f + pose.hipOpen * 0.015f, hipY, stride * 0.04f);
    s.plantKnee = Vector3(0.095f, plantKneeY, stride * 0.025f);
    s.plantAnkle = Vector3(0.095f, 0.07f, 0.02f + stride * 0.035f);

    s.leadHip = Vector3(-0.09f - pose.hipOpen * 0.015f, hipY, 0.02f + stride * 0.14f);
    s.leadKnee = Vector3(-0.095f, leadKneeY, 0.05f + stride * 0.32f);
    s.leadAnkle = Vector3(-0.095f, 0.07f + lift * 0.22f, 0.07f + stride * 0.72f);

    s.upper =
        Matrix4::translation(Vector3(0.0f, hipY, stride * 0.08f)) *
        Matrix4::rotationY(pose.torsoTwist) *
        Matrix4::rotationZ(pose.torsoSide) *
        Matrix4::rotationX(pose.torsoLean) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));

    auto U = [&](const Vector3& p) { return s.upper.transformPoint(p); };

    s.torsoC = U(Vector3(0.0f, (hipY + shoulderY) * 0.52f, 0.01f));
    s.torsoH = (shoulderY - hipY) * 0.58f;

    s.shL = U(Vector3(-0.14f, shoulderY - 0.01f, 0.0f));
    s.shR = U(Vector3(0.14f, shoulderY - 0.01f, 0.0f));

    // Glove stays connected near the chest (Yamamoto glove-side discipline).
    s.gElbow = U(Vector3(
        -0.16f + pose.gloveShoulderYaw * 0.03f,
        shoulderY - 0.10f - pose.gloveElbow * 0.015f,
        0.10f + pose.gloveShoulderPitch * 0.02f
    ));
    s.gWrist = U(Vector3(
        -0.08f + pose.gloveShoulderYaw * 0.04f,
        shoulderY - 0.18f - pose.gloveElbow * 0.02f,
        0.16f + pose.gloveShoulderPitch * 0.03f
    ));

    // Throw arm — moderated offsets so the body never collapses.
    // High 3/4 slot mapping (Yamamoto ~75°): stronger pitch lifts the arm path.
    s.tElbow = U(Vector3(
        0.20f + pose.throwShoulderYaw * 0.07f,
        shoulderY - 0.14f + pose.throwShoulderPitch * 0.08f,
        0.05f + pose.throwShoulderPitch * 0.1f
    ));
    s.tWrist = U(Vector3(
        0.22f + pose.throwShoulderYaw * 0.1f + pose.throwWrist * 0.02f,
        shoulderY - 0.28f + pose.throwElbow * 0.05f + pose.throwShoulderPitch * 0.06f,
        0.12f + pose.throwShoulderPitch * 0.16f + pose.throwWrist * 0.03f
    ));

    s.head = U(Vector3(
        std::sin(pose.headTurn) * 0.015f,
        kHeadY + pose.headNod * 0.01f,
        0.02f
    ));

    return s;
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
    p.throwElbow = lerp(a.throwElbow, b.throwElbow, t);
    p.throwWrist = lerp(a.throwWrist, b.throwWrist, t);
    p.gloveShoulderPitch = lerp(a.gloveShoulderPitch, b.gloveShoulderPitch, t);
    p.gloveShoulderYaw = lerp(a.gloveShoulderYaw, b.gloveShoulderYaw, t);
    p.gloveElbow = lerp(a.gloveElbow, b.gloveElbow, t);
    p.frontLegLift = lerp(a.frontLegLift, b.frontLegLift, t);
    p.frontKneeBend = lerp(a.frontKneeBend, b.frontKneeBend, t);
    p.plantKneeBend = lerp(a.plantKneeBend, b.plantKneeBend, t);
    p.hipOpen = lerp(a.hipOpen, b.hipOpen, t);
    p.stride = lerp(a.stride, b.stride, t);
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

// Yoshinobu Yamamoto–style set: compact, athletic, hands chest-high, quiet glove.
PitcherPose BaseballPlayer3D::pitcherIdlePose(float timeSeconds) {
    PitcherPose pose;
    pose.torsoTwist = std::sin(timeSeconds * 0.9f) * 0.025f;
    pose.torsoLean = 0.04f + std::sin(timeSeconds * 0.65f) * 0.012f;
    pose.torsoSide = std::sin(timeSeconds * 0.45f) * 0.01f;
    pose.headTurn = std::sin(timeSeconds * 0.35f) * 0.06f;
    pose.headNod = std::sin(timeSeconds * 0.8f) * 0.02f;
    // Hands together-ish at chest; throw arm slightly lower, glove firm at sternum.
    pose.throwShoulderPitch = 0.45f + std::sin(timeSeconds * 0.75f) * 0.025f;
    pose.throwShoulderYaw = 0.08f;
    pose.throwElbow = 0.7f;
    pose.throwWrist = 0.1f;
    pose.gloveShoulderPitch = 0.7f;
    pose.gloveShoulderYaw = 0.15f;
    pose.gloveElbow = 0.85f; // tucked tight (Yamamoto glove-side connection)
    pose.plantKneeBend = 0.18f;
    pose.frontKneeBend = 0.14f;
    pose.frontLegLift = 0.02f;
    return pose;
}

// Yoshinobu Yamamoto delivery (right-hander), phased for our joint channels:
//   rocker → high vertical leg kick (~90° hip) → balance → long stride →
//   hips fire while shoulders stay closed → high 3/4 arm (~75°) release →
//   athletic upright finish with glove pulled in.
PitcherPose BaseballPlayer3D::pitcherDeliveryPose(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    PitcherPose pose;

    // Phase envelopes (smoothstep gates).
    float rocker = smoothstep(0.0f, 0.12f, t);                                   // 0.00–0.12
    float legUp = smoothstep(0.08f, 0.32f, t);                                   // rise
    float balance = smoothstep(0.28f, 0.40f, t) * (1.0f - smoothstep(0.40f, 0.48f, t));
    float legHold = legUp * (1.0f - smoothstep(0.42f, 0.55f, t));                // peak hold then drop
    float strideOn = smoothstep(0.40f, 0.56f, t);                                // drive to plate
    float hipFire = smoothstep(0.48f, 0.60f, t);                                 // hips open first
    float shoulderFire = smoothstep(0.54f, 0.64f, t);                            // shoulders lag (separation)
    float release = smoothstep(0.56f, 0.64f, t);                                 // ball out
    float follow = smoothstep(0.62f, 1.0f, t);                                   // finish

    // --- Lower half: high knee, vertical lift, then athletic stride ---
    // Yamamoto: lead knee ~3ft lift, hip ~90°, lands with ~15° knee flex.
    pose.frontLegLift = legHold * 0.98f;
    pose.frontKneeBend = legHold * 0.85f + strideOn * 0.15f + follow * 0.12f;
    pose.plantKneeBend =
        lerp(0.18f, 0.28f, rocker) +
        strideOn * 0.22f +
        follow * 0.12f; // lands flexed, not locked
    pose.hipOpen =
        lerp(0.0f, 0.12f, rocker) +
        hipFire * 0.55f +
        follow * 0.15f;
    pose.stride =
        strideOn * 0.22f +
        follow * 0.1f;

    // --- Trunk: stay tall; coil closed then rotate late (hip-shoulder separation) ---
    // Closed coil while leg is up (negative twist = closed for RHP in our +Z plate frame).
    pose.torsoTwist =
        lerp(0.0f, -0.42f, legUp) * (1.0f - hipFire) +
        hipFire * 0.25f +
        shoulderFire * 0.55f +
        follow * 0.18f;
    // Yamamoto stays relatively upright — avoid huge forward collapse.
    pose.torsoLean =
        lerp(0.04f, 0.08f, rocker) +
        legHold * 0.04f +
        strideOn * 0.12f +
        follow * 0.1f;
    pose.torsoSide =
        lerp(0.0f, -0.06f, rocker) +
        hipFire * 0.05f;

    pose.headTurn =
        lerp(0.0f, 0.1f, legUp) +
        shoulderFire * (-0.12f) +
        follow * (-0.05f);
    pose.headNod =
        legHold * 0.05f +
        release * 0.08f;

    // --- Glove side: tight to chest (elite glove connection) ---
    pose.gloveShoulderPitch =
        lerp(0.7f, 0.85f, rocker) +
        legHold * 0.05f +
        strideOn * (-0.25f) +
        follow * 0.1f;
    pose.gloveShoulderYaw =
        lerp(0.15f, 0.25f, rocker) +
        strideOn * (-0.2f);
    pose.gloveElbow =
        lerp(0.85f, 1.0f, rocker) * (1.0f - strideOn * 0.25f) +
        follow * 0.15f;

    // --- Throwing arm: high 3/4 slot, delayed until hips clear ---
    // Load: hand stays high near head/hat; then swings through high slot out front.
    pose.throwShoulderPitch =
        lerp(0.45f, -0.35f, rocker) +          // slight takeaway
        legHold * (-0.15f) +                   // stay loaded high-back during balance
        release * 1.35f +                      // snap up-and-over through release
        follow * 0.45f;                        // finish across body
    pose.throwShoulderYaw =
        lerp(0.08f, -0.55f, rocker + legHold * 0.5f) + // scap load / closed
        release * 0.75f +
        follow * 0.15f;
    pose.throwElbow =
        lerp(0.7f, 1.15f, rocker) +            // flexed load
        legHold * 0.1f +
        release * (-1.05f) +                   // extension through release
        follow * (-0.2f);
    pose.throwWrist =
        lerp(0.1f, 0.25f, rocker) +
        release * (-0.45f) +
        follow * (-0.15f);

    // Balance-frame polish: at peak leg lift, hold posture a beat.
    float peak = balance;
    pose.frontLegLift = std::max(pose.frontLegLift, peak * 0.92f);
    pose.torsoTwist = pose.torsoTwist * (1.0f - peak * 0.15f) + (-0.4f) * peak * 0.15f;

    return pose;
}

CatcherPose BaseballPlayer3D::catcherIdlePose(float timeSeconds) {
    CatcherPose pose;
    pose.torsoSway = std::sin(timeSeconds * 0.85f) * 0.025f;
    pose.torsoLean = 0.1f + std::sin(timeSeconds * 0.65f) * 0.015f;
    pose.crouchBob = std::sin(timeSeconds * 1.4f) * 0.01f;
    pose.headTurn = std::sin(timeSeconds * 0.35f) * 0.05f;
    pose.mittSide = std::sin(timeSeconds * 0.55f) * 0.012f;
    pose.mittHeight = std::sin(timeSeconds * 0.95f) * 0.01f;
    pose.mittReach = 0.04f;
    pose.gloveOpen = 0.25f;
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
    pose.mittSide = std::clamp(-dx * 0.55f, -0.22f, 0.22f);
    pose.mittHeight = std::clamp((dy - 0.15f) * 0.38f, -0.18f, 0.22f);
    pose.mittReach = std::clamp(0.05f + std::abs(dx) * 0.08f, 0.04f, 0.16f);
    pose.gloveOpen = 0.5f;
    pose.torsoSway = std::clamp(-dx * 0.12f, -0.12f, 0.12f);
    pose.torsoLean = 0.12f + std::clamp((1.35f - dy) * 0.06f, -0.06f, 0.1f);
    pose.headTurn = std::clamp(-dx * 0.22f, -0.3f, 0.3f);
    pose.freeArmBrace = 0.65f;
    return pose;
}

Vector3 BaseballPlayer3D::throwHandLocal(const PitcherPose& pose) {
    PitcherSkeleton s = buildPitcherSkeleton(pose);
    // Slightly past wrist along throw direction for ball center.
    Vector3 toHand = s.tWrist - s.tElbow;
    if (toHand.magnitude() < 0.001f) {
        return s.tWrist + Vector3(0.02f, 0.0f, 0.04f);
    }
    return s.tWrist + toHand.normalized() * 0.045f;
}

Mesh3D BaseballPlayer3D::pitcher(int detail, const PitcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;
    PitcherSkeleton s = buildPitcherSkeleton(pose);

    limb(mesh, s.plantHip, s.plantKnee, 0.078f, 0.065f, pants, detail);
    limb(mesh, s.plantKnee, s.plantAnkle, 0.062f, 0.052f, pantsDeep, detail);
    limb(mesh, s.leadHip, s.leadKnee, 0.078f, 0.065f, pants, detail);
    limb(mesh, s.leadKnee, s.leadAnkle, 0.062f, 0.052f, pantsDeep, detail);
    addCleat(mesh, s.plantAnkle, 0.03f, detail);
    addCleat(mesh, s.leadAnkle, -0.04f, detail);

    ball(mesh, Vector3(0.0f, kHipY, pose.stride * 0.07f), 0.155f, 0.11f, 0.115f, pants, detail);
    ball(mesh, Vector3(0.0f, kHipY + 0.042f, pose.stride * 0.07f), 0.14f, 0.03f, 0.105f, belt, detail);

    // One torso volume + shoulders fused in.
    ball(mesh, s.torsoC, 0.145f, s.torsoH, 0.11f, jersey, detail);
    ball(mesh, s.shL, 0.082f, 0.072f, 0.082f, jersey, detail);
    ball(mesh, s.shR, 0.082f, 0.072f, 0.082f, jersey, detail);
    ball(mesh, s.upper.transformPoint(Vector3(0.0f, kShoulderY + 0.01f, 0.01f)), 0.08f, 0.05f, 0.07f, jerseyDeep, detail);
    ball(mesh, s.upper.transformPoint(Vector3(0.0f, (kHipY + kShoulderY) * 0.52f, 0.1f)), 0.024f, 0.065f, 0.014f, accent, detail);

    // Neck short and thick into head.
    Vector3 neckA = s.upper.transformPoint(Vector3(0.0f, kShoulderY + 0.03f, 0.012f));
    Vector3 neckB = s.head + Vector3(0.0f, -kHeadR * 0.55f, 0.0f);
    limb(mesh, neckA, neckB, 0.045f, 0.042f, skin, detail);

    ball(mesh, s.head, kHeadR, kHeadR * 1.03f, kHeadR * 0.97f, skin, detail);
    if (detail >= 1) {
        ball(mesh, s.head + Vector3(-kHeadR * 0.82f, 0.0f, 0.0f), kHeadR * 0.12f, kHeadR * 0.16f, kHeadR * 0.1f, skinDeep, detail);
        ball(mesh, s.head + Vector3(kHeadR * 0.82f, 0.0f, 0.0f), kHeadR * 0.12f, kHeadR * 0.16f, kHeadR * 0.1f, skinDeep, detail);
    }
    addCap(mesh, s.head, kHeadR, detail);

    limb(mesh, s.shL, s.gElbow, 0.055f, 0.048f, jerseyDeep, detail);
    limb(mesh, s.gElbow, s.gWrist, 0.046f, 0.04f, skin, detail);
    addMitt(mesh, s.gWrist, Vector3(0.2f, -0.1f, 0.85f), 0.3f, detail);

    limb(mesh, s.shR, s.tElbow, 0.055f, 0.048f, jerseyDeep, detail);
    limb(mesh, s.tElbow, s.tWrist, 0.046f, 0.04f, skin, detail);
    ball(mesh, s.tWrist + Vector3(0.01f, -0.01f, 0.015f), 0.038f, 0.038f, 0.042f, skinDeep, detail);

    mesh.rebuildNormals();
    return mesh;
}

Mesh3D BaseballPlayer3D::catcher(int detail, const CatcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float bob = pose.crouchBob;
    const float sway = pose.torsoSway;
    const float hipY = 0.48f + bob;
    const float shoulderY = 1.1f + bob;
    const float headY = 1.32f + bob;
    const float headR = 0.11f;

    for (int side = -1; side <= 1; side += 2) {
        float s = static_cast<float>(side);
        Vector3 hipJ(0.1f * s + sway * 0.012f, hipY, 0.0f);
        Vector3 knee(0.125f * s, 0.31f + bob, 0.05f);
        Vector3 ankle(0.115f * s, 0.07f + bob * 0.18f, 0.1f);
        limb(mesh, hipJ, knee, 0.08f, 0.068f, pants, detail);
        limb(mesh, knee, ankle, 0.062f, 0.052f, pantsDeep, detail);
        ball(mesh, (knee + ankle) * 0.5f + Vector3(0.0f, 0.0f, 0.028f), 0.065f, 0.09f, 0.05f, gear, detail);
        addCleat(mesh, ankle, -0.06f * s, detail);
    }

    ball(mesh, Vector3(sway * 0.02f, hipY, -0.02f), 0.158f, 0.11f, 0.12f, pants, detail);
    ball(mesh, Vector3(sway * 0.02f, hipY + 0.04f, -0.02f), 0.145f, 0.028f, 0.11f, belt, detail);

    Vector3 torso(sway * 0.025f, (hipY + shoulderY) * 0.52f, 0.02f);
    float torsoH = (shoulderY - hipY) * 0.55f;
    ball(mesh, torso, 0.145f, torsoH, 0.112f, jerseyDeep, detail);
    ball(mesh, torso + Vector3(0.0f, 0.015f, 0.04f), 0.15f, torsoH * 0.88f, 0.085f, gear, detail);

    Vector3 shL(-0.135f + sway * 0.02f, shoulderY, 0.02f);
    Vector3 shR(0.135f + sway * 0.02f, shoulderY, 0.02f);
    ball(mesh, shL, 0.08f, 0.07f, 0.08f, gearLight, detail);
    ball(mesh, shR, 0.08f, 0.07f, 0.08f, gearLight, detail);

    Vector3 neckA(sway * 0.015f, shoulderY + 0.02f, 0.02f);
    Vector3 head(sway * 0.015f + pose.headTurn * 0.015f, headY, 0.02f);
    Vector3 neckB = head + Vector3(0.0f, -headR * 0.55f, 0.0f);
    limb(mesh, neckA, neckB, 0.044f, 0.042f, skin, detail);
    ball(mesh, head, headR, headR * 1.03f, headR * 0.97f, skin, detail);
    addHelmet(mesh, head, headR, detail);

    float brace = pose.freeArmBrace;
    Vector3 freeElbow(0.23f, shoulderY - 0.14f, 0.05f);
    Vector3 freeWrist(0.25f + brace * 0.02f, shoulderY - 0.3f - brace * 0.06f, 0.06f + brace * 0.03f);
    limb(mesh, shR, freeElbow, 0.052f, 0.046f, gearDeep, detail);
    limb(mesh, freeElbow, freeWrist, 0.044f, 0.038f, skin, detail);
    ball(mesh, freeWrist, 0.036f, 0.036f, 0.04f, skinDeep, detail);

    Vector3 mittPos(
        -0.36f + pose.mittSide,
        shoulderY - 0.18f + pose.mittHeight,
        0.24f + pose.mittReach
    );
    Vector3 gElbow(
        -0.26f + pose.mittSide * 0.4f,
        shoulderY - 0.12f + pose.mittHeight * 0.3f,
        0.11f + pose.mittReach * 0.25f
    );
    Vector3 gShoulder = shL + Vector3(pose.mittSide * 0.05f, pose.mittHeight * 0.03f, pose.mittReach * 0.03f);
    limb(mesh, gShoulder, gElbow, 0.054f, 0.048f, gearDeep, detail);
    limb(mesh, gElbow, mittPos, 0.046f, 0.04f, skin, detail);
    addMitt(mesh, mittPos, Vector3(-0.1f + pose.mittSide, pose.mittHeight * 0.12f, 0.95f), pose.gloveOpen, detail);

    mesh.rebuildNormals();
    return mesh;
}
