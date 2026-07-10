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

// Duration 1.55s; release index 8 ≈ t_norm 0.66.
// 0 set · 1 rocker · 2 lift · 3 peak kick · 4 balance · 5 break ·
// 6 stride · 7 plant · 8 RELEASE · 9 follow · 10 field · 11 finish
const std::vector<float> kT = {
    0.00f, 0.124f, 0.310f, 0.527f, 0.682f, 0.806f,
    0.899f, 0.976f, 1.023f, 1.147f, 1.333f, 1.55f
};

// Arm helpers (hang-bind). Root is already sideways (~54° closed).
Quaternion setThrowShoulder() { return eul(-1.20f, 0.90f, 0.20f); }
Quaternion setThrowElbow() { return eul(1.48f, 0.0f, 0.10f); }
Quaternion setGloveShoulder() { return eul(-1.15f, 0.85f, 0.15f); }
Quaternion setGloveElbow() { return eul(1.45f, 0.0f, -0.05f); }

Quaternion kickThrowShoulder() { return eul(-1.28f, 0.88f, 0.22f); }
Quaternion kickThrowElbow() { return eul(1.52f, 0.0f, 0.10f); }
Quaternion kickGloveShoulder() { return eul(-1.22f, 0.82f, 0.15f); }
Quaternion kickGloveElbow() { return eul(1.48f, 0.0f, -0.05f); }

Quaternion laybackThrowShoulder() { return eul(-2.90f, -0.50f, 0.70f); }
Quaternion laybackThrowElbow() { return eul(1.48f, 0.15f, 0.0f); }

Quaternion releaseThrowShoulder() { return eul(-1.85f, -0.35f, 0.20f); }
Quaternion releaseThrowElbow() { return eul(0.05f, 0.0f, 0.0f); }

Quaternion releaseGloveShoulder() { return eul(-0.55f, 0.25f, -0.10f); }
Quaternion releaseGloveElbow() { return eul(0.95f, 0.0f, 0.0f); }

Quaternion breakThrowShoulder() { return eul(-1.80f, -0.15f, 0.15f); }
Quaternion breakThrowElbow() { return eul(1.50f, 0.10f, 0.05f); }

// Static set pose used by idle (no fidget).
void pushStaticSetPose(AnimationClip& clip, const SkinnedModel3D& model, const std::vector<float>& times) {
    auto J = [&](const char* n) { return model.findJoint(n); };
    auto sameR = [&](const Quaternion& q) {
        return std::vector<Quaternion>(times.size(), q);
    };
    auto sameP = [&](const Vector3& p) {
        return std::vector<Vector3>(times.size(), p);
    };

    // Extra closed on hips (root already ~54° sideways).
    pushRot(clip, J("Hips"), times, sameR(eul(0.06f, -0.18f, 0.0f)));
    pushPos(clip, J("Hips"), times, sameP(Vector3(0.0f, 0.90f, 0.0f)));
    pushRot(clip, J("Spine"), times, sameR(eul(0.06f, -0.08f, 0.0f)));
    pushRot(clip, J("Chest"), times, sameR(eul(0.04f, -0.06f, 0.0f)));
    // Head already looks plate-ward in bind; keep still.
    pushRot(clip, J("Head"), times, sameR(eul(0.0f, 0.0f, 0.0f)));

    pushRot(clip, J("Shoulder_R"), times, sameR(setThrowShoulder()));
    pushRot(clip, J("Elbow_R"), times, sameR(setThrowElbow()));
    pushRot(clip, J("Wrist_R"), times, sameR(eul(0.10f, 0.0f, 0.0f)));
    pushRot(clip, J("Shoulder_L"), times, sameR(setGloveShoulder()));
    pushRot(clip, J("Elbow_L"), times, sameR(setGloveElbow()));

    // Athletic lower half — both knees soft, plant foot firm, lead slightly open.
    pushRot(clip, J("Hip_R"), times, sameR(eul(0.16f, 0.0f, -0.06f)));
    pushRot(clip, J("Knee_R"), times, sameR(eul(0.28f, 0.0f, 0.0f)));
    pushRot(clip, J("Ankle_R"), times, sameR(eul(-0.08f, 0.0f, 0.0f)));
    pushRot(clip, J("Toe_R"), times, sameR(eul(0.05f, 0.0f, 0.0f)));

    pushRot(clip, J("Hip_L"), times, sameR(eul(0.14f, 0.06f, 0.08f)));
    pushRot(clip, J("Knee_L"), times, sameR(eul(0.24f, 0.0f, 0.0f)));
    pushRot(clip, J("Ankle_L"), times, sameR(eul(-0.06f, 0.0f, 0.0f)));
    pushRot(clip, J("Toe_L"), times, sameR(eul(0.05f, 0.0f, 0.0f)));
}

} // namespace

