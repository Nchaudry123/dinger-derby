#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

constexpr float pi = 3.1415926535f;

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

    // Phases: gather 0–0.25, leg lift 0.15–0.45, drive 0.4–0.55, release 0.55, follow 0.55–1.
    float gather = smoothstep(0.0f, 0.22f, t);
    float legLift = smoothstep(0.12f, 0.42f, t) * (1.0f - smoothstep(0.45f, 0.62f, t));
    float drive = smoothstep(0.40f, 0.55f, t);
    float release = smoothstep(0.50f, 0.58f, t);
    float follow = smoothstep(0.55f, 1.0f, t);

    pose.torsoTwist = lerp(0.0f, -0.55f, gather) + lerp(0.0f, 0.95f, drive) + lerp(0.0f, 0.25f, follow);
    pose.torsoLean = lerp(0.0f, 0.12f, gather) + lerp(0.0f, 0.35f, drive) + lerp(0.0f, 0.2f, follow);
    pose.frontLegLift = legLift * 0.85f;
    pose.stride = drive * 0.22f + follow * 0.08f;

    // Arm: back → overhead → snap forward
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

    // Soft track of mitt toward aim (world coords); model is rotated 180° in the demo.
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
    const float shin = 0.42f;
    const float thigh = 0.40f;
    const float torso = 0.52f;
    const float headR = 0.11f;

    const float hipY = footY + shin + thigh * 0.92f;
    const float shoulderY = hipY + torso;
    const float headY = shoulderY + 0.16f + headR;
    const float stride = pose.stride;
    const float legLift = pose.frontLegLift;

    // Plant leg (right / glove side slightly back)
    addLimb(
        mesh,
        Vector3(0.11f, footY + shin * 0.5f, -0.02f + stride * 0.15f),
        0.055f,
        shin,
        pants,
        detail,
        0.0f,
        -0.05f + pose.torsoLean * 0.15f,
        0.0f
    );
    addLimb(
        mesh,
        Vector3(0.10f, footY + shin + thigh * 0.45f, -0.01f + stride * 0.1f),
        0.065f,
        thigh,
        pants,
        detail,
        0.0f,
        -0.06f,
        -0.04f
    );

    // Lead leg (left) lifts then strides toward plate
    float leadFootZ = 0.04f + stride * 0.85f;
    float leadFootY = footY + legLift * 0.35f;
    float leadShinPitch = 0.08f + legLift * 0.9f - stride * 0.35f;
    addLimb(
        mesh,
        Vector3(-0.11f, leadFootY + shin * 0.5f, leadFootZ),
        0.055f,
        shin,
        pants,
        detail,
        0.0f,
        leadShinPitch,
        0.0f
    );
    addLimb(
        mesh,
        Vector3(-0.10f, hipY - thigh * 0.35f + legLift * 0.15f, 0.02f + stride * 0.35f),
        0.065f,
        thigh,
        pants,
        detail,
        0.0f,
        0.12f + legLift * 0.7f,
        0.05f
    );

    addBox(mesh, Vector3(0.11f, footY * 0.5f, 0.02f + stride * 0.1f), 0.10f, 0.05f, 0.18f, cleats);
    addBox(mesh, Vector3(-0.11f, leadFootY * 0.5f + 0.02f, leadFootZ + 0.04f), 0.10f, 0.05f, 0.18f, cleats);
    addBox(mesh, Vector3(0.11f, footY + 0.08f, 0.0f + stride * 0.1f), 0.07f, 0.10f, 0.07f, sock);
    addBox(mesh, Vector3(-0.11f, leadFootY + 0.08f, leadFootZ), 0.07f, 0.10f, 0.07f, sock);

    // Hips + upper body twist around spine
    addBox(mesh, Vector3(0.0f, hipY, stride * 0.2f), 0.34f, 0.14f, 0.18f, pants, pose.torsoTwist * 0.35f);
    addBox(mesh, Vector3(0.0f, hipY + 0.06f, stride * 0.2f), 0.36f, 0.04f, 0.19f, belt, pose.torsoTwist * 0.35f);

    Matrix4 upper =
        Matrix4::translation(Vector3(0.0f, hipY, stride * 0.25f)) *
        Matrix4::rotationY(pose.torsoTwist) *
        Matrix4::rotationX(pose.torsoLean) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));

    // Build torso/head/arms into a temp mesh then bake with upper transform — simpler: apply upper to centers.
    auto upperPoint = [&](const Vector3& p) {
        return upper.transformPoint(p);
    };

    Vector3 torsoCenter = upperPoint(Vector3(0.0f, hipY + torso * 0.48f, 0.0f));
    addBox(mesh, torsoCenter, 0.38f, torso * 0.9f, 0.20f, jersey, pose.torsoTwist, pose.torsoLean);
    addBox(
        mesh,
        upperPoint(Vector3(0.0f, hipY + torso * 0.55f, 0.105f)),
        0.06f,
        torso * 0.55f,
        0.02f,
        logoRed,
        pose.torsoTwist,
        pose.torsoLean
    );

    Vector3 shoulderL = upperPoint(Vector3(-0.20f, shoulderY - 0.02f, 0.0f));
    Vector3 shoulderR = upperPoint(Vector3(0.20f, shoulderY - 0.02f, 0.0f));
    addSphere(mesh, shoulderL, 0.07f, jersey, detail);
    addSphere(mesh, shoulderR, 0.07f, jersey, detail);

    Vector3 headCenter = upperPoint(Vector3(0.0f, headY, 0.02f));
    addSphere(mesh, headCenter, headR, skin, detail);
    addSphere(mesh, upperPoint(Vector3(0.0f, headY + 0.04f, 0.0f)), headR * 0.92f, capNavy, detail);
    addBox(mesh, upperPoint(Vector3(0.0f, headY - 0.02f, 0.12f)), 0.16f, 0.03f, 0.10f, capNavy, pose.torsoTwist);
    addBox(mesh, upperPoint(Vector3(0.0f, shoulderY + 0.05f, 0.01f)), 0.08f, 0.08f, 0.08f, skin, pose.torsoTwist);

    // Glove arm
    float gPitch = 0.55f + pose.gloveShoulderPitch;
    float gRoll = 0.35f + pose.gloveShoulderRoll;
    Vector3 gloveUpper = upperPoint(Vector3(-0.28f, shoulderY - 0.18f, 0.10f));
    Vector3 gloveFore = upperPoint(Vector3(-0.22f, shoulderY - 0.38f, 0.18f));
    Vector3 mittCenter = upperPoint(Vector3(-0.16f, shoulderY - 0.48f, 0.28f));
    addLimb(mesh, gloveUpper, 0.045f, 0.30f, jersey, detail, pose.torsoTwist, gPitch, gRoll);
    addLimb(mesh, gloveFore, 0.04f, 0.26f, skin, detail, pose.torsoTwist * 0.8f, gPitch + 0.3f, gRoll * 0.5f);
    addSphere(mesh, mittCenter, 0.09f, mitt, detail);
    addSphere(mesh, upperPoint(Vector3(-0.12f, shoulderY - 0.46f, 0.34f)), 0.055f, mittDark, detail);

    // Throwing arm
    float tPitch = 0.25f + pose.throwShoulderPitch;
    float tYaw = -0.2f + pose.throwShoulderYaw;
    float tElbow = 0.15f + pose.throwElbow;
    Vector3 throwUpper = upperPoint(Vector3(0.28f, shoulderY - 0.16f, 0.02f));
    Vector3 throwFore = upperPoint(Vector3(
        0.32f + pose.throwShoulderYaw * 0.08f,
        shoulderY - 0.42f + pose.throwShoulderPitch * 0.05f,
        0.04f + pose.throwShoulderPitch * 0.12f
    ));
    Vector3 hand = upperPoint(Vector3(
        0.34f + pose.throwShoulderYaw * 0.1f,
        shoulderY - 0.58f + pose.throwElbow * 0.06f,
        0.05f + pose.throwShoulderPitch * 0.18f
    ));
    addLimb(mesh, throwUpper, 0.045f, 0.30f, jersey, detail, tYaw, tPitch, -0.2f);
    addLimb(mesh, throwFore, 0.04f, 0.28f, skin, detail, tYaw * 0.7f, tElbow, -0.1f);
    addSphere(mesh, hand, 0.045f, skin, detail);

    mesh.rebuildNormals();
    return mesh;
}

