#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

// ── palette ──────────────────────────────────────────────────────────────
const sf::Color skin(234, 196, 164);
const sf::Color skinDeep(214, 174, 142);
const sf::Color skinShadow(198, 158, 128);
const sf::Color jersey(250, 252, 255);
const sf::Color jerseyDeep(232, 236, 244);
const sf::Color jerseyShade(220, 226, 236);
const sf::Color pants(56, 64, 82);
const sf::Color pantsDeep(46, 52, 68);
const sf::Color pantsShade(40, 46, 60);
const sf::Color belt(42, 44, 52);
const sf::Color cleat(36, 36, 44);
const sf::Color cleatSole(52, 52, 60);
const sf::Color cap(34, 52, 102);
const sf::Color capDeep(26, 42, 86);
const sf::Color accent(208, 50, 58);
const sf::Color gear(46, 62, 90);
const sf::Color gearDeep(38, 50, 72);
const sf::Color gearLight(66, 86, 116);
const sf::Color gearHighlight(78, 98, 130);
const sf::Color mitt(172, 120, 78);
const sf::Color mittDeep(140, 94, 56);
const sf::Color mittPad(188, 138, 92);
const sf::Color sock(250, 250, 255);

// Canonical RHP proportions (meters-ish; feet at y≈0, +Z to plate).
constexpr float kHipY = 0.92f;
constexpr float kShoulderY = 1.42f;
constexpr float kHeadY = 1.64f;
constexpr float kHeadR = 0.112f;
constexpr float kUpperArm = 0.30f;
constexpr float kForearm = 0.27f;
constexpr float kThigh = 0.42f;
constexpr float kShin = 0.40f;

int ringsFor(int detail) {
    return detail >= 2 ? 16 : (detail >= 1 ? 12 : 9);
}

int segsFor(int detail) {
    return detail >= 2 ? 24 : (detail >= 1 ? 18 : 12);
}

float smooth01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Vector3 lerp(const Vector3& a, const Vector3& b, float t) {
    return a + (b - a) * t;
}

float easeInOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

Vector3 safeNorm(const Vector3& v, const Vector3& fallback = Vector3(0.0f, 1.0f, 0.0f)) {
    float m = v.magnitude();
    if (m < 1e-5f) {
        return fallback;
    }
    return v * (1.0f / m);
}

void appendTransformed(
    Mesh3D& dest,
    const Mesh3D& source,
    const Matrix4& transform,
    sf::Color color
) {
    int base = static_cast<int>(dest.vertices.size());
    for (const Vector3& v : source.vertices) {
        dest.vertices.push_back(transform.transformPoint(v));
    }
    for (const Edge3D& e : source.edges) {
        dest.edges.push_back({e.start + base, e.end + base});
    }
    for (const Triangle3D& tri : source.triangles) {
        dest.triangles.push_back({tri.a + base, tri.b + base, tri.c + base});
        dest.triangleColors.push_back(color);
    }
}

void ball(
    Mesh3D& dest,
    const Vector3& c,
    float rx,
    float ry,
    float rz,
    sf::Color color,
    int detail
) {
    Mesh3D s = Mesh3D::sphere(1.0f, ringsFor(detail), segsFor(detail));
    Matrix4 x =
        Matrix4::translation(c) *
        Matrix4::scale(Vector3(rx, ry, rz));
    appendTransformed(dest, s, x, color);
}

// Oriented capsule: elongated shaft + overlapping end caps + mid fills
// so limbs read as one continuous volume while the skeleton animates.
void capsule(
    Mesh3D& dest,
    const Vector3& from,
    const Vector3& to,
    float r0,
    float r1,
    sf::Color color,
    int detail
) {
    Vector3 d = to - from;
    float len = d.magnitude();
    if (len < 0.001f) {
        ball(dest, from, r0, r0, r0, color, detail);
        return;
    }
    Vector3 dir = d * (1.0f / len);
    float yaw = std::atan2(dir.x, dir.z);
    float pitch = std::acos(std::clamp(dir.y, -1.0f, 1.0f));
    float midR = (r0 + r1) * 0.5f;
    Vector3 mid = (from + to) * 0.5f;

    Mesh3D s = Mesh3D::sphere(1.0f, ringsFor(detail), segsFor(detail));
    // Oversized shaft melts into caps — no waist seam while limbs swing.
    Matrix4 shaft =
        Matrix4::translation(mid) *
        Matrix4::rotationY(yaw) *
        Matrix4::rotationX(pitch) *
        Matrix4::scale(Vector3(midR * 1.02f, len * 0.54f + midR * 0.58f, midR * 1.02f));
    appendTransformed(dest, s, shaft, color);
    ball(dest, from, r0 * 1.18f, r0 * 1.18f, r0 * 1.18f, color, detail);
    ball(dest, to, r1 * 1.15f, r1 * 1.15f, r1 * 1.15f, color, detail);

    // Dense mid-fills keep the silhouette fluid at every joint angle.
    int fills = detail >= 2 ? 4 : (detail >= 1 ? 3 : 2);
    for (int i = 1; i <= fills; i++) {
        float u = static_cast<float>(i) / static_cast<float>(fills + 1);
        Vector3 p = from + dir * (len * u);
        float r = lerp(r0, r1, u) * 1.06f;
        ball(dest, p, r, r, r, color, detail);
    }
}

void addCleat(Mesh3D& dest, const Vector3& ankle, int detail) {
    // Foot points +Z (toward plate for lead, still forward for plant).
    ball(dest, ankle + Vector3(0.0f, -0.002f, 0.05f), 0.054f, 0.030f, 0.095f, cleat, detail);
    ball(dest, ankle + Vector3(0.0f, -0.014f, 0.022f), 0.060f, 0.014f, 0.105f, cleatSole, detail);
    ball(dest, ankle + Vector3(0.0f, 0.024f, 0.0f), 0.040f, 0.030f, 0.040f, sock, detail);
    if (detail >= 1) {
        ball(dest, ankle + Vector3(0.0f, -0.008f, 0.10f), 0.028f, 0.016f, 0.030f, cleat, detail);
    }
}

