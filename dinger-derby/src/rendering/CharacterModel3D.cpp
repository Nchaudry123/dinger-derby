#include "CharacterModel3D.h"

#include <algorithm>
#include <cmath>
#include <sstream>

// ============================================================================
// CharacterModel3D — prominent product athlete (v3 silhouette)
// ----------------------------------------------------------------------------
// Game-readable baseball body: bold silhouette, punchy team colors, solid
// volumes that read at stadium camera distance. Joint names unchanged.
//
//  1. Multi-bone limbs + 3-spine torso (Spine → Spine2 → Chest).
//  2. Hard mid-shaft skin weights (no melted elbows/knees).
//  3. Bind = natural stand, arms hang (−Y), slight outward hang, soft knees.
//  4. Limb lengths fixed for tests: UA 0.325, FA 0.265, thigh 0.43, shin 0.405.
//  5. Twist bones isolate roll (throw ER, swing IR, lead-leg plant).
// ============================================================================

namespace CharacterModel3D {
namespace {

// High-contrast outdoor palette — characters pop against grass/dirt/sky.
const sf::Color kSkin(236, 184, 148);
const sf::Color kSkinDeep(204, 152, 118);
const sf::Color kSkinLight(248, 208, 176);
const sf::Color kSkinShadow(168, 122, 94);
const sf::Color kJersey(255, 255, 255);
const sf::Color kJerseyDeep(196, 208, 228);
const sf::Color kPants(24, 32, 52);
const sf::Color kPantsDeep(14, 20, 34);
const sf::Color kPantsLight(56, 68, 92);
const sf::Color kCap(12, 36, 96);           // navy team cap
const sf::Color kCapDeep(6, 20, 58);
const sf::Color kAccent(232, 28, 48);       // hot red piping / number
const sf::Color kAccentLite(255, 90, 70);
const sf::Color kCleat(18, 18, 24);
const sf::Color kSole(48, 48, 56);
const sf::Color kMitt(176, 108, 58);        // warm glove leather
const sf::Color kMittDeep(128, 72, 36);
const sf::Color kMittPad(198, 138, 88);
const sf::Color kGear(28, 48, 88);          // catcher navy armor
const sf::Color kGearDeep(16, 30, 56);
const sf::Color kGearLite(48, 72, 120);
const sf::Color kSock(252, 252, 255);
const sf::Color kBelt(18, 18, 24);
const sf::Color kHair(28, 18, 14);
const sf::Color kHairLite(48, 32, 24);
const sf::Color kEye(12, 10, 10);
const sf::Color kEyeWhite(248, 246, 242);
const sf::Color kUndershirt(48, 92, 180);   // bold blue batting sleeves
const sf::Color kUndershirtDeep(32, 64, 140);
const sf::Color kPantsStripe(232, 232, 240);

// ~1.78 m male. Limb lengths locked for skinned_model_test.
constexpr float kPi = 3.14159265f;
constexpr float kHU = 1.78f / 8.0f;
constexpr float kHeight = 1.78f;
constexpr float kHeadR = kHU * 0.56f;         // larger game-readable head
constexpr float kHipY = 4.12f * kHU;
constexpr float kShoulderY = 6.50f * kHU;
constexpr float kHeadY = kHeight - kHeadR;
constexpr float kUpperArm = 0.325f;           // test lock
constexpr float kForearm = 0.265f;            // test lock
constexpr float kHand = 0.100f;
constexpr float kThigh = 0.430f;
constexpr float kShin = 0.405f;
// Bold athletic silhouette — wide shoulders, taper waist.
constexpr float kShoulderHalf = 0.195f;
constexpr float kWaistR = 0.112f;
constexpr float kChestR = 0.168f;

Vector3 safeNorm(const Vector3& v, const Vector3& fb = Vector3(0.0f, 1.0f, 0.0f)) {
    float m = v.magnitude();
    return m > 1e-6f ? v * (1.0f / m) : fb;
}
float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
float smooth01(float t) {
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
float lerp(float a, float b, float t) { return a + (b - a) * t; }
Vector3 lerpV(const Vector3& a, const Vector3& b, float t) { return a + (b - a) * t; }
Quaternion eul(float rx, float ry, float rz) { return Quaternion::fromEulerXYZ(rx, ry, rz); }

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
        v.joints[0] = j0; v.weights[0] = 1.0f;
        v.joints[1] = v.joints[2] = v.joints[3] = 0;
        v.weights[1] = v.weights[2] = v.weights[3] = 0.0f;
        return;
    }
    float inv = 1.0f / s;
    v.joints[0] = j0; v.weights[0] = w0 * inv;
    v.joints[1] = j1; v.weights[1] = w1 * inv;
    v.joints[2] = j2; v.weights[2] = w2 * inv;
    v.joints[3] = 0;  v.weights[3] = 0.0f;
}

std::vector<Matrix4> restGlobals(const SkinnedModel3D& m) {
    const int n = static_cast<int>(m.joints.size());
    std::vector<Matrix4> G(n, Matrix4::identity());
    for (int i = 0; i < n; i++) {
        int p = m.joints[i].parent;
        G[i] = (p >= 0) ? G[p] * m.joints[i].localRest : m.joints[i].localRest;
    }
    return G;
}

Vector3 worldOf(const std::vector<Matrix4>& G, int j) {
    return G[j].transformPoint(Vector3());
}

// ── mesh primitives ─────────────────────────────────────────────────────

void ball(
    SkinnedModel3D& m,
    const Vector3& c,
    float rx, float ry, float rz,
    sf::Color color,
    int rings, int segs,
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
            float th = (static_cast<float>(s) / segs) * kPi * 2.0f;
            Vector3 n(std::cos(th) * rr, y, std::sin(th) * rr);
            SkinVertex sv;
            sv.position = c + Vector3(n.x * rx, n.y * ry, n.z * rz);
            sv.normal = safeNorm(Vector3(n.x / rx, n.y / ry, n.z / rz));
            sv.color = color;
            setW(sv, j0, w0, j1, w1, j2, w2);
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
            if (ring != 0) addTri(m, a, c0, b);
            if (ring != rings - 1) addTri(m, b, c0, d);
        }
    }
}

// Continuous tube along a joint chain.
// blendHalf: half-width of the soft weight band at each joint, as a fraction
// of the full path (0.05–0.08 = hard game-rig skinning; 0.20+ = melted mush).
// Real-time character guides: keep mid-shaft 100% one bone; only the elbow/
// shoulder rings share weights (3ds Max / Blender arm-skinning practice).
void boneChain(
    SkinnedModel3D& m,
    const std::vector<Vector3>& pts,   // world bind positions along chain
    const std::vector<int>& joints,    // joint per control point (same size as pts)
    const std::vector<float>& radii,   // radius at each control point
    sf::Color colA,
    sf::Color colB,
    float colorBreak,
    int rings,
    int segs,
    float blendHalf = 0.07f
) {
    const int n = static_cast<int>(pts.size());
    if (n < 2 || static_cast<int>(joints.size()) != n || static_cast<int>(radii.size()) != n) {
        return;
    }

    std::vector<float> cum(n, 0.0f);
    for (int i = 1; i < n; i++) {
        cum[i] = cum[i - 1] + (pts[i] - pts[i - 1]).magnitude();
    }
    float total = cum.back();
    if (total < 1e-5f) {
        return;
    }
    blendHalf = clampf(blendHalf, 0.02f, 0.25f);

    auto sample = [&](float t, Vector3& pos, Vector3& tan, float& radius, int& jA, int& jB, float& wB) {
        t = clampf(t, 0.0f, 1.0f);
        float target = t * total;
        int seg = 0;
        while (seg < n - 2 && cum[seg + 1] < target) seg++;
        float u = (target - cum[seg]) / std::max(cum[seg + 1] - cum[seg], 1e-6f);
        // Keep path smooth, but do NOT use path-u as skin weight directly.
        float uPath = smooth01(u);
        pos = lerpV(pts[seg], pts[seg + 1], uPath);
        tan = safeNorm(pts[seg + 1] - pts[seg]);
        if (seg + 2 < n) {
            tan = safeNorm(tan * (1.0f - uPath) + safeNorm(pts[seg + 2] - pts[seg + 1]) * uPath);
        }
        radius = lerp(radii[seg], radii[seg + 1], uPath);
        jA = joints[seg];
        jB = joints[seg + 1];

        // Hard mid-shaft weights: only a narrow band around the joint blends.
        // This is what prevents the "melted blob" when the elbow flexes hard.
        float half = blendHalf;
        if (u < 0.5f - half) {
            wB = 0.0f;
        } else if (u > 0.5f + half) {
            wB = 1.0f;
        } else {
            float s = (u - (0.5f - half)) / (2.0f * half);
            wB = smooth01(s);
        }
        // If both control points share the same joint, force rigid.
        if (jA == jB) {
            wB = 0.0f;
        }
    };

    Vector3 p0, t0; float r0; int ja0, jb0; float wb0;
    sample(0.0f, p0, t0, r0, ja0, jb0, wb0);
    Vector3 up = std::fabs(t0.y) < 0.92f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 side = safeNorm(t0.cross(up));
    Vector3 fwd = safeNorm(side.cross(t0));

    int base = static_cast<int>(m.bindVertices.size());
    for (int i = 0; i <= rings; i++) {
        float t = static_cast<float>(i) / static_cast<float>(rings);
        Vector3 center, tan; float radius; int jA, jB; float wB;
        sample(t, center, tan, radius, jA, jB, wB);

        fwd = safeNorm(fwd - tan * fwd.dot(tan));
        if (fwd.magnitude() < 0.2f) {
            up = std::fabs(tan.y) < 0.92f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
            side = safeNorm(tan.cross(up));
            fwd = safeNorm(side.cross(tan));
        } else {
            side = safeNorm(tan.cross(fwd));
            fwd = safeNorm(side.cross(tan));
        }

        // Mild end soften so segments meet without a hard disk; keep most radius.
        if (t < 0.02f) radius *= 0.85f + 0.15f * (t / 0.02f);
        if (t > 0.98f) radius *= 0.85f + 0.15f * ((1.0f - t) / 0.02f);

        float mix = smooth01((t - (colorBreak - 0.08f)) / 0.16f);
        mix = clampf(mix, 0.0f, 1.0f);
        sf::Color col(
            static_cast<std::uint8_t>(lerp(colA.r, colB.r, mix)),
            static_cast<std::uint8_t>(lerp(colA.g, colB.g, mix)),
            static_cast<std::uint8_t>(lerp(colA.b, colB.b, mix))
        );

        for (int s = 0; s < segs; s++) {
            float ang = (static_cast<float>(s) / segs) * kPi * 2.0f;
            float rx = radius * (1.0f + 0.08f * std::cos(ang * 2.0f));
            float ry = radius * (1.0f - 0.06f * std::cos(ang * 2.0f));
            Vector3 n = safeNorm(side * std::cos(ang) + fwd * std::sin(ang));
            SkinVertex v;
            v.position = center + side * (std::cos(ang) * rx) + fwd * (std::sin(ang) * ry);
            v.normal = n;
            v.color = col;
            setW(v, jA, 1.0f - wB, jB, wB);
            m.bindVertices.push_back(v);
        }
    }

    for (int i = 0; i < rings; i++) {
        for (int s = 0; s < segs; s++) {
            int s1 = (s + 1) % segs;
            int p0i = base + i * segs + s;
            int p1i = base + i * segs + s1;
            int p2i = base + (i + 1) * segs + s;
            int p3i = base + (i + 1) * segs + s1;
            addTri(m, p0i, p2i, p1i);
            addTri(m, p1i, p2i, p3i);
        }
    }
}

// ── animation helpers ───────────────────────────────────────────────────

void pushRot(
    AnimationClip& clip, int joint,
    const std::vector<float>& times,
    const std::vector<Quaternion>& rots
) {
    if (joint < 0 || times.size() != rots.size() || times.empty()) return;
    AnimChannel ch;
    ch.jointIndex = joint;
    ch.path = AnimChannel::Rotation;
    ch.interp = AnimChannel::Linear;
    ch.times = times;
    for (const Quaternion& q : rots) {
        Quaternion n = q.normalized();
        ch.values.push_back(n.x); ch.values.push_back(n.y);
        ch.values.push_back(n.z); ch.values.push_back(n.w);
    }
    clip.channels.push_back(std::move(ch));
}

