#include "SkinnedModel3D.h"

#include <algorithm>
#include <cmath>

namespace {

// Soft game-character palette (reads clean under Gouraud).
const sf::Color kSkin(238, 200, 168);
const sf::Color kSkinDeep(218, 178, 146);
const sf::Color kSkinShadow(200, 160, 132);
const sf::Color kJersey(250, 252, 255);
const sf::Color kJerseyDeep(232, 236, 244);
const sf::Color kJerseyShade(220, 226, 236);
const sf::Color kPants(52, 60, 78);
const sf::Color kPantsDeep(42, 48, 64);
const sf::Color kPantsLight(64, 72, 90);
const sf::Color kCap(30, 48, 98);
const sf::Color kCapDeep(22, 38, 82);
const sf::Color kCapBill(26, 42, 88);
const sf::Color kAccent(208, 46, 54);
const sf::Color kCleat(32, 32, 40);
const sf::Color kCleatSole(48, 48, 56);
const sf::Color kMitt(176, 124, 80);
const sf::Color kMittDeep(140, 94, 56);
const sf::Color kMittPad(190, 140, 94);
const sf::Color kGear(42, 58, 86);
const sf::Color kGearDeep(34, 46, 68);
const sf::Color kGearLight(62, 82, 112);
const sf::Color kSock(248, 248, 252);
const sf::Color kBelt(38, 40, 48);
const sf::Color kBeltBuckle(176, 172, 158);
const sf::Color kHair(40, 30, 26);

constexpr float kPi = 3.14159265f;

Vector3 safeNorm(const Vector3& v, const Vector3& fb = Vector3(0.0f, 1.0f, 0.0f)) {
    float m = v.magnitude();
    return m > 1e-6f ? v * (1.0f / m) : fb;
}

float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

int addJoint(
    SkinnedModel3D& m,
    const std::string& name,
    int parent,
    const Vector3& localPos,
    const Quaternion& localRot = Quaternion::identity()
) {
    Joint3D j;
    j.name = name;
    j.parent = parent;
    j.restTranslation = localPos;
    j.restRotation = localRot;
    j.restScale = Vector3(1.0f, 1.0f, 1.0f);
    j.bakeLocalRest();
    m.joints.push_back(j);
    return static_cast<int>(m.joints.size()) - 1;
}

void addTri(SkinnedModel3D& m, int a, int b, int c) {
    m.triangles.push_back({a, b, c});
}

void setWeights(
    SkinVertex& v,
    int j0, float w0,
    int j1 = 0, float w1 = 0.0f,
    int j2 = 0, float w2 = 0.0f,
    int j3 = 0, float w3 = 0.0f
) {
    float sum = w0 + w1 + w2 + w3;
    if (sum < 1e-6f) {
        v.joints[0] = j0;
        v.weights[0] = 1.0f;
        return;
    }
    float inv = 1.0f / sum;
    v.joints[0] = j0; v.weights[0] = w0 * inv;
    v.joints[1] = j1; v.weights[1] = w1 * inv;
    v.joints[2] = j2; v.weights[2] = w2 * inv;
    v.joints[3] = j3; v.weights[3] = w3 * inv;
}

// Continuous tapered limb: rings extend slightly past endpoints so joints fuse.
// Optional mid joint gets peak weight at mid-span (better elbows/knees).
void addLimb(
    SkinnedModel3D& m,
    int jointA,
    int jointB,
    const Vector3& worldA,
    const Vector3& worldB,
    float r0,
    float r1,
    sf::Color color,
    int rings,
    int segs,
    int jointMid = -1,
    float bulge = 0.08f,
    float extend = 0.06f // fraction of length past ends
) {
    Vector3 d = worldB - worldA;
    float len = d.magnitude();
    if (len < 1e-4f) {
        return;
    }
    Vector3 axis = d * (1.0f / len);
    Vector3 up = std::fabs(axis.y) < 0.9f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
    Vector3 side = safeNorm(axis.cross(up));
    Vector3 fwd = safeNorm(side.cross(axis));

    float start = -extend;
    float end = 1.0f + extend;
    int base = static_cast<int>(m.bindVertices.size());

    for (int i = 0; i <= rings; i++) {
        float t = static_cast<float>(i) / static_cast<float>(rings);
        float u = start + (end - start) * t; // may be <0 or >1
        float uClamped = clampf(u, 0.0f, 1.0f);
        float r = r0 + (r1 - r0) * uClamped;
        // Soft muscle bulge mid-limb; taper near ends for joint melt.
        float edge = std::sin(uClamped * kPi);
        r *= 1.0f + bulge * edge;
        if (u < 0.0f) {
            r *= 1.0f + u * 0.5f; // shrink past start
        }
        if (u > 1.0f) {
            r *= 1.0f - (u - 1.0f) * 0.5f;
        }
        r = std::max(r, r1 * 0.35f);

        Vector3 center = worldA + axis * (len * u);

        // Skin weights: smooth blend A↔B, optional mid peak.
        float wB = uClamped * uClamped * (3.0f - 2.0f * uClamped);
        float wA = 1.0f - wB;
        float wM = 0.0f;
        if (jointMid >= 0) {
            wM = 0.35f * edge;
            float renorm = 1.0f - wM;
            wA *= renorm;
            wB *= renorm;
        }

        for (int s = 0; s < segs; s++) {
            float ang = (static_cast<float>(s) / segs) * kPi * 2.0f;
            Vector3 n = side * std::cos(ang) + fwd * std::sin(ang);
            SkinVertex v;
            v.position = center + n * r;
            v.normal = n;
            v.color = color;
            if (jointMid >= 0) {
                setWeights(v, jointA, wA, jointB, wB, jointMid, wM);
            } else {
                setWeights(v, jointA, wA, jointB, wB);
            }
            m.bindVertices.push_back(v);
        }
    }

    for (int i = 0; i < rings; i++) {
        for (int s = 0; s < segs; s++) {
            int s1 = (s + 1) % segs;
            int a = base + i * segs + s;
            int b = base + i * segs + s1;
            int c = base + (i + 1) * segs + s;
            int d = base + (i + 1) * segs + s1;
            addTri(m, a, c, b);
            addTri(m, b, c, d);
        }
    }
}

// Ellipsoid volume weighted to one or more joints (same color = seamless).
void addVolume(
    SkinnedModel3D& m,
    const Vector3& center,
    float rx,
    float ry,
    float rz,
    sf::Color color,
    int rings,
    int segs,
    int j0, float w0,
    int j1 = 0, float w1 = 0.0f,
    int j2 = 0, float w2 = 0.0f
) {
    int base = static_cast<int>(m.bindVertices.size());
    for (int ring = 0; ring <= rings; ring++) {
        float v = static_cast<float>(ring) / rings;
        float phi = v * kPi;
        float y = std::cos(phi);
        float rr = std::sin(phi);
        for (int s = 0; s < segs; s++) {
            float u = static_cast<float>(s) / segs;
            float theta = u * kPi * 2.0f;
            Vector3 n(std::cos(theta) * rr, y, std::sin(theta) * rr);
            SkinVertex sv;
            sv.position = center + Vector3(n.x * rx, n.y * ry, n.z * rz);
            sv.normal = safeNorm(Vector3(n.x / std::max(rx, 1e-4f), n.y / std::max(ry, 1e-4f), n.z / std::max(rz, 1e-4f)));
            sv.color = color;
            setWeights(sv, j0, w0, j1, w1, j2, w2);
            m.bindVertices.push_back(sv);
        }
    }
    for (int ring = 0; ring < rings; ring++) {
        for (int s = 0; s < segs; s++) {
            int s1 = (s + 1) % segs;
            int a = base + ring * segs + s;
            int b = base + ring * segs + s1;
            int c = base + (ring + 1) * segs + s;
            int d = base + (ring + 1) * segs + s1;
            if (ring != 0) {
                addTri(m, a, c, b);
            }
            if (ring != rings - 1) {
                addTri(m, b, c, d);
            }
        }
    }
}

std::vector<Vector3> jointRestWorld(const SkinnedModel3D& m) {
    const int n = static_cast<int>(m.joints.size());
    std::vector<Matrix4> global(n, Matrix4::identity());
    std::vector<Vector3> pos(n);
    for (int i = 0; i < n; i++) {
        int p = m.joints[i].parent;
        if (p >= 0) {
            global[i] = global[p] * m.joints[i].localRest;
        } else {
            global[i] = m.joints[i].localRest;
        }
        pos[i] = global[i].transformPoint(Vector3());
    }
    return pos;
}

std::vector<Matrix4> jointRestGlobal(const SkinnedModel3D& m) {
    const int n = static_cast<int>(m.joints.size());
    std::vector<Matrix4> global(n, Matrix4::identity());
    for (int i = 0; i < n; i++) {
        int p = m.joints[i].parent;
        if (p >= 0) {
            global[i] = global[p] * m.joints[i].localRest;
        } else {
            global[i] = m.joints[i].localRest;
        }
    }
    return global;
}

// Local-space offset in a joint's rest frame → world position.
Vector3 jointLocal(
    const std::vector<Matrix4>& G,
    int joint,
    float lx, float ly, float lz
) {
    return G[joint].transformPoint(Vector3(lx, ly, lz));
}

SkinnedModel3D makeHumanoid(bool catcher, int detail) {
    SkinnedModel3D m;
    detail = std::clamp(detail, 0, 2);

    // Tessellation budget: high enough for smooth silhouettes under software raster.
    int rings = detail >= 2 ? 12 : (detail >= 1 ? 9 : 6);
    int segs = detail >= 2 ? 18 : (detail >= 1 ? 14 : 10);
    int volR = detail >= 2 ? 12 : (detail >= 1 ? 9 : 7);
    int volS = detail >= 2 ? 18 : (detail >= 1 ? 14 : 10);

    // ── skeleton ────────────────────────────────────────────────────────
    // RHP closed sideways on the rubber; head looks toward home plate.
    const float sideYaw = catcher ? 0.0f : -0.95f;
    int root = addJoint(
        m, "Root", -1, Vector3(0.0f, 0.0f, 0.0f),
        Quaternion::fromEulerXYZ(0.0f, sideYaw, 0.0f)
    );
    int hips = addJoint(m, "Hips", root, Vector3(0.0f, 0.92f, 0.0f));
    int spine = addJoint(m, "Spine", hips, Vector3(0.0f, 0.15f, 0.02f));
    int chest = addJoint(m, "Chest", spine, Vector3(0.0f, 0.17f, 0.02f));
    int neck = addJoint(m, "Neck", chest, Vector3(0.0f, 0.13f, 0.01f));
    int head = addJoint(
        m, "Head", neck, Vector3(0.0f, 0.12f, 0.02f),
        catcher ? Quaternion::identity()
                : Quaternion::fromEulerXYZ(-0.03f, 0.88f, 0.0f)
    );

    int shL = addJoint(m, "Shoulder_L", chest, Vector3(-0.17f, 0.08f, 0.01f));
    int elL = addJoint(m, "Elbow_L", shL, Vector3(0.0f, -0.30f, 0.02f));
    int wrL = addJoint(m, "Wrist_L", elL, Vector3(0.0f, -0.27f, 0.02f));
    int palmL = addJoint(m, "Palm_L", wrL, Vector3(0.0f, -0.05f, 0.04f));

    int shR = addJoint(m, "Shoulder_R", chest, Vector3(0.17f, 0.08f, 0.01f));
    int elR = addJoint(m, "Elbow_R", shR, Vector3(0.0f, -0.30f, 0.02f));
    int wrR = addJoint(m, "Wrist_R", elR, Vector3(0.0f, -0.27f, 0.02f));
    int palmR = addJoint(m, "Palm_R", wrR, Vector3(0.0f, -0.05f, 0.05f));

    int hipR = addJoint(m, "Hip_R", hips, Vector3(0.105f, -0.02f, -0.03f));
    int knR = addJoint(m, "Knee_R", hipR, Vector3(0.0f, -0.42f, 0.02f));
    int anR = addJoint(m, "Ankle_R", knR, Vector3(0.0f, -0.40f, 0.0f));
    int toeR = addJoint(m, "Toe_R", anR, Vector3(0.0f, -0.015f, 0.085f));

    int hipL = addJoint(m, "Hip_L", hips, Vector3(-0.105f, -0.02f, 0.03f));
    int knL = addJoint(m, "Knee_L", hipL, Vector3(0.0f, -0.42f, 0.02f));
    int anL = addJoint(m, "Ankle_L", knL, Vector3(0.0f, -0.40f, 0.02f));
    int toeL = addJoint(m, "Toe_L", anL, Vector3(0.0f, -0.015f, 0.085f));

    if (catcher) {
        m.joints[root].restRotation = Quaternion::identity();
        m.joints[root].bakeLocalRest();
        m.joints[hips].restTranslation = Vector3(0.0f, 0.50f, 0.0f);
        m.joints[hips].bakeLocalRest();
        m.joints[knL].restTranslation = Vector3(0.04f, -0.22f, 0.08f);
        m.joints[knR].restTranslation = Vector3(-0.04f, -0.22f, 0.08f);
        m.joints[knL].bakeLocalRest();
        m.joints[knR].bakeLocalRest();
        m.joints[anL].restTranslation = Vector3(0.0f, -0.22f, 0.10f);
        m.joints[anR].restTranslation = Vector3(0.0f, -0.22f, 0.10f);
        m.joints[anL].bakeLocalRest();
        m.joints[anR].bakeLocalRest();
        m.joints[spine].restTranslation = Vector3(0.0f, 0.12f, 0.04f);
        m.joints[spine].restRotation = Quaternion::fromEulerXYZ(0.15f, 0.0f, 0.0f);
        m.joints[spine].bakeLocalRest();
        m.joints[head].restRotation = Quaternion::identity();
        m.joints[head].bakeLocalRest();
    }

    m.rebuildInverseBindsFromRest();
    auto W = jointRestWorld(m);
    auto G = jointRestGlobal(m);

    const sf::Color upper = catcher ? kGear : kJersey;
    const sf::Color sleeve = catcher ? kGearDeep : kJerseyDeep;

    // ── LEGS (continuous thigh→shin with knee melt) ─────────────────────
    addLimb(m, hipR, knR, W[hipR], W[knR], 0.092f, 0.076f, kPants, rings, segs, hips, 0.10f, 0.08f);
    addVolume(m, W[knR], 0.080f, 0.078f, 0.080f, kPantsLight, volR, volS, knR, 0.55f, hipR, 0.25f, anR, 0.20f);
    addLimb(m, knR, anR, W[knR], W[anR], 0.074f, 0.052f, kPantsDeep, rings, segs, knR, 0.06f, 0.08f);

    addLimb(m, hipL, knL, W[hipL], W[knL], 0.092f, 0.076f, kPants, rings, segs, hips, 0.10f, 0.08f);
    addVolume(m, W[knL], 0.080f, 0.078f, 0.080f, kPantsLight, volR, volS, knL, 0.55f, hipL, 0.25f, anL, 0.20f);
    addLimb(m, knL, anL, W[knL], W[anL], 0.074f, 0.052f, kPantsDeep, rings, segs, knL, 0.06f, 0.08f);

    // Socks + detailed cleats
    for (int side = 0; side < 2; side++) {
        int an = side == 0 ? anR : anL;
        int toe = side == 0 ? toeR : toeL;
        addVolume(m, W[an] + Vector3(0.0f, 0.03f, 0.0f), 0.044f, 0.032f, 0.044f, kSock, volR / 2, volS / 2, an, 1.0f);
        // Shoe upper
        addVolume(m, W[toe] + Vector3(0.0f, 0.01f, -0.01f), 0.050f, 0.030f, 0.078f, kCleat, volR, volS, toe, 0.7f, an, 0.3f);
        // Sole
        addVolume(m, W[toe] + Vector3(0.0f, -0.016f, -0.005f), 0.054f, 0.012f, 0.088f, kCleatSole, volR / 2, volS / 2, toe, 0.75f, an, 0.25f);
        // Heel
        addVolume(m, W[an] + Vector3(0.0f, -0.01f, -0.03f), 0.036f, 0.022f, 0.030f, kCleat, volR / 2, volS / 2, an, 0.8f, toe, 0.2f);
    }

    // ── PELVIS / WAIST (bridges legs → torso, same colors melt seams) ────
    addVolume(m, W[hips], 0.155f, 0.118f, 0.128f, kPants, volR, volS, hips, 1.0f);
    addVolume(m, W[hips] + Vector3(0.0f, 0.055f, 0.0f), 0.142f, 0.028f, 0.115f, kBelt, volR / 2, volS / 2, hips, 0.85f, spine, 0.15f);
    if (!catcher) {
        addVolume(
            m, jointLocal(G, hips, 0.0f, 0.05f, 0.10f),
            0.026f, 0.016f, 0.012f, kBeltBuckle, 5, 8, hips, 1.0f
        );
    }
    addVolume(m, W[hipR], 0.095f, 0.090f, 0.095f, kPants, volR, volS, hipR, 0.6f, hips, 0.4f);
    addVolume(m, W[hipL], 0.095f, 0.090f, 0.095f, kPants, volR, volS, hipL, 0.6f, hips, 0.4f);
    // Soft waist into jersey
    addVolume(
        m, W[hips] + Vector3(0.0f, 0.09f, 0.01f),
        0.138f, 0.055f, 0.108f, kJerseyShade, volR, volS, hips, 0.45f, spine, 0.55f
    );

    // ── TORSO (one continuous jersey mass) ──────────────────────────────
    addLimb(m, hips, spine, W[hips], W[spine], 0.148f, 0.140f, kJersey, rings, segs, spine, 0.05f, 0.10f);
    addLimb(m, spine, chest, W[spine], W[chest], 0.142f, 0.152f, kJersey, rings, segs, chest, 0.06f, 0.10f);
    // Chest plate / volume
    addVolume(
        m, W[chest] + Vector3(0.0f, 0.01f, 0.035f),
        0.140f, 0.105f, 0.100f, kJerseyDeep, volR, volS, chest, 0.7f, spine, 0.3f
    );
    // Upper chest / collarbone bridge between shoulders
    Vector3 collar = (W[shL] + W[shR]) * 0.5f + Vector3(0.0f, 0.015f, 0.01f);
    addVolume(m, collar, 0.130f, 0.055f, 0.078f, upper, volR, volS, chest, 0.5f, shL, 0.25f, shR, 0.25f);
    addVolume(m, W[spine], 0.130f, 0.085f, 0.105f, kJersey, volR, volS, spine, 0.6f, chest, 0.25f, hips, 0.15f);

    if (catcher) {
        addVolume(
            m, W[chest] + Vector3(0.0f, 0.0f, 0.065f),
            0.155f, 0.130f, 0.078f, kGear, volR, volS, chest, 0.85f, spine, 0.15f
        );
        addVolume(
            m, W[chest] + Vector3(0.0f, -0.04f, 0.07f),
            0.120f, 0.055f, 0.050f, kGearDeep, volR / 2, volS / 2, chest, 1.0f
        );
    } else {
        // Subtle number stripe (front of jersey, local +Z of chest).
        addVolume(
            m, jointLocal(G, chest, 0.0f, -0.02f, 0.11f),
            0.016f, 0.055f, 0.010f, kAccent, 5, 8, chest, 1.0f
        );
    }

    // Delts — large, fused into torso + upper arm
    addVolume(m, W[shL], 0.098f, 0.090f, 0.098f, upper, volR, volS, shL, 0.55f, chest, 0.35f, elL, 0.10f);
    addVolume(m, W[shR], 0.098f, 0.090f, 0.098f, upper, volR, volS, shR, 0.55f, chest, 0.35f, elR, 0.10f);
    if (catcher) {
        addVolume(m, W[shL] + Vector3(-0.02f, 0.02f, 0.0f), 0.072f, 0.055f, 0.065f, kGearLight, volR / 2, volS / 2, shL, 1.0f);
        addVolume(m, W[shR] + Vector3(0.02f, 0.02f, 0.0f), 0.072f, 0.055f, 0.065f, kGearLight, volR / 2, volS / 2, shR, 1.0f);
    }

    // ── ARMS ────────────────────────────────────────────────────────────
    addLimb(m, shL, elL, W[shL], W[elL], 0.062f, 0.052f, sleeve, rings, segs, shL, 0.07f, 0.09f);
    addVolume(m, W[elL], 0.052f, 0.050f, 0.052f, kSkin, volR, volS, elL, 0.55f, shL, 0.25f, wrL, 0.20f);
    addLimb(m, elL, wrL, W[elL], W[wrL], 0.050f, 0.040f, kSkin, rings, segs, elL, 0.05f, 0.09f);

    addLimb(m, shR, elR, W[shR], W[elR], 0.062f, 0.052f, sleeve, rings, segs, shR, 0.07f, 0.09f);
    addVolume(m, W[elR], 0.052f, 0.050f, 0.052f, kSkin, volR, volS, elR, 0.55f, shR, 0.25f, wrR, 0.20f);
    addLimb(m, elR, wrR, W[elR], W[wrR], 0.050f, 0.040f, kSkin, rings, segs, elR, 0.05f, 0.09f);

    // Sleeve cuffs
    addVolume(m, W[elL] * 0.35f + W[shL] * 0.65f, 0.056f, 0.056f, 0.056f, kJerseyShade, volR / 2, volS / 2, shL, 0.7f, elL, 0.3f);
    addVolume(m, W[elR] * 0.35f + W[shR] * 0.65f, 0.056f, 0.056f, 0.056f, kJerseyShade, volR / 2, volS / 2, shR, 0.7f, elR, 0.3f);

    // ── HANDS ───────────────────────────────────────────────────────────
    // Throwing hand: palm + finger mass
    addVolume(m, W[palmR], 0.038f, 0.032f, 0.042f, kSkinDeep, volR, volS, palmR, 0.7f, wrR, 0.3f);
    addVolume(m, W[wrR], 0.034f, 0.030f, 0.034f, kSkin, volR / 2, volS / 2, wrR, 0.75f, palmR, 0.25f);
    addVolume(
        m, jointLocal(G, palmR, 0.0f, -0.03f, 0.02f),
        0.028f, 0.018f, 0.030f, kSkinShadow, volR / 2, volS / 2, palmR, 1.0f
    );

    // Mitt: layered pocket silhouette
    addVolume(m, W[palmL], 0.078f, 0.090f, 0.052f, kMitt, volR, volS, palmL, 0.75f, wrL, 0.25f);
    addVolume(
        m, jointLocal(G, palmL, 0.0f, 0.035f, 0.025f),
        0.062f, 0.048f, 0.040f, kMittPad, volR, volS, palmL, 1.0f
    );
    addVolume(
        m, jointLocal(G, palmL, -0.04f, 0.01f, 0.0f),
        0.034f, 0.048f, 0.032f, kMittDeep, volR / 2, volS / 2, palmL, 1.0f
    );
    addVolume(
        m, jointLocal(G, palmL, 0.04f, 0.01f, 0.0f),
        0.032f, 0.046f, 0.030f, kMittDeep, volR / 2, volS / 2, palmL, 1.0f
    );
    addVolume(m, W[wrL], 0.038f, 0.032f, 0.038f, kMittDeep, volR / 2, volS / 2, wrL, 0.7f, palmL, 0.3f);

    // ── NECK + HEAD ─────────────────────────────────────────────────────
    addLimb(m, neck, head, W[neck], W[head], 0.046f, 0.044f, kSkin, rings / 2 + 2, segs, neck, 0.03f, 0.12f);
    addVolume(m, W[neck], 0.048f, 0.040f, 0.048f, kSkin, volR / 2, volS / 2, neck, 0.5f, chest, 0.3f, head, 0.2f);

    // Skull
    addVolume(m, W[head], 0.112f, 0.118f, 0.108f, kSkin, volR, volS, head, 1.0f);
    // Jaw / cheeks
    addVolume(
        m, jointLocal(G, head, 0.0f, -0.035f, 0.035f),
        0.070f, 0.055f, 0.065f, kSkin, volR / 2, volS / 2, head, 1.0f
    );
    // Ears
    addVolume(m, jointLocal(G, head, -0.095f, 0.0f, 0.0f), 0.018f, 0.028f, 0.016f, kSkinDeep, 5, 8, head, 1.0f);
    addVolume(m, jointLocal(G, head, 0.095f, 0.0f, 0.0f), 0.018f, 0.028f, 0.016f, kSkinDeep, 5, 8, head, 1.0f);
    // Hair fringe under cap
    addVolume(
        m, jointLocal(G, head, 0.0f, 0.06f, -0.02f),
        0.090f, 0.030f, 0.080f, kHair, volR / 2, volS / 2, head, 1.0f
    );

    if (catcher) {
        addVolume(
            m, jointLocal(G, head, 0.0f, 0.04f, -0.02f),
            0.120f, 0.078f, 0.120f, kGear, volR, volS, head, 1.0f
        );
        addVolume(
            m, jointLocal(G, head, 0.0f, 0.0f, 0.125f),
            0.072f, 0.068f, 0.018f, kGearDeep, volR / 2, volS / 2, head, 1.0f
        );
        addVolume(
            m, jointLocal(G, head, 0.0f, 0.02f, 0.13f),
            0.055f, 0.012f, 0.012f, kGearLight, 4, 8, head, 1.0f
        );
        addVolume(
            m, jointLocal(G, head, 0.0f, -0.02f, 0.13f),
            0.055f, 0.012f, 0.012f, kGearLight, 4, 8, head, 1.0f
        );
    } else {
        // Cap crown (top only — never cuts face)
        addVolume(
            m, jointLocal(G, head, 0.0f, 0.072f, -0.015f),
            0.100f, 0.038f, 0.100f, kCap, volR, volS, head, 1.0f
        );
        addVolume(
            m, jointLocal(G, head, 0.0f, 0.048f, -0.01f),
            0.102f, 0.016f, 0.102f, kCapDeep, volR / 2, volS / 2, head, 1.0f
        );
        // Bill forward of forehead (local +Z of head → plate after yaw)
        addVolume(
            m, jointLocal(G, head, 0.0f, 0.042f, 0.100f),
            0.058f, 0.014f, 0.048f, kCapBill, volR / 2, volS / 2, head, 1.0f
        );
        addVolume(
            m, jointLocal(G, head, 0.0f, 0.038f, 0.132f),
            0.042f, 0.010f, 0.022f, kCap, 5, 8, head, 1.0f
        );
        // Logo pip
        addVolume(
            m, jointLocal(G, head, 0.0f, 0.062f, 0.055f),
            0.016f, 0.012f, 0.010f, kAccent, 5, 8, head, 1.0f
        );
        // Face mass toward plate
        addVolume(
            m, jointLocal(G, head, 0.0f, -0.005f, 0.095f),
            0.055f, 0.050f, 0.040f, kSkinDeep, volR / 2, volS / 2, head, 1.0f
        );
    }

    return m;
}

} // namespace