void addMitt(Mesh3D& dest, const Vector3& wrist, const Vector3& forward, float open, int detail) {
    Vector3 f = safeNorm(forward, Vector3(0.0f, 0.0f, 1.0f));
    Vector3 up(0.0f, 1.0f, 0.0f);
    Vector3 side = safeNorm(up.cross(f), Vector3(1.0f, 0.0f, 0.0f));
    Vector3 palm = wrist + f * 0.050f;
    float s = 1.0f + open * 0.16f;

    // Pocket + webbing + heel — one mitt silhouette.
    ball(dest, palm, 0.074f * s, 0.086f * s, 0.050f * s, mitt, detail);
    ball(dest, palm + f * 0.024f + up * 0.034f, 0.060f * s, 0.048f * s, 0.040f * s, mittPad, detail);
    ball(dest, palm + side * -0.040f + up * 0.012f, 0.036f, 0.048f, 0.032f, mittDeep, detail);
    ball(dest, palm + side * 0.038f + up * 0.010f, 0.034f, 0.046f, 0.030f, mittDeep, detail);
    ball(dest, wrist + f * 0.010f, 0.038f, 0.032f, 0.038f, mittDeep, detail);
    if (detail >= 1) {
        ball(dest, palm + f * 0.012f + up * 0.055f * s, 0.050f * s, 0.022f, 0.028f, mittPad, detail);
    }
}

void addCap(Mesh3D& dest, const Vector3& head, float r, int detail) {
    // Crown sits on top of skull only — never cuts the face.
    ball(dest, head + Vector3(0.0f, r * 0.62f, -r * 0.05f), r * 0.88f, r * 0.34f, r * 0.88f, cap, detail);
    ball(dest, head + Vector3(0.0f, r * 0.42f, -r * 0.03f), r * 0.92f, r * 0.14f, r * 0.92f, capDeep, detail);
    // Bill in front of forehead.
    ball(dest, head + Vector3(0.0f, r * 0.36f, r * 0.90f), r * 0.48f, r * 0.055f, r * 0.28f, capDeep, detail);
    if (detail >= 1) {
        ball(dest, head + Vector3(0.0f, r * 0.58f, r * 0.46f), r * 0.07f, r * 0.05f, r * 0.04f, accent, detail);
        // Small brim edge for silhouette.
        ball(dest, head + Vector3(0.0f, r * 0.34f, r * 1.05f), r * 0.36f, r * 0.03f, r * 0.10f, cap, detail);
    }
}

void addHelmet(Mesh3D& dest, const Vector3& head, float r, int detail) {
    // Shell over crown/rear — face stays open for eyes.
    ball(dest, head + Vector3(0.0f, r * 0.42f, -r * 0.12f), r * 1.00f, r * 0.62f, r * 1.00f, gear, detail);
    ball(dest, head + Vector3(0.0f, r * 0.10f, -r * 0.55f), r * 0.78f, r * 0.42f, r * 0.40f, gearDeep, detail);
    // Ear flaps.
    ball(dest, head + Vector3(-r * 0.90f, -r * 0.05f, 0.0f), r * 0.22f, r * 0.34f, r * 0.26f, gearDeep, detail);
    ball(dest, head + Vector3(r * 0.90f, -r * 0.05f, 0.0f), r * 0.22f, r * 0.34f, r * 0.26f, gearDeep, detail);
    // Face mask cage (forward only).
    ball(dest, head + Vector3(0.0f, 0.0f, r * 1.12f), r * 0.66f, r * 0.60f, r * 0.12f, gearDeep, detail);
    ball(dest, head + Vector3(0.0f, -r * 0.66f, r * 0.70f), r * 0.36f, r * 0.16f, r * 0.24f, gearDeep, detail);
    if (detail >= 1) {
        ball(dest, head + Vector3(0.0f, r * 0.14f, r * 1.18f), r * 0.46f, r * 0.028f, r * 0.032f, gearHighlight, detail);
        ball(dest, head + Vector3(0.0f, -r * 0.08f, r * 1.18f), r * 0.46f, r * 0.028f, r * 0.032f, gearHighlight, detail);
        ball(dest, head + Vector3(0.0f, -r * 0.28f, r * 1.14f), r * 0.40f, r * 0.024f, r * 0.028f, gear, detail);
    }
}

// Forward-kinematics arm tuned for Yamamoto set / high-kick hands-together /
// layback (elbow ≈ shoulder height) / high-¾ release / finish.
struct ArmChain {
    Vector3 shoulder;
    Vector3 elbow;
    Vector3 wrist;
    Vector3 palm;
};

