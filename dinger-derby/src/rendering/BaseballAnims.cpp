#include "BaseballAnims.h"

#include <algorithm>
#include <cmath>

namespace {

void pushRot(
    AnimationClip& clip,
    int joint,
    const std::vector<float>& times,
    const std::vector<Quaternion>& rots
) {
    if (joint < 0 || times.size() != rots.size() || times.empty()) {
        return;
    }
    AnimChannel ch;
    ch.jointIndex = joint;
    ch.path = AnimChannel::Rotation;
    ch.interp = AnimChannel::Linear;
    ch.times = times;
    ch.values.reserve(rots.size() * 4);
    for (const Quaternion& q : rots) {
        Quaternion n = q.normalized();
        ch.values.push_back(n.x);
        ch.values.push_back(n.y);
        ch.values.push_back(n.z);
        ch.values.push_back(n.w);
    }
    clip.channels.push_back(std::move(ch));
}

void pushPos(
    AnimationClip& clip,
    int joint,
    const std::vector<float>& times,
    const std::vector<Vector3>& positions
) {
    if (joint < 0 || times.size() != positions.size() || times.empty()) {
        return;
    }
    AnimChannel ch;
    ch.jointIndex = joint;
    ch.path = AnimChannel::Translation;
    ch.interp = AnimChannel::Linear;
    ch.times = times;
    ch.values.reserve(positions.size() * 3);
    for (const Vector3& p : positions) {
        ch.values.push_back(p.x);
        ch.values.push_back(p.y);
        ch.values.push_back(p.z);
    }
    clip.channels.push_back(std::move(ch));
}

Quaternion eul(float rx, float ry, float rz) {
    return Quaternion::fromEulerXYZ(rx, ry, rz);
}

// Yamamoto phases (seconds). duration 1.55; release at t_norm 0.66 = 1.023s.
// 0 set · 1 rocker · 2 lift · 3 peak kick · 4 balance · 5 hand break ·
// 6 stride · 7 plant/layback · 8 RELEASE · 9 follow · 10 field · 11 finish
const std::vector<float> kT = {
    0.00f, 0.124f, 0.310f, 0.527f, 0.682f, 0.806f,
    0.899f, 0.976f, 1.023f, 1.147f, 1.333f, 1.55f
};

// ── arm pose helpers (hang-bind: child along -Y) ────────────────────────
// Tuned against world joint positions for Yamamoto silhouettes:
// set hands near sternum · kick hands stay · layback elbow high ·
// release high 3/4 with palm out front toward plate (+Z).

Quaternion setThrowShoulder() { return eul(-1.20f, 0.90f, 0.20f); }
Quaternion setThrowElbow() { return eul(1.48f, 0.0f, 0.10f); }
Quaternion setGloveShoulder() { return eul(-1.15f, 0.85f, 0.15f); }
Quaternion setGloveElbow() { return eul(1.45f, 0.0f, -0.05f); }

// Peak kick: hands stay glued, slightly higher.
Quaternion kickThrowShoulder() { return eul(-1.28f, 0.88f, 0.22f); }
Quaternion kickThrowElbow() { return eul(1.52f, 0.0f, 0.10f); }
Quaternion kickGloveShoulder() { return eul(-1.22f, 0.82f, 0.15f); }
Quaternion kickGloveElbow() { return eul(1.48f, 0.0f, -0.05f); }

// Layback: elbow above shoulder, hand high and cocked (past vertical).
Quaternion laybackThrowShoulder() { return eul(-2.90f, -0.50f, 0.70f); }
Quaternion laybackThrowElbow() { return eul(1.48f, 0.15f, 0.0f); }

// High 3/4 release: palm high, extended toward plate (with open hips).
Quaternion releaseThrowShoulder() { return eul(-1.85f, -0.35f, 0.20f); }
Quaternion releaseThrowElbow() { return eul(0.05f, 0.0f, 0.0f); }

// Glove tucked at chest on release.
Quaternion releaseGloveShoulder() { return eul(-0.55f, 0.25f, -0.10f); }
Quaternion releaseGloveElbow() { return eul(0.95f, 0.0f, 0.0f); }

// Hand-break intermediate (between set and full layback).
Quaternion breakThrowShoulder() { return eul(-1.80f, -0.15f, 0.15f); }
Quaternion breakThrowElbow() { return eul(1.50f, 0.10f, 0.05f); }

} // namespace

