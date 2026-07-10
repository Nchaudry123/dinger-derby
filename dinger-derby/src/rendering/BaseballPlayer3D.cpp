#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

// Clean, readable sports palette — soft value steps so clothing and skin feel painted as one character.
const sf::Color skin(226, 184, 150);
const sf::Color skinDeep(204, 160, 128);
const sf::Color jersey(246, 248, 252);
const sf::Color jerseyDeep(224, 228, 236);
const sf::Color pants(60, 68, 86);
const sf::Color pantsDeep(48, 54, 70);
const sf::Color belt(42, 44, 52);
const sf::Color cleat(40, 40, 48);
const sf::Color cleatSole(52, 52, 60);
const sf::Color cap(36, 56, 104);
const sf::Color capDeep(28, 44, 86);
const sf::Color accent(214, 58, 66);
const sf::Color gear(48, 64, 90);
const sf::Color gearDeep(38, 50, 72);
const sf::Color gearLight(66, 86, 116);
const sf::Color mitt(170, 118, 74);
const sf::Color mittDeep(142, 94, 56);
const sf::Color sock(248, 248, 252);

constexpr float pi = 3.1415926535f;

// Mid-poly density so Gouraud shading reads smooth on continuous volumes.
int ringsFor(int detail) {
    return detail >= 2 ? 11 : (detail >= 1 ? 9 : 8);
}

int segsFor(int detail) {
    return detail >= 2 ? 18 : (detail >= 1 ? 14 : 12);
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

// Ellipsoid volume. Building block for continuous character form.
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

// Limb segment: shaft + end caps so joints never show gaps (continuous character mesh).
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
    if (length < 0.0005f) {
        ball(dest, from, radiusFrom, radiusFrom, radiusFrom, color, detail);
        return;
    }

    Vector3 dir = delta * (1.0f / length);
    float yaw = std::atan2(dir.x, dir.z);
    float pitch = std::acos(std::clamp(dir.y, -1.0f, 1.0f));
    float midR = (radiusFrom + radiusTo) * 0.5f;
    Vector3 mid = (from + to) * 0.5f;

    Mesh3D sphere = Mesh3D::sphere(1.0f, ringsFor(detail), segsFor(detail));
    // Slightly longer shaft so it digs into joint balls.
    Matrix4 shaft =
        Matrix4::translation(mid) *
        Matrix4::rotationY(yaw) *
        Matrix4::rotationX(pitch) *
        Matrix4::scale(Vector3(midR * 0.98f, length * 0.52f + midR * 0.2f, midR * 0.98f));
    appendTransformed(dest, sphere, shaft, color);

    // Joint masses — oversized on purpose so clothing/flesh reads as one mesh.
    ball(dest, from, radiusFrom * 1.12f, radiusFrom * 1.12f, radiusFrom * 1.12f, color, detail);
    ball(dest, to, radiusTo * 1.12f, radiusTo * 1.12f, radiusTo * 1.12f, color, detail);
}

// Continuous torso column: hips → waist → chest → collarbone (one character core).
void torsoColumn(
    Mesh3D& dest,
    const Vector3& hip,
    const Vector3& chest,
    const Vector3& collar,
    float hipW,
    float chestW,
    float depth,
    sf::Color mainColor,
    sf::Color deepColor,
    int detail
) {
    Vector3 waist = (hip + chest) * 0.5f + Vector3(0.0f, 0.0f, depth * 0.05f);
    ball(dest, hip, hipW, hipW * 0.72f, depth * 0.95f, deepColor, detail);
    ball(dest, waist, hipW * 0.92f, (chest.y - hip.y) * 0.38f, depth, mainColor, detail);
    ball(dest, chest, chestW, chestW * 0.78f, depth * 1.05f, mainColor, detail);
    ball(dest, collar, chestW * 0.88f, chestW * 0.55f, depth * 0.95f, deepColor, detail);
    // Fill gaps between stacked volumes.
    ball(dest, (hip + waist) * 0.5f, hipW * 0.9f, hipW * 0.5f, depth * 0.95f, mainColor, detail);
    ball(dest, (waist + chest) * 0.5f, chestW * 0.92f, chestW * 0.45f, depth, mainColor, detail);
    ball(dest, (chest + collar) * 0.5f, chestW * 0.9f, chestW * 0.4f, depth * 0.98f, mainColor, detail);
}