// pitch: -0.5 layback high-back · ~0.45 set/chest · ~1.15 release extension
// yaw:   -0.7 cocked closed · ~0.1 set · +0.5 finish across body
// elbow: 0 extended · ~1 fully bent
ArmChain buildArm(
    const Matrix4& upper,
    float side, // +1 right throw arm, -1 left glove
    float shoulderY,
    float pitch,
    float yaw,
    float elbowBend,
    float wristCurl,
    float upperLen,
    float forearmLen
) {
    ArmChain a;
    Vector3 shLocal(0.142f * side, shoulderY - 0.008f, 0.0f);
    a.shoulder = upper.transformPoint(shLocal);

    // Elevation from straight-down (0) toward overhead (~π).
    // pitch 0 ≈ slightly forward hang; higher pitch lifts the arm toward the slot.
    float elev = 0.95f + pitch * 0.95f;
    elev = std::clamp(elev, 0.15f, 2.6f);
    // Azimuth: 0 = toward plate (+Z), + = first-base side for RHP throw arm.
    float azim = side * 0.22f + yaw * 1.05f;

    Vector3 upperDir(
        std::sin(azim) * std::sin(elev),
        -std::cos(elev),
        std::cos(azim) * std::sin(elev)
    );
    // Cocking bias: when pitch is low/negative and yaw closed, pull elbow back
    // so the hand can lay behind the head (classic Yamamoto arm path).
    if (pitch < 0.25f && yaw < -0.15f) {
        float cock = smooth01((-yaw - 0.15f) / 0.55f) * smooth01((0.25f - pitch) / 0.7f);
        upperDir = safeNorm(upperDir + Vector3(side * 0.15f, 0.35f * cock, -0.55f * cock));
    }
    upperDir = safeNorm(upperDir, Vector3(side * 0.25f, -0.4f, 0.5f));

    Vector3 elbowLocal = shLocal + upperDir * upperLen;
    // Keep elbow near shoulder height during layback (optimal leverage).
    if (pitch < 0.2f && yaw < -0.2f) {
        float targetY = shoulderY - 0.02f;
        elbowLocal.y = lerp(elbowLocal.y, targetY, 0.55f);
    }
    a.elbow = upper.transformPoint(elbowLocal);

    // Elbow hinge: bend folds the forearm toward the midline + slightly up (set)
    // or keeps it nearly colinear on release.
    Vector3 upDir = safeNorm(elbowLocal - shLocal, Vector3(0.0f, -1.0f, 0.0f));
    Vector3 hingeAxis = safeNorm(upDir.cross(Vector3(-side * 0.9f, 0.25f, 0.35f)), Vector3(0.0f, 0.0f, 1.0f));
    float bend = std::clamp(elbowBend, -0.2f, 1.25f) * 1.45f;
    float c = std::cos(bend);
    float s = std::sin(bend);
    Vector3 fore =
        upDir * c +
        hingeAxis.cross(upDir) * s +
        hingeAxis * hingeAxis.dot(upDir) * (1.0f - c);

    // Set / hands-together: pull wrist toward sternum.
    if (elbowBend > 0.65f && pitch > 0.2f && pitch < 0.85f) {
        Vector3 toChest = Vector3(-side * 0.08f, shoulderY - 0.18f, 0.12f) - elbowLocal;
        fore = safeNorm(lerp(fore, safeNorm(toChest), 0.55f));
    }
    // Release: forearm drives toward the plate with a slight downward plane.
    if (elbowBend < 0.35f && pitch > 0.7f) {
        fore = safeNorm(fore * 0.35f + Vector3(-side * 0.05f, -0.12f + wristCurl * 0.04f, 0.92f));
    }
    // Layback: hand high-behind (external rotation look).
    if (pitch < 0.15f && yaw < -0.25f) {
        Vector3 layback = Vector3(side * 0.25f, 0.55f, -0.65f);
        fore = safeNorm(lerp(fore, safeNorm(layback), 0.65f));
    }

    fore = safeNorm(fore, Vector3(0.0f, 0.0f, 1.0f));
    Vector3 wristLocal = elbowLocal + fore * forearmLen;
    a.wrist = upper.transformPoint(wristLocal);
    a.palm = upper.transformPoint(
        wristLocal + fore * 0.055f + Vector3(0.0f, wristCurl * 0.012f, 0.0f)
    );
    return a;
}

struct LegChain {
    Vector3 hip;
    Vector3 knee;
    Vector3 ankle;
};

// Lead lift=1 ≈ Yamamoto ~94 cm vertical knee (near chest), foot tucked under.
LegChain buildLeg(
    float side, // +1 plant (right), -1 lead (left) for RHP
    float hipY,
    float lift,       // 0..1
    float kneeBend,   // 0..1
    float stride,     // lead foot forward toward plate
    float hipOpen,
    bool isLead
) {
    LegChain L;
    float open = hipOpen * 0.045f;
    L.hip = Vector3(
        0.095f * side + (isLead ? -open : open),
        hipY,
        isLead ? stride * 0.10f : stride * 0.02f
    );

    // Peak kick: knee ~1.08 m (chest), almost over the plant for balance.
    float kneeY = 0.50f + lift * 0.58f - kneeBend * 0.04f * (1.0f - lift);
    float kneeZ;
    float kneeX = 0.098f * side;
    if (isLead) {
        // Vertical balance: knee stays stacked, barely forward until stride.
        kneeZ = 0.02f + stride * 0.42f + lift * 0.04f;
        // Pull toward midline on the high kick (Yamamoto closed posture).
        kneeX = -0.06f - lift * 0.02f + stride * 0.02f;
    } else {
        kneeZ = stride * 0.02f;
        kneeY = 0.48f - kneeBend * 0.08f; // plant leg loads deeper on drive
    }
    L.knee = Vector3(kneeX, kneeY, kneeZ);

    float ankleY;
    float ankleZ;
    float ankleX = 0.096f * side;
    if (isLead) {
        float tuck = smooth01(lift);
        // High kick: foot tucked under knee. Stride: foot drives to plate.
        ankleY = lerp(0.065f, L.knee.y - 0.14f, tuck);
        ankleZ = lerp(0.06f + stride * 0.88f, L.knee.z + 0.02f, tuck * (1.0f - smooth01(stride * 3.0f)));
        // Once striding hard, plant the foot forward.
        if (stride > 0.08f) {
            float plant = smooth01((stride - 0.08f) / 0.20f);
            ankleY = lerp(ankleY, 0.065f, plant);
            ankleZ = lerp(ankleZ, 0.06f + stride * 0.90f, plant);
            ankleX = lerp(ankleX, -0.10f, plant);
        }
    } else {
        ankleY = 0.065f;
        ankleZ = 0.02f + stride * 0.03f;
        // Slight drag of plant foot as hips open.
        ankleX = 0.10f + hipOpen * 0.02f;
    }
    L.ankle = Vector3(ankleX, ankleY, ankleZ);

    // Soften knee on front-foot land (~15° flex look).
    if (isLead && stride > 0.12f && lift < 0.25f) {
        L.knee.y = std::max(L.ankle.y + 0.28f, L.knee.y - kneeBend * 0.05f);
        L.knee.z = lerp(L.knee.z, (L.hip.z + L.ankle.z) * 0.5f, 0.35f);
    }
    return L;
}

struct PitcherSkel {
    Matrix4 upper;
    LegChain plant;
    LegChain lead;
    ArmChain glove;
    ArmChain throwArm;
    Vector3 head;
    Vector3 torsoC;
    Vector3 pelvis;
    float torsoH = 0.0f;
};

