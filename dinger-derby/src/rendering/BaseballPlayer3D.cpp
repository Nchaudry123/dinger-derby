#include "BaseballPlayer3D.h"

#include <algorithm>
#include <cmath>

#include "../math/Matrix4.h"

namespace {

// ── palette (game-character, soft Gouraud-friendly) ─────────────────────
const sf::Color skin(236, 198, 166);
const sf::Color skinDeep(216, 176, 144);
const sf::Color skinShadow(200, 160, 130);
const sf::Color skinLight(244, 210, 182);
const sf::Color jersey(252, 253, 255);
const sf::Color jerseyDeep(230, 234, 242);
const sf::Color jerseyShade(216, 222, 234);
const sf::Color jerseyLine(200, 208, 222);
const sf::Color pants(54, 62, 80);
const sf::Color pantsDeep(44, 50, 66);
const sf::Color pantsShade(38, 44, 58);
const sf::Color pantsLight(68, 76, 94);
const sf::Color belt(40, 42, 50);
const sf::Color beltBuckle(180, 175, 160);
const sf::Color cleat(34, 34, 42);
const sf::Color cleatSole(50, 50, 58);
const sf::Color cleatLace(220, 220, 230);
const sf::Color cap(32, 50, 100);
const sf::Color capDeep(24, 40, 84);
const sf::Color capBill(28, 44, 90);
const sf::Color accent(210, 48, 56);
const sf::Color accentDeep(170, 36, 44);
const sf::Color gear(44, 60, 88);
const sf::Color gearDeep(36, 48, 70);
const sf::Color gearLight(64, 84, 114);
const sf::Color gearHighlight(80, 100, 132);
const sf::Color gearPad(52, 68, 98);
const sf::Color mitt(174, 122, 80);
const sf::Color mittDeep(138, 92, 54);
const sf::Color mittPad(192, 142, 96);
const sf::Color mittLace(120, 80, 48);
const sf::Color sock(250, 250, 255);
const sf::Color hair(42, 32, 28);

// Compact athletic RHP (Yamamoto-scale ~5'10"). Feet y≈0, +Z to plate.
constexpr float kHipY = 0.90f;
constexpr float kShoulderY = 1.40f;
constexpr float kHeadY = 1.62f;
constexpr float kHeadR = 0.110f;
constexpr float kUpperArm = 0.295f;
constexpr float kForearm = 0.265f;

int ringsFor(int detail) {
    // High tessellation for smooth silhouettes under Gouraud shading.
    return detail >= 2 ? 18 : (detail >= 1 ? 13 : 9);
}

int segsFor(int detail) {
    return detail >= 2 ? 28 : (detail >= 1 ? 18 : 12);
}

// Smaller spheres for secondary detail (fingers, laces) — still smooth.
int ringsFine(int detail) {
    return detail >= 2 ? 12 : (detail >= 1 ? 9 : 7);
}

int segsFine(int detail) {
    return detail >= 2 ? 18 : (detail >= 1 ? 12 : 10);
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

void ellipsoid(
    Mesh3D& dest,
    const Vector3& c,
    float rx,
    float ry,
    float rz,
    sf::Color color,
    int detail,
    bool fine = false
) {
    int rings = fine ? ringsFine(detail) : ringsFor(detail);
    int segs = fine ? segsFine(detail) : segsFor(detail);
    Mesh3D s = Mesh3D::sphere(1.0f, rings, segs);
    Matrix4 x =
        Matrix4::translation(c) *
        Matrix4::scale(Vector3(rx, ry, rz));
    appendTransformed(dest, s, x, color);
}

void ball(
    Mesh3D& dest,
    const Vector3& c,
    float r,
    sf::Color color,
    int detail,
    bool fine = false
) {
    ellipsoid(dest, c, r, r, r, color, detail, fine);
}

// Continuous tapered limb: dense overlapping spheres along a bone.
// Reads as one smooth volume at every joint angle (no bead seams).
void limb(
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
        ball(dest, from, r0, color, detail);
        return;
    }
    Vector3 dir = d * (1.0f / len);

    // Segment count scales with length + detail for continuous form.
    int n = detail >= 2 ? 9 : (detail >= 1 ? 7 : 5);
    n = std::max(n, static_cast<int>(len / 0.045f) + 2);
    n = std::min(n, detail >= 2 ? 12 : 9);

    for (int i = 0; i <= n; i++) {
        float u = static_cast<float>(i) / static_cast<float>(n);
        // Slight bulge mid-limb for muscle mass.
        float bulge = 1.0f + 0.08f * std::sin(u * 3.14159265f);
        float r = lerp(r0, r1, u) * bulge;
        // Ends slightly larger so joints melt into neighbors.
        if (i == 0) {
            r = r0 * 1.12f;
        }
        if (i == n) {
            r = r1 * 1.10f;
        }
        Vector3 p = from + dir * (len * u);
        // Mild anisotropic squash along the bone for smoother shafts.
        float along = r * 1.08f;
        float across = r * 0.96f;
        float yaw = std::atan2(dir.x, dir.z);
        float pitch = std::acos(std::clamp(dir.y, -1.0f, 1.0f));
        Mesh3D s = Mesh3D::sphere(1.0f, ringsFor(detail), segsFor(detail));
        Matrix4 x =
            Matrix4::translation(p) *
            Matrix4::rotationY(yaw) *
            Matrix4::rotationX(pitch) *
            Matrix4::scale(Vector3(across, along, across));
        appendTransformed(dest, s, x, color);
    }
}

void addCleat(Mesh3D& dest, const Vector3& ankle, int detail) {
    // Detailed cleat: upper, sole, toe, heel, laces, sock cuff.
    ellipsoid(dest, ankle + Vector3(0.0f, 0.002f, 0.048f), 0.052f, 0.032f, 0.092f, cleat, detail);
    ellipsoid(dest, ankle + Vector3(0.0f, -0.016f, 0.028f), 0.058f, 0.013f, 0.108f, cleatSole, detail);
    ellipsoid(dest, ankle + Vector3(0.0f, -0.010f, 0.100f), 0.030f, 0.016f, 0.032f, cleat, detail, true);
    ellipsoid(dest, ankle + Vector3(0.0f, -0.008f, -0.018f), 0.036f, 0.018f, 0.028f, cleat, detail, true);
    ball(dest, ankle + Vector3(0.0f, 0.028f, 0.0f), 0.042f, sock, detail, true);
    if (detail >= 1) {
        // Laces ridge.
        ellipsoid(dest, ankle + Vector3(0.0f, 0.014f, 0.055f), 0.018f, 0.010f, 0.050f, cleatLace, detail, true);
        // Cleat nubs under sole.
        for (int i = 0; i < 3; i++) {
            float z = 0.01f + i * 0.035f;
            ball(dest, ankle + Vector3(-0.018f, -0.024f, z), 0.008f, cleatSole, detail, true);
            ball(dest, ankle + Vector3(0.018f, -0.024f, z), 0.008f, cleatSole, detail, true);
        }
    }
}

void addHand(
    Mesh3D& dest,
    const Vector3& wrist,
    const Vector3& palmDir,
    float open,
    sf::Color color,
    int detail
) {
    Vector3 f = safeNorm(palmDir, Vector3(0.0f, 0.0f, 1.0f));
    Vector3 up(0.0f, 1.0f, 0.0f);
    Vector3 side = safeNorm(up.cross(f), Vector3(1.0f, 0.0f, 0.0f));
    up = safeNorm(f.cross(side));
    Vector3 palm = wrist + f * 0.042f;
    float s = 1.0f + open * 0.12f;

    ellipsoid(dest, palm, 0.034f * s, 0.022f * s, 0.040f * s, color, detail, true);
    ball(dest, wrist, 0.032f, color, detail, true);

    // Fingers — five short segments for a readable hand mass.
    if (detail >= 1) {
        for (int i = -2; i <= 2; i++) {
            float lat = static_cast<float>(i) * 0.014f;
            Vector3 base = palm + side * lat + f * 0.018f + up * 0.006f;
            Vector3 tip = base + f * (0.028f + open * 0.01f) + up * (0.004f - std::abs(i) * 0.002f);
            ball(dest, base, 0.011f, color, detail, true);
            ball(dest, tip, 0.009f, skinShadow, detail, true);
        }
        // Thumb.
        Vector3 thumb = palm + side * -0.028f + f * 0.008f + up * -0.008f;
        ball(dest, thumb, 0.012f, color, detail, true);
    }
}

void addMitt(Mesh3D& dest, const Vector3& wrist, const Vector3& forward, float open, int detail) {
    Vector3 f = safeNorm(forward, Vector3(0.0f, 0.0f, 1.0f));
    Vector3 up(0.0f, 1.0f, 0.0f);
    Vector3 side = safeNorm(up.cross(f), Vector3(1.0f, 0.0f, 0.0f));
    up = safeNorm(f.cross(side));
    Vector3 palm = wrist + f * 0.052f;
    float s = 1.0f + open * 0.18f;

    // Pocket body.
    ellipsoid(dest, palm, 0.078f * s, 0.090f * s, 0.052f * s, mitt, detail);
    // Webbing / fingers flare.
    ellipsoid(dest, palm + f * 0.028f + up * 0.038f, 0.062f * s, 0.050f * s, 0.042f * s, mittPad, detail);
    // Heel + wrist strap.
    ellipsoid(dest, wrist + f * 0.012f, 0.040f, 0.034f, 0.040f, mittDeep, detail, true);
    // Side lobes (index / pinky walls).
    ellipsoid(dest, palm + side * -0.042f + up * 0.014f, 0.036f, 0.050f, 0.034f, mittDeep, detail, true);
    ellipsoid(dest, palm + side * 0.040f + up * 0.012f, 0.034f, 0.048f, 0.032f, mittDeep, detail, true);
    if (detail >= 1) {
        // Pocket crease + lace.
        ellipsoid(dest, palm + f * 0.010f + up * 0.010f, 0.045f * s, 0.018f, 0.020f, mittLace, detail, true);
        ellipsoid(dest, palm + up * 0.058f * s, 0.052f * s, 0.020f, 0.028f, mittPad, detail, true);
        // Thumb stall.
        ellipsoid(dest, palm + side * -0.050f + f * 0.01f, 0.028f, 0.040f, 0.030f, mitt, detail, true);
    }
}

void addCap(Mesh3D& dest, const Vector3& head, float r, int detail) {
    // Crown only on top of skull — face stays fully visible.
    ellipsoid(dest, head + Vector3(0.0f, r * 0.60f, -r * 0.06f), r * 0.90f, r * 0.36f, r * 0.90f, cap, detail);
    ellipsoid(dest, head + Vector3(0.0f, r * 0.40f, -r * 0.04f), r * 0.94f, r * 0.14f, r * 0.94f, capDeep, detail);
    // Structured bill.
    ellipsoid(dest, head + Vector3(0.0f, r * 0.34f, r * 0.88f), r * 0.50f, r * 0.05f, r * 0.30f, capBill, detail);
    ellipsoid(dest, head + Vector3(0.0f, r * 0.32f, r * 1.05f), r * 0.38f, r * 0.028f, r * 0.12f, cap, detail, true);
    if (detail >= 1) {
        // Button + front logo pip.
        ball(dest, head + Vector3(0.0f, r * 0.92f, -r * 0.02f), r * 0.055f, capDeep, detail, true);
        ellipsoid(dest, head + Vector3(0.0f, r * 0.55f, r * 0.48f), r * 0.075f, r * 0.055f, r * 0.04f, accent, detail, true);
        // Side panels soft break.
        ellipsoid(dest, head + Vector3(-r * 0.55f, r * 0.55f, 0.0f), r * 0.22f, r * 0.20f, r * 0.30f, capDeep, detail, true);
        ellipsoid(dest, head + Vector3(r * 0.55f, r * 0.55f, 0.0f), r * 0.22f, r * 0.20f, r * 0.30f, capDeep, detail, true);
    }
}

void addHelmet(Mesh3D& dest, const Vector3& head, float r, int detail) {
    ellipsoid(dest, head + Vector3(0.0f, r * 0.40f, -r * 0.14f), r * 1.02f, r * 0.64f, r * 1.02f, gear, detail);
    ellipsoid(dest, head + Vector3(0.0f, r * 0.08f, -r * 0.58f), r * 0.80f, r * 0.44f, r * 0.42f, gearDeep, detail);
    // Ear flaps.
    ellipsoid(dest, head + Vector3(-r * 0.92f, -r * 0.06f, 0.0f), r * 0.24f, r * 0.36f, r * 0.28f, gearDeep, detail);
    ellipsoid(dest, head + Vector3(r * 0.92f, -r * 0.06f, 0.0f), r * 0.24f, r * 0.36f, r * 0.28f, gearDeep, detail);
    // Face mask frame + bars.
    ellipsoid(dest, head + Vector3(0.0f, 0.0f, r * 1.14f), r * 0.68f, r * 0.62f, r * 0.12f, gearDeep, detail);
    ellipsoid(dest, head + Vector3(0.0f, -r * 0.68f, r * 0.72f), r * 0.38f, r * 0.16f, r * 0.26f, gearDeep, detail);
    if (detail >= 1) {
        for (int i = 0; i < 4; i++) {
            float y = r * (0.22f - i * 0.16f);
            ellipsoid(
                dest,
                head + Vector3(0.0f, y, r * 1.20f),
                r * 0.48f,
                r * 0.022f,
                r * 0.028f,
                i % 2 == 0 ? gearHighlight : gear,
                detail,
                true
            );
        }
        // Chin strap.
        ellipsoid(dest, head + Vector3(0.0f, -r * 0.85f, r * 0.35f), r * 0.22f, r * 0.05f, r * 0.18f, gearPad, detail, true);
    }
}

void addHead(Mesh3D& dest, const Vector3& head, float r, int detail, bool withCap) {
    ellipsoid(dest, head, r * 1.02f, r * 1.06f, r * 0.98f, skin, detail);
    // Jaw / cheek mass.
    ellipsoid(dest, head + Vector3(0.0f, -r * 0.32f, r * 0.30f), r * 0.38f, r * 0.32f, r * 0.36f, skin, detail, true);
    if (detail >= 1) {
        // Ears.
        ellipsoid(dest, head + Vector3(-r * 0.88f, -0.01f, 0.0f), r * 0.12f, r * 0.16f, r * 0.10f, skinDeep, detail, true);
        ellipsoid(dest, head + Vector3(r * 0.88f, -0.01f, 0.0f), r * 0.12f, r * 0.16f, r * 0.10f, skinDeep, detail, true);
        // Brow + nose hint (readable face from camera distance).
        ellipsoid(dest, head + Vector3(0.0f, r * 0.08f, r * 0.78f), r * 0.22f, r * 0.08f, r * 0.10f, skinDeep, detail, true);
        ball(dest, head + Vector3(0.0f, -r * 0.05f, r * 0.88f), r * 0.08f, skinLight, detail, true);
        // Eyes as dark recesses.
        ball(dest, head + Vector3(-r * 0.28f, r * 0.05f, r * 0.82f), r * 0.055f, hair, detail, true);
        ball(dest, head + Vector3(r * 0.28f, r * 0.05f, r * 0.82f), r * 0.055f, hair, detail, true);
        // Hair fringe under cap / helmet.
        ellipsoid(dest, head + Vector3(0.0f, r * 0.55f, -r * 0.20f), r * 0.55f, r * 0.18f, r * 0.40f, hair, detail, true);
    }
    if (withCap) {
        addCap(dest, head, r, detail);
    }
}

// ── skeleton ────────────────────────────────────────────────────────────

struct ArmChain {
    Vector3 shoulder;
    Vector3 elbow;
    Vector3 wrist;
    Vector3 palm;
};

// pitch: -0.5 layback · ~0.48 set · ~1.15 release
// yaw: -0.7 cocked · ~0.08 set · +0.5 finish
// elbow: 0 extended · ~1 bent
ArmChain buildArm(
    const Matrix4& upper,
    float side,
    float shoulderY,
    float pitch,
    float yaw,
    float elbowBend,
    float wristCurl,
    float upperLen,
    float forearmLen
) {
    ArmChain a;
    Vector3 shLocal(0.148f * side, shoulderY - 0.01f, 0.01f);
    a.shoulder = upper.transformPoint(shLocal);

    float elev = std::clamp(0.95f + pitch * 0.95f, 0.12f, 2.65f);
    float azim = side * 0.20f + yaw * 1.08f;

    Vector3 upperDir(
        std::sin(azim) * std::sin(elev),
        -std::cos(elev),
        std::cos(azim) * std::sin(elev)
    );
    // Layback: pull upper arm back/high so elbow sits near shoulder height.
    if (pitch < 0.25f && yaw < -0.15f) {
        float cock = smooth01((-yaw - 0.15f) / 0.55f) * smooth01((0.25f - pitch) / 0.7f);
        upperDir = safeNorm(upperDir + Vector3(side * 0.18f, 0.42f * cock, -0.62f * cock));
    }
    upperDir = safeNorm(upperDir, Vector3(side * 0.25f, -0.4f, 0.5f));

    Vector3 elbowLocal = shLocal + upperDir * upperLen;
    if (pitch < 0.2f && yaw < -0.2f) {
        elbowLocal.y = lerp(elbowLocal.y, shoulderY - 0.01f, 0.60f);
    }
    a.elbow = upper.transformPoint(elbowLocal);

    Vector3 upDir = safeNorm(elbowLocal - shLocal, Vector3(0.0f, -1.0f, 0.0f));
    Vector3 hinge = safeNorm(upDir.cross(Vector3(-side * 0.9f, 0.28f, 0.32f)), Vector3(0.0f, 0.0f, 1.0f));
    float bend = std::clamp(elbowBend, -0.25f, 1.25f) * 1.48f;
    float c = std::cos(bend);
    float s = std::sin(bend);
    Vector3 fore =
        upDir * c +
        hinge.cross(upDir) * s +
        hinge * hinge.dot(upDir) * (1.0f - c);

    // Hands-together set / lift: wrists meet at sternum.
    if (elbowBend > 0.65f && pitch > 0.2f && pitch < 0.85f) {
        Vector3 toChest = Vector3(-side * 0.06f, shoulderY - 0.20f, 0.14f) - elbowLocal;
        fore = safeNorm(lerp(fore, safeNorm(toChest), 0.62f));
    }
    // Release: drive toward plate, slight downhill plane.
    if (elbowBend < 0.35f && pitch > 0.7f) {
        fore = safeNorm(fore * 0.30f + Vector3(-side * 0.04f, -0.14f + wristCurl * 0.05f, 0.95f));
    }
    // Max external rotation look.
    if (pitch < 0.15f && yaw < -0.25f) {
        fore = safeNorm(lerp(fore, safeNorm(Vector3(side * 0.28f, 0.58f, -0.68f)), 0.70f));
    }

    fore = safeNorm(fore, Vector3(0.0f, 0.0f, 1.0f));
    Vector3 wristLocal = elbowLocal + fore * forearmLen;
    a.wrist = upper.transformPoint(wristLocal);
    a.palm = upper.transformPoint(
        wristLocal + fore * 0.058f + Vector3(0.0f, wristCurl * 0.012f, 0.0f)
    );
    return a;
}

struct LegChain {
    Vector3 hip;
    Vector3 knee;
    Vector3 ankle;
};

// Lead lift=1 → Yamamoto ~94cm vertical knee near chest; foot tucked.
LegChain buildLeg(
    float side,
    float hipY,
    float lift,
    float kneeBend,
    float stride,
    float hipOpen,
    bool isLead
) {
    LegChain L;
    float open = hipOpen * 0.048f;
    L.hip = Vector3(
        0.098f * side + (isLead ? -open : open),
        hipY,
        isLead ? stride * 0.10f : stride * 0.02f
    );

    float kneeY;
    float kneeZ;
    float kneeX = 0.100f * side;
    if (isLead) {
        kneeY = 0.50f + lift * 0.60f - kneeBend * 0.03f * (1.0f - lift);
        kneeZ = 0.02f + stride * 0.44f + lift * 0.03f;
        kneeX = -0.055f - lift * 0.025f + stride * 0.02f; // midline on kick
    } else {
        kneeY = 0.48f - kneeBend * 0.09f;
        kneeZ = stride * 0.02f;
    }
    L.knee = Vector3(kneeX, kneeY, kneeZ);

    float ankleY;
    float ankleZ;
    float ankleX = 0.098f * side;
    if (isLead) {
        float tuck = smooth01(lift);
        ankleY = lerp(0.062f, L.knee.y - 0.145f, tuck);
        ankleZ = lerp(0.06f + stride * 0.90f, L.knee.z + 0.02f, tuck * (1.0f - smooth01(stride * 3.0f)));
        if (stride > 0.08f) {
            float plant = smooth01((stride - 0.08f) / 0.20f);
            ankleY = lerp(ankleY, 0.062f, plant);
            ankleZ = lerp(ankleZ, 0.06f + stride * 0.92f, plant);
            ankleX = lerp(ankleX, -0.105f, plant);
        }
    } else {
        ankleY = 0.062f;
        ankleZ = 0.02f + stride * 0.03f;
        ankleX = 0.102f + hipOpen * 0.02f;
    }
    L.ankle = Vector3(ankleX, ankleY, ankleZ);

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
    s.plant = buildLeg(+1.0f, kHipY, 0.0f, pose.plantKneeBend, pose.stride, pose.hipOpen, false);
    s.lead = buildLeg(-1.0f, kHipY, pose.frontLegLift, pose.frontKneeBend, pose.stride, pose.hipOpen, true);

    float pelvisZ = pose.stride * 0.085f;
    s.pelvis = Vector3(0.0f, kHipY, pelvisZ);

    s.upper =
        Matrix4::translation(Vector3(0.0f, kHipY, pelvisZ)) *
        Matrix4::rotationY(pose.torsoTwist + pose.hipOpen * 0.14f) *
        Matrix4::rotationZ(pose.torsoSide) *
        Matrix4::rotationX(pose.torsoLean) *
        Matrix4::translation(Vector3(0.0f, -kHipY, 0.0f));

    s.torsoC = s.upper.transformPoint(Vector3(0.0f, (kHipY + kShoulderY) * 0.52f, 0.018f));
    s.torsoH = (kShoulderY - kHipY) * 0.58f;

    s.glove = buildArm(
        s.upper, -1.0f, kShoulderY,
        pose.gloveShoulderPitch, pose.gloveShoulderYaw, pose.gloveElbow, 0.1f,
        kUpperArm * 0.93f, kForearm * 0.91f
    );
    s.throwArm = buildArm(
        s.upper, +1.0f, kShoulderY,
        pose.throwShoulderPitch, pose.throwShoulderYaw, pose.throwElbow, pose.throwWrist,
        kUpperArm, kForearm
    );

    s.head = s.upper.transformPoint(Vector3(
        std::sin(pose.headTurn) * 0.016f,
        kHeadY + pose.headNod * 0.012f,
        0.024f + pose.headNod * 0.008f
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
            if (span > 0.06f) {
                u = easeInOut(u);
            } else {
                // Snappy arm fire on short spans (~plant→release).
                u = smooth01(u);
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
    // Yamamoto set stillness with tiny breathing.
    p.torsoTwist = std::sin(timeSeconds * 0.65f) * 0.018f;
    p.torsoLean = 0.035f + std::sin(timeSeconds * 0.48f) * 0.008f;
    p.torsoSide = std::sin(timeSeconds * 0.32f) * 0.006f;
    p.headTurn = std::sin(timeSeconds * 0.26f) * 0.045f;
    p.headNod = std::sin(timeSeconds * 0.70f) * 0.012f;
    p.throwShoulderPitch = 0.48f + std::sin(timeSeconds * 0.50f) * 0.015f;
    p.throwShoulderYaw = 0.08f;
    p.throwElbow = 0.90f;
    p.throwWrist = 0.10f;
    p.gloveShoulderPitch = 0.72f;
    p.gloveShoulderYaw = 0.22f;
    p.gloveElbow = 0.99f;
    p.plantKneeBend = 0.15f;
    p.frontKneeBend = 0.12f;
    p.frontLegLift = 0.02f;
    p.hipOpen = 0.04f;
    p.stride = 0.0f;
    return p;
}

// Yamamoto windup from FanGraphs slow-mo (omgz7L7Sfw0) + biomechanics:
// set → rocker → vertical high kick with hands STILL together → balance hold →
// hand break → stride + arm layback → high 3/4 release @ 0.66 → finish.
PitcherPose BaseballPlayer3D::pitcherDeliveryPose(float t) {
    auto setHands = [](PitcherPose& p) {
        p.throwShoulderPitch = 0.48f;
        p.throwShoulderYaw = 0.08f;
        p.throwElbow = 0.90f;
        p.throwWrist = 0.10f;
        p.gloveShoulderPitch = 0.72f;
        p.gloveShoulderYaw = 0.22f;
        p.gloveElbow = 0.99f;
    };

    // 0.00 SET — still, upright, weight on plant leg, hands at sternum.
    PitcherPose k0{};
    k0.torsoLean = 0.035f;
    k0.plantKneeBend = 0.15f;
    k0.frontKneeBend = 0.12f;
    k0.frontLegLift = 0.02f;
    k0.hipOpen = 0.04f;
    setHands(k0);

    // 0.08 ROCKER — slight closed coil, front heel peels.
    PitcherPose k1 = k0;
    k1.torsoTwist = -0.18f;
    k1.torsoLean = 0.045f;
    k1.torsoSide = -0.05f;
    k1.headTurn = 0.03f;
    k1.frontLegLift = 0.24f;
    k1.frontKneeBend = 0.40f;
    k1.plantKneeBend = 0.20f;
    k1.hipOpen = 0.02f;
    k1.throwElbow = 0.93f;
    k1.gloveElbow = 1.01f;

    // 0.20 LEG LIFT rising — knee climbs; hands still together; head locked.
    PitcherPose k2{};
    k2.torsoTwist = -0.34f;
    k2.torsoLean = 0.025f;
    k2.torsoSide = -0.04f;
    k2.headTurn = 0.05f;
    k2.headNod = -0.02f;
    k2.frontLegLift = 0.65f;
    k2.frontKneeBend = 0.80f;
    k2.plantKneeBend = 0.22f;
    k2.hipOpen = 0.05f;
    setHands(k2);
    k2.throwShoulderPitch = 0.50f;
    k2.throwElbow = 0.95f;
    k2.gloveElbow = 1.03f;

    // 0.34 PEAK KICK — vertical knee ~chest, hips flexed, hands together.
    PitcherPose k3{};
    k3.torsoTwist = -0.44f;
    k3.torsoLean = 0.015f;
    k3.torsoSide = -0.03f;
    k3.headTurn = 0.06f;
    k3.headNod = -0.03f;
    k3.frontLegLift = 1.00f;
    k3.frontKneeBend = 0.93f;
    k3.plantKneeBend = 0.24f;
    k3.hipOpen = 0.08f;
    k3.stride = 0.01f;
    setHands(k3);
    k3.throwShoulderPitch = 0.52f;
    k3.throwElbow = 0.97f;
    k3.gloveShoulderPitch = 0.78f;
    k3.gloveElbow = 1.05f;

    // 0.44 BALANCE HOLD — controlled still point over plant leg.
    PitcherPose k4 = k3;
    k4.torsoTwist = -0.46f;
    k4.frontLegLift = 1.00f;
    k4.plantKneeBend = 0.26f;
    k4.headTurn = 0.07f;

    // 0.52 HAND BREAK — ball peels back/up; glove stays front; leg drops.
    PitcherPose k5{};
    k5.torsoTwist = -0.38f;
    k5.torsoLean = 0.055f;
    k5.torsoSide = -0.02f;
    k5.headTurn = 0.05f;
    k5.frontLegLift = 0.55f;
    k5.frontKneeBend = 0.55f;
    k5.plantKneeBend = 0.33f;
    k5.hipOpen = 0.18f;
    k5.stride = 0.08f;
    k5.throwShoulderPitch = 0.05f;
    k5.throwShoulderYaw = -0.38f;
    k5.throwElbow = 1.06f;
    k5.throwWrist = 0.18f;
    k5.gloveShoulderPitch = 0.55f;
    k5.gloveShoulderYaw = 0.12f;
    k5.gloveElbow = 0.85f;

    // 0.58 STRIDE — hips open, shoulders lag; full arm layback loading.
    PitcherPose k6{};
    k6.torsoTwist = -0.24f;
    k6.torsoLean = 0.10f;
    k6.headTurn = 0.04f;
    k6.frontLegLift = 0.16f;
    k6.frontKneeBend = 0.34f;
    k6.plantKneeBend = 0.38f;
    k6.hipOpen = 0.44f;
    k6.stride = 0.19f;
    k6.throwShoulderPitch = -0.40f;
    k6.throwShoulderYaw = -0.64f;
    k6.throwElbow = 1.14f;
    k6.throwWrist = 0.22f;
    k6.gloveShoulderPitch = 0.36f;
    k6.gloveShoulderYaw = -0.06f;
    k6.gloveElbow = 0.68f;

    // 0.63 FOOT PLANT + max ER — front knee ~15°, hand still back.
    PitcherPose k7{};
    k7.torsoTwist = -0.04f;
    k7.torsoLean = 0.13f;
    k7.torsoSide = 0.02f;
    k7.headNod = 0.04f;
    k7.frontLegLift = 0.0f;
    k7.frontKneeBend = 0.27f;
    k7.plantKneeBend = 0.42f;
    k7.hipOpen = 0.56f;
    k7.stride = 0.29f;
    k7.throwShoulderPitch = -0.12f;
    k7.throwShoulderYaw = -0.48f;
    k7.throwElbow = 0.92f;
    k7.throwWrist = 0.06f;
    k7.gloveShoulderPitch = 0.28f;
    k7.gloveShoulderYaw = -0.12f;
    k7.gloveElbow = 0.55f;

    // 0.66 RELEASE — high 3/4, full extension, glove tucked, front wall firm.
    PitcherPose k8{};
    k8.torsoTwist = 0.44f;
    k8.torsoLean = 0.16f;
    k8.torsoSide = 0.05f;
    k8.headNod = 0.10f;
    k8.frontLegLift = 0.0f;
    k8.frontKneeBend = 0.18f;
    k8.plantKneeBend = 0.40f;
    k8.hipOpen = 0.70f;
    k8.stride = 0.31f;
    k8.throwShoulderPitch = 1.18f;
    k8.throwShoulderYaw = 0.32f;
    k8.throwElbow = 0.06f;
    k8.throwWrist = -0.34f;
    k8.gloveShoulderPitch = 0.26f;
    k8.gloveShoulderYaw = -0.14f;
    k8.gloveElbow = 0.48f;

    // 0.74 FOLLOW-THROUGH — arm whip across body.
    PitcherPose k9{};
    k9.torsoTwist = 0.72f;
    k9.torsoLean = 0.20f;
    k9.torsoSide = 0.08f;
    k9.headTurn = -0.12f;
    k9.headNod = 0.08f;
    k9.frontKneeBend = 0.16f;
    k9.plantKneeBend = 0.30f;
    k9.hipOpen = 0.64f;
    k9.stride = 0.29f;
    k9.throwShoulderPitch = 0.70f;
    k9.throwShoulderYaw = 0.58f;
    k9.throwElbow = -0.12f;
    k9.throwWrist = -0.20f;
    k9.gloveShoulderPitch = 0.34f;
    k9.gloveElbow = 0.46f;

    // 0.86 DECEL / fielding approach.
    PitcherPose k10{};
    k10.torsoTwist = 0.48f;
    k10.torsoLean = 0.12f;
    k10.headTurn = -0.06f;
    k10.frontKneeBend = 0.15f;
    k10.plantKneeBend = 0.22f;
    k10.hipOpen = 0.40f;
    k10.stride = 0.18f;
    k10.throwShoulderPitch = 0.46f;
    k10.throwShoulderYaw = 0.28f;
    k10.throwElbow = 0.22f;
    k10.throwWrist = -0.06f;
    k10.gloveShoulderPitch = 0.50f;
    k10.gloveElbow = 0.64f;

    // 1.00 athletic finish.
    PitcherPose k11{};
    k11.torsoTwist = 0.28f;
    k11.torsoLean = 0.08f;
    k11.frontKneeBend = 0.14f;
    k11.plantKneeBend = 0.18f;
    k11.hipOpen = 0.28f;
    k11.stride = 0.12f;
    k11.throwShoulderPitch = 0.40f;
    k11.throwShoulderYaw = 0.14f;
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
        {0.66f, k8},
        {0.74f, k9},
        {0.86f, k10},
        {1.00f, k11}
    };
    return sampleKeys(keys, 12, t);
}

CatcherPose BaseballPlayer3D::catcherIdlePose(float timeSeconds) {
    CatcherPose p;
    p.torsoSway = std::sin(timeSeconds * 0.72f) * 0.030f;
    p.torsoLean = 0.12f + std::sin(timeSeconds * 0.52f) * 0.016f;
    p.crouchBob = std::sin(timeSeconds * 1.35f) * 0.013f;
    p.headTurn = std::sin(timeSeconds * 0.28f) * 0.06f;
    p.mittSide = std::sin(timeSeconds * 0.45f) * 0.016f;
    p.mittHeight = std::sin(timeSeconds * 0.88f) * 0.012f;
    p.mittReach = 0.05f;
    p.gloveOpen = 0.30f + std::sin(timeSeconds * 0.65f) * 0.045f;
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
    p.gloveOpen = 0.60f;
    p.torsoSway = std::clamp(-dx * 0.15f, -0.15f, 0.15f);
    p.torsoLean = 0.14f + std::clamp((1.35f - dy) * 0.08f, -0.08f, 0.12f);
    p.headTurn = std::clamp(-dx * 0.28f, -0.36f, 0.36f);
    p.freeArmBrace = 0.55f;
    p.crouchBob *= 0.30f;
    p.mittSide += std::sin(timeSeconds * 6.0f) * 0.0035f;
    return p;
}

Vector3 BaseballPlayer3D::throwHandLocal(const PitcherPose& pose) {
    return buildPitcherSkel(pose).throwArm.palm;
}

Mesh3D BaseballPlayer3D::pitcher(int detail, const PitcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;
    PitcherSkel s = buildPitcherSkel(pose);

    // ── legs ────────────────────────────────────────────────────────────
    limb(mesh, s.plant.hip, s.plant.knee, 0.090f, 0.074f, pants, detail);
    ball(mesh, s.plant.knee, 0.076f, pantsLight, detail);
    limb(mesh, s.plant.knee, s.plant.ankle, 0.070f, 0.052f, pantsDeep, detail);
    // Sock band above cleat.
    ellipsoid(mesh, s.plant.ankle + Vector3(0.0f, 0.055f, 0.0f), 0.048f, 0.028f, 0.048f, sock, detail, true);

    limb(mesh, s.lead.hip, s.lead.knee, 0.090f, 0.074f, pants, detail);
    ball(mesh, s.lead.knee, 0.076f, pantsLight, detail);
    limb(mesh, s.lead.knee, s.lead.ankle, 0.070f, 0.052f, pantsDeep, detail);
    ellipsoid(mesh, s.lead.ankle + Vector3(0.0f, 0.055f, 0.0f), 0.048f, 0.028f, 0.048f, sock, detail, true);

    addCleat(mesh, s.plant.ankle, detail);
    addCleat(mesh, s.lead.ankle, detail);

    // ── pelvis / belt ───────────────────────────────────────────────────
    ellipsoid(mesh, s.pelvis, 0.168f, 0.120f, 0.125f, pants, detail);
    ellipsoid(mesh, s.pelvis + Vector3(0.0f, 0.048f, 0.0f), 0.150f, 0.028f, 0.115f, belt, detail);
    ball(mesh, s.plant.hip, 0.098f, pants, detail);
    ball(mesh, s.lead.hip, 0.098f, pants, detail);
    if (detail >= 1) {
        // Belt buckle.
        ellipsoid(
            mesh,
            s.pelvis + Vector3(0.0f, 0.050f, 0.100f),
            0.028f, 0.018f, 0.012f,
            beltBuckle, detail, true
        );
    }
    // Soft waist into jersey.
    ellipsoid(mesh, s.pelvis + Vector3(0.0f, 0.085f, 0.01f), 0.140f, 0.055f, 0.105f, jerseyShade, detail);

    // ── torso: multi-layer continuous jersey ────────────────────────────
    ellipsoid(mesh, s.torsoC, 0.158f, s.torsoH * 1.08f, 0.120f, jersey, detail);
    Vector3 belly = s.upper.transformPoint(Vector3(0.0f, kHipY + 0.16f, 0.035f));
    Vector3 chest = s.upper.transformPoint(Vector3(0.0f, kShoulderY - 0.12f, 0.050f));
    Vector3 pecs = s.upper.transformPoint(Vector3(0.0f, kShoulderY - 0.06f, 0.055f));
    ellipsoid(mesh, belly, 0.145f, 0.105f, 0.108f, jersey, detail);
    ellipsoid(mesh, chest, 0.138f, 0.115f, 0.102f, jersey, detail);
    ellipsoid(mesh, pecs, 0.130f, 0.080f, 0.095f, jerseyDeep, detail);

    // Fused delts + collarbone bridge (arms never detach).
    ball(mesh, s.throwArm.shoulder, 0.098f, jersey, detail);
    ball(mesh, s.glove.shoulder, 0.098f, jersey, detail);
    Vector3 collar = (s.throwArm.shoulder + s.glove.shoulder) * 0.5f + Vector3(0.0f, 0.025f, 0.01f);
    ellipsoid(mesh, collar, 0.125f, 0.055f, 0.078f, jersey, detail);
    ellipsoid(
        mesh,
        s.upper.transformPoint(Vector3(0.0f, kShoulderY + 0.02f, 0.015f)),
        0.078f, 0.052f, 0.068f,
        jerseyDeep, detail
    );

    // Chest number stripe + sleeve band hints.
    if (detail >= 1) {
        ellipsoid(
            mesh,
            s.upper.transformPoint(Vector3(0.0f, (kHipY + kShoulderY) * 0.55f, 0.115f)),
            0.018f, 0.055f, 0.010f,
            accent, detail, true
        );
        ellipsoid(
            mesh,
            s.upper.transformPoint(Vector3(0.0f, (kHipY + kShoulderY) * 0.48f, 0.112f)),
            0.040f, 0.018f, 0.010f,
            accentDeep, detail, true
        );
        // Side seams.
        ellipsoid(
            mesh,
            s.upper.transformPoint(Vector3(-0.12f, (kHipY + kShoulderY) * 0.5f, 0.0f)),
            0.012f, 0.12f, 0.04f,
            jerseyLine, detail, true
        );
        ellipsoid(
            mesh,
            s.upper.transformPoint(Vector3(0.12f, (kHipY + kShoulderY) * 0.5f, 0.0f)),
            0.012f, 0.12f, 0.04f,
            jerseyLine, detail, true
        );
    }

    // ── neck + head ─────────────────────────────────────────────────────
    Vector3 neckA = s.upper.transformPoint(Vector3(0.0f, kShoulderY + 0.035f, 0.016f));
    Vector3 neckB = s.head + Vector3(0.0f, -kHeadR * 0.48f, 0.0f);
    limb(mesh, neckA, neckB, 0.046f, 0.043f, skin, detail);
    addHead(mesh, s.head, kHeadR, detail, true);

    // ── glove arm (fuller continuous volumes, soft elbow) ───────────────
    limb(mesh, s.glove.shoulder, s.glove.elbow, 0.072f, 0.056f, jerseyDeep, detail);
    ball(mesh, s.glove.elbow, 0.058f, skin, detail);
    ball(mesh, lerp(s.glove.shoulder, s.glove.elbow, 0.42f), 0.060f, jerseyDeep, detail);
    ellipsoid(mesh, lerp(s.glove.shoulder, s.glove.elbow, 0.72f), 0.060f, 0.058f, 0.060f, jerseyShade, detail, true);
    limb(mesh, s.glove.elbow, s.glove.wrist, 0.054f, 0.038f, skin, detail);
    ball(mesh, lerp(s.glove.elbow, s.glove.wrist, 0.45f), 0.046f, skin, detail);
    Vector3 gloveFwd = safeNorm(s.glove.palm - s.glove.elbow, Vector3(0.15f, -0.1f, 0.9f));
    addMitt(mesh, s.glove.wrist, gloveFwd, 0.32f, detail);

    // ── throw arm (muscle taper + fleshy joints for fluid bends) ─────────
    limb(mesh, s.throwArm.shoulder, s.throwArm.elbow, 0.072f, 0.056f, jerseyDeep, detail);
    ball(mesh, s.throwArm.elbow, 0.058f, skin, detail);
    ball(mesh, lerp(s.throwArm.shoulder, s.throwArm.elbow, 0.40f), 0.062f, jerseyDeep, detail);
    ellipsoid(mesh, lerp(s.throwArm.shoulder, s.throwArm.elbow, 0.72f), 0.060f, 0.058f, 0.060f, jerseyShade, detail, true);
    limb(mesh, s.throwArm.elbow, s.throwArm.wrist, 0.054f, 0.038f, skin, detail);
    ball(mesh, lerp(s.throwArm.elbow, s.throwArm.wrist, 0.45f), 0.046f, skin, detail);
    ball(mesh, s.throwArm.wrist, 0.036f, skinDeep, detail);
    Vector3 handDir = safeNorm(s.throwArm.palm - s.throwArm.wrist, Vector3(0.0f, 0.0f, 1.0f));
    addHand(mesh, s.throwArm.wrist, handDir, 0.20f, skinDeep, detail);

    mesh.rebuildNormals();
    return mesh;
}

Mesh3D BaseballPlayer3D::catcher(int detail, const CatcherPose& pose) {
    detail = std::clamp(detail, 0, 2);
    Mesh3D mesh;

    const float bob = pose.crouchBob;
    const float sway = pose.torsoSway;
    const float lean = pose.torsoLean;
    const float hipY = 0.45f + bob;
    const float shoulderY = 1.06f + bob;
    const float headY = 1.28f + bob;
    const float headR = 0.108f;

    Matrix4 upper =
        Matrix4::translation(Vector3(sway * 0.02f, hipY, 0.0f)) *
        Matrix4::rotationZ(sway * 0.35f) *
        Matrix4::rotationX(lean * 0.55f) *
        Matrix4::translation(Vector3(0.0f, -hipY, 0.0f));
    auto U = [&](const Vector3& p) { return upper.transformPoint(p); };

    // ── crouch legs + shin guards ───────────────────────────────────────
    for (int side = -1; side <= 1; side += 2) {
        float sd = static_cast<float>(side);
        Vector3 hipJ(0.108f * sd + sway * 0.01f, hipY, -0.01f);
        Vector3 knee(0.135f * sd, 0.295f + bob * 0.55f, 0.055f);
        Vector3 ankle(0.120f * sd, 0.062f + bob * 0.12f, 0.115f);
        limb(mesh, hipJ, knee, 0.088f, 0.074f, pants, detail);
        ball(mesh, knee, 0.076f, pantsLight, detail);
        limb(mesh, knee, ankle, 0.068f, 0.052f, pantsDeep, detail);

        // Two-plate shin guard.
        Vector3 shinMid = (knee + ankle) * 0.5f + Vector3(0.0f, 0.0f, 0.030f);
        ellipsoid(mesh, shinMid + Vector3(0.0f, 0.03f, 0.0f), 0.068f, 0.070f, 0.052f, gear, detail);
        ellipsoid(mesh, shinMid + Vector3(0.0f, -0.04f, 0.01f), 0.060f, 0.055f, 0.048f, gearDeep, detail);
        if (detail >= 1) {
            ellipsoid(mesh, shinMid + Vector3(0.0f, 0.0f, 0.04f), 0.050f, 0.08f, 0.018f, gearPad, detail, true);
            // Knee cap plate.
            ball(mesh, knee + Vector3(0.0f, 0.0f, 0.04f), 0.048f, gearLight, detail, true);
        }
        addCleat(mesh, ankle, detail);
    }

    Vector3 pelvis(sway * 0.02f, hipY, -0.02f);
    ellipsoid(mesh, pelvis, 0.165f, 0.115f, 0.122f, pants, detail);
    ellipsoid(mesh, pelvis + Vector3(0.0f, 0.042f, 0.0f), 0.150f, 0.026f, 0.112f, belt, detail);

    // ── torso + segmented chest protector ───────────────────────────────
    Vector3 torso = U(Vector3(0.0f, (hipY + shoulderY) * 0.52f, 0.02f));
    float torsoH = (shoulderY - hipY) * 0.55f;
    ellipsoid(mesh, torso, 0.150f, torsoH, 0.115f, jerseyDeep, detail);
    // Chest protector main + upper + lower plates.
    ellipsoid(mesh, torso + Vector3(0.0f, 0.02f, 0.048f), 0.158f, torsoH * 0.85f, 0.088f, gear, detail);
    ellipsoid(mesh, torso + Vector3(0.0f, 0.08f, 0.055f), 0.130f, torsoH * 0.35f, 0.060f, gearPad, detail);
    ellipsoid(mesh, torso + Vector3(0.0f, -0.06f, 0.050f), 0.125f, torsoH * 0.32f, 0.055f, gearDeep, detail);
    if (detail >= 1) {
        // Strap across shoulders.
        ellipsoid(mesh, torso + Vector3(0.0f, torsoH * 0.55f, 0.02f), 0.14f, 0.025f, 0.05f, gearHighlight, detail, true);
    }

    Vector3 shL = U(Vector3(-0.140f, shoulderY, 0.02f));
    Vector3 shR = U(Vector3(0.140f, shoulderY, 0.02f));
    ball(mesh, shL, 0.090f, gearLight, detail);
    ball(mesh, shR, 0.090f, gearLight, detail);
    // Shoulder caps.
    ellipsoid(mesh, shL + Vector3(-0.025f, 0.02f, 0.0f), 0.072f, 0.055f, 0.068f, gear, detail);
    ellipsoid(mesh, shR + Vector3(0.025f, 0.02f, 0.0f), 0.072f, 0.055f, 0.068f, gear, detail);

    // ── head + helmet ───────────────────────────────────────────────────
    Vector3 head = U(Vector3(pose.headTurn * 0.014f, headY, 0.02f));
    Vector3 neckA = U(Vector3(0.0f, shoulderY + 0.024f, 0.02f));
    Vector3 neckB = head + Vector3(0.0f, -headR * 0.48f, 0.0f);
    limb(mesh, neckA, neckB, 0.044f, 0.042f, skin, detail);
    addHead(mesh, head, headR, detail, false);
    addHelmet(mesh, head, headR, detail);

    // ── free (right) arm ────────────────────────────────────────────────
    float brace = pose.freeArmBrace;
    Vector3 freeElbow = U(Vector3(0.245f, shoulderY - 0.14f, 0.05f));
    Vector3 freeWrist = U(Vector3(
        0.265f + brace * 0.02f,
        shoulderY - 0.30f - brace * 0.055f,
        0.06f + brace * 0.035f
    ));
    limb(mesh, shR, freeElbow, 0.056f, 0.048f, gearDeep, detail);
    ball(mesh, freeElbow, 0.048f, skin, detail);
    limb(mesh, freeElbow, freeWrist, 0.046f, 0.038f, skin, detail);
    addHand(mesh, freeWrist, Vector3(0.1f, -0.3f, 0.5f), 0.2f, skinDeep, detail);

    // ── mitt arm tracks target ──────────────────────────────────────────
    Vector3 mittPos = U(Vector3(
        -0.36f + pose.mittSide,
        shoulderY - 0.16f + pose.mittHeight,
        0.25f + pose.mittReach
    ));
    Vector3 gElbow = U(Vector3(
        -0.26f + pose.mittSide * 0.40f,
        shoulderY - 0.12f + pose.mittHeight * 0.30f,
        0.10f + pose.mittReach * 0.24f
    ));
    Vector3 gShoulder = shL + Vector3(pose.mittSide * 0.03f, pose.mittHeight * 0.02f, pose.mittReach * 0.02f);
    limb(mesh, gShoulder, gElbow, 0.058f, 0.050f, gearDeep, detail);
    ball(mesh, gElbow, 0.050f, skin, detail);
    limb(mesh, gElbow, mittPos, 0.048f, 0.040f, skin, detail);
    Vector3 mittFwd = safeNorm(
        Vector3(-0.06f + pose.mittSide * 0.3f, pose.mittHeight * 0.15f, 1.0f),
        Vector3(0.0f, 0.0f, 1.0f)
    );
    addMitt(mesh, mittPos, mittFwd, pose.gloveOpen, detail);

    mesh.rebuildNormals();
    return mesh;
}