void addCleat(
    Mesh3D& dest,
    const Vector3& ankle,
    float toeYaw,
    int detail
) {
    // Cleat fused to ankle so the leg reads as one continuous form.
    ball(dest, ankle + Vector3(0.0f, -0.01f, 0.04f), 0.06f, 0.035f, 0.095f, cleat, detail, toeYaw);
    ball(dest, ankle + Vector3(0.0f, 0.0f, -0.02f), 0.055f, 0.04f, 0.055f, cleat, detail, toeYaw);
    ball(dest, ankle + Vector3(0.0f, -0.025f, 0.03f), 0.065f, 0.02f, 0.11f, cleatSole, detail, toeYaw);
    ball(dest, ankle + Vector3(0.0f, 0.035f, 0.0f), 0.048f, 0.045f, 0.048f, sock, detail);
}

// Mitten-style hand / baseball glove: continuous, not finger sticks.
void glove(
    Mesh3D& dest,
    const Vector3& wrist,
    const Vector3& aimDir,
    float open,
    int detail,
    bool baseballMitt
) {
    Vector3 forward = aimDir.magnitude() > 0.001f ? aimDir.normalized() : Vector3(0.0f, 0.0f, 1.0f);
    Vector3 palm = wrist + forward * (0.045f + open * 0.015f);
    float s = 1.0f + open * 0.1f;

    if (baseballMitt) {
        ball(dest, palm, 0.078f * s, 0.09f * s, 0.055f * s, mitt, detail);
        ball(dest, palm + forward * 0.035f + Vector3(0.0f, 0.045f, 0.0f), 0.065f * s, 0.05f * s, 0.045f * s, mitt, detail);
        ball(dest, palm + Vector3(-0.05f, 0.02f, 0.0f) + forward * 0.02f, 0.042f, 0.055f, 0.038f, mittDeep, detail);
        ball(dest, palm - forward * 0.02f, 0.05f, 0.04f, 0.035f, mittDeep, detail);
    } else {
        // Closed fist / hand for throwing arm.
        ball(dest, palm, 0.04f, 0.042f, 0.048f, skinDeep, detail);
        ball(dest, palm + forward * 0.02f, 0.035f, 0.03f, 0.03f, skin, detail);
    }
}

void baseballCap(
    Mesh3D& dest,
    const Vector3& head,
    float headR,
    float faceYaw,
    int detail
) {
    // Crown wraps skull.
    ball(dest, head + Vector3(0.0f, headR * 0.22f, -headR * 0.04f), headR * 1.08f, headR * 0.58f, headR * 1.08f, cap, detail, faceYaw);
    // Band
    ball(dest, head + Vector3(0.0f, headR * 0.02f, 0.0f), headR * 1.02f, headR * 0.22f, headR * 1.02f, capDeep, detail, faceYaw);
    // Bill
    ball(dest, head + Vector3(0.0f, -headR * 0.02f, headR * 0.62f), headR * 0.7f, headR * 0.1f, headR * 0.38f, capDeep, detail, faceYaw);
    // Logo nub
    ball(dest, head + Vector3(0.0f, headR * 0.28f, headR * 0.55f), headR * 0.12f, headR * 0.1f, headR * 0.08f, accent, detail, faceYaw);
}