PitcherSkel buildPitcherSkel(const PitcherPose& pose) {
    PitcherSkel s;
    const float hipY = kHipY;
    const float shoulderY = kShoulderY;

    s.plant = buildLeg(+1.0f, hipY, 0.0f, pose.plantKneeBend, pose.stride, pose.hipOpen, false);
    s.lead = buildLeg(-1.0f, hipY, pose.frontLegLift, pose.frontKneeBend, pose.stride, pose.hipOpen, true);

    // Hip open rotates upper slightly; stride shifts COM forward.
    float pelvisZ = pose.stride * 0.08f;
    s.pelvis = Vector3(0.0f, hipY, pelvisZ);

    s.upper =
        Matrix4::translation(Vector3(0.0f, hipY, pelvisZ)) *
        Matrix4::rotationY(pose.torsoTwist + pose.hipOpen * 0.12f) *
        Matrix4::rotationZ(pose.torsoSide) *
        Matrix4::rotationX(pose.torsoLean) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));

    s.torsoC = s.upper.transformPoint(Vector3(0.0f, (hipY + shoulderY) * 0.52f, 0.015f));
    s.torsoH = (shoulderY - hipY) * 0.58f;

    s.glove = buildArm(
        s.upper, -1.0f, shoulderY,
        pose.gloveShoulderPitch, pose.gloveShoulderYaw, pose.gloveElbow, 0.1f,
        kUpperArm * 0.92f, kForearm * 0.90f
    );
    s.throwArm = buildArm(
        s.upper, +1.0f, shoulderY,
        pose.throwShoulderPitch, pose.throwShoulderYaw, pose.throwElbow, pose.throwWrist,
        kUpperArm, kForearm
    );

    s.head = s.upper.transformPoint(Vector3(
        std::sin(pose.headTurn) * 0.016f,
        kHeadY + pose.headNod * 0.012f,
        0.022f + pose.headNod * 0.008f
    ));
    return s;
}

struct PitcherKey {
    float t;
    PitcherPose pose;
};

PitcherPose sampleKeys(const PitcherKey* keys, int count, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t <= keys[0].t) {
        return keys[0].pose;
    }
    if (t >= keys[count - 1].t) {
        return keys[count - 1].pose;
    }
    for (int i = 0; i < count - 1; i++) {
        if (t >= keys[i].t && t <= keys[i + 1].t) {
            float span = keys[i + 1].t - keys[i].t;
            float u = (t - keys[i].t) / span;
            // Long holds (lift/balance) ease smoothly; short snaps (arm fire
            // ~50ms between plant and release) stay nearly linear so the
            // whip reads like Yamamoto's acceleration phase.
            if (span > 0.06f) {
                u = easeInOut(u);
            } else {
                // Slight ease-in: slow start of fire, then crack.
                u = u * u * (3.0f - 2.0f * u * 0.35f);
                u = std::clamp(u, 0.0f, 1.0f);
            }
            return BaseballPlayer3D::blend(keys[i].pose, keys[i + 1].pose, u);
        }
    }
    return keys[count - 1].pose;
}

} // namespace

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

PitcherPose BaseballPlayer3D::pitcherIdlePose(float timeSeconds) {
    PitcherPose p;
    // Yamamoto set: still, upright, weight over back leg, hands chest-high together.
    p.torsoTwist = std::sin(timeSeconds * 0.7f) * 0.02f;
    p.torsoLean = 0.04f + std::sin(timeSeconds * 0.5f) * 0.01f;
    p.torsoSide = std::sin(timeSeconds * 0.35f) * 0.008f;
    p.headTurn = std::sin(timeSeconds * 0.28f) * 0.05f;
    p.headNod = std::sin(timeSeconds * 0.75f) * 0.015f;
    // Hands together at the sternum (ball in throwing hand, glove pocketed).
    p.throwShoulderPitch = 0.48f + std::sin(timeSeconds * 0.55f) * 0.02f;
    p.throwShoulderYaw = 0.08f;
    p.throwElbow = 0.88f;
    p.throwWrist = 0.10f;
    p.gloveShoulderPitch = 0.72f;
    p.gloveShoulderYaw = 0.22f;
    p.gloveElbow = 0.98f;
    p.plantKneeBend = 0.16f;
    p.frontKneeBend = 0.12f;
    p.frontLegLift = 0.02f;
    p.hipOpen = 0.04f;
    p.stride = 0.0f;
    return p;
}

