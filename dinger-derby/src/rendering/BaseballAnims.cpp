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

// Shared time grid matching Yamamoto phases (seconds, duration 1.55).
// t_norm: 0, 0.08, 0.20, 0.34, 0.44, 0.52, 0.58, 0.63, 0.66, 0.74, 0.86, 1.00
const std::vector<float> kTimes = {
    0.00f, 0.124f, 0.310f, 0.527f, 0.682f, 0.806f,
    0.899f, 0.976f, 1.023f, 1.147f, 1.333f, 1.55f
};

} // namespace

namespace BaseballAnims {

AnimationClip yamamotoWindup(const SkinnedModel3D& model) {
    AnimationClip clip;
    clip.name = "yamamoto_windup";
    clip.duration = 1.55f;

    auto J = [&](const char* n) { return model.findJoint(n); };

    // Hips: start closed (set facing plate), then open through plant/release.
    // Ry < 0 = closed to 3B for RHP; body still aims +Z at home.
    pushRot(clip, J("Hips"), kTimes, {
        eul(0.04f, -0.22f, 0.0f),  // SET
        eul(0.05f, -0.28f, -0.03f),
        eul(0.04f, -0.36f, -0.03f),
        eul(0.03f, -0.42f, -0.02f),
        eul(0.03f, -0.44f, -0.02f),
        eul(0.05f, -0.28f, 0.0f),
        eul(0.08f, 0.02f, 0.02f),
        eul(0.10f, 0.28f, 0.04f),
        eul(0.12f, 0.48f, 0.05f), // release
        eul(0.14f, 0.62f, 0.06f),
        eul(0.08f, 0.40f, 0.03f),
        eul(0.04f, 0.18f, 0.0f)
    });

    // Spine / chest: shoulders lag hips (separation), then fire
    pushRot(clip, J("Spine"), kTimes, {
        eul(0.06f, -0.08f, 0.0f),
        eul(0.06f, -0.12f, -0.02f),
        eul(0.04f, -0.18f, -0.03f),
        eul(0.03f, -0.22f, -0.02f),
        eul(0.03f, -0.24f, -0.02f),
        eul(0.06f, -0.18f, -0.01f),
        eul(0.10f, -0.10f, 0.0f),
        eul(0.12f, 0.04f, 0.02f),
        eul(0.14f, 0.30f, 0.04f),
        eul(0.16f, 0.42f, 0.05f),
        eul(0.10f, 0.25f, 0.02f),
        eul(0.06f, 0.10f, 0.0f)
    });
    pushRot(clip, J("Chest"), kTimes, {
        eul(0.04f, -0.06f, 0.0f),
        eul(0.04f, -0.10f, -0.02f),
        eul(0.03f, -0.16f, -0.02f),
        eul(0.02f, -0.20f, -0.02f),
        eul(0.02f, -0.22f, -0.02f),
        eul(0.04f, -0.20f, -0.01f),
        eul(0.06f, -0.14f, 0.0f),
        eul(0.08f, -0.02f, 0.02f),
        eul(0.10f, 0.36f, 0.05f), // fire
        eul(0.12f, 0.50f, 0.06f),
        eul(0.08f, 0.28f, 0.03f),
        eul(0.04f, 0.12f, 0.0f)
    });

    // Head: eyes on the mitt / plate (+Z) the whole delivery
    pushRot(clip, J("Head"), kTimes, {
        eul(-0.02f, 0.18f, 0.0f),
        eul(-0.02f, 0.16f, 0.0f),
        eul(-0.03f, 0.14f, 0.0f),
        eul(-0.04f, 0.12f, 0.0f),
        eul(-0.04f, 0.12f, 0.0f),
        eul(-0.02f, 0.12f, 0.0f),
        eul(0.0f, 0.10f, 0.0f),
        eul(0.02f, 0.08f, 0.0f),
        eul(0.06f, 0.06f, 0.0f),
        eul(0.04f, -0.04f, 0.0f),
        eul(0.02f, 0.02f, 0.0f),
        eul(0.0f, 0.08f, 0.0f)
    });

    // Lead leg: set has slight flex; negative Rx lifts thigh toward plate/chest.
    pushRot(clip, J("Hip_L"), kTimes, {
        eul(0.10f, 0.04f, 0.05f),   // SET
        eul(-0.35f, 0.06f, 0.06f),
        eul(-1.00f, 0.08f, 0.08f),
        eul(-1.52f, 0.10f, 0.06f),  // peak kick
        eul(-1.55f, 0.10f, 0.06f),  // hold
        eul(-0.90f, 0.05f, 0.04f),
        eul(-0.30f, 0.0f, 0.02f),
        eul(-0.06f, -0.02f, 0.0f),
        eul(-0.04f, -0.04f, 0.0f),
        eul(-0.04f, -0.04f, 0.0f),
        eul(-0.05f, -0.02f, 0.0f),
        eul(0.08f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Knee_L"), kTimes, {
        eul(0.18f, 0.0f, 0.0f),
        eul(0.60f, 0.0f, 0.0f),
        eul(1.20f, 0.0f, 0.0f),
        eul(1.55f, 0.0f, 0.0f),
        eul(1.58f, 0.0f, 0.0f),
        eul(1.00f, 0.0f, 0.0f),
        eul(0.40f, 0.0f, 0.0f),
        eul(0.28f, 0.0f, 0.0f),
        eul(0.20f, 0.0f, 0.0f),
        eul(0.18f, 0.0f, 0.0f),
        eul(0.16f, 0.0f, 0.0f),
        eul(0.14f, 0.0f, 0.0f)
    });
    // Drive COM toward plate on stride
    pushPos(clip, J("Hips"), kTimes, {
        Vector3(0.0f, 0.90f, 0.00f),
        Vector3(0.0f, 0.90f, 0.00f),
        Vector3(0.0f, 0.91f, 0.00f),
        Vector3(0.0f, 0.92f, 0.01f),
        Vector3(0.0f, 0.92f, 0.01f),
        Vector3(0.0f, 0.90f, 0.04f),
        Vector3(0.0f, 0.88f, 0.10f),
        Vector3(0.0f, 0.87f, 0.16f),
        Vector3(0.0f, 0.86f, 0.18f),
        Vector3(0.0f, 0.87f, 0.16f),
        Vector3(0.0f, 0.88f, 0.10f),
        Vector3(0.0f, 0.90f, 0.06f)
    });

    // Plant leg load
    pushRot(clip, J("Hip_R"), kTimes, {
        eul(0.12f, 0.0f, -0.04f),
        eul(0.14f, 0.0f, -0.05f),
        eul(0.16f, 0.0f, -0.05f),
        eul(0.18f, 0.0f, -0.04f),
        eul(0.20f, 0.0f, -0.04f),
        eul(0.28f, 0.0f, -0.02f),
        eul(0.34f, 0.0f, 0.0f),
        eul(0.36f, 0.0f, 0.02f),
        eul(0.30f, 0.0f, 0.04f),
        eul(0.22f, 0.0f, 0.04f),
        eul(0.14f, 0.0f, 0.02f),
        eul(0.12f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Knee_R"), kTimes, {
        eul(0.22f, 0.0f, 0.0f),
        eul(0.24f, 0.0f, 0.0f),
        eul(0.26f, 0.0f, 0.0f),
        eul(0.28f, 0.0f, 0.0f),
        eul(0.30f, 0.0f, 0.0f),
        eul(0.38f, 0.0f, 0.0f),
        eul(0.44f, 0.0f, 0.0f),
        eul(0.46f, 0.0f, 0.0f),
        eul(0.40f, 0.0f, 0.0f),
        eul(0.30f, 0.0f, 0.0f),
        eul(0.22f, 0.0f, 0.0f),
        eul(0.20f, 0.0f, 0.0f)
    });

    // Arms: bind pose already holds set (hands at chest). Channels start near 0,
    // then peel into layback and high 3/4 release toward +Z.
    pushRot(clip, J("Shoulder_L"), kTimes, {
        eul(0.0f, 0.0f, 0.0f),
        eul(0.02f, 0.02f, 0.0f),
        eul(0.04f, 0.04f, 0.0f),
        eul(0.05f, 0.05f, 0.0f),
        eul(0.05f, 0.05f, 0.0f),
        eul(0.15f, -0.10f, -0.05f), // break — glove stays front
        eul(0.25f, -0.15f, -0.08f),
        eul(0.30f, -0.18f, -0.10f),
        eul(0.28f, -0.20f, -0.12f), // tucked
        eul(0.22f, -0.10f, -0.06f),
        eul(0.15f, -0.05f, -0.02f),
        eul(0.05f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Elbow_L"), kTimes, {
        eul(0.0f, 0.0f, 0.0f),
        eul(0.02f, 0.0f, 0.0f),
        eul(0.04f, 0.0f, 0.0f),
        eul(0.05f, 0.0f, 0.0f),
        eul(0.05f, 0.0f, 0.0f),
        eul(-0.15f, 0.0f, 0.0f),
        eul(-0.25f, 0.0f, 0.0f),
        eul(-0.30f, 0.0f, 0.0f),
        eul(-0.28f, 0.0f, 0.0f),
        eul(-0.15f, 0.0f, 0.0f),
        eul(-0.05f, 0.0f, 0.0f),
        eul(0.0f, 0.0f, 0.0f)
    });

    pushRot(clip, J("Shoulder_R"), kTimes, {
        eul(0.0f, 0.0f, 0.0f),       // SET (bind holds chest)
        eul(0.02f, -0.02f, 0.0f),
        eul(0.04f, -0.05f, 0.0f),
        eul(0.05f, -0.08f, 0.0f),
        eul(0.05f, -0.10f, 0.0f),
        eul(0.15f, -0.55f, -0.25f),  // hand break → cock
        eul(0.35f, -1.10f, -0.50f),  // layback
        eul(0.20f, -0.90f, -0.40f),  // plant / max ER
        eul(-1.15f, 0.20f, 0.25f),   // RELEASE high 3/4 to plate
        eul(-0.90f, 0.55f, 0.45f),   // follow
        eul(-0.45f, 0.30f, 0.20f),
        eul(-0.15f, 0.10f, 0.05f)
    });
    pushRot(clip, J("Elbow_R"), kTimes, {
        eul(0.0f, 0.0f, 0.0f),
        eul(0.02f, 0.0f, 0.0f),
        eul(0.04f, 0.0f, 0.0f),
        eul(0.05f, 0.0f, 0.0f),
        eul(0.05f, 0.0f, 0.0f),
        eul(0.20f, 0.0f, 0.0f),
        eul(0.35f, 0.0f, 0.0f),
        eul(0.15f, 0.0f, 0.0f),
        eul(-0.85f, 0.0f, 0.0f), // extension
        eul(-0.70f, 0.0f, 0.0f),
        eul(-0.30f, 0.0f, 0.0f),
        eul(-0.10f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Wrist_R"), kTimes, {
        eul(0.0f, 0.0f, 0.0f),
        eul(0.0f, 0.0f, 0.0f),
        eul(0.02f, 0.0f, 0.0f),
        eul(0.04f, 0.0f, 0.0f),
        eul(0.04f, 0.0f, 0.0f),
        eul(0.10f, 0.0f, 0.0f),
        eul(0.15f, 0.0f, 0.0f),
        eul(0.05f, 0.0f, 0.0f),
        eul(-0.35f, 0.0f, 0.0f), // snap
        eul(-0.18f, 0.0f, 0.0f),
        eul(-0.05f, 0.0f, 0.0f),
        eul(0.0f, 0.0f, 0.0f)
    });

    return clip;
}

AnimationClip pitcherIdle(const SkinnedModel3D& model) {
    // Quiet SET on the rubber: face plate, closed to 3B, hands at chest, soft knees.
    // Matches Yamamoto t=0 so Ready doesn't look like a T-pose.
    AnimationClip clip;
    clip.name = "pitcher_idle";
    clip.duration = 2.0f;
    auto J = [&](const char* n) { return model.findJoint(n); };
    std::vector<float> times = {0.0f, 1.0f, 2.0f};

    // Keep the bind-pose set and only breathe lightly (rest already faces plate).
    pushRot(clip, J("Hips"), times, {
        eul(0.04f, -0.22f, 0.0f),
        eul(0.045f, -0.20f, 0.01f),
        eul(0.04f, -0.22f, 0.0f)
    });
    pushRot(clip, J("Spine"), times, {
        eul(0.06f, -0.08f, 0.0f),
        eul(0.07f, -0.06f, 0.01f),
        eul(0.06f, -0.08f, 0.0f)
    });
    pushRot(clip, J("Chest"), times, {
        eul(0.04f, -0.06f, 0.0f),
        eul(0.05f, -0.05f, 0.0f),
        eul(0.04f, -0.06f, 0.0f)
    });
    pushRot(clip, J("Head"), times, {
        eul(-0.02f, 0.18f, 0.0f),
        eul(-0.01f, 0.16f, 0.01f),
        eul(-0.02f, 0.18f, 0.0f)
    });
    // Identity-ish deltas — rest skeleton already holds hands at sternum.
    pushRot(clip, J("Shoulder_R"), times, {
        eul(0.0f, 0.0f, 0.0f), eul(0.02f, 0.01f, 0.0f), eul(0.0f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Shoulder_L"), times, {
        eul(0.0f, 0.0f, 0.0f), eul(0.02f, -0.01f, 0.0f), eul(0.0f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Elbow_R"), times, {
        eul(0.0f, 0.0f, 0.0f), eul(0.03f, 0.0f, 0.0f), eul(0.0f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Elbow_L"), times, {
        eul(0.0f, 0.0f, 0.0f), eul(0.03f, 0.0f, 0.0f), eul(0.0f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Hip_R"), times, {
        eul(0.12f, 0.0f, -0.04f), eul(0.13f, 0.0f, -0.04f), eul(0.12f, 0.0f, -0.04f)
    });
    pushRot(clip, J("Hip_L"), times, {
        eul(0.10f, 0.04f, 0.05f), eul(0.11f, 0.04f, 0.05f), eul(0.10f, 0.04f, 0.05f)
    });
    pushRot(clip, J("Knee_R"), times, {
        eul(0.22f, 0.0f, 0.0f), eul(0.24f, 0.0f, 0.0f), eul(0.22f, 0.0f, 0.0f)
    });
    pushRot(clip, J("Knee_L"), times, {
        eul(0.18f, 0.0f, 0.0f), eul(0.20f, 0.0f, 0.0f), eul(0.18f, 0.0f, 0.0f)
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
        eul(0.40f, 0.20f, 0.10f),
        eul(0.42f, 0.22f, 0.10f),
        eul(0.40f, 0.20f, 0.10f)
    });
    return clip;
}

} // namespace BaseballAnims