void catcherHelmet(
    Mesh3D& dest,
    const Vector3& head,
    float headR,
    int detail
) {
    // Shell wraps head continuously.
    ball(dest, head + Vector3(0.0f, headR * 0.12f, -headR * 0.05f), headR * 1.18f, headR * 0.85f, headR * 1.18f, gear, detail);
    ball(dest, head + Vector3(0.0f, headR * 0.25f, 0.0f), headR * 1.1f, headR * 0.55f, headR * 1.1f, gearDeep, detail);
    // Face mask as soft cage mass, not separate bars.
    ball(dest, head + Vector3(0.0f, -headR * 0.05f, headR * 0.7f), headR * 0.9f, headR * 0.8f, headR * 0.42f, gearDeep, detail);
    ball(dest, head + Vector3(0.0f, -headR * 0.55f, headR * 0.45f), headR * 0.55f, headR * 0.28f, headR * 0.4f, gearDeep, detail);
    // Ear covers fused to shell
    ball(dest, head + Vector3(-headR * 0.85f, 0.0f, 0.0f), headR * 0.28f, headR * 0.4f, headR * 0.35f, gear, detail);
    ball(dest, head + Vector3(headR * 0.85f, 0.0f, 0.0f), headR * 0.28f, headR * 0.4f, headR * 0.35f, gear, detail);
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

    // Proportions closer to a presentable game character: ~1.75 unit tall, clear limb length.
    const float stride = pose.stride;
    const float legLift = pose.frontLegLift;

    const float hipY = 0.90f;
    const float chestY = 1.22f;
    const float collarY = 1.40f;
    const float shoulderY = 1.38f;
    const float headY = 1.62f;
    const float headR = 0.118f;

    // ---- Legs (continuous hip→knee→ankle) ----
    Vector3 plantHip(0.09f, hipY - 0.01f, stride * 0.08f);
    Vector3 plantKnee(0.095f, 0.46f, stride * 0.05f);
    Vector3 plantAnkle(0.095f, 0.075f, 0.02f + stride * 0.06f);

    Vector3 leadHip(-0.09f, hipY - 0.01f, 0.02f + stride * 0.2f);
    Vector3 leadKnee(-0.095f, 0.48f + legLift * 0.2f, 0.05f + stride * 0.4f);
    Vector3 leadAnkle(-0.095f, 0.075f + legLift * 0.28f, 0.08f + stride * 0.85f);

    limb(mesh, plantHip, plantKnee, 0.078f, 0.065f, pants, detail);
    limb(mesh, plantKnee, plantAnkle, 0.062f, 0.05f, pantsDeep, detail);
    limb(mesh, leadHip, leadKnee, 0.078f, 0.065f, pants, detail);
    limb(mesh, leadKnee, leadAnkle, 0.062f, 0.05f, pantsDeep, detail);
    addCleat(mesh, plantAnkle, 0.04f, detail);
    addCleat(mesh, leadAnkle, -0.06f, detail);

    // ---- Core torso (one body, not stacked crates) ----
    Matrix4 upper =
        Matrix4::translation(Vector3(0.0f, hipY, stride * 0.14f)) *
        Matrix4::rotationY(pose.torsoTwist) *
        Matrix4::rotationX(pose.torsoLean) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));
    auto U = [&](const Vector3& p) { return upper.transformPoint(p); };

    Vector3 hip = U(Vector3(0.0f, hipY, 0.0f));
    Vector3 chest = U(Vector3(0.0f, chestY, 0.01f));
    Vector3 collar = U(Vector3(0.0f, collarY, 0.0f));
    torsoColumn(mesh, hip, chest, collar, 0.15f, 0.155f, 0.11f, jersey, jerseyDeep, detail);

    // Belt sits in the hip fold
    ball(mesh, U(Vector3(0.0f, hipY + 0.04f, 0.0f)), 0.145f, 0.03f, 0.105f, belt, detail);
    // Soft jersey stripe (painted on torso, not a floating plate)
    ball(mesh, U(Vector3(0.0f, chestY - 0.02f, 0.095f)), 0.03f, 0.1f, 0.018f, accent, detail);

    // Neck bridges head to collar
    ball(mesh, U(Vector3(0.0f, collarY + 0.05f, 0.015f)), 0.042f, 0.05f, 0.042f, skin, detail);

    // Head + cap
    Vector3 head = U(Vector3(0.0f, headY, 0.02f));
    ball(mesh, head, headR, headR * 1.05f, headR * 0.96f, skin, detail);
    baseballCap(mesh, head, headR, pose.torsoTwist, detail);

    // Shoulders grow out of collar volume
    Vector3 shL = U(Vector3(-0.17f, shoulderY, 0.0f));
    Vector3 shR = U(Vector3(0.17f, shoulderY, 0.0f));
    ball(mesh, shL, 0.085f, 0.075f, 0.085f, jersey, detail);
    ball(mesh, shR, 0.085f, 0.075f, 0.085f, jersey, detail);
    // Sleeve roots — same jersey color so arms read as clothing continuity
    ball(mesh, U(Vector3(-0.12f, shoulderY - 0.04f, 0.0f)), 0.07f, 0.06f, 0.07f, jerseyDeep, detail);
    ball(mesh, U(Vector3(0.12f, shoulderY - 0.04f, 0.0f)), 0.07f, 0.06f, 0.07f, jerseyDeep, detail);

    // Glove arm: jersey sleeve → skin forearm → mitt
    Vector3 gElbow = U(Vector3(-0.25f, shoulderY - 0.18f, 0.09f));
    Vector3 gWrist = U(Vector3(-0.15f, shoulderY - 0.36f, 0.20f));
    limb(mesh, shL, gElbow, 0.058f, 0.05f, jerseyDeep, detail);
    limb(mesh, gElbow, gWrist, 0.048f, 0.04f, skin, detail);
    glove(mesh, gWrist, Vector3(0.2f, -0.15f, 0.85f), 0.25f + pose.gloveShoulderRoll * 0.1f, detail, true);

    // Throwing arm
    Vector3 tElbow = U(Vector3(
        0.26f + pose.throwShoulderYaw * 0.06f,
        shoulderY - 0.20f + pose.throwShoulderPitch * 0.04f,
        0.03f + pose.throwShoulderPitch * 0.09f
    ));
    Vector3 tWrist = U(Vector3(
        0.28f + pose.throwShoulderYaw * 0.09f,
        shoulderY - 0.44f + pose.throwElbow * 0.04f,
        0.05f + pose.throwShoulderPitch * 0.15f
    ));
    limb(mesh, shR, tElbow, 0.058f, 0.05f, jerseyDeep, detail);
    limb(mesh, tElbow, tWrist, 0.048f, 0.04f, skin, detail);
    glove(mesh, tWrist, Vector3(0.1f, -0.2f, 0.5f), 0.0f, detail, false);

    mesh.rebuildNormals();
    return mesh;
}

