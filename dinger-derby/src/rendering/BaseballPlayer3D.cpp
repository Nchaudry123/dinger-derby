#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

// KH2-inspired sports kit: clean values, soft contrast, readable from a distance.
const sf::Color skin(220, 176, 142);
const sf::Color skinShade(198, 152, 120);
const sf::Color jersey(242, 244, 248);
const sf::Color jerseyShade(220, 224, 232);
const sf::Color pants(56, 64, 80);
const sf::Color pantsShade(46, 52, 66);
const sf::Color belt(38, 40, 48);
const sf::Color cleats(34, 34, 40);
const sf::Color cap(34, 52, 96);
const sf::Color capShade(26, 40, 78);
const sf::Color accent(210, 56, 64);
const sf::Color gear(44, 58, 82);
const sf::Color gearShade(34, 46, 66);
const sf::Color gearLight(58, 76, 104);
const sf::Color mitt(164, 112, 70);
const sf::Color mittShade(136, 90, 54);
const sf::Color sock(246, 246, 250);

constexpr float pi = 3.1415926535f;

int ringsFor(int detail) {
    // Enough rings for smooth Gouraud like mid-poly PS2 characters.
    return detail >= 2 ? 10 : (detail >= 1 ? 8 : 7);
}

int segsFor(int detail) {
    return detail >= 2 ? 16 : (detail >= 1 ? 12 : 10);
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

// Smooth ellipsoid. Primary volume primitive for KH-style shape language.
void addEllipsoid(
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

// Align local +Y to direction `dir`, place capsule between two points.
// Joint balls at the ends keep limbs continuous with the torso (classic game-char construction).
void addCapsule(
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
        addEllipsoid(dest, from, radiusFrom, radiusFrom, radiusFrom, color, detail);
        return;
    }

    Vector3 dir = delta * (1.0f / length);
    float yaw = std::atan2(dir.x, dir.z);
    float pitch = std::acos(std::clamp(dir.y, -1.0f, 1.0f));
    float midRadius = (radiusFrom + radiusTo) * 0.5f;
    Vector3 mid = (from + to) * 0.5f;

    // Shaft: elongated sphere along Y after orientation.
    float halfLen = length * 0.5f;
    Mesh3D sphere = Mesh3D::sphere(1.0f, ringsFor(detail), segsFor(detail));
    Matrix4 shaft =
        Matrix4::translation(mid) *
        Matrix4::rotationY(yaw) *
        Matrix4::rotationX(pitch) *
        Matrix4::scale(Vector3(midRadius, halfLen * 0.92f + midRadius * 0.15f, midRadius));
    appendTransformed(dest, sphere, shaft, color);

    // End balls slightly larger for seamless joints.
    addEllipsoid(dest, from, radiusFrom * 1.05f, radiusFrom * 1.05f, radiusFrom * 1.05f, color, detail);
    addEllipsoid(dest, to, radiusTo * 1.05f, radiusTo * 1.05f, radiusTo * 1.05f, color, detail);
}

void addFoot(
    Mesh3D& dest,
    const Vector3& ankle,
    float yaw,
    int detail
) {
    addEllipsoid(dest, ankle + Vector3(0.0f, -0.015f, 0.035f), 0.055f, 0.03f, 0.10f, cleats, detail, yaw);
    addEllipsoid(dest, ankle + Vector3(0.0f, 0.01f, -0.02f), 0.05f, 0.035f, 0.055f, cleats, detail, yaw);
    addEllipsoid(dest, ankle + Vector3(0.0f, 0.04f, 0.0f), 0.045f, 0.05f, 0.045f, sock, detail);
}

// Soft baseball glove: one palm mass + thumb lobe + finger ridge (not separate slabs).
void addGlove(
    Mesh3D& dest,
    const Vector3& wrist,
    const Vector3& forward,
    float open,
    int detail
) {
    Vector3 f = forward.magnitude() > 0.001f ? forward.normalized() : Vector3(0.0f, 0.0f, 1.0f);
    Vector3 palm = wrist + f * (0.05f + open * 0.02f) + Vector3(0.0f, 0.01f, 0.0f);
    float s = 1.0f + open * 0.12f;

    addEllipsoid(dest, palm, 0.075f * s, 0.085f * s, 0.05f * s, mitt, detail);
    addEllipsoid(dest, palm + f * 0.04f + Vector3(0.0f, 0.05f, 0.0f), 0.06f * s, 0.05f * s, 0.04f * s, mitt, detail);
    addEllipsoid(dest, palm + Vector3(-0.05f, 0.02f, 0.0f) + f * 0.02f, 0.04f, 0.055f, 0.035f, mittShade, detail);
    addEllipsoid(dest, palm + Vector3(0.0f, 0.0f, 0.0f) - f * 0.02f, 0.05f, 0.04f, 0.03f, mittShade, detail);
}

// Simple cap that wraps the skull instead of floating plates.
void addCap(Mesh3D& dest, const Vector3& headCenter, float headR, float yaw, int detail) {
    addEllipsoid(
        dest,
        headCenter + Vector3(0.0f, headR * 0.28f, -headR * 0.05f),
        headR * 1.05f,
        headR * 0.55f,
        headR * 1.05f,
        cap,
        detail,
        yaw
    );
    addEllipsoid(
        dest,
        headCenter + Vector3(0.0f, headR * 0.05f, headR * 0.55f),
        headR * 0.72f,
        headR * 0.12f,
        headR * 0.42f,
        capShade,
        detail,
        yaw
    );
    // Tiny logo bump
    addEllipsoid(
        dest,
        headCenter + Vector3(0.0f, headR * 0.35f, headR * 0.55f),
        headR * 0.14f,
        headR * 0.1f,
        headR * 0.08f,
        accent,
        detail,
        yaw
    );
}

}