// Frame-accurate Yamamoto windup (FanGraphs slow-mo omgz7L7Sfw0 + biomechanics):
// set → rocker → vertical high kick (hands STILL together) → balance hold →
// hand break → stride/plant with arm layback → high 3/4 release (~0.66) → finish.
PitcherPose BaseballPlayer3D::pitcherDeliveryPose(float t) {
    auto setHands = [](PitcherPose& p) {
        // Compact set / lift: both hands meet at chest.
        p.throwShoulderPitch = 0.48f;
        p.throwShoulderYaw = 0.08f;
        p.throwElbow = 0.88f;
        p.throwWrist = 0.10f;
        p.gloveShoulderPitch = 0.72f;
        p.gloveShoulderYaw = 0.22f;
        p.gloveElbow = 0.98f;
    };

    // ── 0.00 SET ────────────────────────────────────────────────────────
    // Still rubber set. Upright, relaxed, weight on plant (back) leg.
    PitcherPose k0{};
    k0.torsoLean = 0.04f;
    k0.plantKneeBend = 0.16f;
    k0.frontKneeBend = 0.12f;
    k0.frontLegLift = 0.02f;
    k0.hipOpen = 0.04f;
    setHands(k0);

    // ── 0.08 ROCKER ─────────────────────────────────────────────────────
    // Tiny closed coil; front heel peels; hands stay glued.
    PitcherPose k1 = k0;
    k1.torsoTwist = -0.18f;
    k1.torsoLean = 0.05f;
    k1.torsoSide = -0.05f;
    k1.headTurn = 0.03f;
    k1.frontLegLift = 0.22f;
    k1.frontKneeBend = 0.38f;
    k1.plantKneeBend = 0.20f;
    k1.hipOpen = 0.02f;
    k1.throwElbow = 0.92f;
    k1.gloveElbow = 1.00f;

    // ── 0.20 LEG LIFT rising ────────────────────────────────────────────
    // Knee climbing straight up. Hands STILL together at chest. Head locked.
    PitcherPose k2{};
    k2.torsoTwist = -0.32f;
    k2.torsoLean = 0.03f; // stays stacked over back leg — no early lean
    k2.torsoSide = -0.04f;
    k2.headTurn = 0.05f;
    k2.headNod = -0.02f;
    k2.frontLegLift = 0.62f;
    k2.frontKneeBend = 0.78f;
    k2.plantKneeBend = 0.22f;
    k2.hipOpen = 0.05f;
    k2.stride = 0.0f;
    setHands(k2);
    k2.throwShoulderPitch = 0.50f;
    k2.throwElbow = 0.94f;
    k2.gloveElbow = 1.02f;

    // ── 0.34 PEAK KICK ──────────────────────────────────────────────────
    // Vertical balance: lead knee ~chest (94cm), hips ~92° flex, hands together.
    PitcherPose k3{};
    k3.torsoTwist = -0.42f;
    k3.torsoLean = 0.02f; // still over plant leg
    k3.torsoSide = -0.03f;
    k3.headTurn = 0.06f;
    k3.headNod = -0.03f;
    k3.frontLegLift = 1.00f;
    k3.frontKneeBend = 0.92f;
    k3.plantKneeBend = 0.24f;
    k3.hipOpen = 0.08f;
    k3.stride = 0.01f;
    setHands(k3);
    k3.throwShoulderPitch = 0.52f;
    k3.throwShoulderYaw = 0.05f;
    k3.throwElbow = 0.96f;
    k3.gloveShoulderPitch = 0.78f;
    k3.gloveElbow = 1.04f;

    // ── 0.44 BALANCE HOLD ───────────────────────────────────────────────
    // Brief still point — controlled potential energy. Hands still together.
    PitcherPose k4 = k3;
    k4.torsoTwist = -0.45f;
    k4.frontLegLift = 1.00f;
    k4.plantKneeBend = 0.26f;
    k4.headTurn = 0.07f; // eyes locked on mitt

    // ── 0.52 HAND BREAK + START DRIVE ───────────────────────────────────
    // Hands separate: ball hand peels back/up, glove stays front. Lead leg drops.
    PitcherPose k5{};
    k5.torsoTwist = -0.38f;
    k5.torsoLean = 0.06f;
    k5.torsoSide = -0.02f;
    k5.headTurn = 0.05f;
    k5.frontLegLift = 0.55f;
    k5.frontKneeBend = 0.55f;
    k5.plantKneeBend = 0.32f;
    k5.hipOpen = 0.18f;
    k5.stride = 0.08f;
    // Throw arm starts the path back — not yet full layback.
    k5.throwShoulderPitch = 0.05f;
    k5.throwShoulderYaw = -0.35f;
    k5.throwElbow = 1.05f;
    k5.throwWrist = 0.18f;
    // Glove begins to stay front / tuck.
    k5.gloveShoulderPitch = 0.55f;
    k5.gloveShoulderYaw = 0.12f;
    k5.gloveElbow = 0.85f;

    // ── 0.58 STRIDE / EARLY PLANT ───────────────────────────────────────
    // Hips start open; shoulders still closed (separation). Arm loading back.
    PitcherPose k6{};
    k6.torsoTwist = -0.22f; // shoulders lag
    k6.torsoLean = 0.10f;
    k6.headTurn = 0.04f;
    k6.frontLegLift = 0.18f;
    k6.frontKneeBend = 0.35f;
    k6.plantKneeBend = 0.38f;
    k6.hipOpen = 0.42f; // hips already turning
    k6.stride = 0.18f;
    // Full layback: elbow ~ shoulder height, hand behind head.
    k6.throwShoulderPitch = -0.38f;
    k6.throwShoulderYaw = -0.62f;
    k6.throwElbow = 1.12f;
    k6.throwWrist = 0.22f;
    // Glove pulls tight to chest (glove-side connection).
    k6.gloveShoulderPitch = 0.38f;
    k6.gloveShoulderYaw = -0.05f;
    k6.gloveElbow = 0.70f;

    // ── 0.63 FOOT PLANT + MAX EXTERNAL ROTATION ─────────────────────────
    // Front foot lands ~15° flex. Hips open ~45°, shoulders still trailing.
    // Hand still moving BACK relative to plate (negative work / rubber band).
    PitcherPose k7{};
    k7.torsoTwist = -0.05f;
    k7.torsoLean = 0.13f;
    k7.torsoSide = 0.02f;
    k7.headNod = 0.04f;
    k7.frontLegLift = 0.0f;
    k7.frontKneeBend = 0.28f; // soft land ~15°
    k7.plantKneeBend = 0.42f;
    k7.hipOpen = 0.55f;
    k7.stride = 0.28f; // ~85% height scaled into model space
    k7.throwShoulderPitch = -0.15f;
    k7.throwShoulderYaw = -0.48f;
    k7.throwElbow = 0.95f;
    k7.throwWrist = 0.08f;
    k7.gloveShoulderPitch = 0.30f;
    k7.gloveShoulderYaw = -0.10f;
    k7.gloveElbow = 0.58f;

    // ── 0.66 RELEASE ────────────────────────────────────────────────────
    // High 3/4 slot, full extension, glove tucked, front side is a firm wall.
    // Arm path: max ER → ball leave in ~50ms (snappy between k7→k8).
    PitcherPose k8{};
    k8.torsoTwist = 0.42f;
    k8.torsoLean = 0.16f;
    k8.torsoSide = 0.05f;
    k8.headNod = 0.10f;
    k8.frontLegLift = 0.0f;
    k8.frontKneeBend = 0.20f;
    k8.plantKneeBend = 0.40f;
    k8.hipOpen = 0.68f;
    k8.stride = 0.30f;
    k8.throwShoulderPitch = 1.15f;
    k8.throwShoulderYaw = 0.30f;
    k8.throwElbow = 0.08f; // nearly straight
    k8.throwWrist = -0.32f; // snap
    k8.gloveShoulderPitch = 0.28f;
    k8.gloveShoulderYaw = -0.12f;
    k8.gloveElbow = 0.50f; // tucked to chest

    // ── 0.74 FOLLOW-THROUGH ─────────────────────────────────────────────
    // Arm whip across body over front leg; torso fully open.
    PitcherPose k9{};
    k9.torsoTwist = 0.70f;
    k9.torsoLean = 0.20f;
    k9.torsoSide = 0.08f;
    k9.headTurn = -0.12f;
    k9.headNod = 0.08f;
    k9.frontKneeBend = 0.18f;
    k9.plantKneeBend = 0.30f;
    k9.hipOpen = 0.62f;
    k9.stride = 0.28f;
    k9.throwShoulderPitch = 0.72f;
    k9.throwShoulderYaw = 0.55f;
    k9.throwElbow = -0.10f;
    k9.throwWrist = -0.20f;
    k9.gloveShoulderPitch = 0.35f;
    k9.gloveElbow = 0.48f;

    // ── 0.86 DECEL / FIELDING APPROACH ──────────────────────────────────
    PitcherPose k10{};
    k10.torsoTwist = 0.48f;
    k10.torsoLean = 0.12f;
    k10.headTurn = -0.06f;
    k10.frontKneeBend = 0.16f;
    k10.plantKneeBend = 0.22f;
    k10.hipOpen = 0.40f;
    k10.stride = 0.18f;
    k10.throwShoulderPitch = 0.48f;
    k10.throwShoulderYaw = 0.28f;
    k10.throwElbow = 0.22f;
    k10.throwWrist = -0.06f;
    k10.gloveShoulderPitch = 0.50f;
    k10.gloveElbow = 0.65f;

    // ── 1.00 ATHLETIC FINISH ────────────────────────────────────────────
    PitcherPose k11{};
    k11.torsoTwist = 0.28f;
    k11.torsoLean = 0.08f;
    k11.frontKneeBend = 0.14f;
    k11.plantKneeBend = 0.18f;
    k11.hipOpen = 0.28f;
    k11.stride = 0.12f;
    k11.throwShoulderPitch = 0.40f;
    k11.throwShoulderYaw = 0.15f;
    k11.throwElbow = 0.30f;
    k11.gloveShoulderPitch = 0.55f;
    k11.gloveElbow = 0.72f;

    const PitcherKey keys[] = {
        {0.00f, k0},
        {0.08f, k1},
        {0.20f, k2},
        {0.34f, k3},
        {0.44f, k4},
        {0.52f, k5},
        {0.58f, k6},
        {0.63f, k7},
        {0.66f, k8}, // RELEASE
        {0.74f, k9},
        {0.86f, k10},
        {1.00f, k11}
    };
    return sampleKeys(keys, 12, t);
}

