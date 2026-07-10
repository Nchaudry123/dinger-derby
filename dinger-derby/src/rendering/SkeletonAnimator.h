#pragma once

#include <vector>

#include "../math/Matrix4.h"
#include "../math/Quaternion.h"
#include "../math/Vector3.h"
#include "SkinnedModel3D.h"

// Samples animation clips into local joint TRS, builds global + skin matrices.
class SkeletonAnimator {
public:
    void setModel(const SkinnedModel3D& model);
    void resetToRest();

    // timeSeconds in [0, duration]; loop wraps.
    void applyClip(const AnimationClip& clip, float timeSeconds, bool loop = false);
    // t01 in [0,1] for a clip with known duration (Yamamoto windup).
    void applyClipNormalized(const AnimationClip& clip, float t01);

    const std::vector<Matrix4>& skinMatrices() const { return skinMatrices_; }
    const std::vector<Matrix4>& globalPose() const { return globalPose_; }
    const std::vector<Matrix4>& localPose() const { return localPose_; }

    Vector3 jointWorldPosition(int jointIndex) const;
    Vector3 jointWorldPosition(const std::string& name) const;

    // Ball joint if present, else Palm_R, else wrist+offset — for pitch glue.
    Vector3 throwHandWorld(const Matrix4& modelWorld) const;

private:
    const SkinnedModel3D* model_ = nullptr;
    std::vector<Matrix4> localPose_;
    std::vector<Matrix4> globalPose_;
    std::vector<Matrix4> skinMatrices_;
    // Rest local TRS decomposed for channel overrides.
    std::vector<Vector3> restT_;
    std::vector<Quaternion> restR_;
    std::vector<Vector3> restS_;

    void rebuildGlobals();
    void sampleChannel(
        const AnimChannel& channel,
        float time,
        Vector3& t,
        Quaternion& r,
        Vector3& s
    ) const;
};