PitcherPose BaseballPlayer3D::pitcherIdlePose(float timeSeconds) {
    PitcherPose pose;
    pose.torsoTwist = std::sin(timeSeconds * 1.25f) * 0.035f;
    pose.torsoLean = std::sin(timeSeconds * 0.85f) * 0.018f;
    pose.throwShoulderPitch = std::sin(timeSeconds * 1.05f) * 0.04f;
    pose.gloveShoulderPitch = std::sin(timeSeconds * 1.05f + 0.5f) * 0.035f;
    pose.frontLegLift = std::max(0.0f, std::sin(timeSeconds * 0.65f)) * 0.025f;
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
    pose.torsoSway = std::sin(timeSeconds * 1.0f) * 0.03f;
    pose.crouchBob = std::sin(timeSeconds * 1.6f) * 0.01f;
    pose.mittSide = std::sin(timeSeconds * 0.75f) * 0.018f;
    pose.mittHeight = std::sin(timeSeconds * 1.15f) * 0.012f;
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

    // Stylized athlete proportions (~KH-era readability): clear torso, tapered legs, slightly large head.
    const float hipY = 0.92f;
    const float shoulderY = 1.42f;
    const float headY = 1.68f;
    const float headR = 0.115f;
    const float stride = pose.stride;
    const float legLift = pose.frontLegLift;

    // --- Legs ---
    Vector3 plantHip(0.095f, hipY - 0.02f, stride * 0.1f);
    Vector3 plantKnee(0.10f, 0.48f, stride * 0.06f);
    Vector3 plantAnkle(0.10f, 0.08f, 0.02f + stride * 0.08f);

    Vector3 leadHip(-0.095f, hipY - 0.02f, 0.02f + stride * 0.22f);
    Vector3 leadKnee(-0.10f, 0.50f + legLift * 0.22f, 0.05f + stride * 0.42f);
    Vector3 leadAnkle(-0.10f, 0.08f + legLift * 0.3f, 0.08f + stride * 0.88f);

    addCapsule(mesh, plantHip, plantKnee, 0.075f, 0.062f, pants, detail);
    addCapsule(mesh, plantKnee, plantAnkle, 0.06f, 0.05f, pantsShade, detail);
    addCapsule(mesh, leadHip, leadKnee, 0.075f, 0.062f, pants, detail);
    addCapsule(mesh, leadKnee, leadAnkle, 0.06f, 0.05f, pantsShade, detail);
    addFoot(mesh, plantAnkle, 0.05f, detail);
    addFoot(mesh, leadAnkle, -0.08f, detail);

    // Pelvis / belt as continuous hip volume
    addEllipsoid(mesh, Vector3(0.0f, hipY, stride * 0.12f), 0.155f, 0.10f, 0.11f, pants, detail);
    addEllipsoid(mesh, Vector3(0.0f, hipY + 0.045f, stride * 0.12f), 0.145f, 0.035f, 0.10f, belt, detail);

    // Upper body with twist/lean
    Matrix4 upper =
        Matrix4::translation(Vector3(0.0f, hipY, stride * 0.16f)) *
        Matrix4::rotationY(pose.torsoTwist) *
        Matrix4::rotationX(pose.torsoLean) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));
    auto up = [&](const Vector3& p) { return upper.transformPoint(p); };

    // Torso: abdomen + chest + upper chest (overlapping = one form)
    addEllipsoid(mesh, up(Vector3(0.0f, hipY + 0.16f, 0.0f)), 0.13f, 0.14f, 0.10f, jerseyShade, detail);
    addEllipsoid(mesh, up(Vector3(0.0f, hipY + 0.32f, 0.01f)), 0.15f, 0.15f, 0.115f, jersey, detail);
    addEllipsoid(mesh, up(Vector3(0.0f, shoulderY - 0.08f, 0.0f)), 0.145f, 0.11f, 0.11f, jersey, detail);
    // Soft accent (number area)
    addEllipsoid(mesh, up(Vector3(0.0f, hipY + 0.30f, 0.095f)), 0.035f, 0.09f, 0.02f, accent, detail);

    // Collar / neck bridge
    addEllipsoid(mesh, up(Vector3(0.0f, shoulderY + 0.02f, 0.01f)), 0.07f, 0.05f, 0.06f, jerseyShade, detail);
    addEllipsoid(mesh, up(Vector3(0.0f, shoulderY + 0.08f, 0.015f)), 0.045f, 0.055f, 0.045f, skin, detail);

    // Head
    Vector3 head = up(Vector3(0.0f, headY, 0.02f));
    addEllipsoid(mesh, head, headR, headR * 1.06f, headR * 0.96f, skin, detail);
    addCap(mesh, head, headR, pose.torsoTwist, detail);

    // Shoulders
    Vector3 shL = up(Vector3(-0.175f, shoulderY - 0.01f, 0.0f));
    Vector3 shR = up(Vector3(0.175f, shoulderY - 0.01f, 0.0f));
    addEllipsoid(mesh, shL, 0.08f, 0.07f, 0.08f, jersey, detail);
    addEllipsoid(mesh, shR, 0.08f, 0.07f, 0.08f, jersey, detail);

    // Glove arm
    Vector3 gElbow = up(Vector3(-0.26f, shoulderY - 0.20f, 0.10f));
    Vector3 gWrist = up(Vector3(-0.16f, shoulderY - 0.38f, 0.22f));
    addCapsule(mesh, shL, gElbow, 0.055f, 0.048f, jerseyShade, detail);
    addCapsule(mesh, gElbow, gWrist, 0.046f, 0.04f, skin, detail);
    addGlove(mesh, gWrist, Vector3(0.15f, -0.2f, 0.8f), 0.2f + pose.gloveShoulderRoll * 0.15f, detail);

    // Throwing arm
    Vector3 tElbow = up(Vector3(
        0.28f + pose.throwShoulderYaw * 0.07f,
        shoulderY - 0.22f + pose.throwShoulderPitch * 0.05f,
        0.04f + pose.throwShoulderPitch * 0.1f
    ));
    Vector3 tWrist = up(Vector3(
        0.30f + pose.throwShoulderYaw * 0.1f,
        shoulderY - 0.46f + pose.throwElbow * 0.05f,
        0.06f + pose.throwShoulderPitch * 0.16f
    ));
    addCapsule(mesh, shR, tElbow, 0.055f, 0.048f, jerseyShade, detail);
    addCapsule(mesh, tElbow, tWrist, 0.046f, 0.04f, skin, detail);
    addEllipsoid(mesh, tWrist, 0.038f, 0.04f, 0.045f, skinShade, detail);

    mesh.rebuildNormals();
    return mesh;
}