CatcherPose BaseballPlayer3D::catcherIdlePose(float timeSeconds) {
    CatcherPose p;
    p.torsoSway = std::sin(timeSeconds * 0.75f) * 0.032f;
    p.torsoLean = 0.12f + std::sin(timeSeconds * 0.55f) * 0.018f;
    p.crouchBob = std::sin(timeSeconds * 1.40f) * 0.014f;
    p.headTurn = std::sin(timeSeconds * 0.30f) * 0.07f;
    p.mittSide = std::sin(timeSeconds * 0.48f) * 0.018f;
    p.mittHeight = std::sin(timeSeconds * 0.92f) * 0.014f;
    p.mittReach = 0.05f;
    p.gloveOpen = 0.30f + std::sin(timeSeconds * 0.68f) * 0.05f;
    p.freeArmBrace = 0.90f;
    return p;
}

CatcherPose BaseballPlayer3D::catcherReceivePose(
    float timeSeconds,
    float aimX,
    float aimY,
    float catcherWorldX,
    float catcherWorldY
) {
    CatcherPose p = catcherIdlePose(timeSeconds);
    float dx = aimX - catcherWorldX;
    float dy = aimY - catcherWorldY;
    p.mittSide = std::clamp(-dx * 0.62f, -0.28f, 0.28f);
    p.mittHeight = std::clamp((dy - 0.14f) * 0.42f, -0.22f, 0.26f);
    p.mittReach = std::clamp(0.05f + std::abs(dx) * 0.10f + std::abs(dy - 0.9f) * 0.04f, 0.04f, 0.20f);
    p.gloveOpen = 0.58f;
    p.torsoSway = std::clamp(-dx * 0.15f, -0.15f, 0.15f);
    p.torsoLean = 0.14f + std::clamp((1.35f - dy) * 0.08f, -0.08f, 0.12f);
    p.headTurn = std::clamp(-dx * 0.28f, -0.36f, 0.36f);
    p.freeArmBrace = 0.55f;
    // Soften idle bob while framing so mitt tracks cleanly.
    p.crouchBob *= 0.35f;
    p.mittSide += std::sin(timeSeconds * 6.0f) * 0.004f; // tiny ready flutter
    return p;
}

Vector3 BaseballPlayer3D::throwHandLocal(const PitcherPose& pose) {
    PitcherSkel s = buildPitcherSkel(pose);
    return s.throwArm.palm;
}