Mesh3D BaseballPlayer3D::catcher(int detail, const CatcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float bob = pose.crouchBob;
    const float sway = pose.torsoSway;

    const float hipY = 0.48f + bob;
    const float chestY = 0.88f + bob;
    const float collarY = 1.08f + bob;
    const float shoulderY = 1.10f + bob;
    const float headY = 1.34f + bob;
    const float headR = 0.115f;

    // Crouch legs — same continuous construction as pitcher
    for (int side = -1; side <= 1; side += 2) {
        float s = static_cast<float>(side);
        Vector3 hipJ(0.12f * s + sway * 0.02f, hipY - 0.02f, 0.0f);
        Vector3 knee(0.15f * s, 0.32f + bob, 0.05f);
        Vector3 ankle(0.14f * s, 0.07f + bob * 0.25f, 0.10f);

        limb(mesh, hipJ, knee, 0.08f, 0.068f, pants, detail);
        limb(mesh, knee, ankle, 0.065f, 0.05f, pantsDeep, detail);
        // Shin guard = slightly thicker front volume of the same limb, not armor pieces
        Vector3 shin = (knee + ankle) * 0.5f + Vector3(0.0f, 0.0f, 0.03f);
        ball(mesh, shin, 0.068f, 0.10f, 0.055f, gear, detail);
        addCleat(mesh, ankle, -0.1f * s, detail);
    }

    // Core + gear as one crouching body
    Vector3 hip(sway * 0.03f, hipY, -0.02f);
    Vector3 chest(sway * 0.05f, chestY, 0.02f);
    Vector3 collar(sway * 0.04f, collarY, 0.01f);
    torsoColumn(mesh, hip, chest, collar, 0.16f, 0.15f, 0.12f, jerseyDeep, jerseyDeep, detail);

    // Chest protector shell hugging the torso (slightly larger, same centerline)
    ball(mesh, chest + Vector3(0.0f, 0.02f, 0.05f), 0.155f, 0.18f, 0.09f, gear, detail);
    ball(mesh, chest + Vector3(0.0f, 0.08f, 0.07f), 0.13f, 0.12f, 0.07f, gearDeep, detail);
    ball(mesh, hip + Vector3(0.0f, 0.04f, 0.0f), 0.15f, 0.03f, 0.11f, belt, detail);

    // Shoulders / neck / head
    Vector3 shL(-0.165f + sway * 0.03f, shoulderY, 0.02f);
    Vector3 shR(0.165f + sway * 0.03f, shoulderY, 0.02f);
    ball(mesh, shL, 0.082f, 0.072f, 0.082f, gearLight, detail);
    ball(mesh, shR, 0.082f, 0.072f, 0.082f, gearLight, detail);
    ball(mesh, Vector3(sway * 0.03f, shoulderY - 0.04f, 0.0f), 0.12f, 0.06f, 0.09f, gearDeep, detail);
    ball(mesh, Vector3(sway * 0.03f, collarY + 0.05f, 0.02f), 0.042f, 0.05f, 0.042f, skin, detail);

    Vector3 head(sway * 0.03f, headY, 0.02f);
    ball(mesh, head, headR, headR * 1.04f, headR * 0.96f, skin, detail);
    catcherHelmet(mesh, head, headR, detail);

    // Free arm
    Vector3 freeElbow(0.26f, shoulderY - 0.15f, 0.05f);
    Vector3 freeWrist(0.30f, shoulderY - 0.34f, 0.06f);
    limb(mesh, shR, freeElbow, 0.055f, 0.048f, gearDeep, detail);
    limb(mesh, freeElbow, freeWrist, 0.046f, 0.038f, skin, detail);
    glove(mesh, freeWrist, Vector3(0.2f, -0.4f, 0.3f), 0.0f, detail, false);

    // Glove arm tracks mitt
    Vector3 mittPos(
        -0.42f + pose.mittSide,
        shoulderY - 0.22f + pose.mittHeight,
        0.30f + pose.mittReach
    );
    Vector3 gElbow(
        -0.32f + pose.mittSide * 0.55f,
        shoulderY - 0.14f + pose.mittHeight * 0.45f,
        0.14f + pose.mittReach * 0.35f
    );
    Vector3 gShoulder = shL + Vector3(pose.mittSide * 0.1f, pose.mittHeight * 0.06f, pose.mittReach * 0.06f);
    limb(mesh, gShoulder, gElbow, 0.056f, 0.05f, gearDeep, detail);
    limb(mesh, gElbow, mittPos, 0.048f, 0.04f, skin, detail);
    glove(
        mesh,
        mittPos,
        Vector3(-0.15f + pose.mittSide, pose.mittHeight * 0.25f, 0.9f),
        pose.gloveOpen,
        detail,
        true
    );

    mesh.rebuildNormals();
    return mesh;
}