void pushPos(
    AnimationClip& clip, int joint,
    const std::vector<float>& times,
    const std::vector<Vector3>& positions
) {
    if (joint < 0 || times.size() != positions.size() || times.empty()) return;
    AnimChannel ch;
    ch.jointIndex = joint;
    ch.path = AnimChannel::Translation;
    ch.interp = AnimChannel::Linear;
    ch.times = times;
    for (const Vector3& p : positions) {
        ch.values.push_back(p.x); ch.values.push_back(p.y); ch.values.push_back(p.z);
    }
    clip.channels.push_back(std::move(ch));
}

std::vector<Quaternion> holdQ(const Quaternion& q, int n) {
    return std::vector<Quaternion>(static_cast<size_t>(n), q);
}
std::vector<Vector3> holdP(const Vector3& p, int n) {
    return std::vector<Vector3>(static_cast<size_t>(n), p);
}

// Arm rotation helpers for hang-down bind (−Y bone).
// shoulder: rx = forward raise, rz = out to side (+L / −R), ry = twist
// elbow: rx = bend (positive flexes forearm up toward bicep when hanging)

void attachClips(SkinnedModel3D& m, Role role) {
    auto J = [&](const char* n) { return m.findJoint(n); };
    const float hipY = m.joints[J("Hips")].restTranslation.y;
    const Vector3 hipRestT = m.joints[J("Hips")].restTranslation;

    // ── REST: pure bind (arms straight) ─────────────────────────────────
    {
        AnimationClip clip;
        clip.name = "rest";
        clip.duration = 1.0f;
        pushRot(clip, J("Hips"), {0.0f, 1.0f}, holdQ(eul(0, 0, 0), 2));
        m.clips.push_back(std::move(clip));
    }

    // ── IDLE: breathing + weight shift, arms stay nearly straight ───────
    {
        AnimationClip clip;
        clip.name = "idle";
        clip.duration = 2.8f;
        std::vector<float> t = {0.0f, 0.7f, 1.4f, 2.1f, 2.8f};
        int n = 5;
        pushPos(clip, J("Hips"), t, {
            hipRestT,
            hipRestT + Vector3(0.008f, 0.006f, 0),
            hipRestT + Vector3(0, 0.010f, 0),
            hipRestT + Vector3(-0.008f, 0.006f, 0),
            hipRestT
        });
        pushRot(clip, J("Hips"), t, {
            eul(0, 0, 0), eul(0, 0.03f, 0), eul(0, 0, 0), eul(0, -0.03f, 0), eul(0, 0, 0)
        });
        pushRot(clip, J("Spine"), t, {
            eul(0.02f, 0, 0), eul(0.03f, 0.02f, 0), eul(0.035f, 0, 0), eul(0.03f, -0.02f, 0), eul(0.02f, 0, 0)
        });
        pushRot(clip, J("Spine2"), t, {
            eul(0.015f, 0, 0), eul(0.025f, 0.015f, 0), eul(0.03f, 0, 0), eul(0.025f, -0.015f, 0), eul(0.015f, 0, 0)
        });
        pushRot(clip, J("Chest"), t, {
            eul(0.01f, 0, 0), eul(0.02f, 0.015f, 0), eul(0.025f, 0, 0), eul(0.02f, -0.015f, 0), eul(0.01f, 0, 0)
        });
        pushRot(clip, J("Head"), t, {
            eul(0, 0, 0), eul(0, 0.04f, 0), eul(-0.02f, 0, 0), eul(0, -0.04f, 0), eul(0, 0, 0)
        });
        // Arms: microscopic settle only (keep outward sign)
        pushRot(clip, J("Shoulder_L"), t, holdQ(eul(0.02f, 0, -0.04f), n));
        pushRot(clip, J("Shoulder_R"), t, holdQ(eul(0.02f, 0, +0.04f), n));
        pushRot(clip, J("Elbow_L"), t, {
            eul(0.12f, 0, 0), eul(0.14f, 0, 0), eul(0.13f, 0, 0), eul(0.14f, 0, 0), eul(0.12f, 0, 0)
        });
        pushRot(clip, J("Elbow_R"), t, {
            eul(0.12f, 0, 0), eul(0.14f, 0, 0), eul(0.13f, 0, 0), eul(0.14f, 0, 0), eul(0.12f, 0, 0)
        });
        pushRot(clip, J("Hip_L"), t, {
            eul(0.02f, 0, 0.02f), eul(0.03f, 0, 0.03f), eul(0.02f, 0, 0.02f), eul(0.03f, 0, 0.02f), eul(0.02f, 0, 0.02f)
        });
        pushRot(clip, J("Hip_R"), t, {
            eul(0.02f, 0, -0.02f), eul(0.03f, 0, -0.02f), eul(0.02f, 0, -0.02f), eul(0.03f, 0, -0.03f), eul(0.02f, 0, -0.02f)
        });
        pushRot(clip, J("Knee_L"), t, holdQ(eul(0.06f, 0, 0), n));
        pushRot(clip, J("Knee_R"), t, holdQ(eul(0.06f, 0, 0), n));
        // Quiet femoral roll so idle isn't a frozen tube.
        pushRot(clip, J("ThighTwist_L"), t, {
            eul(0, 0.01f, 0), eul(0, 0.02f, 0), eul(0, 0.01f, 0), eul(0, 0.015f, 0), eul(0, 0.01f, 0)
        });
        pushRot(clip, J("ThighTwist_R"), t, {
            eul(0, -0.01f, 0), eul(0, -0.02f, 0), eul(0, -0.01f, 0), eul(0, -0.015f, 0), eul(0, -0.01f, 0)
        });
        m.clips.push_back(std::move(clip));
    }

    // Arm convention (hang bind, bone along −Y):
    //   R_z(θ)*(0,-1,0) = (sin θ, −cos θ, 0)
    //   Right arm out to +X  => θ = +π/2
    //   Left  arm out to −X  => θ = −π/2
    //   Forward raise (both) => negative rx (toward +Z)

    // ── T-POSE ──────────────────────────────────────────────────────────
    {
        AnimationClip clip;
        clip.name = "tpose";
        clip.duration = 1.0f;
        std::vector<float> t = {0.0f, 1.0f};
        pushRot(clip, J("Shoulder_L"), t, holdQ(eul(0.0f, 0.0f, -1.55f), 2));
        pushRot(clip, J("Shoulder_R"), t, holdQ(eul(0.0f, 0.0f, +1.55f), 2));
        pushRot(clip, J("Elbow_L"), t, holdQ(eul(0.05f, 0, 0), 2));
        pushRot(clip, J("Elbow_R"), t, holdQ(eul(0.05f, 0, 0), 2));
        pushRot(clip, J("Wrist_L"), t, holdQ(eul(0, 0, 0), 2));
        pushRot(clip, J("Wrist_R"), t, holdQ(eul(0, 0, 0), 2));
        pushRot(clip, J("Spine"), t, holdQ(eul(0, 0, 0), 2));
        pushRot(clip, J("Hip_L"), t, holdQ(eul(0, 0, 0.05f), 2));
        pushRot(clip, J("Hip_R"), t, holdQ(eul(0, 0, -0.05f), 2));
        m.clips.push_back(std::move(clip));
    }

    // ── ARMS OUT (45°) ──────────────────────────────────────────────────
    {
        AnimationClip clip;
        clip.name = "arms_out";
        clip.duration = 1.0f;
        std::vector<float> t = {0.0f, 1.0f};
        pushRot(clip, J("Shoulder_L"), t, holdQ(eul(0.0f, 0.0f, -0.75f), 2));
        pushRot(clip, J("Shoulder_R"), t, holdQ(eul(0.0f, 0.0f, +0.75f), 2));
        pushRot(clip, J("Elbow_L"), t, holdQ(eul(0.15f, 0, 0), 2));
        pushRot(clip, J("Elbow_R"), t, holdQ(eul(0.15f, 0, 0), 2));
        m.clips.push_back(std::move(clip));
    }

    // ── WAVE — right arm raises to the SIDE (+X), then elbows wave ──────
    {
        AnimationClip clip;
        clip.name = "wave";
        clip.duration = 2.0f;
        std::vector<float> t = {0.0f, 0.35f, 0.65f, 0.95f, 1.25f, 1.55f, 2.0f};
        pushRot(clip, J("Spine"), t, {
            eul(0, 0, 0), eul(0.03f, 0.06f, 0), eul(0.03f, 0.06f, 0),
            eul(0.03f, 0.06f, 0), eul(0.03f, 0.06f, 0), eul(0.03f, 0.06f, 0), eul(0, 0, 0)
        });
        pushRot(clip, J("Chest"), t, {
            eul(0, 0, 0), eul(0.02f, 0.08f, 0), eul(0.02f, 0.08f, 0),
            eul(0.02f, 0.08f, 0), eul(0.02f, 0.08f, 0), eul(0.02f, 0.08f, 0), eul(0, 0, 0)
        });
        pushRot(clip, J("Head"), t, {
            eul(0, 0, 0), eul(0, -0.12f, 0), eul(0, -0.12f, 0),
            eul(0, -0.12f, 0), eul(0, -0.12f, 0), eul(0, -0.12f, 0), eul(0, 0, 0)
        });
        pushRot(clip, J("Clavicle_R"), t, {
            eul(0, 0, 0), eul(0.04f, 0, 0.10f), eul(0.04f, 0, 0.10f),
            eul(0.04f, 0, 0.10f), eul(0.04f, 0, 0.10f), eul(0.04f, 0, 0.10f), eul(0, 0, 0)
        });
        // rz > 0 lifts right arm out to +X; slight −rx brings it forward for a wave
        pushRot(clip, J("Shoulder_R"), t, {
            eul(0.02f, 0, 0.04f),
            eul(-0.25f, 0.10f, 1.35f),
            eul(-0.20f, 0.20f, 1.40f),
            eul(-0.22f, 0.00f, 1.32f),
            eul(-0.20f, 0.20f, 1.40f),
            eul(-0.22f, 0.00f, 1.32f),
            eul(0.02f, 0, 0.04f)
        });
        pushRot(clip, J("Elbow_R"), t, {
            eul(0.12f, 0, 0),
            eul(1.10f, 0, 0),
            eul(0.80f, 0.15f, 0),
            eul(1.15f, -0.12f, 0),
            eul(0.80f, 0.15f, 0),
            eul(1.10f, 0, 0),
            eul(0.12f, 0, 0)
        });
        pushRot(clip, J("Wrist_R"), t, {
            eul(0, 0, 0),
            eul(0.15f, 0, 0.2f),
            eul(-0.20f, 0, -0.25f),
            eul(0.20f, 0, 0.25f),
            eul(-0.20f, 0, -0.25f),
            eul(0.10f, 0, 0.1f),
            eul(0, 0, 0)
        });
        pushRot(clip, J("Shoulder_L"), t, holdQ(eul(0.02f, 0, -0.04f), 7));
        pushRot(clip, J("Elbow_L"), t, holdQ(eul(0.12f, 0, 0), 7));
        m.clips.push_back(std::move(clip));
    }

    // ── THROW PREVIEW — ball in glove → windup → throw to plate ────────
    // Story: (0-4) ball locked in mitt · (5) hand break pulls ball free ·
    // (6-7) stiff cock · (8-9) fire to plate · (10+) follow.
    // Stiff-arm rule: PRIMARY motion on Shoulder + Elbow + Wrist only.
    // UpperArm / HumTwist / Forearm / ProTwist stay SMALL so limbs don't
    // look like noodles. Hang-bind: −rx raise, +rz open R, ry = twist.
    {
        AnimationClip clip;
        clip.name = "throw_preview";
        clip.duration = 2.20f;
        // 0 set · 1 rocker · 2 lift · 3 peak kick · 4 balance · 5 BREAK ·
        // 6 stride cock · 7 plant · 8 accel · 9 RELEASE · 10 follow ·
        // 11 decel · 12 finish · 13 settle
        std::vector<float> t = {
            0.00f, 0.18f, 0.36f, 0.52f, 0.64f, 0.78f,
            0.92f, 1.04f, 1.14f, 1.22f, 1.42f, 1.64f, 1.88f, 2.20f
        };
        const int n = static_cast<int>(t.size());

        // COM: rock back, tall on kick, drive toward plate (+Z).
        pushPos(clip, J("Hips"), t, {
            hipRestT + Vector3(0.00f, -0.01f,  0.00f),
            hipRestT + Vector3(0.00f, -0.03f, -0.04f),
            hipRestT + Vector3(0.00f,  0.02f, -0.01f),
            hipRestT + Vector3(0.00f,  0.04f,  0.00f),
            hipRestT + Vector3(0.00f,  0.04f,  0.00f),
            hipRestT + Vector3(0.00f,  0.00f,  0.05f),
            hipRestT + Vector3(0.00f, -0.04f,  0.14f),
            hipRestT + Vector3(0.00f, -0.07f,  0.22f),
            hipRestT + Vector3(0.00f, -0.08f,  0.27f),
            hipRestT + Vector3(0.00f, -0.08f,  0.30f),
            hipRestT + Vector3(0.00f, -0.05f,  0.24f),
            hipRestT + Vector3(0.00f, -0.02f,  0.14f),
            hipRestT + Vector3(0.00f,  0.00f,  0.05f),
            hipRestT
        });

        // Sideways set → open toward plate. Stay closed through plant.
        pushRot(clip, J("Hips"), t, {
            eul(0.05f, -1.20f, 0.00f),
            eul(0.07f, -1.30f, -0.03f),
            eul(0.04f, -1.36f, -0.03f),
            eul(0.02f, -1.38f, -0.02f),
            eul(0.02f, -1.38f, -0.02f),
            eul(0.05f, -1.22f,  0.00f),  // break still side-on
            eul(0.10f, -0.90f,  0.03f),
            eul(0.14f, -0.50f,  0.04f),  // plant closed
            eul(0.16f,  0.00f,  0.05f),  // hips fire
            eul(0.16f,  0.38f,  0.05f),  // RELEASE
            eul(0.14f,  0.72f,  0.04f),
            eul(0.10f,  0.40f,  0.02f),
            eul(0.05f,  0.14f,  0.01f),
            eul(0.02f,  0.03f,  0.00f)
        });

        pushRot(clip, J("Spine"), t, {
            eul(0.05f, -0.06f, 0), eul(0.06f, -0.12f, -0.02f), eul(0.04f, -0.16f, -0.02f),
            eul(0.03f, -0.18f, -0.02f), eul(0.03f, -0.18f, -0.02f), eul(0.05f, -0.20f, -0.01f),
            eul(0.10f, -0.26f, 0.01f), eul(0.13f, -0.28f, 0.02f), eul(0.15f, -0.04f, 0.04f),
            eul(0.16f,  0.26f, 0.04f), eul(0.13f,  0.42f, 0.03f), eul(0.09f,  0.20f, 0.02f),
            eul(0.05f,  0.07f, 0.01f), eul(0.02f,  0.02f, 0)
        });
        // Spine2 mid corkscrew — lag Spine, lead Chest for fluid trunk whip.
        pushRot(clip, J("Spine2"), t, {
            eul(0.045f, -0.055f, 0), eul(0.055f, -0.11f, -0.02f), eul(0.04f, -0.15f, -0.02f),
            eul(0.03f, -0.17f, -0.02f), eul(0.03f, -0.17f, -0.02f), eul(0.045f, -0.21f, -0.01f),
            eul(0.085f, -0.28f, 0.005f), eul(0.12f, -0.30f, 0.02f), eul(0.145f, -0.05f, 0.04f),
            eul(0.155f,  0.28f, 0.045f), eul(0.12f,  0.44f, 0.035f), eul(0.08f,  0.21f, 0.02f),
            eul(0.045f,  0.07f, 0.01f), eul(0.02f,  0.02f, 0)
        });
        pushRot(clip, J("Chest"), t, {
            eul(0.04f, -0.05f, 0), eul(0.05f, -0.10f, -0.02f), eul(0.04f, -0.14f, -0.02f),
            eul(0.03f, -0.16f, -0.02f), eul(0.03f, -0.16f, -0.02f), eul(0.04f, -0.22f, -0.01f),
            eul(0.07f, -0.30f, 0), eul(0.11f, -0.32f, 0.02f), eul(0.14f, -0.06f, 0.04f),
            eul(0.15f,  0.30f, 0.05f), eul(0.11f,  0.46f, 0.04f), eul(0.07f,  0.22f, 0.02f),
            eul(0.04f,  0.07f, 0.01f), eul(0.02f,  0.02f, 0)
        });
        pushRot(clip, J("Head"), t, {
            eul(-0.04f, 0.82f, 0), eul(-0.05f, 0.88f, 0), eul(-0.06f, 0.90f, 0),
            eul(-0.06f, 0.90f, 0), eul(-0.06f, 0.88f, 0), eul(-0.04f, 0.68f, 0),
            eul(-0.02f, 0.42f, 0), eul(0.00f, 0.22f, 0), eul(0.04f, 0.06f, 0),
            eul(0.07f, 0.00f, 0), eul(0.05f, -0.10f, 0), eul(0.02f, -0.04f, 0),
            eul(0, 0, 0), eul(0, 0, 0)
        });

        // Mild clavicle only — big shrugs read as rubber shoulders.
        pushRot(clip, J("Clavicle_R"), t, {
            eul(0.02f, 0, 0.04f), eul(0.03f, 0, 0.05f), eul(0.03f, 0, 0.05f),
            eul(0.03f, 0, 0.05f), eul(0.03f, 0, 0.05f), eul(0.00f, -0.04f, 0.08f),
            eul(-0.04f, -0.08f, 0.10f), eul(-0.06f, -0.10f, 0.12f), eul(-0.02f, -0.04f, 0.08f),
            eul(0.08f, 0.06f, 0.06f), eul(0.04f, 0.03f, 0.03f), eul(0.02f, 0.01f, 0.02f),
            eul(0.02f, 0, 0.02f), eul(0.02f, 0, 0.02f)
        });
        pushRot(clip, J("Clavicle_L"), t, {
            eul(0.02f, 0, -0.04f), eul(0.03f, 0, -0.05f), eul(0.03f, 0, -0.05f),
            eul(0.03f, 0, -0.05f), eul(0.03f, 0, -0.05f), eul(0.05f, 0.03f, -0.07f),
            eul(0.06f, 0.04f, -0.08f), eul(0.07f, 0.05f, -0.08f), eul(0.05f, 0.03f, -0.05f),
            eul(0.03f, 0.02f, -0.03f), eul(0.03f, 0.01f, -0.03f), eul(0.02f, 0, -0.02f),
            eul(0.02f, 0, -0.02f), eul(0.02f, 0, -0.02f)
        });

        // ── THROW ARM — stiff primary chain ─────────────────────────────
        // Keys 0-4: R hand DEEP in glove (ball hidden in mitt pocket).
        // Key 5: BREAK — peel ball free, start cock path as one rigid limb.
        // Shoulder carries elevation/abduction; elbow is a clean hinge.
        pushRot(clip, J("Shoulder_R"), t, {
            eul(-0.62f,  0.28f,  0.28f),  // 0 set — hand deep in mitt
            eul(-0.64f,  0.28f,  0.28f),  // 1
            eul(-0.66f,  0.30f,  0.30f),  // 2 lift (still in glove)
            eul(-0.68f,  0.30f,  0.30f),  // 3 kick
            eul(-0.68f,  0.30f,  0.30f),  // 4 balance HOLD glove box
            eul(-1.20f, -0.02f,  0.65f),  // 5 BREAK — pull ball out/up
            eul(-1.80f, -0.12f,  0.70f),  // 6 stride cock (unit lift)
            eul(-2.15f, -0.22f,  0.66f),  // 7 plant high elbow
            eul(-1.70f, -0.06f,  0.30f),  // 8 accel — drive forward
            eul(-1.35f,  0.02f,  0.14f),  // 9 RELEASE high ¾
            eul(-0.68f,  0.12f,  0.78f),  // 10 follow across
            eul(-0.32f,  0.08f,  0.42f),
            eul(-0.10f,  0.03f,  0.14f),
            eul( 0.02f,  0.00f,  0.04f)
        });
        // Intermediate bones: nearly rigid (tiny assist only).
        pushRot(clip, J("UpperArm_R"), t, {
            eul(-0.04f, 0.04f, 0), eul(-0.04f, 0.04f, 0), eul(-0.04f, 0.05f, 0),
            eul(-0.04f, 0.05f, 0), eul(-0.04f, 0.05f, 0), eul(-0.06f, -0.08f, 0),
            eul(-0.08f, -0.18f, 0), eul(-0.10f, -0.22f, 0), eul(-0.05f, -0.08f, 0),
            eul(-0.02f, 0.04f, 0), eul(-0.02f, 0.08f, 0), eul(-0.01f, 0.04f, 0),
            eul(0, 0, 0), eul(0, 0, 0)
        });
        pushRot(clip, J("HumTwist_R"), t, {
            eul(0, 0.08f, 0), eul(0, 0.08f, 0), eul(0, 0.10f, 0), eul(0, 0.10f, 0),
            eul(0, 0.10f, 0), eul(0, -0.15f, 0), eul(0, -0.35f, 0), eul(0, -0.48f, 0),
            eul(0, -0.12f, 0), eul(0, 0.12f, 0), eul(0, 0.22f, 0), eul(0, 0.10f, 0),
            eul(0, 0.02f, 0), eul(0, 0, 0)
        });
        // Elbow: clean hinge. Set flexed in glove; cock stays bent; release extends.
        pushRot(clip, J("Elbow_R"), t, {
            eul(1.15f, 0, 0),  // set deep in glove
            eul(1.15f, 0, 0),
            eul(1.18f, 0, 0),
            eul(1.20f, 0, 0),
            eul(1.20f, 0, 0),
            eul(1.28f, 0, 0),  // break still bent
            eul(1.35f, 0, 0),  // cock
            eul(1.38f, 0, 0),  // plant — athletic flex, not collapsed
            eul(0.55f, 0, 0),  // accel extend
            eul(0.08f, 0, 0),  // RELEASE nearly straight
            eul(0.05f, 0, 0),
            eul(0.15f, 0, 0),
            eul(0.18f, 0, 0),
            eul(0.12f, 0, 0)
        });
        pushRot(clip, J("Forearm_R"), t, holdQ(eul(0.02f, 0.02f, 0), n));
        // Pronation: mild lag → release snap (not a hose twist).
        pushRot(clip, J("ProTwist_R"), t, {
            eul(0, 0.05f, 0), eul(0, 0.05f, 0), eul(0, 0.05f, 0), eul(0, 0.05f, 0),
            eul(0, 0.05f, 0), eul(0, 0.12f, 0), eul(0, 0.18f, 0), eul(0, 0.22f, 0),
            eul(0, 0.02f, 0), eul(0, -0.40f, 0), eul(0, -0.22f, 0), eul(0, -0.08f, 0),
            eul(0, -0.02f, 0), eul(0, 0, 0)
        });
        pushRot(clip, J("Wrist_R"), t, {
            eul(0.10f, 0.02f, 0.04f), eul(0.10f, 0.02f, 0.04f), eul(0.12f, 0.02f, 0.05f),
            eul(0.12f, 0.02f, 0.05f), eul(0.12f, 0.02f, 0.05f), eul(0.22f, 0.04f, 0.08f),
            eul(0.28f, 0.05f, 0.10f), eul(0.32f, 0.06f, 0.12f), eul(0.10f, 0.02f, 0.04f),
            eul(-0.35f, -0.06f, -0.08f), eul(-0.18f, -0.03f, -0.04f), eul(-0.06f, 0, 0),
            eul(0, 0, 0), eul(0, 0, 0)
        });
        pushRot(clip, J("Palm_R"), t, {
            eul(0.05f, 0, 0), eul(0.05f, 0, 0), eul(0.06f, 0, 0), eul(0.06f, 0, 0),
            eul(0.06f, 0, 0), eul(0.10f, 0.02f, 0), eul(0.12f, 0.03f, 0), eul(0.14f, 0.04f, 0),
            eul(0.05f, 0.01f, 0), eul(-0.10f, -0.02f, 0), eul(-0.05f, 0, 0),
            eul(-0.02f, 0, 0), eul(0, 0, 0), eul(0, 0, 0)
        });
        // Ball nestled deep in glove pocket through balance, then rides palm out.
        const Vector3 ballInGlove(0.00f, 0.006f, 0.018f);
        const Vector3 ballInHand(0.00f, -0.014f, 0.032f);
        const Vector3 ballRelease(0.00f, -0.020f, 0.040f);
        pushPos(clip, J("Ball"), t, {
            ballInGlove, ballInGlove, ballInGlove, ballInGlove, ballInGlove,
            ballInHand, ballInHand, ballInHand, ballInHand,
            ballRelease,  // release — ball still on palm at release frame
            ballRelease + Vector3(0, -0.02f, 0.08f), // start leaving hand
            ballRelease + Vector3(0, -0.05f, 0.22f),
            ballRelease + Vector3(0, -0.08f, 0.40f),
            ballRelease + Vector3(0, -0.10f, 0.55f)
        });
        // Fingers: wrap ball in glove → firm grip on cock → open at release.
        auto fingerGrip = [&](const char* name, float inGlove, float grip) {
            pushRot(clip, J(name), t, {
                eul(inGlove, 0, 0), eul(inGlove, 0, 0), eul(inGlove, 0, 0), eul(inGlove, 0, 0),
                eul(inGlove, 0, 0), eul(grip, 0, 0), eul(grip + 0.05f, 0, 0), eul(grip + 0.08f, 0, 0),
                eul(grip * 0.4f, 0, 0), eul(0.04f, 0, 0), eul(0.08f, 0, 0), eul(0.10f, 0, 0),
                eul(0.06f, 0, 0), eul(0.04f, 0, 0)
            });
        };
        fingerGrip("Index_R", 0.55f, 0.50f);
        fingerGrip("Middle_R", 0.58f, 0.55f);
        fingerGrip("Ring_R", 0.55f, 0.52f);
        fingerGrip("Pinky_R", 0.48f, 0.45f);
        pushRot(clip, J("Thumb_R"), t, {
            eul(0.25f, 0.18f, 0.30f), eul(0.25f, 0.18f, 0.30f), eul(0.25f, 0.18f, 0.30f),
            eul(0.25f, 0.18f, 0.30f), eul(0.25f, 0.18f, 0.30f), eul(0.28f, 0.20f, 0.32f),
            eul(0.30f, 0.22f, 0.34f), eul(0.32f, 0.24f, 0.36f), eul(0.14f, 0.10f, 0.14f),
            eul(0.04f, 0.04f, 0.06f), eul(0.08f, 0.05f, 0.08f), eul(0.10f, 0.04f, 0.06f),
            eul(0.06f, 0.03f, 0.04f), eul(0.04f, 0.02f, 0.02f)
        });

        // ── GLOVE ARM — locked box with throw hand, then target tuck ────
        // Keys 0-4: mitt + ball hand clamped together. Break separates cleanly.
        pushRot(clip, J("Shoulder_L"), t, {
            eul(-0.58f,  0.52f,  0.88f),  // set mitt clamped on ball hand
            eul(-0.60f,  0.52f,  0.86f),
            eul(-0.62f,  0.48f,  0.82f),
            eul(-0.64f,  0.46f,  0.78f),
            eul(-0.64f,  0.46f,  0.78f),  // hold glove box
            eul(-0.45f,  0.20f,  0.25f),  // break — mitt stays brief target
            eul(-0.38f,  0.05f, -0.06f),
            eul(-0.32f, -0.02f, -0.14f),
            eul(-0.28f, -0.04f, -0.12f),
            eul(-0.26f, -0.04f, -0.10f),
            eul(-0.32f, -0.05f, -0.12f),
            eul(-0.36f, -0.06f, -0.14f),
            eul(-0.20f, -0.03f, -0.08f),
            eul(0.02f, 0.00f, -0.04f)
        });
        pushRot(clip, J("UpperArm_L"), t, {
            eul(-0.04f, 0.08f, 0.03f), eul(-0.04f, 0.08f, 0.03f), eul(-0.04f, 0.07f, 0.02f),
            eul(-0.04f, 0.06f, 0.02f), eul(-0.04f, 0.06f, 0.02f), eul(-0.03f, 0.02f, 0),
            eul(-0.02f, 0, 0), eul(-0.02f, 0, 0), eul(-0.02f, 0, 0),
            eul(-0.02f, 0, 0), eul(-0.02f, 0, 0), eul(-0.02f, 0, 0),
            eul(-0.01f, 0, 0), eul(0, 0, 0)
        });
        pushRot(clip, J("HumTwist_L"), t, holdQ(eul(0, 0.08f, 0), n));
        pushRot(clip, J("Elbow_L"), t, {
            eul(1.20f, 0, 0), eul(1.20f, 0, 0), eul(1.22f, 0, 0), eul(1.24f, 0, 0),
            eul(1.24f, 0, 0), eul(0.95f, 0, 0), eul(0.85f, 0, 0), eul(0.78f, 0, 0),
            eul(0.74f, 0, 0), eul(0.70f, 0, 0), eul(0.78f, 0, 0), eul(0.85f, 0, 0),
            eul(0.40f, 0, 0), eul(0.12f, 0, 0)
        });
        pushRot(clip, J("Forearm_L"), t, holdQ(eul(0.02f, 0.03f, 0), n));
        pushRot(clip, J("ProTwist_L"), t, holdQ(eul(0, 0.04f, 0), n));
        pushRot(clip, J("Wrist_L"), t, holdQ(eul(0.08f, 0, 0), n));

        // LEAD LEG (L): peel → high knee → stride to plate → plant brace.
        pushRot(clip, J("Hip_L"), t, {
            eul(0.10f, 0.08f, 0.06f),
            eul(-0.50f, 0.10f, 0.08f),
            eul(-1.10f, 0.12f, 0.10f),
            eul(-1.58f, 0.14f, 0.10f),   // peak kick
            eul(-1.60f, 0.14f, 0.10f),
            eul(-1.10f, 0.10f, 0.06f),
            eul(-0.42f, 0.04f, 0.02f),
            eul(-0.06f, -0.02f, 0.00f),  // plant toward plate
            eul(-0.04f, -0.03f, 0.00f),
            eul(-0.03f, -0.04f, 0.00f),
            eul(-0.03f, -0.04f, 0.00f),
            eul(0.00f, -0.02f, 0.00f),
            eul(0.04f, 0.00f, 0.00f),
            eul(0.05f, 0.00f, 0.02f)
        });
        pushRot(clip, J("ThighTwist_L"), t, {
            eul(0, 0.04f, 0), eul(0, 0.10f, 0), eul(0, 0.14f, 0), eul(0, 0.16f, 0),
            eul(0, 0.16f, 0), eul(0, 0.10f, 0), eul(0, 0.04f, 0), eul(0, -0.04f, 0),
            eul(0, -0.08f, 0), eul(0, -0.10f, 0), eul(0, -0.06f, 0), eul(0, -0.02f, 0),
            eul(0, 0.0f, 0), eul(0, 0.0f, 0)
        });
        pushRot(clip, J("Knee_L"), t, {
            eul(0.12f, 0, 0), eul(0.72f, 0, 0), eul(1.28f, 0, 0), eul(1.62f, 0, 0),
            eul(1.64f, 0, 0), eul(1.15f, 0, 0), eul(0.52f, 0, 0), eul(0.26f, 0, 0),
            eul(0.20f, 0, 0), eul(0.16f, 0, 0), eul(0.14f, 0, 0), eul(0.12f, 0, 0),
            eul(0.10f, 0, 0), eul(0.10f, 0, 0)
        });
        pushRot(clip, J("ShinTwist_L"), t, {
            eul(0, 0.02f, 0), eul(0, 0.06f, 0), eul(0, 0.10f, 0), eul(0, 0.12f, 0),
            eul(0, 0.12f, 0), eul(0, 0.06f, 0), eul(0, 0.02f, 0), eul(0, -0.04f, 0),
            eul(0, -0.08f, 0), eul(0, -0.06f, 0), eul(0, -0.03f, 0), eul(0, 0.0f, 0),
            eul(0, 0.0f, 0), eul(0, 0.0f, 0)
        });
        pushRot(clip, J("Ankle_L"), t, {
            eul(-0.06f, 0, 0), eul(0.32f, 0, 0), eul(0.52f, 0, 0), eul(0.62f, 0, 0),
            eul(0.62f, 0, 0), eul(0.25f, 0, 0), eul(-0.05f, 0, 0), eul(-0.18f, 0, 0),
            eul(-0.16f, 0, 0), eul(-0.14f, 0, 0), eul(-0.12f, 0, 0), eul(-0.08f, 0, 0),
            eul(-0.06f, 0, 0), eul(-0.05f, 0, 0)
        });
        pushRot(clip, J("Toe_L"), t, {
            eul(0.05f, 0, 0), eul(0.24f, 0, 0), eul(0.40f, 0, 0), eul(0.44f, 0, 0),
            eul(0.44f, 0, 0), eul(0.18f, 0, 0), eul(0.05f, 0, 0), eul(-0.05f, 0, 0),
            eul(-0.03f, 0, 0), eul(-0.02f, 0, 0), eul(0.00f, 0, 0), eul(0.02f, 0, 0),
            eul(0.04f, 0, 0), eul(0.05f, 0, 0)
        });

        // PLANT LEG (R): load rubber → drive toward plate.
        pushRot(clip, J("Hip_R"), t, {
            eul(0.16f, 0, -0.06f), eul(0.24f, 0, -0.08f), eul(0.30f, 0, -0.07f),
            eul(0.34f, 0, -0.06f), eul(0.36f, 0, -0.06f), eul(0.42f, 0, -0.02f),
            eul(0.50f, 0, 0.02f), eul(0.52f, 0, 0.04f), eul(0.48f, 0, 0.05f),
            eul(0.42f, 0, 0.06f), eul(0.28f, 0, 0.05f), eul(0.18f, 0, 0.03f),
            eul(0.12f, 0, 0.01f), eul(0.08f, 0, -0.02f)
        });
        pushRot(clip, J("ThighTwist_R"), t, {
            eul(0, -0.04f, 0), eul(0, -0.06f, 0), eul(0, -0.08f, 0), eul(0, -0.08f, 0),
            eul(0, -0.08f, 0), eul(0, -0.04f, 0), eul(0, 0.02f, 0), eul(0, 0.06f, 0),
            eul(0, 0.10f, 0), eul(0, 0.12f, 0), eul(0, 0.06f, 0), eul(0, 0.02f, 0),
            eul(0, 0.0f, 0), eul(0, 0.0f, 0)
        });
        pushRot(clip, J("Knee_R"), t, {
            eul(0.24f, 0, 0), eul(0.34f, 0, 0), eul(0.40f, 0, 0), eul(0.44f, 0, 0),
            eul(0.48f, 0, 0), eul(0.55f, 0, 0), eul(0.60f, 0, 0), eul(0.56f, 0, 0),
            eul(0.48f, 0, 0), eul(0.42f, 0, 0), eul(0.32f, 0, 0), eul(0.24f, 0, 0),
            eul(0.18f, 0, 0), eul(0.12f, 0, 0)
        });
        pushRot(clip, J("ShinTwist_R"), t, {
            eul(0, -0.02f, 0), eul(0, -0.04f, 0), eul(0, -0.05f, 0), eul(0, -0.05f, 0),
            eul(0, -0.05f, 0), eul(0, -0.02f, 0), eul(0, 0.02f, 0), eul(0, 0.05f, 0),
            eul(0, 0.08f, 0), eul(0, 0.10f, 0), eul(0, 0.05f, 0), eul(0, 0.02f, 0),
            eul(0, 0.0f, 0), eul(0, 0.0f, 0)
        });
        pushRot(clip, J("Ankle_R"), t, {
            eul(-0.10f, 0, 0), eul(-0.16f, 0, 0), eul(-0.18f, 0, 0), eul(-0.20f, 0, 0),
            eul(-0.22f, 0, 0), eul(-0.12f, 0, 0), eul(0.04f, 0, 0), eul(0.12f, 0, 0),
            eul(0.16f, 0, 0), eul(0.18f, 0, 0), eul(0.10f, 0, 0), eul(0.02f, 0, 0),
            eul(-0.04f, 0, 0), eul(-0.06f, 0, 0)
        });
        pushRot(clip, J("Toe_R"), t, {
            eul(0.05f, 0, 0), eul(0.08f, 0, 0), eul(0.10f, 0, 0), eul(0.12f, 0, 0),
            eul(0.12f, 0, 0), eul(0.06f, 0, 0), eul(-0.04f, 0, 0), eul(-0.12f, 0, 0),
            eul(-0.16f, 0, 0), eul(-0.18f, 0, 0), eul(-0.08f, 0, 0), eul(0.00f, 0, 0),
            eul(0.03f, 0, 0), eul(0.05f, 0, 0)
        });

        m.clips.push_back(std::move(clip));
    }

    // ── CROUCH / CATCHER READY — athletic squat + mitt target ───────────
    {
        AnimationClip clip;
        clip.name = "crouch";
        clip.duration = 1.0f;
        std::vector<float> t = {0.0f, 1.0f};
        const bool isCatcher = role == Role::Catcher;
        float cy = isCatcher ? m.joints[J("Hips")].restTranslation.y : 0.60f;
        pushPos(clip, J("Hips"), t, holdP(Vector3(0, cy, isCatcher ? 0.03f : 0.04f), 2));
        pushRot(clip, J("Spine"), t, holdQ(eul(isCatcher ? 0.22f : 0.28f, 0, 0), 2));
        pushRot(clip, J("Chest"), t, holdQ(eul(isCatcher ? 0.08f : 0.10f, 0, 0), 2));
        pushRot(clip, J("Head"), t, holdQ(eul(isCatcher ? -0.04f : -0.08f, 0, 0), 2));
        if (!isCatcher) {
            pushRot(clip, J("Hip_L"), t, holdQ(eul(0.85f, 0.06f, 0.12f), 2));
            pushRot(clip, J("Hip_R"), t, holdQ(eul(0.85f, -0.06f, -0.12f), 2));
            pushRot(clip, J("Knee_L"), t, holdQ(eul(1.45f, 0, 0), 2));
            pushRot(clip, J("Knee_R"), t, holdQ(eul(1.45f, 0, 0), 2));
            pushRot(clip, J("Ankle_L"), t, holdQ(eul(-0.25f, 0, 0), 2));
            pushRot(clip, J("Ankle_R"), t, holdQ(eul(-0.25f, 0, 0), 2));
        } else {
            // Catcher bind is already crouched — small settle only.
            pushRot(clip, J("Hip_L"), t, holdQ(eul(0.12f, 0.04f, 0.08f), 2));
            pushRot(clip, J("Hip_R"), t, holdQ(eul(0.12f, -0.04f, -0.08f), 2));
            pushRot(clip, J("Knee_L"), t, holdQ(eul(0.18f, 0, 0), 2));
            pushRot(clip, J("Knee_R"), t, holdQ(eul(0.18f, 0, 0), 2));
            pushRot(clip, J("Ankle_L"), t, holdQ(eul(-0.08f, 0, 0), 2));
            pushRot(clip, J("Ankle_R"), t, holdQ(eul(-0.08f, 0, 0), 2));
        }
        // Mitt up as target (L), bare hand near thigh/knee (R).
        pushRot(clip, J("Shoulder_L"), t, holdQ(eul(-0.85f, -0.15f, -0.55f), 2));
        pushRot(clip, J("Elbow_L"), t, holdQ(eul(1.15f, 0, 0), 2));
        pushRot(clip, J("Wrist_L"), t, holdQ(eul(0.10f, 0, 0.08f), 2));
        pushRot(clip, J("Shoulder_R"), t, holdQ(eul(-0.35f, 0.10f, 0.25f), 2));
        pushRot(clip, J("Elbow_R"), t, holdQ(eul(0.85f, 0, 0), 2));
        pushRot(clip, J("Wrist_R"), t, holdQ(eul(0.08f, 0, 0), 2));
        m.clips.push_back(std::move(clip));
    }

    // ── CATCHER IDLE — crouch breathe + glove micro-frame ───────────────
    if (role == Role::Catcher) {
        AnimationClip clip;
        clip.name = "catcher_idle";
        clip.duration = 2.4f;
        std::vector<float> t = {0.0f, 0.6f, 1.2f, 1.8f, 2.4f};
        const int n = 5;
        const float cy = m.joints[J("Hips")].restTranslation.y;
        pushPos(clip, J("Hips"), t, {
            Vector3(0, cy, 0.03f),
            Vector3(0, cy + 0.008f, 0.03f),
            Vector3(0, cy + 0.012f, 0.03f),
            Vector3(0, cy + 0.006f, 0.03f),
            Vector3(0, cy, 0.03f)
        });
        pushRot(clip, J("Spine"), t, {
            eul(0.20f, 0, 0), eul(0.22f, 0.02f, 0), eul(0.23f, 0, 0),
            eul(0.22f, -0.02f, 0), eul(0.20f, 0, 0)
        });
        pushRot(clip, J("Chest"), t, {
            eul(0.08f, 0, 0), eul(0.09f, 0.015f, 0), eul(0.10f, 0, 0),
            eul(0.09f, -0.015f, 0), eul(0.08f, 0, 0)
        });
        pushRot(clip, J("Head"), t, {
            eul(-0.04f, 0, 0), eul(-0.03f, 0.04f, 0), eul(-0.05f, 0, 0),
            eul(-0.03f, -0.04f, 0), eul(-0.04f, 0, 0)
        });
        // Glove target holds the zone, slight frame.
        pushRot(clip, J("Shoulder_L"), t, {
            eul(-0.85f, -0.15f, -0.55f), eul(-0.88f, -0.12f, -0.52f),
            eul(-0.86f, -0.16f, -0.56f), eul(-0.88f, -0.18f, -0.54f),
            eul(-0.85f, -0.15f, -0.55f)
        });
        pushRot(clip, J("Elbow_L"), t, {
            eul(1.15f, 0, 0), eul(1.18f, 0, 0), eul(1.14f, 0, 0),
            eul(1.17f, 0, 0), eul(1.15f, 0, 0)
        });
        pushRot(clip, J("Wrist_L"), t, {
            eul(0.10f, 0, 0.08f), eul(0.12f, 0.02f, 0.10f), eul(0.08f, -0.02f, 0.06f),
            eul(0.12f, 0.02f, 0.10f), eul(0.10f, 0, 0.08f)
        });
        pushRot(clip, J("Shoulder_R"), t, holdQ(eul(-0.35f, 0.10f, 0.25f), n));
        pushRot(clip, J("Elbow_R"), t, holdQ(eul(0.85f, 0, 0), n));
        pushRot(clip, J("Hip_L"), t, holdQ(eul(0.12f, 0.04f, 0.08f), n));
        pushRot(clip, J("Hip_R"), t, holdQ(eul(0.12f, -0.04f, -0.08f), n));
        pushRot(clip, J("Knee_L"), t, holdQ(eul(0.18f, 0, 0), n));
        pushRot(clip, J("Knee_R"), t, holdQ(eul(0.18f, 0, 0), n));
        m.clips.push_back(std::move(clip));
    }

    // ── RECEIVE — mitt tracks a pitch through the zone (subtle) ────────
    if (role == Role::Catcher) {
        AnimationClip clip;
        clip.name = "receive";
        clip.duration = 1.2f;
        std::vector<float> t = {0.0f, 0.25f, 0.55f, 0.85f, 1.2f};
        const float cy = m.joints[J("Hips")].restTranslation.y;
        pushPos(clip, J("Hips"), t, holdP(Vector3(0, cy, 0.03f), 5));
        pushRot(clip, J("Spine"), t, {
            eul(0.20f, 0, 0), eul(0.18f, 0.04f, 0), eul(0.16f, 0.02f, 0),
            eul(0.18f, -0.02f, 0), eul(0.20f, 0, 0)
        });
        pushRot(clip, J("Chest"), t, {
            eul(0.08f, 0, 0), eul(0.06f, 0.05f, 0), eul(0.05f, 0.03f, 0),
            eul(0.06f, -0.02f, 0), eul(0.08f, 0, 0)
        });
        pushRot(clip, J("Head"), t, {
            eul(-0.02f, 0, 0), eul(0.02f, 0.06f, 0), eul(0.04f, 0.02f, 0),
            eul(0.00f, -0.02f, 0), eul(-0.02f, 0, 0)
        });
        // Glove rises into the zone then softens the catch.
        pushRot(clip, J("Shoulder_L"), t, {
            eul(-0.85f, -0.15f, -0.55f),
            eul(-1.05f, -0.10f, -0.48f),  // present target
            eul(-0.95f, -0.08f, -0.42f),  // receive
            eul(-0.80f, -0.12f, -0.50f),  // soft hands in
            eul(-0.85f, -0.15f, -0.55f)
        });
        pushRot(clip, J("Elbow_L"), t, {
            eul(1.15f, 0, 0), eul(1.05f, 0, 0), eul(0.95f, 0, 0),
            eul(1.10f, 0, 0), eul(1.15f, 0, 0)
        });
        pushRot(clip, J("Wrist_L"), t, {
            eul(0.10f, 0, 0.08f), eul(0.05f, 0, 0.12f), eul(-0.08f, 0, 0.06f),
            eul(0.12f, 0, 0.04f), eul(0.10f, 0, 0.08f)
        });
        pushRot(clip, J("Shoulder_R"), t, {
            eul(-0.35f, 0.10f, 0.25f), eul(-0.45f, 0.12f, 0.30f),
            eul(-0.50f, 0.10f, 0.28f), eul(-0.40f, 0.10f, 0.26f),
            eul(-0.35f, 0.10f, 0.25f)
        });
        pushRot(clip, J("Elbow_R"), t, {
            eul(0.85f, 0, 0), eul(0.95f, 0, 0), eul(1.00f, 0, 0),
            eul(0.90f, 0, 0), eul(0.85f, 0, 0)
        });
        m.clips.push_back(std::move(clip));
    }

    // ── WALK CYCLE — full locomotion (was completely missing) ───────────
    {
        AnimationClip clip;
        clip.name = "walk";
        clip.duration = 1.0f;
        // 0 contact L · 1 mid · 2 contact R · 3 mid · 4 loop
        std::vector<float> t = {0.0f, 0.25f, 0.50f, 0.75f, 1.0f};
        pushPos(clip, J("Hips"), t, {
            hipRestT + Vector3(0, 0.00f, 0),
            hipRestT + Vector3(0, 0.015f, 0),
            hipRestT + Vector3(0, 0.00f, 0),
            hipRestT + Vector3(0, 0.015f, 0),
            hipRestT
        });
        pushRot(clip, J("Hips"), t, {
            eul(0.02f, 0.04f, 0), eul(0.02f, 0, 0), eul(0.02f, -0.04f, 0),
            eul(0.02f, 0, 0), eul(0.02f, 0.04f, 0)
        });
        pushRot(clip, J("Spine"), t, {
            eul(0.04f, -0.03f, 0), eul(0.04f, 0, 0), eul(0.04f, 0.03f, 0),
            eul(0.04f, 0, 0), eul(0.04f, -0.03f, 0)
        });
        pushRot(clip, J("Chest"), t, {
            eul(0.02f, -0.04f, 0), eul(0.02f, 0, 0), eul(0.02f, 0.04f, 0),
            eul(0.02f, 0, 0), eul(0.02f, -0.04f, 0)
        });
        // Legs
        pushRot(clip, J("Hip_L"), t, {
            eul(0.45f, 0, 0.02f), eul(0.05f, 0, 0.02f), eul(-0.40f, 0, 0.02f),
            eul(0.05f, 0, 0.02f), eul(0.45f, 0, 0.02f)
        });
        pushRot(clip, J("Knee_L"), t, {
            eul(0.15f, 0, 0), eul(0.85f, 0, 0), eul(0.20f, 0, 0),
            eul(0.10f, 0, 0), eul(0.15f, 0, 0)
        });
        pushRot(clip, J("Ankle_L"), t, {
            eul(-0.10f, 0, 0), eul(0.15f, 0, 0), eul(-0.05f, 0, 0),
            eul(-0.12f, 0, 0), eul(-0.10f, 0, 0)
        });
        pushRot(clip, J("Hip_R"), t, {
            eul(-0.40f, 0, -0.02f), eul(0.05f, 0, -0.02f), eul(0.45f, 0, -0.02f),
            eul(0.05f, 0, -0.02f), eul(-0.40f, 0, -0.02f)
        });
        pushRot(clip, J("Knee_R"), t, {
            eul(0.20f, 0, 0), eul(0.10f, 0, 0), eul(0.15f, 0, 0),
            eul(0.85f, 0, 0), eul(0.20f, 0, 0)
        });
        pushRot(clip, J("Ankle_R"), t, {
            eul(-0.05f, 0, 0), eul(-0.12f, 0, 0), eul(-0.10f, 0, 0),
            eul(0.15f, 0, 0), eul(-0.05f, 0, 0)
        });
        // Opposite arm swing (rx swings forward/back while hanging)
        pushRot(clip, J("Shoulder_L"), t, {
            eul(-0.40f, 0, -0.06f), eul(0.05f, 0, -0.05f), eul(0.40f, 0, -0.06f),
            eul(0.05f, 0, -0.05f), eul(-0.40f, 0, -0.06f)
        });
        pushRot(clip, J("Elbow_L"), t, {
            eul(0.45f, 0, 0), eul(0.30f, 0, 0), eul(0.35f, 0, 0),
            eul(0.30f, 0, 0), eul(0.45f, 0, 0)
        });
        pushRot(clip, J("Shoulder_R"), t, {
            eul(0.40f, 0, 0.06f), eul(0.05f, 0, 0.05f), eul(-0.40f, 0, 0.06f),
            eul(0.05f, 0, 0.05f), eul(0.40f, 0, 0.06f)
        });
        pushRot(clip, J("Elbow_R"), t, {
            eul(0.35f, 0, 0), eul(0.30f, 0, 0), eul(0.45f, 0, 0),
            eul(0.30f, 0, 0), eul(0.35f, 0, 0)
        });
        m.clips.push_back(std::move(clip));
    }

    (void)role;
}