Mesh3D BaseballPlayer3D::catcher(int detail, const CatcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float bob = pose.crouchBob;
    const float sway = pose.torsoSway;
    const float hipY = 0.50f + bob;
    const float shoulderY = 1.15f + bob;
    const float headY = 1.42f + bob;
    const float headR = 0.112f;

    // Crouched legs — continuous capsules, shin guard as soft shell on the front.
    for (int side = -1; side <= 1; side += 2) {
        float s = static_cast<float>(side);
        Vector3 hip(0.13f * s, hipY - 0.03f, 0.0f);
        Vector3 knee(0.16f * s, 0.34f + bob, 0.05f);
        Vector3 ankle(0.15f * s, 0.07f + bob * 0.3f, 0.10f);

        addCapsule(mesh, hip, knee, 0.08f, 0.068f, pants, detail);
        addCapsule(mesh, knee, ankle, 0.065f, 0.052f, pantsShade, detail);
        // Guard bulge fused to shin (not floating armor plate)
        Vector3 shinMid = (knee + ankle) * 0.5f + Vector3(0.0f, 0.0f, 0.035f);
        addEllipsoid(mesh, shinMid, 0.07f, 0.11f, 0.055f, gear, detail);
        addFoot(mesh, ankle, -0.12f * s, detail);
    }

    // Hips
    addEllipsoid(mesh, Vector3(sway * 0.03f, hipY, -0.02f), 0.175f, 0.105f, 0.13f, pants, detail);
    addEllipsoid(mesh, Vector3(sway * 0.03f, hipY + 0.04f, -0.02f), 0.16f, 0.03f, 0.115f, belt, detail);

    // Torso + chest protector as layered soft shells
    float chestY = (hipY + shoulderY) * 0.52f;
    addEllipsoid(mesh, Vector3(sway * 0.05f, chestY - 0.02f, 0.01f), 0.15f, 0.20f, 0.12f, jerseyShade, detail);
    addEllipsoid(mesh, Vector3(sway * 0.05f, chestY + 0.04f, 0.06f), 0.155f, 0.19f, 0.09f, gear, detail);
    addEllipsoid(mesh, Vector3(sway * 0.05f, chestY + 0.1f, 0.08f), 0.13f, 0.12f, 0.06f, gearShade, detail);

    Vector3 shL(-0.175f + sway * 0.04f, shoulderY, 0.02f);
    Vector3 shR(0.175f + sway * 0.04f, shoulderY, 0.02f);
    addEllipsoid(mesh, shL, 0.085f, 0.075f, 0.085f, gearLight, detail);
    addEllipsoid(mesh, shR, 0.085f, 0.075f, 0.085f, gearLight, detail);
    addEllipsoid(mesh, Vector3(sway * 0.03f, shoulderY - 0.05f, 0.0f), 0.13f, 0.07f, 0.09f, gearShade, detail);

    // Neck / head / helmet
    addEllipsoid(mesh, Vector3(sway * 0.03f, shoulderY + 0.08f, 0.02f), 0.045f, 0.055f, 0.045f, skin, detail);
    Vector3 head(sway * 0.04f, headY, 0.02f);
    addEllipsoid(mesh, head, headR, headR * 1.05f, headR * 0.96f, skin, detail);
    // Helmet shell wraps head
    addEllipsoid(mesh, head + Vector3(0.0f, 0.03f, -0.01f), headR * 1.12f, headR * 0.7f, headR * 1.12f, gear, detail);
    // Mask as soft face volume (not bar cage)
    addEllipsoid(mesh, head + Vector3(0.0f, -0.01f, 0.08f), headR * 0.85f, headR * 0.75f, headR * 0.45f, gearShade, detail);
    addEllipsoid(mesh, head + Vector3(0.0f, -0.07f, 0.05f), headR * 0.55f, headR * 0.28f, headR * 0.4f, gearShade, detail);

    // Free arm
    Vector3 freeElbow(0.28f, shoulderY - 0.16f, 0.05f);
    Vector3 freeWrist(0.32f, shoulderY - 0.36f, 0.07f);
    addCapsule(mesh, shR, freeElbow, 0.052f, 0.046f, gearShade, detail);
    addCapsule(mesh, freeElbow, freeWrist, 0.044f, 0.038f, skin, detail);
    addEllipsoid(mesh, freeWrist, 0.036f, 0.036f, 0.04f, skinShade, detail);

    // Glove arm + mitt
    Vector3 mittPos(
        -0.44f + pose.mittSide,
        shoulderY - 0.24f + pose.mittHeight,
        0.32f + pose.mittReach
    );
    Vector3 gElbow(
        -0.34f + pose.mittSide * 0.6f,
        shoulderY - 0.16f + pose.mittHeight * 0.5f,
        0.16f + pose.mittReach * 0.4f
    );
    Vector3 gShoulder = shL + Vector3(pose.mittSide * 0.12f, pose.mittHeight * 0.08f, pose.mittReach * 0.08f);
    addCapsule(mesh, gShoulder, gElbow, 0.054f, 0.048f, gearShade, detail);
    addCapsule(mesh, gElbow, mittPos, 0.046f, 0.04f, skin, detail);
    addGlove(mesh, mittPos, Vector3(-0.2f + pose.mittSide, pose.mittHeight * 0.3f, 0.9f), pose.gloveOpen, detail);

    mesh.rebuildNormals();
    return mesh;
}