Mesh3D BaseballPlayer3D::pitcher(int detail, const PitcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;
    PitcherSkel s = buildPitcherSkel(pose);

    // Compact athletic frame (Yamamoto-scale proportions) built as continuous
    // fused volumes so the high kick / layback / release stay fluid on camera.

    // ── legs (same-color thigh→shin with big knee melt) ─────────────────
    capsule(mesh, s.plant.hip, s.plant.knee, 0.088f, 0.072f, pants, detail);
    ball(mesh, s.plant.knee, 0.074f, 0.070f, 0.074f, pants, detail);
    capsule(mesh, s.plant.knee, s.plant.ankle, 0.068f, 0.052f, pantsDeep, detail);
    capsule(mesh, s.lead.hip, s.lead.knee, 0.088f, 0.072f, pants, detail);
    ball(mesh, s.lead.knee, 0.074f, 0.070f, 0.074f, pants, detail);
    capsule(mesh, s.lead.knee, s.lead.ankle, 0.068f, 0.052f, pantsDeep, detail);
    addCleat(mesh, s.plant.ankle, detail);
    addCleat(mesh, s.lead.ankle, detail);

    // ── pelvis bridges hips into one mass (no floaty legs) ──────────────
    ball(mesh, s.pelvis, 0.165f, 0.118f, 0.122f, pants, detail);
    ball(mesh, s.pelvis + Vector3(0.0f, 0.045f, 0.0f), 0.148f, 0.028f, 0.112f, belt, detail);
    ball(mesh, s.plant.hip, 0.096f, 0.090f, 0.096f, pants, detail);
    ball(mesh, s.lead.hip, 0.096f, 0.090f, 0.096f, pants, detail);
    // Soft waist fill into jersey.
    ball(mesh, s.pelvis + Vector3(0.0f, 0.08f, 0.01f), 0.135f, 0.055f, 0.100f, jerseyShade, detail);

    // ── torso: stacked fused volumes = one continuous jersey silhouette ─
    ball(mesh, s.torsoC, 0.155f, s.torsoH * 1.05f, 0.118f, jersey, detail);
    Vector3 lowerChest = s.upper.transformPoint(Vector3(0.0f, kHipY + 0.18f, 0.03f));
    Vector3 upperChest = s.upper.transformPoint(Vector3(0.0f, kShoulderY - 0.10f, 0.045f));
    ball(mesh, lowerChest, 0.142f, 0.10f, 0.105f, jersey, detail);
    ball(mesh, upperChest, 0.135f, 0.11f, 0.100f, jersey, detail);
    // Delts fused into torso (same jersey color) so arms never detach.
    ball(mesh, s.throwArm.shoulder, 0.095f, 0.088f, 0.095f, jersey, detail);
    ball(mesh, s.glove.shoulder, 0.095f, 0.088f, 0.095f, jersey, detail);
    ball(mesh, (s.throwArm.shoulder + s.glove.shoulder) * 0.5f + Vector3(0.0f, 0.02f, 0.0f),
         0.12f, 0.06f, 0.08f, jersey, detail);
    // Collar.
    ball(mesh, s.upper.transformPoint(Vector3(0.0f, kShoulderY + 0.018f, 0.014f)),
         0.075f, 0.050f, 0.065f, jerseyDeep, detail);
    // Subtle chest stripe.
    ball(mesh, s.upper.transformPoint(Vector3(0.0f, (kHipY + kShoulderY) * 0.55f, 0.110f)),
         0.018f, 0.050f, 0.010f, accent, detail);

    // ── head (short neck, continuous into cap) ──────────────────────────
    Vector3 neckA = s.upper.transformPoint(Vector3(0.0f, kShoulderY + 0.032f, 0.015f));
    Vector3 neckB = s.head + Vector3(0.0f, -kHeadR * 0.48f, 0.0f);
    capsule(mesh, neckA, neckB, 0.044f, 0.042f, skin, detail);
    ball(mesh, s.head, kHeadR * 1.02f, kHeadR * 1.05f, kHeadR * 0.98f, skin, detail);
    if (detail >= 1) {
        ball(mesh, s.head + Vector3(-kHeadR * 0.80f, -0.01f, 0.0f),
             kHeadR * 0.12f, kHeadR * 0.15f, kHeadR * 0.10f, skinDeep, detail);
        ball(mesh, s.head + Vector3(kHeadR * 0.80f, -0.01f, 0.0f),
             kHeadR * 0.12f, kHeadR * 0.15f, kHeadR * 0.10f, skinDeep, detail);
        ball(mesh, s.head + Vector3(0.0f, -kHeadR * 0.32f, kHeadR * 0.38f),
             kHeadR * 0.36f, kHeadR * 0.30f, kHeadR * 0.32f, skin, detail);
    }
    addCap(mesh, s.head, kHeadR, detail);

    // ── glove arm: jersey sleeve melts into skin forearm ────────────────
    Vector3 gMid = (s.glove.shoulder + s.glove.elbow) * 0.5f;
    capsule(mesh, s.glove.shoulder, s.glove.elbow, 0.060f, 0.050f, jerseyDeep, detail);
    ball(mesh, gMid, 0.055f, 0.055f, 0.055f, jerseyDeep, detail);
    ball(mesh, s.glove.elbow, 0.050f, 0.048f, 0.050f, skin, detail);
    capsule(mesh, s.glove.elbow, s.glove.wrist, 0.048f, 0.040f, skin, detail);
    Vector3 gloveFwd = safeNorm(s.glove.palm - s.glove.elbow, Vector3(0.15f, -0.1f, 0.9f));
    addMitt(mesh, s.glove.wrist, gloveFwd, 0.32f, detail);

    // ── throw arm: continuous path through layback + release ────────────
    Vector3 tMid = (s.throwArm.shoulder + s.throwArm.elbow) * 0.5f;
    capsule(mesh, s.throwArm.shoulder, s.throwArm.elbow, 0.060f, 0.050f, jerseyDeep, detail);
    ball(mesh, tMid, 0.055f, 0.055f, 0.055f, jerseyDeep, detail);
    ball(mesh, s.throwArm.elbow, 0.050f, 0.048f, 0.050f, skin, detail);
    capsule(mesh, s.throwArm.elbow, s.throwArm.wrist, 0.048f, 0.040f, skin, detail);
    // Palm + fingers — ball glues to palm in the demo.
    ball(mesh, s.throwArm.palm, 0.040f, 0.038f, 0.044f, skinDeep, detail);
    if (detail >= 1) {
        Vector3 fingers = s.throwArm.palm + safeNorm(s.throwArm.palm - s.throwArm.wrist) * 0.028f;
        ball(mesh, fingers, 0.030f, 0.024f, 0.032f, skinShadow, detail);
        ball(mesh, s.throwArm.wrist, 0.038f, 0.036f, 0.038f, skin, detail);
    }

    mesh.rebuildNormals();
    return mesh;
}