namespace BaseballAnims {

AnimationClip yamamotoWindup(const SkinnedModel3D& model) {
    // Full-body Yamamoto delivery: legs drive the kick/stride/plant while the
    // upper half holds hands together, then layback and high 3/4 release.
    AnimationClip clip;
    clip.name = "yamamoto_windup";
    clip.duration = 1.55f;
    auto J = [&](const char* n) { return model.findJoint(n); };

    // ── HIPS / COM (full-body drive) ────────────────────────────────────
    pushRot(clip, J("Hips"), kT, {
        eul(0.06f, -0.18f, 0.0f),   // set — sideways closed
        eul(0.08f, -0.28f, -0.04f), // rocker load back
        eul(0.05f, -0.36f, -0.04f), // lift
        eul(0.02f, -0.42f, -0.03f), // peak kick
        eul(0.02f, -0.44f, -0.03f), // balance
        eul(0.05f, -0.28f, 0.0f),   // break
        eul(0.10f, 0.02f, 0.03f),   // stride open
        eul(0.14f, 0.22f, 0.05f),   // plant
        eul(0.16f, 0.48f, 0.07f),   // RELEASE
        eul(0.18f, 0.65f, 0.09f),   // follow
        eul(0.10f, 0.40f, 0.05f),
        eul(0.06f, 0.18f, 0.0f)
    });
    // Vertical bounce + forward drive (COM).
    pushPos(clip, J("Hips"), kT, {
        Vector3(0.0f, 0.90f, 0.00f),
        Vector3(0.0f, 0.88f, -0.03f), // rocker sits into back leg
        Vector3(0.0f, 0.91f, -0.01f),
        Vector3(0.0f, 0.93f, 0.00f),  // tall on plant leg at kick
        Vector3(0.0f, 0.93f, 0.00f),
        Vector3(0.0f, 0.90f, 0.04f),
        Vector3(0.0f, 0.86f, 0.14f),  // long stride drive
        Vector3(0.0f, 0.84f, 0.22f),
        Vector3(0.0f, 0.83f, 0.26f),  // release over front side
        Vector3(0.0f, 0.84f, 0.24f),
        Vector3(0.0f, 0.87f, 0.14f),
        Vector3(0.0f, 0.90f, 0.06f)
    });

    // ── TORSO separation ────────────────────────────────────────────────
    pushRot(clip, J("Spine"), kT, {
        eul(0.06f, -0.08f, 0.0f),
        eul(0.08f, -0.14f, -0.03f),
        eul(0.05f, -0.20f, -0.03f),
        eul(0.03f, -0.26f, -0.02f),
        eul(0.03f, -0.28f, -0.02f),
        eul(0.05f, -0.22f, -0.01f),
        eul(0.10f, -0.14f, 0.0f),
        eul(0.12f, -0.06f, 0.02f),
        eul(0.16f, 0.30f, 0.05f),
        eul(0.18f, 0.48f, 0.07f),
        eul(0.10f, 0.26f, 0.03f),
        eul(0.06f, 0.10f, 0.0f)
    });
    pushRot(clip, J("Chest"), kT, {
        eul(0.04f, -0.06f, 0.0f),
        eul(0.05f, -0.12f, -0.02f),
        eul(0.04f, -0.18f, -0.02f),
        eul(0.02f, -0.24f, -0.02f),
        eul(0.02f, -0.26f, -0.02f),
        eul(0.03f, -0.26f, -0.01f),
        eul(0.05f, -0.20f, 0.0f),
        eul(0.06f, -0.12f, 0.02f),
        eul(0.14f, 0.34f, 0.06f),
        eul(0.16f, 0.50f, 0.08f),
        eul(0.08f, 0.28f, 0.04f),
        eul(0.04f, 0.10f, 0.0f)
    });
    pushRot(clip, J("Head"), kT, {
        eul(0.0f, 0.0f, 0.0f),
        eul(0.0f, -0.02f, 0.0f),
        eul(-0.02f, -0.04f, 0.0f),
        eul(-0.03f, -0.05f, 0.0f),
        eul(-0.03f, -0.05f, 0.0f),
        eul(-0.02f, -0.04f, 0.0f),
        eul(0.0f, -0.02f, 0.0f),
        eul(0.02f, 0.0f, 0.0f),
        eul(0.06f, 0.02f, 0.0f),
        eul(0.04f, -0.08f, 0.0f),
        eul(0.02f, -0.02f, 0.0f),
        eul(0.0f, 0.0f, 0.0f)
    });

    // ── LEAD LEG full chain (hip → knee → ankle → toe) ──────────────────
    pushRot(clip, J("Hip_L"), kT, {
        eul(0.14f, 0.06f, 0.08f),
        eul(-0.55f, 0.10f, 0.10f),  // rocker peel high
        eul(-1.15f, 0.12f, 0.12f),
        eul(-1.65f, 0.14f, 0.10f),  // PEAK kick
        eul(-1.68f, 0.14f, 0.10f),  // HOLD
        eul(-1.00f, 0.10f, 0.06f),
        eul(-0.40f, 0.04f, 0.02f),  // stride down
        eul(-0.10f, -0.02f, 0.0f),  // plant
        eul(-0.05f, -0.04f, 0.0f),
        eul(-0.05f, -0.04f, 0.0f),
        eul(0.04f, -0.02f, 0.0f),
        eul(0.10f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Knee_L"), kT, {
        eul(0.24f, 0.0f, 0.0f),
        eul(0.75f, 0.0f, 0.0f),
        eul(1.30f, 0.0f, 0.0f),
        eul(1.65f, 0.0f, 0.0f),
        eul(1.68f, 0.0f, 0.0f),
        eul(1.10f, 0.0f, 0.0f),
        eul(0.50f, 0.0f, 0.0f),
        eul(0.30f, 0.0f, 0.0f),
        eul(0.24f, 0.0f, 0.0f),
        eul(0.20f, 0.0f, 0.0f),
        eul(0.18f, 0.0f, 0.0f),
        eul(0.16f, 0.0f, 0.0f)
    });
    // Toe pointed on kick, flexed into plant.
    pushRot(clip, J("Ankle_L"), kT, {
        eul(-0.06f, 0.0f, 0.0f),
        eul(0.35f, 0.0f, 0.0f),  // point
        eul(0.55f, 0.0f, 0.0f),
        eul(0.65f, 0.0f, 0.0f),
        eul(0.65f, 0.0f, 0.0f),
        eul(0.30f, 0.0f, 0.0f),
        eul(-0.05f, 0.0f, 0.0f),
        eul(-0.18f, 0.0f, 0.0f), // plant brace
        eul(-0.15f, 0.0f, 0.0f),
        eul(-0.12f, 0.0f, 0.0f),
        eul(-0.08f, 0.0f, 0.0f),
        eul(-0.06f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Toe_L"), kT, {
        eul(0.05f, 0.0f, 0.0f),
        eul(0.25f, 0.0f, 0.0f),
        eul(0.40f, 0.0f, 0.0f),
        eul(0.45f, 0.0f, 0.0f),
        eul(0.45f, 0.0f, 0.0f),
        eul(0.20f, 0.0f, 0.0f),
        eul(0.05f, 0.0f, 0.0f),
        eul(-0.05f, 0.0f, 0.0f),
        eul(-0.02f, 0.0f, 0.0f),
        eul(0.0f, 0.0f, 0.0f),
        eul(0.02f, 0.0f, 0.0f),
        eul(0.05f, 0.0f, 0.0f)
    });

    // ── PLANT LEG full chain ────────────────────────────────────────────
    pushRot(clip, J("Hip_R"), kT, {
        eul(0.16f, 0.0f, -0.06f),
        eul(0.22f, 0.0f, -0.08f), // load
        eul(0.26f, 0.0f, -0.07f),
        eul(0.30f, 0.0f, -0.05f), // deep on balance
        eul(0.32f, 0.0f, -0.05f),
        eul(0.40f, 0.0f, -0.02f), // drive
        eul(0.48f, 0.0f, 0.02f),
        eul(0.50f, 0.0f, 0.04f),
        eul(0.42f, 0.0f, 0.06f),
        eul(0.30f, 0.0f, 0.05f),
        eul(0.20f, 0.0f, 0.03f),
        eul(0.14f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Knee_R"), kT, {
        eul(0.28f, 0.0f, 0.0f),
        eul(0.36f, 0.0f, 0.0f),
        eul(0.40f, 0.0f, 0.0f),
        eul(0.45f, 0.0f, 0.0f),
        eul(0.48f, 0.0f, 0.0f),
        eul(0.55f, 0.0f, 0.0f),
        eul(0.60f, 0.0f, 0.0f),
        eul(0.58f, 0.0f, 0.0f),
        eul(0.48f, 0.0f, 0.0f),
        eul(0.36f, 0.0f, 0.0f),
        eul(0.28f, 0.0f, 0.0f),
        eul(0.22f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Ankle_R"), kT, {
        eul(-0.08f, 0.0f, 0.0f),
        eul(-0.15f, 0.0f, 0.0f), // dig rubber
        eul(-0.18f, 0.0f, 0.0f),
        eul(-0.20f, 0.0f, 0.0f),
        eul(-0.22f, 0.0f, 0.0f),
        eul(-0.12f, 0.0f, 0.0f),
        eul(0.05f, 0.0f, 0.0f),  // push off
        eul(0.15f, 0.0f, 0.0f),
        eul(0.20f, 0.0f, 0.0f),  // trailing foot lifts
        eul(0.12f, 0.0f, 0.0f),
        eul(0.0f, 0.0f, 0.0f),
        eul(-0.06f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Toe_R"), kT, {
        eul(0.05f, 0.0f, 0.0f),
        eul(0.10f, 0.0f, 0.0f),
        eul(0.12f, 0.0f, 0.0f),
        eul(0.14f, 0.0f, 0.0f),
        eul(0.14f, 0.0f, 0.0f),
        eul(0.08f, 0.0f, 0.0f),
        eul(-0.05f, 0.0f, 0.0f),
        eul(-0.15f, 0.0f, 0.0f),
        eul(-0.20f, 0.0f, 0.0f),
        eul(-0.10f, 0.0f, 0.0f),
        eul(0.0f, 0.0f, 0.0f),
        eul(0.05f, 0.0f, 0.0f)
    });

    // ── THROW ARM ───────────────────────────────────────────────────────
    pushRot(clip, J("Shoulder_R"), kT, {
        setThrowShoulder(),
        setThrowShoulder(),
        kickThrowShoulder(),
        kickThrowShoulder(),
        kickThrowShoulder(),
        breakThrowShoulder(),
        laybackThrowShoulder(),
        laybackThrowShoulder(),
        releaseThrowShoulder(),
        eul(-1.20f, 0.55f, 0.50f),
        eul(-0.70f, 0.35f, 0.30f),
        eul(-0.40f, 0.15f, 0.10f)
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
        releaseThrowElbow(),
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
        eul(-0.42f, 0.0f, 0.0f),
        eul(-0.22f, 0.0f, 0.0f),
        eul(-0.05f, 0.0f, 0.0f),
        eul(0.0f, 0.0f, 0.0f)
    });

    // ── GLOVE ARM ───────────────────────────────────────────────────────
    pushRot(clip, J("Shoulder_L"), kT, {
        setGloveShoulder(),
        setGloveShoulder(),
        kickGloveShoulder(),
        kickGloveShoulder(),
        kickGloveShoulder(),
        eul(-0.80f, 0.45f, 0.05f),
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
    // Completely still set — no arm fidget / breathing keys.
    AnimationClip clip;
    clip.name = "pitcher_idle";
    clip.duration = 1.0f;
    pushStaticSetPose(clip, model, {0.0f, 1.0f});
    return clip;
}

AnimationClip catcherIdle(const SkinnedModel3D& model) {
    AnimationClip clip;
    clip.name = "catcher_idle";
    clip.duration = 1.0f;
    auto J = [&](const char* n) { return model.findJoint(n); };
    std::vector<float> times = {0.0f, 1.0f};
    auto same = [&](const Quaternion& q) {
        return std::vector<Quaternion>(times.size(), q);
    };
    pushRot(clip, J("Spine"), times, same(eul(0.15f, 0.0f, 0.0f)));
    pushRot(clip, J("Shoulder_L"), times, same(eul(-0.70f, -0.30f, -0.15f)));
    pushRot(clip, J("Elbow_L"), times, same(eul(1.10f, 0.0f, 0.0f)));
    pushRot(clip, J("Hip_L"), times, same(eul(0.3f, 0.0f, 0.1f)));
    pushRot(clip, J("Hip_R"), times, same(eul(0.3f, 0.0f, -0.1f)));
    pushRot(clip, J("Knee_L"), times, same(eul(0.8f, 0.0f, 0.0f)));
    pushRot(clip, J("Knee_R"), times, same(eul(0.8f, 0.0f, 0.0f)));
    return clip;
}

} // namespace BaseballAnims