// ── main builder (v2 athletic rebuild) ──────────────────────────────────

SkinnedModel3D buildInternal(Role role, Detail detailLevel) {
    SkinnedModel3D m;
    int d = static_cast<int>(detailLevel);
    // Dense enough for product demos; still CPU-skin friendly.
    int rings = d >= 2 ? 24 : (d >= 1 ? 16 : 10);
    int segs = d >= 2 ? 26 : (d >= 1 ? 18 : 12);
    int hr = d >= 2 ? 12 : 9;
    int hs = d >= 2 ? 18 : 14;

    const bool catcher = role == Role::Catcher;
    const bool pitcher = role == Role::Pitcher;
    const bool athlete = role == Role::Athlete;

    // ── skeleton: multi-spine + multi-bone limbs ────────────────────────
    int root = addJoint(m, "Root", -1, Vector3());
    int hips = addJoint(m, "Hips", root, Vector3(0.0f, kHipY, 0.0f));
    int spine = addJoint(m, "Spine", hips, Vector3(0.0f, 0.115f, 0.016f));
    int spine2 = addJoint(m, "Spine2", spine, Vector3(0.0f, 0.125f, 0.014f));
    int chest = addJoint(m, "Chest", spine2, Vector3(0.0f, 0.145f, 0.012f));
    int neck = addJoint(m, "Neck", chest, Vector3(0.0f, 0.125f, 0.012f));
    int head = addJoint(m, "Head", neck, Vector3(0.0f, 0.150f, 0.020f));

    // Broad clavicle shelf — hero shoulder width.
    int clavL = addJoint(m, "Clavicle_L", chest, Vector3(-0.065f, 0.110f, 0.020f));
    int clavR = addJoint(m, "Clavicle_R", chest, Vector3(0.065f, 0.110f, 0.020f));

    const float uaA = kUpperArm * 0.48f;
    const float uaB = kUpperArm * 0.52f;
    const float faA = kForearm * 0.52f;
    const float faB = kForearm * 0.48f;
    const float thA = kThigh * 0.48f;
    const float thB = kThigh * 0.52f;
    const float shA = kShin * 0.52f;
    const float shB = kShin * 0.48f;

    // Soft A-pose hang; shoulders sit wide of the torso.
    int shL = addJoint(m, "Shoulder_L", clavL, Vector3(-kShoulderHalf + 0.02f, 0.0f, 0.0f), eul(0.02f, 0, -0.14f));
    int uaL = addJoint(m, "UpperArm_L", shL, Vector3(0.0f, -uaA, 0.008f));
    int htL = addJoint(m, "HumTwist_L", uaL, Vector3(0.0f, -uaB, 0.008f));
    int elL = addJoint(m, "Elbow_L", htL, Vector3(0.0f, 0.0f, 0.0f), eul(0.06f, 0, 0));
    int faL = addJoint(m, "Forearm_L", elL, Vector3(0.0f, -faA, 0.006f));
    int ptL = addJoint(m, "ProTwist_L", faL, Vector3(0.0f, -faB, 0.004f));
    int wrL = addJoint(m, "Wrist_L", ptL, Vector3(0.0f, 0.0f, 0.0f));
    int palmL = addJoint(m, "Palm_L", wrL, Vector3(0.0f, -kHand * 0.55f, 0.016f));

    int shR = addJoint(m, "Shoulder_R", clavR, Vector3(kShoulderHalf - 0.02f, 0.0f, 0.0f), eul(0.02f, 0, +0.14f));
    int uaR = addJoint(m, "UpperArm_R", shR, Vector3(0.0f, -uaA, 0.008f));
    int htR = addJoint(m, "HumTwist_R", uaR, Vector3(0.0f, -uaB, 0.008f));
    int elR = addJoint(m, "Elbow_R", htR, Vector3(0.0f, 0.0f, 0.0f), eul(0.06f, 0, 0));
    int faR = addJoint(m, "Forearm_R", elR, Vector3(0.0f, -faA, 0.006f));
    int ptR = addJoint(m, "ProTwist_R", faR, Vector3(0.0f, -faB, 0.004f));
    int wrR = addJoint(m, "Wrist_R", ptR, Vector3(0.0f, 0.0f, 0.0f));
    int palmR = addJoint(m, "Palm_R", wrR, Vector3(0.0f, -kHand * 0.55f, 0.018f));

    int fIdxR = addJoint(m, "Index_R", palmR, Vector3(0.013f, -0.032f, 0.011f));
    int fMidR = addJoint(m, "Middle_R", palmR, Vector3(0.004f, -0.036f, 0.013f));
    int fRngR = addJoint(m, "Ring_R", palmR, Vector3(-0.007f, -0.034f, 0.011f));
    int fPnkR = addJoint(m, "Pinky_R", palmR, Vector3(-0.015f, -0.030f, 0.007f));
    int fThmR = addJoint(m, "Thumb_R", palmR, Vector3(0.024f, -0.008f, 0.009f), eul(0.15f, 0.4f, 0.55f));
    int fIdxL = addJoint(m, "Index_L", palmL, Vector3(-0.013f, -0.032f, 0.011f));
    int fMidL = addJoint(m, "Middle_L", palmL, Vector3(-0.004f, -0.036f, 0.013f));
    int fRngL = addJoint(m, "Ring_L", palmL, Vector3(0.007f, -0.034f, 0.011f));
    int fPnkL = addJoint(m, "Pinky_L", palmL, Vector3(0.015f, -0.030f, 0.007f));
    int fThmL = addJoint(m, "Thumb_L", palmL, Vector3(-0.024f, -0.008f, 0.009f), eul(0.15f, -0.4f, -0.55f));
    int ballJ = addJoint(m, "Ball", palmR, Vector3(0.0f, -0.012f, 0.030f));
    (void)fIdxR; (void)fMidR; (void)fRngR; (void)fPnkR; (void)fThmR;
    (void)fIdxL; (void)fMidL; (void)fRngL; (void)fPnkL; (void)fThmL;

    // Legs: wider stance, multi-bone + twist.
    int hipR = addJoint(m, "Hip_R", hips, Vector3(0.105f, -0.028f, 0.0f), eul(0.02f, 0, -0.04f));
    int thR = addJoint(m, "Thigh_R", hipR, Vector3(0.0f, -thA, 0.008f));
    int ttR = addJoint(m, "ThighTwist_R", thR, Vector3(0.0f, -thB, 0.008f));
    int knR = addJoint(m, "Knee_R", ttR, Vector3(0.0f, 0.0f, 0.0f), eul(0.05f, 0, 0));
    int shinR = addJoint(m, "Shin_R", knR, Vector3(0.0f, -shA, 0.005f));
    int stR = addJoint(m, "ShinTwist_R", shinR, Vector3(0.0f, -shB, 0.003f));
    int anR = addJoint(m, "Ankle_R", stR, Vector3(0.0f, 0.0f, 0.0f), eul(-0.04f, 0, 0));
    int toeR = addJoint(m, "Toe_R", anR, Vector3(0.0f, -0.012f, 0.095f));

    int hipL = addJoint(m, "Hip_L", hips, Vector3(-0.105f, -0.028f, 0.0f), eul(0.02f, 0, 0.04f));
    int thL = addJoint(m, "Thigh_L", hipL, Vector3(0.0f, -thA, 0.008f));
    int ttL = addJoint(m, "ThighTwist_L", thL, Vector3(0.0f, -thB, 0.008f));
    int knL = addJoint(m, "Knee_L", ttL, Vector3(0.0f, 0.0f, 0.0f), eul(0.05f, 0, 0));
    int shinL = addJoint(m, "Shin_L", knL, Vector3(0.0f, -shA, 0.005f));
    int stL = addJoint(m, "ShinTwist_L", shinL, Vector3(0.0f, -shB, 0.003f));
    int anL = addJoint(m, "Ankle_L", stL, Vector3(0.0f, 0.0f, 0.0f), eul(-0.04f, 0, 0));
    int toeL = addJoint(m, "Toe_L", anL, Vector3(0.0f, -0.012f, 0.095f));

    if (catcher) {
        m.joints[hips].restTranslation = Vector3(0, 0.50f, 0.05f);
        m.joints[hips].bakeLocalRest();
        m.joints[hipL].restRotation = eul(0.52f, 0.08f, 0.16f);
        m.joints[hipR].restRotation = eul(0.52f, -0.08f, -0.16f);
        m.joints[hipL].bakeLocalRest();
        m.joints[hipR].bakeLocalRest();
        m.joints[thL].restTranslation = Vector3(0.02f, -0.10f, 0.05f);
        m.joints[thR].restTranslation = Vector3(-0.02f, -0.10f, 0.05f);
        m.joints[ttL].restTranslation = Vector3(0.02f, -0.10f, 0.05f);
        m.joints[ttR].restTranslation = Vector3(-0.02f, -0.10f, 0.05f);
        m.joints[thL].bakeLocalRest();
        m.joints[thR].bakeLocalRest();
        m.joints[ttL].bakeLocalRest();
        m.joints[ttR].bakeLocalRest();
        m.joints[knL].restRotation = eul(1.12f, 0, 0);
        m.joints[knR].restRotation = eul(1.12f, 0, 0);
        m.joints[knL].bakeLocalRest();
        m.joints[knR].bakeLocalRest();
        m.joints[shinL].restTranslation = Vector3(0, -0.10f, 0.06f);
        m.joints[shinR].restTranslation = Vector3(0, -0.10f, 0.06f);
        m.joints[stL].restTranslation = Vector3(0, -0.10f, 0.06f);
        m.joints[stR].restTranslation = Vector3(0, -0.10f, 0.06f);
        m.joints[shinL].bakeLocalRest();
        m.joints[shinR].bakeLocalRest();
        m.joints[stL].bakeLocalRest();
        m.joints[stR].bakeLocalRest();
        m.joints[anL].restRotation = eul(-0.18f, 0, 0);
        m.joints[anR].restRotation = eul(-0.18f, 0, 0);
        m.joints[anL].bakeLocalRest();
        m.joints[anR].bakeLocalRest();
        m.joints[spine].restRotation = eul(0.10f, 0, 0);
        m.joints[spine2].restRotation = eul(0.08f, 0, 0);
        m.joints[chest].restRotation = eul(0.05f, 0, 0);
        m.joints[spine].bakeLocalRest();
        m.joints[spine2].bakeLocalRest();
        m.joints[chest].bakeLocalRest();
        m.joints[shL].restRotation = eul(-0.72f, -0.12f, -0.48f);
        m.joints[elL].restRotation = eul(1.05f, 0, 0);
        m.joints[shR].restRotation = eul(-0.28f, 0.08f, 0.20f);
        m.joints[elR].restRotation = eul(0.75f, 0, 0);
        m.joints[shL].bakeLocalRest();
        m.joints[elL].bakeLocalRest();
        m.joints[shR].bakeLocalRest();
        m.joints[elR].bakeLocalRest();
    }

    m.rebuildInverseBindsFromRest();
    auto G = restGlobals(m);
    auto W = [&](int j) { return worldOf(G, j); };

    sf::Color torso = catcher ? kGear : (athlete ? kUndershirt : kJersey);
    sf::Color torsoD = catcher ? kGearDeep : (athlete ? kUndershirtDeep : kJerseyDeep);
    sf::Color sleeve = catcher ? kGearDeep : (athlete ? kSkin : kJerseyDeep);

    // ── LEGS: bold athletic volumes (read at distance) ──────────────────
    auto makeLeg = [&](int jHip, int jTh, int jTt, int jKn, int jShin, int jSt, int jAn, int jToe) {
        Vector3 pH = W(jHip), pTh = W(jTh), pK = W(jKn), pSh = W(jShin), pA = W(jAn), pT = W(jToe);
        boneChain(
            m,
            {pH, pTh, pK + Vector3(0, 0.014f, 0.004f)},
            {jHip, jTh, jTt},
            {0.092f, 0.088f, 0.064f},
            kPants, kPantsDeep, 0.40f,
            rings + 14, segs,
            0.042f
        );
        boneChain(
            m,
            {pK, pSh, pA + Vector3(0, 0.012f, 0)},
            {jKn, jShin, jSt},
            {0.060f, 0.056f, 0.040f},
            kPants, kPantsDeep, 0.52f,
            rings + 12, segs,
            0.042f
        );
        // Outer pant stripe (team flash).
        Vector3 midTh = lerpV(pH, pK, 0.45f) + Vector3(pH.x > 0 ? 0.055f : -0.055f, 0, 0);
        ball(m, midTh, 0.012f, 0.12f, 0.012f, kPantsStripe, 5, 8, jTh, 0.7f, jTt, 0.3f);
        ball(m, pH, 0.070f, 0.064f, 0.068f, kPants, hr + 1, hs, jHip, 0.72f, hips, 0.28f);
        ball(m, pK, 0.056f, 0.050f, 0.054f, kPantsLight, hr, hs - 2, jKn, 0.74f, jTt, 0.13f, jShin, 0.13f);
        // Tall white socks.
        ball(m, pA + Vector3(0, 0.055f, 0), 0.040f, 0.055f, 0.040f, kSock, 8, 12, jAn, 0.75f, jSt, 0.25f);
        ball(m, pA + Vector3(0, 0.028f, 0), 0.038f, 0.028f, 0.038f, kSock, 7, 12, jAn, 0.92f, jSt, 0.08f);
        // Bold cleats.
        ball(m, pT, 0.048f, 0.028f, 0.092f, kCleat, hr, hs - 2, jToe, 0.82f, jAn, 0.18f);
        ball(m, pT + Vector3(0, -0.015f, 0.012f), 0.050f, 0.012f, 0.096f, kSole, 5, 10, jToe, 0.88f, jAn, 0.12f);
        ball(m, pT + Vector3(0, 0.008f, 0.04f), 0.022f, 0.014f, 0.030f, kAccent, 4, 6, jToe, 0.9f, jAn, 0.1f);
    };
    makeLeg(hipR, thR, ttR, knR, shinR, stR, anR, toeR);
    makeLeg(hipL, thL, ttL, knL, shinL, stL, anL, toeL);

    if (catcher) {
        auto shinGuard = [&](int jKn, int jAn) {
            Vector3 pK = W(jKn), pA = W(jAn);
            Vector3 mid = lerpV(pK, pA, 0.45f) + Vector3(0, 0, 0.038f);
            ball(m, pK + Vector3(0, -0.02f, 0.038f), 0.068f, 0.056f, 0.050f, kGear, hr, hs - 2, jKn, 0.88f, jAn, 0.12f);
            ball(m, mid, 0.058f, 0.105f, 0.045f, kGearLite, hr, hs - 2, jKn, 0.40f, jAn, 0.60f);
            ball(m, pA + Vector3(0, 0.04f, 0.032f), 0.050f, 0.048f, 0.040f, kGear, 7, 12, jAn, 0.9f, jKn, 0.1f);
            // Guard rivet accents.
            ball(m, mid + Vector3(0, 0.02f, 0.02f), 0.012f, 0.012f, 0.012f, kAccent, 4, 6, jKn, 0.5f, jAn, 0.5f);
        };
        shinGuard(knL, anL);
        shinGuard(knR, anR);
    }

    // ── PELVIS + TORSO (hero V-torso) ────────────────────────────────────
    ball(m, W(hips), 0.145f, 0.110f, 0.122f, kPants, hr + 1, hs + 2, hips, 1.0f);
    ball(m, W(hips) + Vector3(0, 0.055f, 0), 0.130f, 0.026f, 0.108f, kBelt, 6, 14, hips, 0.92f, spine, 0.08f);
    // Belt buckle flash.
    ball(m, W(hips) + Vector3(0, 0.055f, 0.09f), 0.028f, 0.022f, 0.014f, kAccent, 4, 6, hips, 1.0f);

    boneChain(
        m,
        {
            W(hips) + Vector3(0, 0.04f, 0.005f),
            W(spine),
            W(spine2),
            W(chest),
            W(chest) + Vector3(0, 0.060f, 0.014f)
        },
        {hips, spine, spine2, chest, chest},
        {kWaistR + 0.016f, kWaistR + 0.012f, kChestR * 0.94f, kChestR, kChestR * 0.96f},
        torso, torsoD, 0.46f,
        rings + 16, segs + 4,
        0.050f
    );
    ball(m, W(spine), 0.118f, 0.085f, 0.100f, torso, hr + 1, hs, spine, 0.62f, hips, 0.20f, spine2, 0.18f);
    ball(m, W(spine2), 0.142f, 0.098f, 0.118f, torso, hr + 1, hs, spine2, 0.70f, spine, 0.15f, chest, 0.15f);
    ball(m, W(chest), 0.172f, 0.112f, 0.130f, torso, hr + 2, hs + 2, chest, 0.88f, spine2, 0.12f);
    // Deep pec / jersey front mass.
    ball(m, W(chest) + Vector3(0, -0.012f, 0.055f), 0.140f, 0.092f, 0.080f, torsoD, hr + 1, hs, chest, 1.0f);
    // Traps / collarbone shelf (wide silhouette).
    ball(m, (W(shL) + W(shR)) * 0.5f + Vector3(0, 0.018f, 0.012f),
         0.125f, 0.038f, 0.060f, torso, hr, hs - 2, chest, 0.88f, spine2, 0.12f);
    ball(m, W(clavL), 0.042f, 0.032f, 0.040f, torso, hr - 1, hs - 2, clavL, 0.78f, chest, 0.22f);
    ball(m, W(clavR), 0.042f, 0.032f, 0.040f, torso, hr - 1, hs - 2, clavR, 0.78f, chest, 0.22f);

    if (pitcher) {
        // Large jersey number "17" plate (reads at camera).
        ball(m, G[chest].transformPoint(Vector3(0, 0.02f, 0.112f)), 0.055f, 0.070f, 0.014f, kAccent, 6, 10, chest, 1.0f);
        ball(m, G[chest].transformPoint(Vector3(-0.018f, 0.02f, 0.120f)), 0.014f, 0.055f, 0.008f, kJersey, 4, 6, chest, 1.0f);
        ball(m, G[chest].transformPoint(Vector3(0.018f, 0.02f, 0.120f)), 0.014f, 0.055f, 0.008f, kJersey, 4, 6, chest, 1.0f);
        // Red sleeve cuffs + shoulder piping.
        ball(m, W(uaR) + Vector3(0.01f, 0, 0), 0.056f, 0.022f, 0.056f, kAccent, 5, 10, uaR, 0.9f, htR, 0.1f);
        ball(m, W(uaL) + Vector3(-0.01f, 0, 0), 0.056f, 0.022f, 0.056f, kAccent, 5, 10, uaL, 0.9f, htL, 0.1f);
        ball(m, W(shR) + Vector3(0.02f, 0.02f, 0), 0.040f, 0.018f, 0.040f, kAccentLite, 4, 8, shR, 0.8f, clavR, 0.2f);
        ball(m, W(shL) + Vector3(-0.02f, 0.02f, 0), 0.040f, 0.018f, 0.040f, kAccentLite, 4, 8, shL, 0.8f, clavL, 0.2f);
    }
    if (athlete) {
        // Compression sleeves already blue; add chest band + number.
        ball(m, G[chest].transformPoint(Vector3(0, 0.0f, 0.100f)), 0.048f, 0.060f, 0.012f, kAccent, 6, 10, chest, 1.0f);
        ball(m, W(chest) + Vector3(0, -0.08f, 0.04f), 0.130f, 0.022f, 0.100f, kUndershirtDeep, 5, 12, chest, 0.7f, spine2, 0.3f);
    }
    if (catcher) {
        // Thick chest protector with layered plates + red trim.
        ball(m, W(chest) + Vector3(0, 0.02f, 0.085f), 0.165f, 0.120f, 0.070f, kGear, hr + 1, hs, chest, 1.0f);
        ball(m, W(chest) + Vector3(0, 0.095f, 0.075f), 0.130f, 0.065f, 0.052f, kGearLite, hr, hs - 2, chest, 0.9f, spine2, 0.1f);
        ball(m, W(chest) + Vector3(0, -0.065f, 0.075f), 0.140f, 0.065f, 0.050f, kGearDeep, hr, hs - 2, chest, 0.85f, spine, 0.15f);
        ball(m, W(chest) + Vector3(0, 0.02f, 0.125f), 0.050f, 0.070f, 0.012f, kAccent, 5, 8, chest, 1.0f);
        ball(m, W(shL) + Vector3(0.02f, 0.03f, 0.02f), 0.062f, 0.048f, 0.058f, kGear, hr - 1, hs - 2, shL, 0.7f, clavL, 0.3f);
        ball(m, W(shR) + Vector3(-0.02f, 0.03f, 0.02f), 0.062f, 0.048f, 0.058f, kGear, hr - 1, hs - 2, shR, 0.7f, clavR, 0.3f);
    }

    // ── ARMS: chunky delts / biceps / forearms (hero silhouette) ────────
    auto makeArm = [&](
        int jClav, int jSh, int jUa, int jHt, int jEl, int jFa, int jPt, int jWr, int jPalm,
        bool mitt, bool fingers, bool leftHand
    ) {
        Vector3 pS = W(jSh), pUa = W(jUa), pE = W(jEl), pFa = W(jFa), pW = W(jWr), pP = W(jPalm);
        Vector3 axisU = safeNorm(pE - pS);
        Vector3 axisF = safeNorm(pW - pE);

        const float rDeltoid = 0.066f;
        const float rUpper = 0.058f;
        const float rElbow = 0.046f;
        const float rFore = 0.048f;
        const float rWrist = 0.034f;

        Vector3 u0 = pS + axisU * 0.016f;
        Vector3 u1 = pUa;
        Vector3 u2 = pE - axisU * 0.016f;
        boneChain(
            m, {u0, u1, u2}, {jSh, jUa, jHt},
            {rDeltoid, rUpper, rElbow},
            sleeve, sleeve, 0.5f, rings + 12, segs, 0.036f
        );

        Vector3 f0 = pE + axisF * 0.012f;
        Vector3 f1 = pFa;
        Vector3 f2 = pW - axisF * 0.010f;
        boneChain(
            m, {f0, f1, f2}, {jEl, jFa, jPt},
            {rElbow, rFore, rWrist},
            kSkin, kSkinDeep, 0.55f, rings + 12, segs, 0.036f
        );

        // Oversized deltoid cap — key read at distance.
        ball(m, pS + axisU * 0.012f, 0.064f, 0.058f, 0.064f, sleeve, hr, hs - 2,
             jSh, 0.90f, jClav, 0.10f);
        ball(m, pE, 0.042f, 0.038f, 0.042f, kSkin, hr - 1, hs - 2,
             jEl, 0.84f, jHt, 0.08f, jFa, 0.08f);
        ball(m, pUa + axisU * 0.02f, 0.058f, 0.052f, 0.058f, torsoD, hr - 1, hs - 2,
             jUa, 0.92f, jHt, 0.08f);
        ball(m, pW, 0.032f, 0.028f, 0.032f, kSkinDeep, 7, 12, jWr, 0.90f, jPt, 0.10f);

        if (mitt) {
            // Oversized mitt — iconic baseball prop.
            ball(m, pP, 0.078f, 0.095f, 0.055f, kMitt, hr + 1, hs, jPalm, 0.92f, jWr, 0.08f);
            ball(m, pP + Vector3(0, 0.032f, 0.020f), 0.058f, 0.042f, 0.038f, kMittPad, hr - 1, hs - 2, jPalm, 1.0f);
            ball(m, pP + Vector3(-0.040f, 0.010f, 0), 0.030f, 0.045f, 0.028f, kMittDeep, 6, 10, jPalm, 1.0f);
            ball(m, pP + Vector3(0.038f, 0.008f, 0), 0.028f, 0.040f, 0.026f, kMittDeep, 6, 10, jPalm, 1.0f);
            ball(m, pP + Vector3(0, -0.02f, 0.03f), 0.035f, 0.025f, 0.025f, kMittPad, 5, 8, jPalm, 1.0f);
            ball(m, pW, 0.030f, 0.026f, 0.030f, kMittDeep, 5, 8, jWr, 0.85f, jPalm, 0.15f);
            // Glove webbing highlight.
            ball(m, pP + Vector3(0, 0.04f, 0.03f), 0.018f, 0.014f, 0.018f, kAccentLite, 4, 6, jPalm, 1.0f);
        } else if (fingers) {
            ball(m, pP, 0.034f, 0.018f, 0.042f, kSkinDeep, hr - 1, hs - 2, jPalm, 0.92f, jWr, 0.08f);
            Vector3 dir = safeNorm(pP - pW, Vector3(0, -1, 0));
            Vector3 side = safeNorm(dir.cross(Vector3(0, 0, 1)), Vector3(1, 0, 0));
            const char* fingerNamesR[] = {"Index_R", "Middle_R", "Ring_R", "Pinky_R"};
            const char* fingerNamesL[] = {"Index_L", "Middle_L", "Ring_L", "Pinky_L"};
            const char* const* fingerNames = leftHand ? fingerNamesL : fingerNamesR;
            float sideSign = leftHand ? -1.0f : 1.0f;
            for (int f = 0; f < 4; f++) {
                float lat = (static_cast<float>(f) - 1.5f) * 0.012f * sideSign;
                Vector3 b0 = pP + side * lat + dir * 0.012f;
                Vector3 b1 = b0 + dir * 0.055f;
                int jF = m.findJoint(fingerNames[f]);
                if (jF < 0) jF = jPalm;
                boneChain(m, {b0, b1}, {jPalm, jF}, {0.010f, 0.007f}, kSkin, kSkinDeep, 0.5f, 6, 8, 0.05f);
            }
            int jTh = m.findJoint(leftHand ? "Thumb_L" : "Thumb_R");
            if (jTh < 0) jTh = jPalm;
            Vector3 tb = pP - side * (0.028f * sideSign) + dir * 0.004f;
            Vector3 tt = tb - side * (0.014f * sideSign) + dir * 0.028f;
            boneChain(m, {tb, tt}, {jPalm, jTh}, {0.011f, 0.008f}, kSkin, kSkinDeep, 0.5f, 6, 8, 0.05f);
        }
    };
    makeArm(clavL, shL, uaL, htL, elL, faL, ptL, wrL, palmL, !athlete, athlete, true);
    makeArm(clavR, shR, uaR, htR, elR, faR, ptR, wrR, palmR, false, true, false);
    (void)ballJ;

    // ── NECK + HEAD (large, readable face) ──────────────────────────────
    boneChain(
        m,
        {
            W(chest) + Vector3(0, 0.050f, 0.014f),
            W(neck),
            W(head) + Vector3(0, -kHeadR * 0.78f, 0),
            W(head) + Vector3(0, -kHeadR * 0.48f, 0)
        },
        {chest, neck, head, head},
        {0.062f, 0.052f, 0.056f, 0.064f},
        kSkin, kSkin, 0.5f,
        16, segs
    );
    ball(m, W(neck), 0.056f, 0.058f, 0.056f, kSkin, 10, 14, neck, 0.55f, chest, 0.25f, head, 0.20f);
    // Large skull + jaw for silhouette.
    ball(m, W(head), kHeadR * 0.98f, kHeadR * 1.10f, kHeadR * 0.96f, kSkin, hr + 2, hs + 2, head, 1.0f);
    ball(m, G[head].transformPoint(Vector3(0, -0.062f, 0.032f)), 0.056f, 0.044f, 0.050f, kSkin, 9, 14, head, 1.0f);
    // Ears.
    ball(m, G[head].transformPoint(Vector3(-0.105f, 0, 0)), 0.016f, 0.026f, 0.014f, kSkinDeep, 5, 8, head, 1.0f);
    ball(m, G[head].transformPoint(Vector3(0.105f, 0, 0)), 0.016f, 0.026f, 0.014f, kSkinDeep, 5, 8, head, 1.0f);
    // Brow ridge.
    ball(m, G[head].transformPoint(Vector3(0, 0.028f, 0.088f)), 0.048f, 0.014f, 0.018f, kSkinDeep, 5, 8, head, 1.0f);
    // Nose.
    ball(m, G[head].transformPoint(Vector3(0, -0.005f, 0.108f)), 0.014f, 0.020f, 0.022f, kSkinLight, 5, 8, head, 1.0f);
    // Eyes — white + dark pupil (reads on screen).
    ball(m, G[head].transformPoint(Vector3(-0.032f, 0.016f, 0.092f)), 0.016f, 0.012f, 0.010f, kEyeWhite, 5, 8, head, 1.0f);
    ball(m, G[head].transformPoint(Vector3(0.032f, 0.016f, 0.092f)), 0.016f, 0.012f, 0.010f, kEyeWhite, 5, 8, head, 1.0f);
    ball(m, G[head].transformPoint(Vector3(-0.032f, 0.016f, 0.100f)), 0.008f, 0.008f, 0.006f, kEye, 4, 6, head, 1.0f);
    ball(m, G[head].transformPoint(Vector3(0.032f, 0.016f, 0.100f)), 0.008f, 0.008f, 0.006f, kEye, 4, 6, head, 1.0f);
    // Mouth.
    ball(m, G[head].transformPoint(Vector3(0, -0.048f, 0.090f)), 0.022f, 0.008f, 0.010f, kSkinShadow, 4, 6, head, 1.0f);
    // Full hair volume.
    ball(m, G[head].transformPoint(Vector3(0, 0.065f, -0.010f)), 0.098f, 0.048f, 0.092f, kHair, 10, 14, head, 1.0f);
    ball(m, G[head].transformPoint(Vector3(0, 0.040f, -0.050f)), 0.090f, 0.040f, 0.055f, kHairLite, 8, 12, head, 1.0f);

    if (catcher) {
        // Bulky helmet + thick cage.
        ball(m, G[head].transformPoint(Vector3(0, 0.05f, -0.02f)), 0.132f, 0.095f, 0.128f, kGear, hr + 1, hs + 2, head, 1.0f);
        ball(m, G[head].transformPoint(Vector3(0, 0.025f, 0.110f)), 0.080f, 0.075f, 0.024f, kGearLite, 7, 12, head, 1.0f);
        for (float y : {-0.015f, 0.015f, 0.045f, 0.070f}) {
            ball(m, G[head].transformPoint(Vector3(0, y, 0.132f)), 0.065f, 0.011f, 0.013f, kGearDeep, 4, 8, head, 1.0f);
        }
        for (float x : {-0.035f, -0.012f, 0.012f, 0.035f}) {
            ball(m, G[head].transformPoint(Vector3(x, 0.025f, 0.134f)), 0.010f, 0.058f, 0.012f, kGearDeep, 4, 6, head, 1.0f);
        }
        ball(m, G[head].transformPoint(Vector3(0, -0.09f, 0.065f)), 0.048f, 0.042f, 0.036f, kGearDeep, 6, 10, head, 0.7f, neck, 0.3f);
        // Helmet ear flaps.
        ball(m, G[head].transformPoint(Vector3(-0.09f, 0.0f, 0.02f)), 0.028f, 0.040f, 0.035f, kGear, 5, 8, head, 1.0f);
        ball(m, G[head].transformPoint(Vector3(0.09f, 0.0f, 0.02f)), 0.028f, 0.040f, 0.035f, kGear, 5, 8, head, 1.0f);
    } else if (pitcher) {
        // Tall navy cap + long bill + red button.
        ball(m, G[head].transformPoint(Vector3(0, 0.088f, -0.014f)), 0.108f, 0.038f, 0.108f, kCap, hr + 1, hs + 2, head, 1.0f);
        ball(m, G[head].transformPoint(Vector3(0, 0.060f, -0.006f)), 0.110f, 0.016f, 0.110f, kCapDeep, 6, 12, head, 1.0f);
        ball(m, G[head].transformPoint(Vector3(0, 0.048f, 0.105f)), 0.062f, 0.012f, 0.050f, kCapDeep, 6, 12, head, 1.0f);
        ball(m, G[head].transformPoint(Vector3(0, 0.045f, 0.145f)), 0.048f, 0.010f, 0.022f, kCap, 5, 10, head, 1.0f);
        ball(m, G[head].transformPoint(Vector3(0, 0.095f, 0.02f)), 0.018f, 0.014f, 0.014f, kAccent, 5, 8, head, 1.0f);
        // Cap logo patch.
        ball(m, G[head].transformPoint(Vector3(0, 0.075f, 0.055f)), 0.020f, 0.016f, 0.010f, kAccentLite, 4, 6, head, 1.0f);
    } else {
        // Athlete: headband for identity.
        ball(m, G[head].transformPoint(Vector3(0, 0.055f, 0.02f)), 0.100f, 0.018f, 0.095f, kAccent, 5, 12, head, 1.0f);
    }

    // Soft waist blend pants → jersey.
    ball(m, W(hips) + Vector3(0, 0.072f, 0.014f), 0.132f, 0.050f, 0.105f, torso, hr + 1, hs, hips, 0.38f, spine, 0.62f);

    attachClips(m, role);
    (void)toeL; (void)toeR;
    return m;
}

} // namespace