Mesh3D BaseballPlayer3D::catcher(int detail, const CatcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float footY = 0.05f + pose.crouchBob * 0.5f;
    const float hipY = 0.55f + pose.crouchBob;
    const float shoulderY = 1.28f + pose.crouchBob + pose.torsoSway * 0.05f;
    const float headY = 1.58f + pose.crouchBob;

    addBox(mesh, Vector3(-0.18f, 0.22f + pose.crouchBob, 0.06f), 0.14f, 0.38f, 0.16f, gear, 0.15f, 0.35f, 0.0f);
    addBox(mesh, Vector3(0.18f, 0.22f + pose.crouchBob, 0.06f), 0.14f, 0.38f, 0.16f, gear, -0.15f, 0.35f, 0.0f);
    addBox(mesh, Vector3(-0.20f, footY, 0.10f), 0.12f, 0.06f, 0.20f, cleats);
    addBox(mesh, Vector3(0.20f, footY, 0.10f), 0.12f, 0.06f, 0.20f, cleats);

    addLimb(mesh, Vector3(-0.16f, 0.42f + pose.crouchBob, 0.02f), 0.07f, 0.34f, pants, detail, 0.2f, 0.9f, 0.15f);
    addLimb(mesh, Vector3(0.16f, 0.42f + pose.crouchBob, 0.02f), 0.07f, 0.34f, pants, detail, -0.2f, 0.9f, -0.15f);

    addBox(mesh, Vector3(0.0f, hipY, -0.02f), 0.40f, 0.18f, 0.24f, pants, pose.torsoSway);
    addBox(mesh, Vector3(0.0f, hipY + 0.08f, -0.02f), 0.42f, 0.05f, 0.25f, belt, pose.torsoSway);

    float chestY = (hipY + shoulderY) * 0.5f;
    addBox(mesh, Vector3(pose.torsoSway * 0.08f, chestY, 0.04f), 0.42f, 0.58f, 0.22f, gear, pose.torsoSway);
    addBox(mesh, Vector3(pose.torsoSway * 0.08f, chestY + 0.02f, 0.14f), 0.36f, 0.48f, 0.06f, gearHighlight, pose.torsoSway);
    addBox(mesh, Vector3(0.0f, shoulderY - 0.08f, -0.02f), 0.46f, 0.16f, 0.20f, jersey, pose.torsoSway);

    addSphere(mesh, Vector3(-0.24f + pose.torsoSway * 0.05f, shoulderY, 0.02f), 0.075f, gear, detail);
    addSphere(mesh, Vector3(0.24f + pose.torsoSway * 0.05f, shoulderY, 0.02f), 0.075f, gear, detail);

    // Mitt tracks aim (offsets in model space; +Z is "forward" on crouch model before world 180°).
    Vector3 mittBase(
        -0.48f + pose.mittSide,
        shoulderY - 0.32f + pose.mittHeight,
        0.34f + pose.mittReach
    );
    Vector3 forearm(
        -0.42f + pose.mittSide * 0.7f,
        shoulderY - 0.28f + pose.mittHeight * 0.7f,
        0.22f + pose.mittReach * 0.6f
    );
    Vector3 upperArm(
        -0.34f + pose.mittSide * 0.4f,
        shoulderY - 0.14f + pose.mittHeight * 0.35f,
        0.12f + pose.mittReach * 0.3f
    );

    addLimb(mesh, upperArm, 0.05f, 0.28f, gear, detail, 0.1f + pose.mittSide, 0.7f - pose.mittHeight, 0.4f);
    addLimb(mesh, forearm, 0.045f, 0.24f, skin, detail, 0.05f + pose.mittSide, 1.0f, 0.2f);

    float open = 1.0f + pose.gloveOpen * 0.25f;
    addSphere(mesh, mittBase, 0.11f * open, mitt, detail);
    addSphere(mesh, mittBase + Vector3(-0.04f, 0.04f, 0.06f), 0.06f, mittDark, detail);
    addBox(
        mesh,
        mittBase + Vector3(-0.02f, -0.02f, 0.08f),
        0.08f + pose.gloveOpen * 0.04f,
        0.14f,
        0.04f,
        mittDark,
        0.4f + pose.mittSide,
        0.3f,
        0.0f
    );

    addLimb(mesh, Vector3(0.32f, shoulderY - 0.16f, 0.06f), 0.05f, 0.28f, gear, detail, -0.15f, 0.55f, -0.35f);
    addLimb(mesh, Vector3(0.36f, shoulderY - 0.36f, 0.08f), 0.045f, 0.24f, skin, detail, -0.1f, 0.4f, -0.2f);
    addSphere(mesh, Vector3(0.38f, shoulderY - 0.50f, 0.08f), 0.045f, skin, detail);

    addSphere(mesh, Vector3(pose.torsoSway * 0.05f, headY, 0.02f), 0.12f, skin, detail);
    addSphere(mesh, Vector3(pose.torsoSway * 0.05f, headY + 0.02f, 0.0f), 0.125f, gear, detail);
    addBox(mesh, Vector3(pose.torsoSway * 0.05f, headY - 0.02f, 0.12f), 0.18f, 0.14f, 0.08f, gearHighlight);
    addBox(mesh, Vector3(0.0f, headY + 0.04f, 0.155f), 0.14f, 0.02f, 0.02f, gear);
    addBox(mesh, Vector3(0.0f, headY - 0.04f, 0.155f), 0.14f, 0.02f, 0.02f, gear);
    addBox(mesh, Vector3(-0.05f, headY, 0.155f), 0.02f, 0.12f, 0.02f, gear);
    addBox(mesh, Vector3(0.05f, headY, 0.155f), 0.02f, 0.12f, 0.02f, gear);
    addBox(mesh, Vector3(0.0f, shoulderY + 0.08f, 0.02f), 0.09f, 0.10f, 0.09f, skin);

    mesh.rebuildNormals();
    return mesh;
}
