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
// Rotations are written for a clavicle→shoulder→elbow→wrist chain so the
// arm reads as a long whip instead of two hinged tubes.
Quaternion setThrowShoulder() { return eul(-1.15f, 0.95f, 0.18f); }
Quaternion setThrowElbow() { return eul(1.55f, 0.05f, 0.08f); }
Quaternion setGloveShoulder() { return eul(-1.10f, 0.90f, 0.12f); }
Quaternion setGloveElbow() { return eul(1.52f, -0.02f, -0.04f); }

Quaternion kickThrowShoulder() { return eul(-1.22f, 0.92f, 0.20f); }
Quaternion kickThrowElbow() { return eul(1.60f, 0.05f, 0.08f); }
Quaternion kickGloveShoulder() { return eul(-1.18f, 0.88f, 0.14f); }
Quaternion kickGloveElbow() { return eul(1.56f, -0.02f, -0.04f); }

// Early cock: hand starts peeling back while elbow stays high.
Quaternion earlyCockShoulder() { return eul(-1.55f, 0.25f, 0.35f); }
Quaternion earlyCockElbow() { return eul(1.58f, 0.12f, 0.06f); }

// Full scapular load / max external rotation — elbow up, hand deep behind.
Quaternion laybackThrowShoulder() { return eul(-2.70f, -1.15f, 0.70f); }
Quaternion laybackThrowElbow() { return eul(1.55f, 0.16f, 0.05f); }

// Acceleration frame: elbow leads the hand (kinetic chain).
Quaternion accelThrowShoulder() { return eul(-2.10f, -0.40f, 0.40f); }
Quaternion accelThrowElbow() { return eul(0.60f, 0.06f, 0.02f); }

// High 3/4 release — long lever, hand above shoulder line toward plate.
Quaternion releaseThrowShoulder() { return eul(-1.75f, -0.05f, 0.18f); }
Quaternion releaseThrowElbow() { return eul(0.05f, 0.01f, -0.02f); }

// Pronated finish across the body.
Quaternion followThrowShoulder() { return eul(-1.05f, 0.65f, 0.55f); }
Quaternion followThrowElbow() { return eul(-0.12f, 0.05f, 0.08f); }

Quaternion releaseGloveShoulder() { return eul(-0.50f, 0.22f, -0.12f); }
Quaternion releaseGloveElbow() { return eul(0.90f, 0.0f, 0.0f); }

Quaternion breakThrowShoulder() { return eul(-1.70f, -0.05f, 0.28f); }
Quaternion breakThrowElbow() { return eul(1.55f, 0.14f, 0.06f); }