SkinnedModel3D build(Role role, Detail detail) {
    return buildInternal(role, detail);
}

BuildInfo inspect(const SkinnedModel3D& model) {
    BuildInfo info;
    info.jointCount = static_cast<int>(model.joints.size());
    info.vertexCount = static_cast<int>(model.bindVertices.size());
    info.triangleCount = static_cast<int>(model.triangles.size());
    float maxY = 0, minY = 0;
    for (const SkinVertex& v : model.bindVertices) {
        maxY = std::max(maxY, v.position.y);
        minY = std::min(minY, v.position.y);
    }
    info.heightMeters = maxY - minY;
    std::ostringstream oss;
    oss << info.jointCount << " joints · " << info.vertexCount << " verts · "
        << info.triangleCount << " tris · h≈" << static_cast<int>(info.heightMeters * 100)
        << "cm · " << model.clips.size() << " clips";
    info.summary = oss.str();
    return info;
}

const AnimationClip* findClip(const SkinnedModel3D& model, const std::string& name) {
    return model.findClip(name);
}

std::vector<std::string> clipNames(const SkinnedModel3D& model) {
    std::vector<std::string> names;
    for (const AnimationClip& c : model.clips) names.push_back(c.name);
    return names;
}

const char* roleName(Role role) {
    switch (role) {
        case Role::Athlete: return "Athlete";
        case Role::Pitcher: return "Pitcher";
        case Role::Catcher: return "Catcher";
    }
    return "Athlete";
}

const char* detailName(Detail detail) {
    switch (detail) {
        case Detail::Low: return "Low";
        case Detail::Medium: return "Medium";
        case Detail::High: return "High";
    }
    return "High";
}

} // namespace CharacterModel3D
