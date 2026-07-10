#include "SkinnedModel3D.h"

#include <algorithm>
#include <cmath>

namespace {

const sf::Color kSkin(236, 198, 166);
const sf::Color kSkinDeep(216, 176, 144);
const sf::Color kJersey(252, 253, 255);
const sf::Color kJerseyDeep(228, 232, 240);
const sf::Color kPants(54, 62, 80);
const sf::Color kPantsDeep(44, 50, 66);
const sf::Color kCap(32, 50, 100);
const sf::Color kCapDeep(24, 40, 84);
const sf::Color kAccent(210, 48, 56);
const sf::Color kCleat(34, 34, 42);
const sf::Color kMitt(174, 122, 80);
const sf::Color kMittDeep(138, 92, 54);
const sf::Color kGear(44, 60, 88);
const sf::Color kGearDeep(36, 48, 70);
const sf::Color kSock(250, 250, 255);
const sf::Color kBelt(40, 42, 50);

constexpr float kPi = 3.14159265f;

Vector3 safeNorm(const Vector3& v, const Vector3& fb = Vector3(0.0f, 1.0f, 0.0f)) {
    float m = v.magnitude();
    return m > 1e-6f ? v * (1.0f / m) : fb;
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

// Oriented capsule mesh between joint A and joint B (indices), weighted to both.
void addLimbSegment(
    SkinnedModel3D& m,
    int jointA,
    int jointB,
    const Vector3& worldA,
    const Vector3& worldB,
    float r0,
    float r1,
    sf::Color color,
    int rings,
    int segs
) {
    Vector3 d = worldB - worldA;
    float len = d.magnitude();
    if (len < 1e-4f) {
        return;
    }
    Vector3 axis = d * (1.0f / len);

    // Build orthonormal basis (axis = local Y).
    Vector3 up = std::fabs(axis.y) < 0.9f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
    Vector3 side = safeNorm(axis.cross(up));
    Vector3 fwd = safeNorm(side.cross(axis));

    int base = static_cast<int>(m.bindVertices.size());
    for (int i = 0; i <= rings; i++) {
        float u = static_cast<float>(i) / static_cast<float>(rings);
        float r = r0 + (r1 - r0) * u;
        // Muscle bulge.
        r *= 1.0f + 0.06f * std::sin(u * kPi);
        Vector3 center = worldA + axis * (len * u);
        // Weight blend: near A prefer A, near B prefer B.
        float wB = u * u * (3.0f - 2.0f * u);
        float wA = 1.0f - wB;

        for (int s = 0; s < segs; s++) {
            float ang = (static_cast<float>(s) / segs) * kPi * 2.0f;
            Vector3 n = side * std::cos(ang) + fwd * std::sin(ang);
            SkinVertex v;
            v.position = center + n * r;
            v.normal = n;
            v.color = color;
            v.joints[0] = jointA;
            v.joints[1] = jointB;
            v.joints[2] = 0;
            v.joints[3] = 0;
            v.weights[0] = wA;
            v.weights[1] = wB;
            v.weights[2] = 0.0f;
            v.weights[3] = 0.0f;
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

void addBall(
    SkinnedModel3D& m,
    int joint,
    const Vector3& center,
    float rx,
    float ry,
    float rz,
    sf::Color color,
    int rings,
    int segs
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
            sv.normal = safeNorm(Vector3(n.x / rx, n.y / ry, n.z / rz));
            sv.color = color;
            sv.joints[0] = joint;
            sv.weights[0] = 1.0f;
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

// Compute world rest positions for all joints.
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

SkinnedModel3D makeHumanoid(bool catcher, int detail) {
    SkinnedModel3D m;
    detail = std::clamp(detail, 0, 2);
    // Higher tessellation so skinned limbs stay smooth and continuous.
    int rings = detail >= 2 ? 11 : (detail >= 1 ? 8 : 5);
    int segs = detail >= 2 ? 16 : (detail >= 1 ? 12 : 9);
    int ballR = detail >= 2 ? 12 : (detail >= 1 ? 9 : 7);
    int ballS = detail >= 2 ? 16 : (detail >= 1 ? 12 : 10);

    // ── skeleton ────────────────────────────────────────────────────────
    // RHP: Root is rotated closed so the body stands SIDEWAYS on the rubber
    // (profile toward plate). Head turns back toward home (+Z). Glove=-X, throw=+X.
    const float sideYaw = catcher ? 0.0f : -0.95f; // ~54° closed for pitcher set
    int root = addJoint(
        m, "Root", -1, Vector3(0.0f, 0.0f, 0.0f),
        Quaternion::fromEulerXYZ(0.0f, sideYaw, 0.0f)
    );
    int hips = addJoint(m, "Hips", root, Vector3(0.0f, 0.90f, 0.0f));
    int spine = addJoint(m, "Spine", hips, Vector3(0.0f, 0.14f, 0.02f));
    int chest = addJoint(m, "Chest", spine, Vector3(0.0f, 0.18f, 0.02f));
    int neck = addJoint(m, "Neck", chest, Vector3(0.0f, 0.14f, 0.01f));
    // Look toward plate against closed root (undo most of sideYaw on the head).
    int head = addJoint(
        m, "Head", neck, Vector3(0.0f, 0.12f, 0.03f),
        catcher ? Quaternion::identity()
                : Quaternion::fromEulerXYZ(-0.04f, 0.85f, 0.0f)
    );

    // Arms hang along -Y for clean throw FK.
    int shL = addJoint(m, "Shoulder_L", chest, Vector3(-0.16f, 0.09f, 0.01f));
    int elL = addJoint(m, "Elbow_L", shL, Vector3(0.0f, -0.29f, 0.02f));
    int wrL = addJoint(m, "Wrist_L", elL, Vector3(0.0f, -0.26f, 0.02f));
    int palmL = addJoint(m, "Palm_L", wrL, Vector3(0.0f, -0.05f, 0.04f));

    int shR = addJoint(m, "Shoulder_R", chest, Vector3(0.16f, 0.09f, 0.01f));
    int elR = addJoint(m, "Elbow_R", shR, Vector3(0.0f, -0.29f, 0.02f));
    int wrR = addJoint(m, "Wrist_R", elR, Vector3(0.0f, -0.26f, 0.02f));
    int palmR = addJoint(m, "Palm_R", wrR, Vector3(0.0f, -0.05f, 0.05f));

    // Plant = right (back foot), lead = left (stride foot).
    int hipR = addJoint(m, "Hip_R", hips, Vector3(0.11f, -0.02f, -0.04f));
    int knR = addJoint(m, "Knee_R", hipR, Vector3(0.0f, -0.41f, 0.02f));
    int anR = addJoint(m, "Ankle_R", knR, Vector3(0.0f, -0.39f, 0.0f));
    int toeR = addJoint(m, "Toe_R", anR, Vector3(0.0f, -0.02f, 0.08f));

    int hipL = addJoint(m, "Hip_L", hips, Vector3(-0.11f, -0.02f, 0.03f));
    int knL = addJoint(m, "Knee_L", hipL, Vector3(0.0f, -0.41f, 0.02f));
    int anL = addJoint(m, "Ankle_L", knL, Vector3(0.0f, -0.39f, 0.02f));
    int toeL = addJoint(m, "Toe_L", anL, Vector3(0.0f, -0.02f, 0.08f));

    if (catcher) {
        m.joints[root].restRotation = Quaternion::identity();
        m.joints[root].bakeLocalRest();
        m.joints[hips].restTranslation = Vector3(0.0f, 0.48f, 0.0f);
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

    // ── denser continuous mesh ──────────────────────────────────────────
    auto limb = [&](int ja, int jb, float r0, float r1, sf::Color c) {
        addLimbSegment(m, ja, jb, W[ja], W[jb], r0, r1, c, rings, segs);
    };
    auto jointBall = [&](int j, float r, sf::Color c) {
        addBall(m, j, W[j], r, r * 0.95f, r, c, ballR, ballS);
    };

    // Legs + knee melt balls (keep silhouette intact under big kicks).
    limb(hipR, knR, 0.090f, 0.074f, kPants);
    jointBall(knR, 0.078f, kPants);
    limb(knR, anR, 0.072f, 0.052f, kPantsDeep);
    limb(hipL, knL, 0.090f, 0.074f, kPants);
    jointBall(knL, 0.078f, kPants);
    limb(knL, anL, 0.072f, 0.052f, kPantsDeep);

    // Cleats on toes so feet read during stride.
    addBall(m, toeR, W[toeR], 0.052f, 0.028f, 0.090f, kCleat, ballR, ballS);
    addBall(m, toeL, W[toeL], 0.052f, 0.028f, 0.090f, kCleat, ballR, ballS);
    addBall(m, anR, W[anR] + Vector3(0.0f, -0.012f, 0.02f), 0.058f, 0.014f, 0.070f, kCleat, ballR / 2, ballS / 2);
    addBall(m, anL, W[anL] + Vector3(0.0f, -0.012f, 0.02f), 0.058f, 0.014f, 0.070f, kCleat, ballR / 2, ballS / 2);
    jointBall(anR, 0.040f, kSock);
    jointBall(anL, 0.040f, kSock);

    // Pelvis / belt / waist fill into jersey
    addBall(m, hips, W[hips], 0.160f, 0.115f, 0.125f, kPants, ballR, ballS);
    addBall(m, hips, W[hips] + Vector3(0.0f, 0.05f, 0.0f), 0.145f, 0.026f, 0.112f, kBelt, ballR / 2, ballS / 2);
    jointBall(hipR, 0.092f, kPants);
    jointBall(hipL, 0.092f, kPants);

    // Torso: multi-layer continuous jersey
    limb(hips, spine, 0.145f, 0.138f, kJersey);
    limb(spine, chest, 0.140f, 0.150f, kJersey);
    addBall(m, chest, W[chest] + Vector3(0.0f, 0.02f, 0.04f), 0.138f, 0.100f, 0.105f, kJerseyDeep, ballR, ballS);
    addBall(m, spine, W[spine], 0.135f, 0.090f, 0.110f, kJersey, ballR, ballS);
    if (catcher) {
        addBall(m, chest, W[chest] + Vector3(0.0f, 0.0f, 0.07f), 0.150f, 0.125f, 0.075f, kGear, ballR, ballS);
    } else {
        addBall(m, chest, W[chest] + Vector3(0.0f, 0.0f, 0.11f), 0.018f, 0.055f, 0.012f, kAccent, 5, 8);
    }

    // Delts fused into torso
    sf::Color upper = catcher ? kGear : kJersey;
    jointBall(shL, 0.095f, upper);
    jointBall(shR, 0.095f, upper);
    addBall(m, chest, (W[shL] + W[shR]) * 0.5f + Vector3(0.0f, 0.02f, 0.0f),
            0.12f, 0.06f, 0.08f, upper, ballR, ballS);

    // Arms + elbow melt
    sf::Color sleeve = catcher ? kGearDeep : kJerseyDeep;
    limb(shL, elL, 0.060f, 0.052f, sleeve);
    jointBall(elL, 0.050f, kSkin);
    limb(elL, wrL, 0.050f, 0.040f, kSkin);
    limb(shR, elR, 0.060f, 0.052f, sleeve);
    jointBall(elR, 0.050f, kSkin);
    limb(elR, wrR, 0.050f, 0.040f, kSkin);

    // Hands / mitt
    addBall(m, palmR, W[palmR], 0.040f, 0.034f, 0.044f, kSkinDeep, ballR, ballS);
    addBall(m, palmL, W[palmL], 0.075f, 0.085f, 0.050f, kMitt, ballR, ballS);
    addBall(m, palmL, W[palmL] + Vector3(0.0f, 0.03f, 0.02f), 0.058f, 0.042f, 0.038f, kMittDeep, ballR / 2, ballS / 2);

    // Neck + head
    limb(neck, head, 0.044f, 0.042f, kSkin);
    addBall(m, head, W[head], 0.112f, 0.118f, 0.108f, kSkin, ballR, ballS);
    if (catcher) {
        addBall(m, head, W[head] + Vector3(0.0f, 0.04f, -0.02f), 0.118f, 0.078f, 0.118f, kGear, ballR, ballS);
        addBall(m, head, W[head] + Vector3(0.0f, 0.0f, 0.12f), 0.072f, 0.068f, 0.020f, kGearDeep, ballR / 2, ballS / 2);
    } else {
        // Cap: crown on top; bill along local +Z of head (toward plate after head yaw).
        addBall(m, head, W[head] + Vector3(0.0f, 0.075f, -0.02f), 0.100f, 0.040f, 0.100f, kCap, ballR, ballS);
        addBall(m, head, W[head] + Vector3(0.0f, 0.045f, 0.100f), 0.060f, 0.014f, 0.045f, kCapDeep, ballR / 2, ballS / 2);
        addBall(m, head, W[head] + Vector3(0.0f, 0.040f, 0.130f), 0.042f, 0.010f, 0.024f, kCap, 5, 8);
        addBall(m, head, W[head] + Vector3(0.0f, 0.065f, 0.055f), 0.015f, 0.012f, 0.010f, kAccent, 5, 8);
        addBall(m, head, W[head] + Vector3(0.0f, -0.01f, 0.095f), 0.058f, 0.052f, 0.042f, kSkinDeep, ballR / 2, ballS / 2);
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
    out.vertices.resize(bindVertices.size());
    out.vertexNormals.resize(bindVertices.size());
    out.triangles = triangles;
    out.triangleColors.assign(triangles.size(), sf::Color(220, 220, 220));
    out.edges.clear();

    // Per-triangle colors from first vertex albedo (stable clothing regions).
    // Build vertex colors parallel then assign by tri.
    std::vector<sf::Color> vertColor(bindVertices.size(), sf::Color::White);

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

    out.triangleColors.resize(triangles.size());
    for (int t = 0; t < static_cast<int>(triangles.size()); t++) {
        const Triangle3D& tri = triangles[t];
        // Average albedo of corners.
        auto avg = [](sf::Color a, sf::Color b, sf::Color c) {
            return sf::Color(
                static_cast<std::uint8_t>((a.r + b.r + c.r) / 3),
                static_cast<std::uint8_t>((a.g + b.g + c.g) / 3),
                static_cast<std::uint8_t>((a.b + b.b + c.b) / 3)
            );
        };
        out.triangleColors[t] = avg(vertColor[tri.a], vertColor[tri.b], vertColor[tri.c]);
    }
    out.rebuildNormals();
}

SkinnedModel3D SkinnedModel3D::makeProceduralPitcher(int detail) {
    SkinnedModel3D m = makeHumanoid(false, detail);
    return m;
}

SkinnedModel3D SkinnedModel3D::makeProceduralCatcher(int detail) {
    return makeHumanoid(true, detail);
}
