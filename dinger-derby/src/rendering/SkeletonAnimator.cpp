#include "SkeletonAnimator.h"

#include <algorithm>
#include <cmath>

void SkeletonAnimator::setModel(const SkinnedModel3D& model) {
    model_ = &model;
    const int n = static_cast<int>(model.joints.size());
    localPose_.assign(n, Matrix4::identity());
    globalPose_.assign(n, Matrix4::identity());
    skinMatrices_.assign(n, Matrix4::identity());
    restT_.resize(n);
    restR_.resize(n);
    restS_.resize(n);
    for (int i = 0; i < n; i++) {
        restT_[i] = model.joints[i].restTranslation;
        restR_[i] = model.joints[i].restRotation;
        restS_[i] = model.joints[i].restScale;
    }
    resetToRest();
}

void SkeletonAnimator::resetToRest() {
    if (!model_) {
        return;
    }
    const int n = static_cast<int>(model_->joints.size());
    for (int i = 0; i < n; i++) {
        localPose_[i] = Matrix4::fromTrs(restT_[i], restR_[i], restS_[i]);
    }
    rebuildGlobals();
}

void SkeletonAnimator::rebuildGlobals() {
    if (!model_) {
        return;
    }
    const int n = static_cast<int>(model_->joints.size());
    for (int i = 0; i < n; i++) {
        int p = model_->joints[i].parent;
        if (p >= 0 && p < n) {
            globalPose_[i] = globalPose_[p] * localPose_[i];
        } else {
            globalPose_[i] = localPose_[i];
        }
        skinMatrices_[i] = globalPose_[i] * model_->joints[i].inverseBind;
    }
}

void SkeletonAnimator::sampleChannel(
    const AnimChannel& channel,
    float time,
    Vector3& t,
    Quaternion& r,
    Vector3& s
) const {
    if (channel.times.empty()) {
        return;
    }
    const int count = static_cast<int>(channel.times.size());
    if (time <= channel.times.front()) {
        if (channel.path == AnimChannel::Translation) {
            t = Vector3(channel.values[0], channel.values[1], channel.values[2]);
        } else if (channel.path == AnimChannel::Scale) {
            s = Vector3(channel.values[0], channel.values[1], channel.values[2]);
        } else {
            r = Quaternion::fromXyzw(
                channel.values[0], channel.values[1], channel.values[2], channel.values[3]
            );
        }
        return;
    }
    if (time >= channel.times.back()) {
        int last = count - 1;
        if (channel.path == AnimChannel::Translation) {
            t = Vector3(
                channel.values[last * 3 + 0],
                channel.values[last * 3 + 1],
                channel.values[last * 3 + 2]
            );
        } else if (channel.path == AnimChannel::Scale) {
            s = Vector3(
                channel.values[last * 3 + 0],
                channel.values[last * 3 + 1],
                channel.values[last * 3 + 2]
            );
        } else {
            r = Quaternion::fromXyzw(
                channel.values[last * 4 + 0],
                channel.values[last * 4 + 1],
                channel.values[last * 4 + 2],
                channel.values[last * 4 + 3]
            );
        }
        return;
    }

    int i1 = 1;
    while (i1 < count && channel.times[i1] < time) {
        i1++;
    }
    int i0 = i1 - 1;
    float t0 = channel.times[i0];
    float t1 = channel.times[i1];
    float u = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
    if (channel.interp == AnimChannel::Step) {
        u = 0.0f;
    }

    if (channel.path == AnimChannel::Translation) {
        Vector3 a(
            channel.values[i0 * 3 + 0],
            channel.values[i0 * 3 + 1],
            channel.values[i0 * 3 + 2]
        );
        Vector3 b(
            channel.values[i1 * 3 + 0],
            channel.values[i1 * 3 + 1],
            channel.values[i1 * 3 + 2]
        );
        t = a + (b - a) * u;
    } else if (channel.path == AnimChannel::Scale) {
        Vector3 a(
            channel.values[i0 * 3 + 0],
            channel.values[i0 * 3 + 1],
            channel.values[i0 * 3 + 2]
        );
        Vector3 b(
            channel.values[i1 * 3 + 0],
            channel.values[i1 * 3 + 1],
            channel.values[i1 * 3 + 2]
        );
        s = a + (b - a) * u;
    } else {
        Quaternion a = Quaternion::fromXyzw(
            channel.values[i0 * 4 + 0],
            channel.values[i0 * 4 + 1],
            channel.values[i0 * 4 + 2],
            channel.values[i0 * 4 + 3]
        );
        Quaternion b = Quaternion::fromXyzw(
            channel.values[i1 * 4 + 0],
            channel.values[i1 * 4 + 1],
            channel.values[i1 * 4 + 2],
            channel.values[i1 * 4 + 3]
        );
        r = Quaternion::slerp(a, b, u);
    }
}