// Clavicle shrug / reach — small but sells shoulder girdle motion.
Quaternion clavSet() { return eul(0.02f, 0.0f, 0.04f); }
Quaternion clavKick() { return eul(0.04f, 0.0f, 0.06f); }
Quaternion clavLayback() { return eul(-0.08f, -0.12f, 0.14f); }
Quaternion clavRelease() { return eul(0.10f, 0.08f, 0.06f); }
Quaternion clavFollow() { return eul(0.04f, 0.10f, 0.02f); }

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

    pushRot(clip, J("Clavicle_R"), times, sameR(clavSet()));
    pushRot(clip, J("Clavicle_L"), times, sameR(eul(0.02f, 0.0f, -0.04f)));
    pushRot(clip, J("Shoulder_R"), times, sameR(setThrowShoulder()));
    pushRot(clip, J("Elbow_R"), times, sameR(setThrowElbow()));
    pushRot(clip, J("Wrist_R"), times, sameR(eul(0.12f, 0.05f, 0.08f)));
    pushRot(clip, J("Palm_R"), times, sameR(eul(0.08f, 0.0f, 0.0f)));
    pushRot(clip, J("Shoulder_L"), times, sameR(setGloveShoulder()));
    pushRot(clip, J("Elbow_L"), times, sameR(setGloveElbow()));
    pushRot(clip, J("Wrist_L"), times, sameR(eul(0.08f, 0.0f, 0.0f)));

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

    // ── CLAVICLES (shoulder girdle — shrug, load, reach) ────────────────
    // Keys: set, rocker, lift, peak, balance, break, stride, plant, RELEASE, follow, field, finish
    pushRot(clip, J("Clavicle_R"), kT, {
        clavSet(),
        clavSet(),
        clavKick(),
        clavKick(),
        clavKick(),
        eul(-0.02f, -0.06f, 0.10f),  // break — start loading scapula
        clavLayback(),
        eul(-0.10f, -0.14f, 0.16f),  // plant max load
        eul(0.14f, 0.10f, 0.08f),    // release — clavicle lifts slot high
        clavFollow(),
        eul(0.02f, 0.04f, 0.02f),
        clavSet()
    });
    pushRot(clip, J("Clavicle_L"), kT, {
        eul(0.02f, 0.0f, -0.04f),
        eul(0.02f, 0.0f, -0.04f),
        eul(0.04f, 0.0f, -0.06f),
        eul(0.04f, 0.0f, -0.06f),
        eul(0.04f, 0.0f, -0.06f),
        eul(0.06f, 0.04f, -0.08f),  // glove side braces
        eul(0.08f, 0.06f, -0.10f),
        eul(0.10f, 0.08f, -0.08f),
        eul(0.06f, 0.04f, -0.04f),  // tuck
        eul(0.04f, 0.02f, -0.03f),
        eul(0.03f, 0.0f, -0.04f),
        eul(0.02f, 0.0f, -0.04f)
    });

    // ── THROW ARM — long continuous whip ────────────────────────────────
    // Elbow LEADS the hand out of layback; wrist lags then pronates at release.
    pushRot(clip, J("Shoulder_R"), kT, {
        setThrowShoulder(),
        setThrowShoulder(),
        kickThrowShoulder(),
        kickThrowShoulder(),
        kickThrowShoulder(),
        breakThrowShoulder(),   // hand break / early cock
        laybackThrowShoulder(), // max ER
        accelThrowShoulder(),   // elbow drives (plant)
        releaseThrowShoulder(), // high 3/4
        followThrowShoulder(),  // whip across
        eul(-0.65f, 0.40f, 0.32f),
        eul(-0.38f, 0.18f, 0.12f)
    });
    pushRot(clip, J("Elbow_R"), kT, {
        setThrowElbow(),
        setThrowElbow(),
        kickThrowElbow(),
        kickThrowElbow(),
        kickThrowElbow(),
        breakThrowElbow(),
        laybackThrowElbow(),    // tightly flexed behind head
        accelThrowElbow(),      // rapid extension starts
        releaseThrowElbow(),    // near-straight at release
        followThrowElbow(),     // slight hyperextension feel
        eul(0.18f, 0.0f, 0.04f),
        eul(0.32f, 0.0f, 0.02f)
    });
    // Wrist: cocked on load → lag → snap/pronate through release → relax.
    pushRot(clip, J("Wrist_R"), kT, {
        eul(0.12f, 0.05f, 0.08f),
        eul(0.12f, 0.05f, 0.08f),
        eul(0.14f, 0.06f, 0.10f),
        eul(0.15f, 0.06f, 0.10f),
        eul(0.15f, 0.06f, 0.10f),
        eul(0.28f, 0.12f, 0.18f),  // cock
        eul(0.38f, 0.18f, 0.22f),  // max lag (hand back)
        eul(0.20f, 0.08f, 0.10f),  // catching up
        eul(-0.48f, -0.22f, -0.18f), // SNAP + pronation
        eul(-0.28f, -0.12f, -0.10f),
        eul(-0.08f, -0.04f, -0.02f),
        eul(0.02f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Palm_R"), kT, {
        eul(0.08f, 0.0f, 0.0f),
        eul(0.08f, 0.0f, 0.0f),
        eul(0.10f, 0.0f, 0.0f),
        eul(0.10f, 0.0f, 0.0f),
        eul(0.10f, 0.0f, 0.0f),
        eul(0.16f, 0.04f, 0.0f),
        eul(0.22f, 0.08f, 0.0f),
        eul(0.10f, 0.02f, 0.0f),
        eul(-0.18f, -0.06f, 0.0f), // fingers forward at release
        eul(-0.10f, -0.02f, 0.0f),
        eul(-0.02f, 0.0f, 0.0f),
        eul(0.04f, 0.0f, 0.0f)
    });

    // ── GLOVE ARM — steady front side, then tucks cleanly ───────────────
    pushRot(clip, J("Shoulder_L"), kT, {
        setGloveShoulder(),
        setGloveShoulder(),
        kickGloveShoulder(),
        kickGloveShoulder(),
        kickGloveShoulder(),
        eul(-0.85f, 0.48f, 0.08f),  // break — stay front
        eul(-0.68f, 0.38f, -0.04f), // stride
        eul(-0.55f, 0.28f, -0.10f), // plant brace
        releaseGloveShoulder(),
        eul(-0.48f, 0.18f, -0.10f),
        eul(-0.62f, 0.32f, -0.04f),
        eul(-0.92f, 0.58f, 0.04f)
    });
    pushRot(clip, J("Elbow_L"), kT, {
        setGloveElbow(),
        setGloveElbow(),
        kickGloveElbow(),
        kickGloveElbow(),
        kickGloveElbow(),
        eul(1.28f, 0.0f, 0.0f),
        eul(1.10f, 0.0f, 0.0f),
        eul(0.98f, 0.0f, 0.0f),
        releaseGloveElbow(),
        eul(0.78f, 0.0f, 0.0f),
        eul(0.98f, 0.0f, 0.0f),
        eul(1.22f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Wrist_L"), kT, {
        eul(0.08f, 0.0f, 0.0f),
        eul(0.08f, 0.0f, 0.0f),
        eul(0.10f, 0.0f, 0.0f),
        eul(0.10f, 0.0f, 0.0f),
        eul(0.10f, 0.0f, 0.0f),
        eul(0.12f, 0.0f, 0.0f),
        eul(0.10f, 0.0f, 0.0f),
        eul(0.06f, 0.0f, 0.0f),
        eul(0.04f, 0.0f, 0.0f),
        eul(0.04f, 0.0f, 0.0f),
        eul(0.06f, 0.0f, 0.0f),
        eul(0.08f, 0.0f, 0.0f)
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

// ── Batter: Ohtani-inspired RHB (hang-bind) ────────────────────────────
// Model faces +Z (pitcher after world rotY(π)). RHB: rear = R, lead = L.
//
// ANTI-NOODLE RULE (same as throw_preview):
//   Drive ONLY Shoulder + Elbow (hinge rx) + Wrist.
//   UpperArm / HumTwist / Forearm / ProTwist stay ~identity.
//   Prefer modest shoulder ry (twist) — large ry is what melts the mesh.
// Hang-bind: −rx raise, +rz open R, −rz open L, elbow +rx flexes.
namespace {

// Upright closed set — tall, not crouchy.
Quaternion ohtaniHipSet() { return eul(0.04f, -0.55f, 0.02f); }
Quaternion ohtaniSpineSet() { return eul(0.05f, -0.36f, 0.02f); }
Quaternion ohtaniChestSet() { return eul(0.04f, -0.38f, 0.02f); }
Quaternion ohtaniHeadSet() { return eul(-0.02f, 0.55f, 0.0f); }

// Ready set: hands together high-chest, elbows up, rigid mid-chain.
// Tuned for clean silhouette (no noodle melt).
Quaternion ohtaniShoulderR() { return eul(-1.22f, 0.70f, 0.32f); }
Quaternion ohtaniElbowR() { return eul(1.46f, 0.0f, 0.0f); } // pure hinge
Quaternion ohtaniWristR() { return eul(0.10f, 0.04f, 0.05f); }
Quaternion ohtaniShoulderL() { return eul(-0.70f, 0.52f, 0.62f); }
Quaternion ohtaniElbowL() { return eul(1.30f, 0.0f, 0.0f); }
Quaternion ohtaniWristL() { return eul(0.08f, 0.0f, 0.0f); }

// Wide athletic base, soft knees.
Quaternion ohtaniHipL() { return eul(0.16f, 0.12f, 0.16f); }
Quaternion ohtaniHipR() { return eul(0.22f, -0.06f, -0.12f); }
Quaternion ohtaniKneeL() { return eul(0.32f, 0.0f, 0.0f); }
Quaternion ohtaniKneeR() { return eul(0.40f, 0.0f, 0.0f); }

// Contact: tight hands mid-body (sep ≈ 0.25).
Quaternion ohtaniContactShR() { return eul(-1.00f, -0.50f, -0.05f); }
Quaternion ohtaniContactElR() { return eul(0.45f, 0.0f, 0.0f); }
Quaternion ohtaniContactShL() { return eul(-0.80f, -0.35f, 0.10f); }
Quaternion ohtaniContactElL() { return eul(0.50f, 0.0f, 0.0f); }

// Finish wrap (sep ≈ 0.25).
Quaternion ohtaniFinishShR() { return eul(-0.60f, -0.30f, -0.75f); }
Quaternion ohtaniFinishElR() { return eul(1.05f, 0.0f, 0.0f); }
Quaternion ohtaniFinishShL() { return eul(-0.60f, -0.15f, 0.40f); }
Quaternion ohtaniFinishElL() { return eul(1.10f, 0.0f, 0.0f); }

// Zero multi-bone arm mid-chain so limbs stay rigid tubes.
void pushArmStiff(
    AnimationClip& clip,
    const SkinnedModel3D& model,
    const std::vector<float>& times
) {
    auto J = [&](const char* n) { return model.findJoint(n); };
    auto zero = std::vector<Quaternion>(times.size(), eul(0, 0, 0));
    for (const char* n : {
             "UpperArm_R", "HumTwist_R", "Forearm_R", "ProTwist_R",
             "UpperArm_L", "HumTwist_L", "Forearm_L", "ProTwist_L"}) {
        pushRot(clip, J(n), times, zero);
    }
}

} // namespace

AnimationClip batterStance(const SkinnedModel3D& model) {
    // Ohtani ready: upright, high hands, wide base, quiet eyes, bat-tip waggle.
    AnimationClip clip;
    clip.name = "batter_stance";
    clip.duration = 2.6f;
    auto J = [&](const char* n) { return model.findJoint(n); };
    const Vector3 hipRest = model.joints[J("Hips")].restTranslation;
    // Rhythm: weight rock + subtle hand/bat settle (not a big leg kick in the box).
    std::vector<float> t = {0.0f, 0.65f, 1.30f, 1.95f, 2.6f};

    pushPos(clip, J("Hips"), t, {
        hipRest + Vector3(0.0f, 0.0f, 0.0f),
        hipRest + Vector3(0.006f, 0.010f, 0.0f),   // slight rear load
        hipRest + Vector3(0.0f, 0.014f, 0.0f),
        hipRest + Vector3(-0.005f, 0.008f, 0.0f),
        hipRest
    });
    pushRot(clip, J("Hips"), t, {
        ohtaniHipSet(),
        eul(0.05f, -0.60f, 0.02f),
        eul(0.03f, -0.64f, 0.01f),
        eul(0.05f, -0.61f, 0.03f),
        ohtaniHipSet()
    });
    pushRot(clip, J("Spine"), t, {
        ohtaniSpineSet(),
        eul(0.06f, -0.40f, 0.02f),
        eul(0.07f, -0.44f, 0.02f),
        eul(0.05f, -0.41f, 0.01f),
        ohtaniSpineSet()
    });
    pushRot(clip, J("Chest"), t, {
        ohtaniChestSet(),
        eul(0.05f, -0.44f, 0.02f),
        eul(0.06f, -0.48f, 0.02f),
        eul(0.04f, -0.45f, 0.01f),
        ohtaniChestSet()
    });
    // Quiet head — track pitcher, minimal noise.
    pushRot(clip, J("Head"), t, {
        ohtaniHeadSet(),
        eul(-0.02f, 0.55f, 0.0f),
        eul(-0.03f, 0.60f, 0.0f),
        eul(-0.02f, 0.56f, 0.0f),
        ohtaniHeadSet()
    });

    pushRot(clip, J("Clavicle_R"), t, {
        eul(0.04f, 0, 0.08f), eul(0.05f, 0, 0.09f), eul(0.04f, 0, 0.08f),
        eul(0.05f, 0, 0.09f), eul(0.04f, 0, 0.08f)
    });
    pushRot(clip, J("Clavicle_L"), t, {
        eul(0.03f, 0, -0.06f), eul(0.04f, 0, -0.07f), eul(0.03f, 0, -0.06f),
        eul(0.04f, 0, -0.07f), eul(0.03f, 0, -0.06f)
    });

    // Stiff mid-chain — no noodle twist on multi-bone arms.
    pushArmStiff(clip, model, t);

    // High-hand set: shoulders + elbows only; micro wrist waggle for bat tip.
    pushRot(clip, J("Shoulder_R"), t, {
        ohtaniShoulderR(),
        eul(-1.23f, 0.74f, 0.30f),
        eul(-1.27f, 0.76f, 0.26f),
        eul(-1.24f, 0.75f, 0.29f),
        ohtaniShoulderR()
    });
    pushRot(clip, J("Elbow_R"), t, {
        ohtaniElbowR(),
        eul(1.50f, 0, 0), eul(1.46f, 0, 0), eul(1.49f, 0, 0), ohtaniElbowR()
    });
    pushRot(clip, J("Wrist_R"), t, {
        ohtaniWristR(),
        eul(0.16f, 0.06f, 0.08f),  // tip back (wrist only)
        eul(0.08f, 0.03f, 0.03f),
        eul(0.14f, 0.05f, 0.06f),
        ohtaniWristR()
    });
    pushRot(clip, J("Palm_R"), t, {
        eul(0.08f, 0.02f, 0), eul(0.10f, 0.03f, 0), eul(0.06f, 0.02f, 0),
        eul(0.09f, 0.02f, 0), eul(0.08f, 0.02f, 0)
    });
    pushRot(clip, J("Shoulder_L"), t, {
        ohtaniShoulderL(),
        eul(-0.70f, 0.54f, 0.66f),
        eul(-0.74f, 0.56f, 0.64f),
        eul(-0.71f, 0.55f, 0.65f),
        ohtaniShoulderL()
    });
    pushRot(clip, J("Elbow_L"), t, {
        ohtaniElbowL(),
        eul(1.30f, 0, 0), eul(1.26f, 0, 0), eul(1.29f, 0, 0), ohtaniElbowL()
    });
    pushRot(clip, J("Wrist_L"), t, {
        ohtaniWristL(),
        eul(0.10f, 0, 0), eul(0.06f, 0, 0), eul(0.09f, 0, 0), ohtaniWristL()
    });

    // Firm two-hand grip.
    pushRot(clip, J("Index_R"), t, {
        eul(0.58f, 0, 0), eul(0.59f, 0, 0), eul(0.57f, 0, 0), eul(0.58f, 0, 0), eul(0.58f, 0, 0)
    });
    pushRot(clip, J("Middle_R"), t, {
        eul(0.60f, 0, 0), eul(0.61f, 0, 0), eul(0.59f, 0, 0), eul(0.60f, 0, 0), eul(0.60f, 0, 0)
    });
    pushRot(clip, J("Ring_R"), t, {
        eul(0.57f, 0, 0), eul(0.58f, 0, 0), eul(0.56f, 0, 0), eul(0.57f, 0, 0), eul(0.57f, 0, 0)
    });
    pushRot(clip, J("Pinky_R"), t, {
        eul(0.52f, 0, 0), eul(0.53f, 0, 0), eul(0.51f, 0, 0), eul(0.52f, 0, 0), eul(0.52f, 0, 0)
    });
    pushRot(clip, J("Thumb_R"), t, {
        eul(0.30f, 0.20f, 0.30f), eul(0.31f, 0.20f, 0.30f), eul(0.29f, 0.20f, 0.30f),
        eul(0.30f, 0.20f, 0.30f), eul(0.30f, 0.20f, 0.30f)
    });

    // Wide base, soft knees, gentle rock onto rear side.
    pushRot(clip, J("Hip_L"), t, {
        ohtaniHipL(),
        eul(0.18f, 0.12f, 0.19f), eul(0.14f, 0.11f, 0.17f),
        eul(0.17f, 0.12f, 0.18f), ohtaniHipL()
    });
    pushRot(clip, J("Hip_R"), t, {
        ohtaniHipR(),
        eul(0.25f, -0.06f, -0.15f), eul(0.20f, -0.05f, -0.13f),
        eul(0.24f, -0.06f, -0.14f), ohtaniHipR()
    });
    pushRot(clip, J("Knee_L"), t, {
        ohtaniKneeL(), eul(0.34f, 0, 0), eul(0.30f, 0, 0), eul(0.33f, 0, 0), ohtaniKneeL()
    });
    pushRot(clip, J("Knee_R"), t, {
        ohtaniKneeR(), eul(0.43f, 0, 0), eul(0.38f, 0, 0), eul(0.41f, 0, 0), ohtaniKneeR()
    });
    pushRot(clip, J("Ankle_L"), t, {
        eul(-0.06f, 0, 0), eul(-0.07f, 0, 0), eul(-0.05f, 0, 0),
        eul(-0.06f, 0, 0), eul(-0.06f, 0, 0)
    });
    pushRot(clip, J("Ankle_R"), t, {
        eul(-0.10f, 0, 0), eul(-0.12f, 0, 0), eul(-0.09f, 0, 0),
        eul(-0.11f, 0, 0), eul(-0.10f, 0, 0)
    });
    return clip;
}

AnimationClip batterSwing(const SkinnedModel3D& model) {
    // Premium RHB swing — 7 keys for smooth kinetic chain:
    // 0 load coil · 1 toe-tap · 2 stride · 3 plant · 4 CONTACT · 5 extend · 6 high wrap
    // Contact at t_norm ≈ 0.42 to match bat.swingT / PCI.
    AnimationClip clip;
    clip.name = "batter_swing";
    clip.duration = 0.58f;
    auto J = [&](const char* n) { return model.findJoint(n); };
    const Vector3 hipRest = model.joints[J("Hips")].restTranslation;

    // Absolute times (s) → t_norm: 0 · 0.12 · 0.24 · 0.34 · 0.42 · 0.62 · 1.0
    std::vector<float> t = {0.00f, 0.07f, 0.14f, 0.20f, 0.245f, 0.36f, 0.58f};

    pushPos(clip, J("Hips"), t, {
        hipRest + Vector3(0.04f, -0.03f, -0.04f),  // coil into rear hip
        hipRest + Vector3(0.03f, -0.01f, -0.02f),  // toe-tap balance
        hipRest + Vector3(0.01f, -0.04f,  0.04f),  // stride
        hipRest + Vector3(-0.01f, -0.06f, 0.08f),  // plant sit
        hipRest + Vector3(-0.03f, -0.05f, 0.12f),  // CONTACT drive
        hipRest + Vector3(-0.05f, -0.02f, 0.08f),  // extension
        hipRest + Vector3(-0.06f,  0.02f, 0.02f)   // finish tall
    });

    // Hips: closed coil → explosive open (power source).
    pushRot(clip, J("Hips"), t, {
        eul(0.07f, -0.85f, 0.02f),
        eul(0.08f, -0.78f, 0.02f),
        eul(0.10f, -0.40f, 0.04f),
        eul(0.13f, -0.05f, 0.06f),
        eul(0.15f,  0.48f, 0.08f),  // CONTACT
        eul(0.12f,  1.00f, 0.05f),
        eul(0.06f,  1.32f, 0.02f)
    });
    // Separation: torso lags hips, then snaps.
    pushRot(clip, J("Spine"), t, {
        eul(0.09f, -0.58f, 0.02f),
        eul(0.10f, -0.52f, 0.02f),
        eul(0.12f, -0.28f, 0.03f),
        eul(0.13f, -0.05f, 0.05f),
        eul(0.11f,  0.24f, 0.06f),
        eul(0.08f,  0.82f, 0.05f),
        eul(0.05f,  1.10f, 0.02f)
    });
    pushRot(clip, J("Chest"), t, {
        eul(0.06f, -0.62f, 0.02f),
        eul(0.07f, -0.55f, 0.02f),
        eul(0.09f, -0.30f, 0.03f),
        eul(0.10f, -0.08f, 0.04f),
        eul(0.09f,  0.30f, 0.06f),
        eul(0.06f,  0.90f, 0.05f),
        eul(0.04f,  1.20f, 0.02f)
    });
    pushRot(clip, J("Head"), t, {
        eul(-0.03f, 0.60f, 0.0f),
        eul(-0.03f, 0.52f, 0.0f),
        eul(-0.02f, 0.32f, 0.0f),
        eul(0.00f,  0.12f, 0.0f),
        eul(0.03f,  0.02f, 0.0f),  // eyes on ball
        eul(0.05f, -0.28f, 0.0f),
        eul(0.02f, -0.70f, 0.0f)   // track flight
    });

    pushRot(clip, J("Clavicle_R"), t, {
        eul(0.05f, 0, 0.10f), eul(0.05f, 0, 0.10f), eul(0.03f, -0.02f, 0.11f),
        eul(0.02f, -0.03f, 0.10f), eul(0.06f, 0.03f, 0.06f), eul(0.04f, 0.04f, 0.03f),
        eul(0.02f, 0.02f, 0.02f)
    });
    pushRot(clip, J("Clavicle_L"), t, {
        eul(0.03f, 0, -0.07f), eul(0.03f, 0, -0.07f), eul(0.04f, 0.02f, -0.08f),
        eul(0.05f, 0.03f, -0.08f), eul(0.03f, 0.01f, -0.04f), eul(0.02f, 0, -0.03f),
        eul(0.02f, 0, -0.02f)
    });

    pushArmStiff(clip, model, t);

    // Arms: high load → keep connected → short path → contact → wrap.
    pushRot(clip, J("Shoulder_R"), t, {
        ohtaniShoulderR(),
        eul(-1.30f, 0.65f, 0.28f),
        eul(-1.22f, 0.30f, 0.20f),
        eul(-1.08f, -0.05f, 0.12f),
        ohtaniContactShR(),
        eul(-0.70f, -0.42f, -0.40f),
        ohtaniFinishShR()
    });
    pushRot(clip, J("Elbow_R"), t, {
        ohtaniElbowR(),
        eul(1.50f, 0, 0),
        eul(1.35f, 0, 0),
        eul(0.95f, 0, 0),
        ohtaniContactElR(),
        eul(0.32f, 0, 0),
        ohtaniFinishElR()
    });
    pushRot(clip, J("Wrist_R"), t, {
        eul(0.14f, 0.06f, 0.08f),
        eul(0.16f, 0.07f, 0.10f),
        eul(0.12f, 0.05f, 0.06f),
        eul(0.06f, 0.02f, 0.03f),
        eul(-0.02f, -0.02f, -0.02f),
        eul(-0.14f, -0.08f, -0.06f),
        eul(-0.04f, -0.02f, -0.02f)
    });
    pushRot(clip, J("Palm_R"), t, {
        eul(0.08f, 0.02f, 0), eul(0.10f, 0.03f, 0), eul(0.07f, 0.02f, 0),
        eul(0.04f, 0.01f, 0), eul(0.01f, 0, 0), eul(-0.05f, -0.02f, 0),
        eul(-0.02f, 0, 0)
    });

    pushRot(clip, J("Shoulder_L"), t, {
        ohtaniShoulderL(),
        eul(-0.78f, 0.50f, 0.55f),
        eul(-0.82f, 0.20f, 0.35f),
        eul(-0.82f, -0.02f, 0.18f),
        ohtaniContactShL(),
        eul(-0.62f, -0.22f, 0.28f),
        ohtaniFinishShL()
    });
    pushRot(clip, J("Elbow_L"), t, {
        ohtaniElbowL(),
        eul(1.30f, 0, 0),
        eul(1.15f, 0, 0),
        eul(0.90f, 0, 0),
        ohtaniContactElL(),
        eul(0.78f, 0, 0),
        ohtaniFinishElL()
    });
    pushRot(clip, J("Wrist_L"), t, {
        eul(0.08f, 0, 0), eul(0.08f, 0, 0), eul(0.06f, 0, 0), eul(0.04f, 0, 0),
        eul(0.02f, 0, 0), eul(-0.03f, 0.02f, 0.02f), eul(-0.01f, 0.01f, 0.01f)
    });

    auto grip = [&](const char* name, float g0, float gContact, float gFinish) {
        pushRot(clip, J(name), t, {
            eul(g0, 0, 0), eul(g0, 0, 0), eul(g0 * 0.98f, 0, 0), eul(g0 * 0.95f, 0, 0),
            eul(gContact, 0, 0), eul(gFinish * 1.4f, 0, 0), eul(gFinish, 0, 0)
        });
    };
    grip("Index_R", 0.58f, 0.50f, 0.16f);
    grip("Middle_R", 0.60f, 0.52f, 0.18f);
    grip("Ring_R", 0.57f, 0.48f, 0.16f);
    grip("Pinky_R", 0.52f, 0.44f, 0.14f);
    pushRot(clip, J("Thumb_R"), t, {
        eul(0.30f, 0.20f, 0.30f), eul(0.30f, 0.20f, 0.30f), eul(0.29f, 0.19f, 0.29f),
        eul(0.27f, 0.17f, 0.26f), eul(0.24f, 0.14f, 0.22f), eul(0.12f, 0.07f, 0.10f),
        eul(0.06f, 0.03f, 0.05f)
    });

    // Lead leg: modern low toe-tap → stride → plant brace.
    pushRot(clip, J("Hip_L"), t, {
        eul(0.14f, 0.12f, 0.16f),
        eul(-0.90f, 0.12f, 0.14f),  // toe-tap lift
        eul(-0.35f, 0.06f, 0.08f),
        eul(-0.08f, -0.02f, 0.04f),
        eul(-0.03f, -0.05f, 0.02f), // plant at contact
        eul(0.08f, -0.06f, 0.0f),
        eul(0.14f, -0.04f, 0.0f)
    });
    pushRot(clip, J("Knee_L"), t, {
        eul(0.30f, 0, 0), eul(1.28f, 0, 0), eul(0.70f, 0, 0), eul(0.42f, 0, 0),
        eul(0.50f, 0, 0), eul(0.36f, 0, 0), eul(0.26f, 0, 0)
    });
    pushRot(clip, J("Ankle_L"), t, {
        eul(-0.06f, 0, 0), eul(0.38f, 0, 0), eul(0.05f, 0, 0), eul(-0.10f, 0, 0),
        eul(-0.22f, 0, 0), eul(-0.12f, 0, 0), eul(-0.06f, 0, 0)
    });
    pushRot(clip, J("Toe_L"), t, {
        eul(0.04f, 0, 0), eul(0.22f, 0, 0), eul(0.08f, 0, 0), eul(0.0f, 0, 0),
        eul(-0.06f, 0, 0), eul(-0.02f, 0, 0), eul(0.02f, 0, 0)
    });

    // Rear leg: load → drive → trail.
    pushRot(clip, J("Hip_R"), t, {
        eul(0.32f, -0.08f, -0.15f),
        eul(0.36f, -0.07f, -0.13f),
        eul(0.44f, -0.04f, -0.08f),
        eul(0.50f, 0.00f, -0.02f),
        eul(0.55f, 0.06f, 0.06f),  // drive
        eul(0.34f, 0.08f, 0.08f),
        eul(0.14f, 0.05f, 0.05f)
    });
    pushRot(clip, J("Knee_R"), t, {
        eul(0.44f, 0, 0), eul(0.48f, 0, 0), eul(0.58f, 0, 0), eul(0.60f, 0, 0),
        eul(0.50f, 0, 0), eul(0.34f, 0, 0), eul(0.40f, 0, 0)
    });
    pushRot(clip, J("Ankle_R"), t, {
        eul(-0.15f, 0, 0), eul(-0.17f, 0, 0), eul(-0.10f, 0, 0), eul(-0.04f, 0, 0),
        eul(0.12f, 0, 0), eul(0.20f, 0, 0), eul(0.04f, 0, 0)
    });
    pushRot(clip, J("Toe_R"), t, {
        eul(0.05f, 0, 0), eul(0.08f, 0, 0), eul(0.06f, 0, 0), eul(0.04f, 0, 0),
        eul(-0.06f, 0, 0), eul(-0.16f, 0, 0), eul(-0.04f, 0, 0)
    });

    return clip;
}

} // namespace BaseballAnims