int SkinnedModel3D::findJoint(const std::string& name) const {
    for (int i = 0; i < static_cast<int>(joints.size()); i++) {
        if (joints[i].name == name) {
            return i;
        }
    }
    return -1;
}

const AnimationClip* SkinnedModel3D::findClip(const std::string& name) const {
    for (const AnimationClip& c : clips) {
        if (c.name == name) {
            return &c;
        }
    }
    return nullptr;
}

void SkinnedModel3D::rebuildInverseBindsFromRest() {
    const int n = static_cast<int>(joints.size());
    std::vector<Matrix4> global(n, Matrix4::identity());
    for (int i = 0; i < n; i++) {
        joints[i].bakeLocalRest();
        int p = joints[i].parent;
        if (p >= 0 && p < n) {
            global[i] = global[p] * joints[i].localRest;
        } else {
            global[i] = joints[i].localRest;
        }
        joints[i].inverseBind = global[i].inverse();
    }
}

Mesh3D SkinnedModel3D::skinToMesh(const std::vector<Matrix4>& skinMatrices) const {
    Mesh3D out;
    skinInto(skinMatrices, out);
    return out;
}

void SkinnedModel3D::skinInto(const std::vector<Matrix4>& skinMatrices, Mesh3D& out) const {
    const int nJ = static_cast<int>(skinMatrices.size());
    const bool reuse =
        out.vertices.size() == bindVertices.size() &&
        out.triangles.size() == triangles.size();

    if (!reuse) {
        out.vertices.resize(bindVertices.size());
        out.vertexNormals.resize(bindVertices.size());
        out.triangles = triangles;
        out.triangleColors.resize(triangles.size());
    }
    out.edges.clear();

    std::vector<sf::Color> vertColor(bindVertices.size());

    for (int i = 0; i < static_cast<int>(bindVertices.size()); i++) {
        const SkinVertex& sv = bindVertices[i];
        vertColor[i] = sv.color;
        Vector3 p;
        Vector3 n;
        float wSum = 0.0f;
        for (int k = 0; k < 4; k++) {
            float w = sv.weights[k];
            int j = sv.joints[k];
            if (w <= 0.0f || j < 0 || j >= nJ) {
                continue;
            }
            wSum += w;
            p += skinMatrices[j].transformPoint(sv.position) * w;
            n += skinMatrices[j].transformDirection3x3(sv.normal) * w;
        }
        if (wSum < 1e-6f) {
            out.vertices[i] = sv.position;
            out.vertexNormals[i] = sv.normal;
        } else {
            out.vertices[i] = p * (1.0f / wSum);
            float nm = n.magnitude();
            out.vertexNormals[i] = nm > 1e-6f ? n * (1.0f / nm) : sv.normal;
        }
    }

    if (!reuse || out.triangleColors.size() != triangles.size()) {
        out.triangleColors.resize(triangles.size());
        for (int t = 0; t < static_cast<int>(triangles.size()); t++) {
            const Triangle3D& tri = triangles[t];
            const sf::Color& a = vertColor[tri.a];
            const sf::Color& b = vertColor[tri.b];
            const sf::Color& c = vertColor[tri.c];
            out.triangleColors[t] = sf::Color(
                static_cast<std::uint8_t>((a.r + b.r + c.r) / 3),
                static_cast<std::uint8_t>((a.g + b.g + c.g) / 3),
                static_cast<std::uint8_t>((a.b + b.b + c.b) / 3)
            );
        }
    }

    // Prefer smooth vertex normals from skin; rebuildTriangleNormals only if empty.
    if (out.vertexNormals.empty()) {
        out.rebuildNormals();
    } else {
        // Still need triangle normals for some raster paths.
        out.rebuildNormals();
        // rebuildNormals overwrites vertex normals from faces — for skinned meshes
        // we want skinned normals. Re-apply skinned vertex normals after.
        // Actually rebuildNormals overwrites vertexNormals. So recompute skinned normals again:
        for (int i = 0; i < static_cast<int>(bindVertices.size()); i++) {
            const SkinVertex& sv = bindVertices[i];
            Vector3 n;
            float wSum = 0.0f;
            for (int k = 0; k < 4; k++) {
                float w = sv.weights[k];
                int j = sv.joints[k];
                if (w <= 0.0f || j < 0 || j >= nJ) {
                    continue;
                }
                wSum += w;
                n += skinMatrices[j].transformDirection3x3(sv.normal) * w;
            }
            float nm = n.magnitude();
            out.vertexNormals[i] = nm > 1e-6f ? n * (1.0f / nm) : Vector3(0.0f, 1.0f, 0.0f);
        }
    }
}

SkinnedModel3D SkinnedModel3D::makeProceduralPitcher(int detail) {
    return makeHumanoid(false, detail);
}

SkinnedModel3D SkinnedModel3D::makeProceduralCatcher(int detail) {
    return makeHumanoid(true, detail);
}