namespace BaseballAnims {

AnimationClip yamamotoWindup(const SkinnedModel3D& model) {
    // Frame-driven after FanGraphs Yamamoto slow-mo (omgz7L7Sfw0):
    // still set → rocker → vertical high kick (hands STILL together) →
    // balance hold → hand break → stride with arm layback → high 3/4 release
    // → long finish. Release key is index 8 (t_norm ≈ 0.66).
    AnimationClip clip;
    clip.name = "yamamoto_windup";
    clip.duration = 1.55f;

    auto J = [&](const char* n) { return model.findJoint(n); };

    // ── HIPS: closed through kick, open late (hip-shoulder separation) ──
    pushRot(clip, J("Hips"), kT, {
        eul(0.05f, -0.28f, 0.0f),   // set closed
        eul(0.06f, -0.34f, -0.03f), // rocker more closed
        eul(0.04f, -0.40f, -0.03f), // lift
        eul(0.02f, -0.45f, -0.02f), // peak kick — still closed
        eul(0.02f, -0.46f, -0.02f), // balance
        eul(0.04f, -0.32f, 0.0f),   // hand break — hips barely start
        eul(0.07f, -0.05f, 0.02f),  // stride — hips begin open
        eul(0.09f, 0.18f, 0.03f),   // plant — open but not fully
        eul(0.12f, 0.42f, 0.05f),   // RELEASE — hips clear
        eul(0.14f, 0.60f, 0.07f),   // follow
        eul(0.08f, 0.38f, 0.04f),
        eul(0.04f, 0.18f, 0.0f)
    });

    // COM drive toward plate only after balance (not early lean).
    pushPos(clip, J("Hips"), kT, {
        Vector3(0.0f, 0.90f, 0.00f),
        Vector3(0.0f, 0.90f, -0.01f), // slight rocker back
        Vector3(0.0f, 0.91f, 0.00f),
        Vector3(0.0f, 0.92f, 0.00f),
        Vector3(0.0f, 0.92f, 0.00f),
        Vector3(0.0f, 0.90f, 0.03f),
        Vector3(0.0f, 0.88f, 0.10f),
        Vector3(0.0f, 0.86f, 0.17f),
        Vector3(0.0f, 0.85f, 0.20f),
        Vector3(0.0f, 0.86f, 0.18f),
        Vector3(0.0f, 0.88f, 0.12f),
        Vector3(0.0f, 0.90f, 0.06f)
    });

    // ── SPINE/CHEST: stay closed until fire (classic separation) ────────
    pushRot(clip, J("Spine"), kT, {
        eul(0.06f, -0.10f, 0.0f),
        eul(0.07f, -0.14f, -0.02f),
        eul(0.05f, -0.20f, -0.03f),
        eul(0.03f, -0.26f, -0.02f),
        eul(0.03f, -0.28f, -0.02f),
        eul(0.05f, -0.24f, -0.01f), // still closed at break
        eul(0.08f, -0.18f, 0.0f),   // stride — shoulders lag
        eul(0.10f, -0.10f, 0.02f),  // plant — still lagging
        eul(0.14f, 0.28f, 0.05f),   // RELEASE — spine fires
        eul(0.16f, 0.45f, 0.06f),
        eul(0.10f, 0.25f, 0.03f),
        eul(0.06f, 0.12f, 0.0f)
    });
    pushRot(clip, J("Chest"), kT, {
        eul(0.04f, -0.08f, 0.0f),
        eul(0.05f, -0.12f, -0.02f),
        eul(0.04f, -0.18f, -0.02f),
        eul(0.02f, -0.24f, -0.02f),
        eul(0.02f, -0.26f, -0.02f),
        eul(0.03f, -0.26f, -0.01f), // shoulders closed through break
        eul(0.05f, -0.22f, 0.0f),   // stride
        eul(0.06f, -0.14f, 0.02f),  // plant — still closed (separation!)
        eul(0.12f, 0.32f, 0.06f),   // RELEASE — chest fires square to plate
        eul(0.14f, 0.48f, 0.08f),
        eul(0.08f, 0.28f, 0.04f),
        eul(0.04f, 0.12f, 0.0f)
    });

    // Head locked on target through balance; slight nod at release.
    pushRot(clip, J("Head"), kT, {
        eul(-0.02f, 0.20f, 0.0f),
        eul(-0.02f, 0.18f, 0.0f),
        eul(-0.03f, 0.16f, 0.0f),
        eul(-0.04f, 0.14f, 0.0f),
        eul(-0.04f, 0.14f, 0.0f),
        eul(-0.02f, 0.14f, 0.0f),
        eul(0.0f, 0.12f, 0.0f),
        eul(0.02f, 0.10f, 0.0f),
        eul(0.08f, 0.08f, 0.0f),
        eul(0.05f, -0.06f, 0.0f),
        eul(0.02f, 0.04f, 0.0f),
        eul(0.0f, 0.10f, 0.0f)
    });

    // ── LEAD LEG (L): vertical high kick, foot tucked, then long stride ──
    // Neg Rx lifts thigh toward plate/chest. Knee flex tucks shin under.
    pushRot(clip, J("Hip_L"), kT, {
        eul(0.12f, 0.04f, 0.06f),   // set soft
        eul(-0.40f, 0.08f, 0.08f),  // rocker peel
        eul(-1.05f, 0.10f, 0.10f),  // climbing
        eul(-1.58f, 0.12f, 0.08f),  // PEAK ~chest height, vertical
        eul(-1.60f, 0.12f, 0.08f),  // HOLD balance
        eul(-0.95f, 0.08f, 0.05f),  // start down
        eul(-0.35f, 0.02f, 0.02f),  // stride
        eul(-0.08f, -0.02f, 0.0f),  // plant
        eul(-0.04f, -0.04f, 0.0f),  // release brace
        eul(-0.04f, -0.04f, 0.0f),
        eul(0.02f, -0.02f, 0.0f),
        eul(0.08f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Knee_L"), kT, {
        eul(0.20f, 0.0f, 0.0f),
        eul(0.65f, 0.0f, 0.0f),
        eul(1.25f, 0.0f, 0.0f),
        eul(1.60f, 0.0f, 0.0f), // tight tuck under thigh
        eul(1.62f, 0.0f, 0.0f),
        eul(1.05f, 0.0f, 0.0f),
        eul(0.45f, 0.0f, 0.0f),
        eul(0.28f, 0.0f, 0.0f), // ~15° land flex
        eul(0.22f, 0.0f, 0.0f),
        eul(0.18f, 0.0f, 0.0f),
        eul(0.16f, 0.0f, 0.0f),
        eul(0.14f, 0.0f, 0.0f)
    });

    // ── PLANT LEG (R): load, then drive ─────────────────────────────────
    pushRot(clip, J("Hip_R"), kT, {
        eul(0.14f, 0.0f, -0.05f),
        eul(0.18f, 0.0f, -0.06f),
        eul(0.20f, 0.0f, -0.05f),
        eul(0.22f, 0.0f, -0.04f),
        eul(0.24f, 0.0f, -0.04f),
        eul(0.32f, 0.0f, -0.02f),
        eul(0.38f, 0.0f, 0.0f),
        eul(0.40f, 0.0f, 0.02f),
        eul(0.34f, 0.0f, 0.04f),
        eul(0.24f, 0.0f, 0.04f),
        eul(0.16f, 0.0f, 0.02f),
        eul(0.12f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Knee_R"), kT, {
        eul(0.24f, 0.0f, 0.0f),
        eul(0.28f, 0.0f, 0.0f),
        eul(0.30f, 0.0f, 0.0f),
        eul(0.32f, 0.0f, 0.0f),
        eul(0.34f, 0.0f, 0.0f),
        eul(0.42f, 0.0f, 0.0f),
        eul(0.48f, 0.0f, 0.0f),
        eul(0.50f, 0.0f, 0.0f),
        eul(0.42f, 0.0f, 0.0f),
        eul(0.32f, 0.0f, 0.0f),
        eul(0.24f, 0.0f, 0.0f),
        eul(0.20f, 0.0f, 0.0f)
    });

    // ── THROW ARM (R): the money path ───────────────────────────────────
    // set/lift/balance: hands together at chest
    // break: peel back/up
    // plant: full layback (hand high-behind, elbow shoulder-high)
    // release: high 3/4 extension to the plate
    pushRot(clip, J("Shoulder_R"), kT, {
        setThrowShoulder(),          // 0 set — hands chest
        setThrowShoulder(),          // 1 rocker — hands stay
        kickThrowShoulder(),         // 2 lift
        kickThrowShoulder(),         // 3 peak kick — STILL together
        kickThrowShoulder(),         // 4 balance hold
        breakThrowShoulder(),        // 5 hand break — peel back/up
        laybackThrowShoulder(),      // 6 stride — loading
        laybackThrowShoulder(),      // 7 plant — max ER / layback
        releaseThrowShoulder(),      // 8 RELEASE high 3/4
        eul(-1.20f, 0.55f, 0.50f),   // 9 follow across
        eul(-0.70f, 0.35f, 0.30f),   // 10
        eul(-0.40f, 0.15f, 0.10f)    // 11 finish
    });
    pushRot(clip, J("Elbow_R"), kT, {
        setThrowElbow(),
        setThrowElbow(),
        kickThrowElbow(),
        kickThrowElbow(),
        kickThrowElbow(),
        breakThrowElbow(),
        laybackThrowElbow(),
        laybackThrowElbow(),
        releaseThrowElbow(),         // nearly straight
        eul(-0.08f, 0.0f, 0.0f),
        eul(0.20f, 0.0f, 0.0f),
        eul(0.35f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Wrist_R"), kT, {
        eul(0.10f, 0.0f, 0.0f),
        eul(0.10f, 0.0f, 0.0f),
        eul(0.12f, 0.0f, 0.0f),
        eul(0.14f, 0.0f, 0.0f),
        eul(0.14f, 0.0f, 0.0f),
        eul(0.22f, 0.0f, 0.0f),
        eul(0.28f, 0.0f, 0.0f),
        eul(0.12f, 0.0f, 0.0f),
        eul(-0.42f, 0.0f, 0.0f), // snap
        eul(-0.22f, 0.0f, 0.0f),
        eul(-0.05f, 0.0f, 0.0f),
        eul(0.0f, 0.0f, 0.0f)
    });

    // ── GLOVE ARM (L): glued through balance, then tuck to chest ────────
    pushRot(clip, J("Shoulder_L"), kT, {
        setGloveShoulder(),
        setGloveShoulder(),
        kickGloveShoulder(),
        kickGloveShoulder(),
        kickGloveShoulder(),
        eul(-0.80f, 0.45f, 0.05f),   // break — stay front
        eul(-0.65f, 0.35f, -0.05f),
        releaseGloveShoulder(),
        releaseGloveShoulder(),
        eul(-0.50f, 0.20f, -0.08f),
        eul(-0.60f, 0.30f, -0.05f),
        eul(-0.90f, 0.55f, 0.05f)
    });
    pushRot(clip, J("Elbow_L"), kT, {
        setGloveElbow(),
        setGloveElbow(),
        kickGloveElbow(),
        kickGloveElbow(),
        kickGloveElbow(),
        eul(1.20f, 0.0f, 0.0f),
        eul(1.05f, 0.0f, 0.0f),
        releaseGloveElbow(),
        releaseGloveElbow(),
        eul(0.80f, 0.0f, 0.0f),
        eul(0.95f, 0.0f, 0.0f),
        eul(1.20f, 0.0f, 0.0f)
    });

    return clip;
}

AnimationClip pitcherIdle(const SkinnedModel3D& model) {
    // Match windup t=0 set exactly so Ready looks like Yamamoto on the rubber.
    AnimationClip clip;
    clip.name = "pitcher_idle";
    clip.duration = 2.0f;
    auto J = [&](const char* n) { return model.findJoint(n); };
    std::vector<float> times = {0.0f, 1.0f, 2.0f};

    pushRot(clip, J("Hips"), times, {
        eul(0.05f, -0.28f, 0.0f),
        eul(0.055f, -0.26f, 0.01f),
        eul(0.05f, -0.28f, 0.0f)
    });
    pushRot(clip, J("Spine"), times, {
        eul(0.06f, -0.10f, 0.0f),
        eul(0.07f, -0.08f, 0.01f),
        eul(0.06f, -0.10f, 0.0f)
    });
    pushRot(clip, J("Chest"), times, {
        eul(0.04f, -0.08f, 0.0f),
        eul(0.05f, -0.06f, 0.0f),
        eul(0.04f, -0.08f, 0.0f)
    });
    pushRot(clip, J("Head"), times, {
        eul(-0.02f, 0.20f, 0.0f),
        eul(-0.01f, 0.18f, 0.01f),
        eul(-0.02f, 0.20f, 0.0f)
    });
    pushRot(clip, J("Shoulder_R"), times, {
        setThrowShoulder(),
        eul(-0.93f, 0.54f, 0.34f),
        setThrowShoulder()
    });
    pushRot(clip, J("Elbow_R"), times, {
        setThrowElbow(), eul(1.33f, 0.0f, 0.15f), setThrowElbow()
    });
    pushRot(clip, J("Wrist_R"), times, {
        eul(0.10f, 0.0f, 0.0f), eul(0.12f, 0.0f, 0.0f), eul(0.10f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Shoulder_L"), times, {
        setGloveShoulder(),
        eul(-0.88f, -0.48f, -0.28f),
        setGloveShoulder()
    });
    pushRot(clip, J("Elbow_L"), times, {
        setGloveElbow(), eul(1.28f, 0.0f, -0.10f), setGloveElbow()
    });
    pushRot(clip, J("Hip_R"), times, {
        eul(0.14f, 0.0f, -0.05f), eul(0.15f, 0.0f, -0.05f), eul(0.14f, 0.0f, -0.05f)
    });
    pushRot(clip, J("Hip_L"), times, {
        eul(0.12f, 0.04f, 0.06f), eul(0.13f, 0.04f, 0.06f), eul(0.12f, 0.04f, 0.06f)
    });
    pushRot(clip, J("Knee_R"), times, {
        eul(0.24f, 0.0f, 0.0f), eul(0.26f, 0.0f, 0.0f), eul(0.24f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Knee_L"), times, {
        eul(0.20f, 0.0f, 0.0f), eul(0.22f, 0.0f, 0.0f), eul(0.20f, 0.0f, 0.0f)
    });
    return clip;
}

AnimationClip catcherIdle(const SkinnedModel3D& model) {
    AnimationClip clip;
    clip.name = "catcher_idle";
    clip.duration = 2.0f;
    auto J = [&](const char* n) { return model.findJoint(n); };
    std::vector<float> times = {0.0f, 1.0f, 2.0f};
    pushRot(clip, J("Spine"), times, {
        eul(0.15f, 0.0f, 0.0f),
        eul(0.17f, 0.03f, 0.0f),
        eul(0.15f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Shoulder_L"), times, {
        eul(-0.70f, -0.30f, -0.15f),
        eul(-0.72f, -0.32f, -0.15f),
        eul(-0.70f, -0.30f, -0.15f)
    });
    pushRot(clip, J("Elbow_L"), times, {
        eul(1.10f, 0.0f, 0.0f), eul(1.12f, 0.0f, 0.0f), eul(1.10f, 0.0f, 0.0f)
    });
    return clip;
}

} // namespace BaseballAnims
