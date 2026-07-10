#include "SkinnedModel3D.h"

#include <algorithm>
#include <cmath>

namespace {

// Clean sports-game palette
const sf::Color kSkin(235, 195, 160);
const sf::Color kSkinDeep(210, 170, 140);
const sf::Color kJersey(248, 250, 252);
const sf::Color kJerseyDeep(225, 230, 238);
const sf::Color kPants(48, 56, 72);
const sf::Color kPantsDeep(38, 44, 58);
const sf::Color kCap(28, 44, 96);
const sf::Color kCapDeep(20, 34, 80);
const sf::Color kAccent(200, 40, 48);
const sf::Color kCleat(28, 28, 34);
const sf::Color kSole(44, 44, 50);
const sf::Color kMitt(168, 115, 72);
const sf::Color kMittDeep(130, 88, 52);
const sf::Color kGear(40, 56, 84);
const sf::Color kGearDeep(32, 44, 66);
const sf::Color kSock(245, 245, 250);
const sf::Color kBelt(36, 38, 44);
const sf::Color kHair(36, 28, 24);

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

void setW(SkinVertex& v, int j0, float w0, int j1 = 0, float w1 = 0.0f, int j2 = 0, float w2 = 0.0f) {
    float s = w0 + w1 + w2;
    if (s < 1e-6f) {
        v.joints[0] = j0;
        v.weights[0] = 1.0f;
        return;
    }
    float inv = 1.0f / s;
    v.joints[0] = j0; v.weights[0] = w0 * inv;
    v.joints[1] = j1; v.weights[1] = w1 * inv;
    v.joints[2] = j2; v.weights[2] = w2 * inv;
    v.joints[3] = 0;  v.weights[3] = 0.0f;
}

// Solid capsule: ONE continuous tapered tube between two points (no bead stack).
// Built as a lathed cylinder with hemispherical ends, smooth weight blend.
void solidLimb(
    SkinnedModel3D& m,
    int jA, int jB,
    const Vector3& a, const Vector3& b,
    float rA, float rB,
    sf::Color color,
    int rings, int segs
) {
    Vector3 d = b - a;
    float len = d.magnitude();
    if (len < 1e-4f) {
        return;
    }
    Vector3 axis = d * (1.0f / len);
    Vector3 up = std::fabs(axis.y) < 0.92f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
    Vector3 side = safeNorm(axis.cross(up));
    Vector3 fwd = safeNorm(side.cross(axis));

    // rings along length; extra rings form soft end caps.
    int cap = std::max(2, rings / 5);
    int total = rings + cap * 2;
    int base = static_cast<int>(m.bindVertices.size());

    for (int i = 0; i <= total; i++) {
        float t = static_cast<float>(i) / static_cast<float>(total);
        // Map t into [-capFrac, 1+capFrac] then compress ends into hemispheres.
        float u = t; // 0..1 along full capsule
        float along, radius, wB;

        if (i <= cap) {
            // Start hemisphere
            float phi = (static_cast<float>(i) / cap) * (kPi * 0.5f); // 0..pi/2
            along = -rA * std::cos(phi);
            radius = rA * std::sin(phi);
            if (radius < 0.001f) {
                radius = 0.001f;
            }
            wB = 0.0f;
        } else if (i >= total - cap) {
            // End hemisphere
            float k = static_cast<float>(i - (total - cap)) / cap; // 0..1
            float phi = k * (kPi * 0.5f);
            along = len + rB * std::sin(phi);
            radius = rB * std::cos(phi);
            if (radius < 0.001f) {
                radius = 0.001f;
            }
            wB = 1.0f;
        } else {
            // Shaft
            float s = static_cast<float>(i - cap) / static_cast<float>(rings);
            along = s * len;
            radius = rA + (rB - rA) * s;
            // Mild mid bulge
            radius *= 1.0f + 0.05f * std::sin(s * kPi);
            wB = s * s * (3.0f - 2.0f * s);
        }

        Vector3 center = a + axis * along;
        float wA = 1.0f - wB;

        for (int s = 0; s < segs; s++) {
            float ang = (static_cast<float>(s) / segs) * kPi * 2.0f;
            Vector3 n = side * std::cos(ang) + fwd * std::sin(ang);
            SkinVertex v;
            v.position = center + n * radius;
            v.normal = n;
            v.color = color;
            setW(v, jA, wA, jB, wB);
            m.bindVertices.push_back(v);
        }
    }

    for (int i = 0; i < total; i++) {
        for (int s = 0; s < segs; s++) {
            int s1 = (s + 1) % segs;
            int p0 = base + i * segs + s;
            int p1 = base + i * segs + s1;
            int p2 = base + (i + 1) * segs + s;
            int p3 = base + (i + 1) * segs + s1;
            addTri(m, p0, p2, p1);
            addTri(m, p1, p2, p3);
        }
    }
}

// Ellipsoid volume — used sparingly for head, pelvis, hands, gear.
void solidBall(
    SkinnedModel3D& m,
    const Vector3& c,
    float rx, float ry, float rz,
    sf::Color color,
    int rings, int segs,
    int j0, float w0,
    int j1 = 0, float w1 = 0.0f
) {
    int base = static_cast<int>(m.bindVertices.size());
    for (int ring = 0; ring <= rings; ring++) {
        float v = static_cast<float>(ring) / rings;
        float phi = v * kPi;
        float y = std::cos(phi);
        float rr = std::sin(phi);
        for (int s = 0; s < segs; s++) {
            float u = static_cast<float>(s) / segs;
            float th = u * kPi * 2.0f;
            Vector3 n(std::cos(th) * rr, y, std::sin(th) * rr);
            SkinVertex sv;
            sv.position = c + Vector3(n.x * rx, n.y * ry, n.z * rz);
            sv.normal = safeNorm(Vector3(
                n.x / std::max(rx, 1e-4f),
                n.y / std::max(ry, 1e-4f),
                n.z / std::max(rz, 1e-4f)
            ));
            sv.color = color;
            setW(sv, j0, w0, j1, w1);
            m.bindVertices.push_back(sv);
        }
    }
    for (int ring = 0; ring < rings; ring++) {
        for (int s = 0; s < segs; s++) {
            int s1 = (s + 1) % segs;
            int a = base + ring * segs + s;
            int b = base + ring * segs + s1;
            int c0 = base + (ring + 1) * segs + s;
            int d = base + (ring + 1) * segs + s1;
            if (ring != 0) {
                addTri(m, a, c0, b);
            }
            if (ring != rings - 1) {
                addTri(m, b, c0, d);
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
        global[i] = (p >= 0) ? global[p] * m.joints[i].localRest : m.joints[i].localRest;
        pos[i] = global[i].transformPoint(Vector3());
    }
    return pos;
}

std::vector<Matrix4> jointRestGlobal(const SkinnedModel3D& m) {
    const int n = static_cast<int>(m.joints.size());
    std::vector<Matrix4> global(n, Matrix4::identity());
    for (int i = 0; i < n; i++) {
        int p = m.joints[i].parent;
        global[i] = (p >= 0) ? global[p] * m.joints[i].localRest : m.joints[i].localRest;
    }
    return global;
}

Vector3 jl(const std::vector<Matrix4>& G, int j, float x, float y, float z) {
    return G[j].transformPoint(Vector3(x, y, z));
}

// Classic human proportions (~1.75m), solid game-character construction.
// Fewer pieces, clear silhouette — intentional low-poly sports look, not melted spheres.
SkinnedModel3D makeHumanoid(bool catcher, int detail) {
    SkinnedModel3D m;
    detail = std::clamp(detail, 0, 2);
    int rings = detail >= 2 ? 10 : (detail >= 1 ? 8 : 6);
    int segs = detail >= 2 ? 14 : (detail >= 1 ? 12 : 10);
    int hr = detail >= 2 ? 10 : 8;
    int hs = detail >= 2 ? 14 : 12;

    // ── skeleton (RHP closed profile on rubber) ─────────────────────────
    const float sideYaw = catcher ? 0.0f : -0.95f;
    int root = addJoint(m, "Root", -1, Vector3(), Quaternion::fromEulerXYZ(0.0f, sideYaw, 0.0f));
    int hips = addJoint(m, "Hips", root, Vector3(0.0f, 0.92f, 0.0f));
    int spine = addJoint(m, "Spine", hips, Vector3(0.0f, 0.16f, 0.015f));
    int chest = addJoint(m, "Chest", spine, Vector3(0.0f, 0.18f, 0.015f));
    int neck = addJoint(m, "Neck", chest, Vector3(0.0f, 0.12f, 0.01f));
    int head = addJoint(
        m, "Head", neck, Vector3(0.0f, 0.13f, 0.02f),
        catcher ? Quaternion::identity() : Quaternion::fromEulerXYZ(-0.02f, 0.90f, 0.0f)
    );

    int shL = addJoint(m, "Shoulder_L", chest, Vector3(-0.18f, 0.06f, 0.0f));
    int elL = addJoint(m, "Elbow_L", shL, Vector3(0.0f, -0.30f, 0.015f));
    int wrL = addJoint(m, "Wrist_L", elL, Vector3(0.0f, -0.27f, 0.015f));
    int palmL = addJoint(m, "Palm_L", wrL, Vector3(0.0f, -0.055f, 0.04f));

    int shR = addJoint(m, "Shoulder_R", chest, Vector3(0.18f, 0.06f, 0.0f));
    int elR = addJoint(m, "Elbow_R", shR, Vector3(0.0f, -0.30f, 0.015f));
    int wrR = addJoint(m, "Wrist_R", elR, Vector3(0.0f, -0.27f, 0.015f));
    int palmR = addJoint(m, "Palm_R", wrR, Vector3(0.0f, -0.055f, 0.045f));

    int hipR = addJoint(m, "Hip_R", hips, Vector3(0.10f, -0.03f, -0.03f));
    int knR = addJoint(m, "Knee_R", hipR, Vector3(0.0f, -0.43f, 0.015f));
    int anR = addJoint(m, "Ankle_R", knR, Vector3(0.0f, -0.41f, 0.0f));
    int toeR = addJoint(m, "Toe_R", anR, Vector3(0.0f, -0.02f, 0.09f));

    int hipL = addJoint(m, "Hip_L", hips, Vector3(-0.10f, -0.03f, 0.03f));
    int knL = addJoint(m, "Knee_L", hipL, Vector3(0.0f, -0.43f, 0.015f));
    int anL = addJoint(m, "Ankle_L", knL, Vector3(0.0f, -0.41f, 0.02f));
    int toeL = addJoint(m, "Toe_L", anL, Vector3(0.0f, -0.02f, 0.09f));

    if (catcher) {
        m.joints[root].restRotation = Quaternion::identity();
        m.joints[root].bakeLocalRest();
        m.joints[hips].restTranslation = Vector3(0.0f, 0.50f, 0.0f);
        m.joints[hips].bakeLocalRest();
        m.joints[knL].restTranslation = Vector3(0.05f, -0.22f, 0.08f);
        m.joints[knR].restTranslation = Vector3(-0.05f, -0.22f, 0.08f);
        m.joints[knL].bakeLocalRest();
        m.joints[knR].bakeLocalRest();
        m.joints[anL].restTranslation = Vector3(0.0f, -0.22f, 0.10f);
        m.joints[anR].restTranslation = Vector3(0.0f, -0.22f, 0.10f);
        m.joints[anL].bakeLocalRest();
        m.joints[anR].bakeLocalRest();
        m.joints[spine].restRotation = Quaternion::fromEulerXYZ(0.14f, 0.0f, 0.0f);
        m.joints[spine].bakeLocalRest();
        m.joints[head].restRotation = Quaternion::identity();
        m.joints[head].bakeLocalRest();
    }

    m.rebuildInverseBindsFromRest();
    auto W = jointRestWorld(m);
    auto G = jointRestGlobal(m);

    // ═══════════════════════════════════════════════════════════════════
    // MESH — solid body parts (one capsule per limb segment, few volumes)
    // ═══════════════════════════════════════════════════════════════════

    // Legs — clear thigh / shin tubes
    solidLimb(m, hipR, knR, W[hipR], W[knR], 0.085f, 0.070f, kPants, rings, segs);
    solidLimb(m, knR, anR, W[knR], W[anR], 0.068f, 0.050f, kPantsDeep, rings, segs);
    solidLimb(m, hipL, knL, W[hipL], W[knL], 0.085f, 0.070f, kPants, rings, segs);
    solidLimb(m, knL, anL, W[knL], W[anL], 0.068f, 0.050f, kPantsDeep, rings, segs);

    // Feet
    for (int s = 0; s < 2; s++) {
        int an = s == 0 ? anR : anL;
        int toe = s == 0 ? toeR : toeL;
        solidBall(m, W[toe], 0.048f, 0.026f, 0.080f, kCleat, hr, hs, toe, 0.75f, an, 0.25f);
        solidBall(m, W[toe] + Vector3(0.0f, -0.014f, 0.0f), 0.050f, 0.010f, 0.086f, kSole, 5, 8, toe, 0.8f, an, 0.2f);
        solidBall(m, W[an] + Vector3(0.0f, 0.025f, 0.0f), 0.040f, 0.028f, 0.040f, kSock, 6, 10, an, 1.0f);
    }

    // Pelvis — single solid pants mass
    solidBall(m, W[hips], 0.145f, 0.105f, 0.118f, kPants, hr, hs, hips, 1.0f);
    solidBall(m, W[hips] + Vector3(0.0f, 0.052f, 0.0f), 0.132f, 0.022f, 0.105f, kBelt, 5, 10, hips, 0.9f, spine, 0.1f);
    // Hip sockets (same pants color, small)
    solidBall(m, W[hipR], 0.072f, 0.068f, 0.072f, kPants, 6, 10, hipR, 0.65f, hips, 0.35f);
    solidBall(m, W[hipL], 0.072f, 0.068f, 0.072f, kPants, 6, 10, hipL, 0.65f, hips, 0.35f);

    // Torso — ONE jersey capsule hips→chest, plus a chest plate
    solidLimb(m, hips, chest, W[hips], W[chest], 0.130f, 0.145f, kJersey, rings + 2, segs);
    solidBall(m, W[chest] + Vector3(0.0f, 0.0f, 0.03f), 0.125f, 0.095f, 0.090f, kJerseyDeep, hr, hs, chest, 0.8f, spine, 0.2f);
    // Collar
    solidBall(m, W[neck], 0.055f, 0.040f, 0.050f, kJerseyDeep, 6, 10, neck, 0.5f, chest, 0.5f);

    if (catcher) {
        solidBall(m, W[chest] + Vector3(0.0f, 0.0f, 0.06f), 0.140f, 0.115f, 0.070f, kGear, hr, hs, chest, 1.0f);
    } else {
        solidBall(m, jl(G, chest, 0.0f, -0.01f, 0.105f), 0.014f, 0.048f, 0.008f, kAccent, 4, 6, chest, 1.0f);
    }

    // Shoulders — modest delts (not giant balloons)
    sf::Color upper = catcher ? kGear : kJersey;
    solidBall(m, W[shL], 0.072f, 0.065f, 0.072f, upper, hr, hs, shL, 0.7f, chest, 0.3f);
    solidBall(m, W[shR], 0.072f, 0.065f, 0.072f, upper, hr, hs, shR, 0.7f, chest, 0.3f);

    // Arms — clear tubes
    sf::Color sleeve = catcher ? kGearDeep : kJerseyDeep;
    solidLimb(m, shL, elL, W[shL], W[elL], 0.052f, 0.046f, sleeve, rings, segs);
    solidLimb(m, elL, wrL, W[elL], W[wrL], 0.044f, 0.036f, kSkin, rings, segs);
    solidLimb(m, shR, elR, W[shR], W[elR], 0.052f, 0.046f, sleeve, rings, segs);
    solidLimb(m, elR, wrR, W[elR], W[wrR], 0.044f, 0.036f, kSkin, rings, segs);

    // Hands
    solidBall(m, W[palmR], 0.036f, 0.030f, 0.040f, kSkinDeep, 7, 10, palmR, 0.75f, wrR, 0.25f);
    solidBall(m, jl(G, palmR, 0.0f, -0.028f, 0.015f), 0.026f, 0.016f, 0.028f, kSkin, 5, 8, palmR, 1.0f);

    // Mitt (distinct glove shape)
    solidBall(m, W[palmL], 0.070f, 0.082f, 0.048f, kMitt, hr, hs, palmL, 0.8f, wrL, 0.2f);
    solidBall(m, jl(G, palmL, 0.0f, 0.032f, 0.02f), 0.055f, 0.040f, 0.036f, kMitt, 7, 10, palmL, 1.0f);
    solidBall(m, jl(G, palmL, -0.038f, 0.01f, 0.0f), 0.030f, 0.042f, 0.028f, kMittDeep, 5, 8, palmL, 1.0f);
    solidBall(m, W[wrL], 0.034f, 0.028f, 0.034f, kMittDeep, 5, 8, wrL, 0.7f, palmL, 0.3f);

    // Neck
    solidLimb(m, neck, head, W[neck], W[head], 0.040f, 0.042f, kSkin, 6, segs);

    // Head — ROUND skull (not elongated)
    const float headR = 0.105f;
    solidBall(m, W[head], headR, headR * 1.05f, headR * 0.98f, kSkin, hr, hs, head, 1.0f);
    // Chin
    solidBall(m, jl(G, head, 0.0f, -0.04f, 0.03f), 0.055f, 0.045f, 0.050f, kSkin, 6, 10, head, 1.0f);
    // Ears
    solidBall(m, jl(G, head, -0.10f, 0.0f, 0.0f), 0.016f, 0.024f, 0.014f, kSkinDeep, 4, 6, head, 1.0f);
    solidBall(m, jl(G, head, 0.10f, 0.0f, 0.0f), 0.016f, 0.024f, 0.014f, kSkinDeep, 4, 6, head, 1.0f);
    // Hair under cap
    solidBall(m, jl(G, head, 0.0f, 0.055f, -0.02f), 0.085f, 0.028f, 0.075f, kHair, 6, 10, head, 1.0f);

    if (catcher) {
        solidBall(m, jl(G, head, 0.0f, 0.04f, -0.02f), 0.115f, 0.072f, 0.115f, kGear, hr, hs, head, 1.0f);
        solidBall(m, jl(G, head, 0.0f, 0.0f, 0.12f), 0.068f, 0.062f, 0.016f, kGearDeep, 6, 10, head, 1.0f);
    } else {
        // Cap: flat crown ON TOP of skull only
        solidBall(m, jl(G, head, 0.0f, 0.078f, -0.01f), 0.095f, 0.032f, 0.095f, kCap, hr, hs, head, 1.0f);
        solidBall(m, jl(G, head, 0.0f, 0.055f, -0.005f), 0.098f, 0.014f, 0.098f, kCapDeep, 5, 10, head, 1.0f);
        // Bill: thin plate in front of forehead (local +Z of head)
        solidBall(m, jl(G, head, 0.0f, 0.045f, 0.095f), 0.055f, 0.012f, 0.042f, kCapDeep, 5, 10, head, 1.0f);
        solidBall(m, jl(G, head, 0.0f, 0.042f, 0.125f), 0.040f, 0.008f, 0.018f, kCap, 4, 8, head, 1.0f);
        // Logo
        solidBall(m, jl(G, head, 0.0f, 0.065f, 0.050f), 0.014f, 0.010f, 0.008f, kAccent, 4, 6, head, 1.0f);
    }

    (void)toeR;
    (void)toeL;
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
        global[i] = (p >= 0 && p < n) ? global[p] * joints[i].localRest : joints[i].localRest;
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
    out.vertices.resize(bindVertices.size());
    out.vertexNormals.resize(bindVertices.size());
    out.triangles = triangles;
    out.triangleColors.resize(triangles.size());
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

    for (int t = 0; t < static_cast<int>(triangles.size()); t++) {
        const Triangle3D& tri = triangles[t];
        const sf::Color& ca = vertColor[tri.a];
        const sf::Color& cb = vertColor[tri.b];
        const sf::Color& cc = vertColor[tri.c];
        out.triangleColors[t] = sf::Color(
            static_cast<std::uint8_t>((ca.r + cb.r + cc.r) / 3),
            static_cast<std::uint8_t>((ca.g + cb.g + cc.g) / 3),
            static_cast<std::uint8_t>((ca.b + cb.b + cc.b) / 3)
        );
    }

    // Triangle normals for raster; keep skinned vertex normals.
    out.rebuildNormals();
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

SkinnedModel3D SkinnedModel3D::makeProceduralPitcher(int detail) {
    return makeHumanoid(false, detail);
}

SkinnedModel3D SkinnedModel3D::makeProceduralCatcher(int detail) {
    return makeHumanoid(true, detail);
}