void SkeletonAnimator::applyClip(const AnimationClip& clip, float timeSeconds, bool loop) {
    if (!model_ || model_->joints.empty()) {
        return;
    }
    float time = timeSeconds;
    if (loop && clip.duration > 1e-5f) {
        time = std::fmod(time, clip.duration);
        if (time < 0.0f) {
            time += clip.duration;
        }
    } else {
        time = std::clamp(time, 0.0f, clip.duration > 0.0f ? clip.duration : time);
    }

    const int n = static_cast<int>(model_->joints.size());
    std::vector<Vector3> t = restT_;
    std::vector<Quaternion> r = restR_;
    std::vector<Vector3> s = restS_;

    for (const AnimChannel& ch : clip.channels) {
        if (ch.jointIndex < 0 || ch.jointIndex >= n) {
            continue;
        }
        sampleChannel(ch, time, t[ch.jointIndex], r[ch.jointIndex], s[ch.jointIndex]);
    }

    for (int i = 0; i < n; i++) {
        localPose_[i] = Matrix4::fromTrs(t[i], r[i], s[i]);
    }
    rebuildGlobals();
}

void SkeletonAnimator::applyClipNormalized(const AnimationClip& clip, float t01) {
    float dur = clip.duration > 1e-5f ? clip.duration : 1.0f;
    applyClip(clip, std::clamp(t01, 0.0f, 1.0f) * dur, false);
}

Vector3 SkeletonAnimator::jointWorldPosition(int jointIndex) const {
    if (!model_ || jointIndex < 0 || jointIndex >= static_cast<int>(globalPose_.size())) {
        return Vector3();
    }
    return globalPose_[jointIndex].transformPoint(Vector3());
}

Vector3 SkeletonAnimator::jointWorldPosition(const std::string& name) const {
    if (!model_) {
        return Vector3();
    }
    return jointWorldPosition(model_->findJoint(name));
}

Vector3 SkeletonAnimator::throwHandWorld(const Matrix4& modelWorld) const {
    // Prefer Ball joint (CharacterModel3D), then palm, then wrist+offset.
    int ball = model_ ? model_->findJoint("Ball") : -1;
    if (ball >= 0) {
        return modelWorld.transformPoint(jointWorldPosition(ball));
    }
    int palm = model_ ? model_->findJoint("Palm_R") : -1;
    if (palm >= 0) {
        return modelWorld.transformPoint(jointWorldPosition(palm));
    }
    int wrist = model_ ? model_->findJoint("Wrist_R") : -1;
    int elbow = model_ ? model_->findJoint("Elbow_R") : -1;
    if (wrist < 0) {
        return modelWorld.transformPoint(Vector3(0.2f, 1.2f, 0.15f));
    }
    Vector3 w = jointWorldPosition(wrist);
    if (elbow >= 0) {
        Vector3 e = jointWorldPosition(elbow);
        Vector3 dir = w - e;
        float m = dir.magnitude();
        if (m > 1e-4f) {
            w = w + dir * (0.06f / m);
        }
    }
    return modelWorld.transformPoint(w);
}