Mesh3D BaseballPlayer3D::catcher(int detail, const CatcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float bob = pose.crouchBob;
    const float sway = pose.torsoSway;
    const float lean = pose.torsoLean;
    const float hipY = 0.46f + bob;
    const float shoulderY = 1.08f + bob;
    const float headY = 1.30f + bob;
    const float headR = 0.108f;

    // Upper-body lean/sway as a small rigid transform about the hips.
    Matrix4 upper =
        Matrix4::translation(Vector3(sway * 0.02f, hipY, 0.0f)) *
        Matrix4::rotationZ(sway * 0.35f) *
        Matrix4::rotationX(lean * 0.55f) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));

    auto U = [&](const Vector3& p) { return upper.transformPoint(p); };

    // ── crouch legs ─────────────────────────────────────────────────────
    for (int side = -1; side <= 1; side += 2) {
        float s = static_cast<float>(side);
        Vector3 hipJ(0.105f * s + sway * 0.01f, hipY, -0.01f);
        Vector3 knee(0.132f * s, 0.30f + bob * 0.6f, 0.055f);
        Vector3 ankle(0.118f * s, 0.065f + bob * 0.12f, 0.11f);
        capsule(mesh, hipJ, knee, 0.084f, 0.070f, pants, detail);
        capsule(mesh, knee, ankle, 0.064f, 0.050f, pantsDeep, detail);
        // Shin guard
        Vector3 shinMid = (knee + ankle) * 0.5f + Vector3(0.0f, 0.0f, 0.028f);
        ball(mesh, shinMid, 0.066f, 0.095f, 0.050f, gear, detail);
        ball(mesh, shinMid + Vector3(0.0f, 0.02f, 0.01f), 0.058f, 0.055f, 0.042f, gearDeep, detail);
        addCleat(mesh, ankle, detail);
    }

    Vector3 pelvis(sway * 0.02f, hipY, -0.02f);
    ball(mesh, pelvis, 0.160f, 0.112f, 0.120f, pants, detail);
    ball(mesh, pelvis + Vector3(0.0f, 0.040f, 0.0f), 0.146f, 0.026f, 0.110f, belt, detail);

    // ── torso + chest protector (one mass, same gear family) ────────────
    Vector3 torso = U(Vector3(0.0f, (hipY + shoulderY) * 0.52f, 0.02f));
    float torsoH = (shoulderY - hipY) * 0.55f;
    ball(mesh, torso, 0.148f, torsoH, 0.112f, jerseyDeep, detail);
    // Chest protector sits on front of jersey.
    ball(mesh, torso + Vector3(0.0f, 0.01f, 0.042f), 0.155f, torsoH * 0.90f, 0.085f, gear, detail);
    ball(mesh, torso + Vector3(0.0f, -0.02f, 0.055f), 0.120f, torsoH * 0.45f, 0.055f, gearDeep, detail);

    Vector3 shL = U(Vector3(-0.138f, shoulderY, 0.02f));
    Vector3 shR = U(Vector3(0.138f, shoulderY, 0.02f));
    ball(mesh, shL, 0.084f, 0.074f, 0.084f, gearLight, detail);
    ball(mesh, shR, 0.084f, 0.074f, 0.084f, gearLight, detail);
    // Shoulder pads.
    ball(mesh, shL + Vector3(-0.02f, 0.02f, 0.0f), 0.070f, 0.055f, 0.065f, gear, detail);
    ball(mesh, shR + Vector3(0.02f, 0.02f, 0.0f), 0.070f, 0.055f, 0.065f, gear, detail);

    // ── head + helmet ───────────────────────────────────────────────────
    Vector3 head = U(Vector3(pose.headTurn * 0.014f, headY, 0.02f));
    Vector3 neckA = U(Vector3(0.0f, shoulderY + 0.022f, 0.02f));
    Vector3 neckB = head + Vector3(0.0f, -headR * 0.50f, 0.0f);
    capsule(mesh, neckA, neckB, 0.042f, 0.040f, skin, detail);
    ball(mesh, head, headR, headR * 1.04f, headR * 0.96f, skin, detail);
    addHelmet(mesh, head, headR, detail);

    // ── free (right) arm brace ──────────────────────────────────────────
    float brace = pose.freeArmBrace;
    Vector3 freeElbow = U(Vector3(0.24f, shoulderY - 0.14f, 0.05f));
    Vector3 freeWrist = U(Vector3(
        0.26f + brace * 0.02f,
        shoulderY - 0.30f - brace * 0.055f,
        0.06f + brace * 0.035f
    ));
    capsule(mesh, shR, freeElbow, 0.054f, 0.046f, gearDeep, detail);
    capsule(mesh, freeElbow, freeWrist, 0.044f, 0.036f, skin, detail);
    ball(mesh, freeWrist, 0.036f, 0.034f, 0.038f, skinDeep, detail);

    // ── mitt arm tracks target ──────────────────────────────────────────
    Vector3 mittPos = U(Vector3(
        -0.36f + pose.mittSide,
        shoulderY - 0.16f + pose.mittHeight,
        0.24f + pose.mittReach
    ));
    Vector3 gElbow = U(Vector3(
        -0.26f + pose.mittSide * 0.40f,
        shoulderY - 0.12f + pose.mittHeight * 0.30f,
        0.10f + pose.mittReach * 0.24f
    ));
    Vector3 gShoulder = shL + Vector3(pose.mittSide * 0.03f, pose.mittHeight * 0.02f, pose.mittReach * 0.02f);
    capsule(mesh, gShoulder, gElbow, 0.056f, 0.048f, gearDeep, detail);
    capsule(mesh, gElbow, mittPos, 0.046f, 0.038f, skin, detail);
    Vector3 mittFwd = safeNorm(
        Vector3(-0.06f + pose.mittSide * 0.3f, pose.mittHeight * 0.15f, 1.0f),
        Vector3(0.0f, 0.0f, 1.0f)
    );
    addMitt(mesh, mittPos, mittFwd, pose.gloveOpen, detail);

    mesh.rebuildNormals();
    return mesh;
}
