#include "rendering/CharacterModel3D.h"
#include "rendering/SkeletonAnimator.h"
#include "rendering/SkinnedModel3D.h"

#include <cmath>
#include <iostream>

static int gFails = 0;

static void expect(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        gFails++;
    }
}

int main() {
    SkinnedModel3D pitcher = CharacterModel3D::build(
        CharacterModel3D::Role::Pitcher,
        CharacterModel3D::Detail::Medium
    );
    expect(!pitcher.joints.empty(), "has joints");
    expect(!pitcher.bindVertices.empty(), "has verts");
    expect(!pitcher.triangles.empty(), "has tris");
    expect(pitcher.findJoint("Wrist_R") >= 0, "has Wrist_R");
    expect(pitcher.findJoint("Palm_R") >= 0, "has Palm_R");
    expect(pitcher.findJoint("Ball") >= 0, "has Ball attach");
    expect(pitcher.findJoint("HumTwist_R") >= 0, "has HumTwist_R");
    expect(pitcher.findJoint("Knee_L") >= 0, "has Knee_L");
    expect(pitcher.findClip("throw_preview") != nullptr, "has throw_preview");
    expect(pitcher.findClip("idle") != nullptr, "has idle");

    SkeletonAnimator anim;
    anim.setModel(pitcher);
    anim.resetToRest();
    Mesh3D rest = pitcher.skinToMesh(anim.skinMatrices());
    expect(rest.vertices.size() == pitcher.bindVertices.size(), "skin vert count");
    expect(!rest.triangles.empty(), "skin tris");

    for (const Vector3& v : rest.vertices) {
        expect(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z), "finite vert");
    }

    const AnimationClip* throwClip = pitcher.findClip("throw_preview");
    expect(throwClip != nullptr, "throw clip ptr");
    if (!throwClip) {
        return 1;
    }
    expect(throwClip->duration > 1.5f, "throw duration");
    expect(!throwClip->channels.empty(), "throw channels");

    // Peak kick ~ t=0.52 / 2.20 ≈ 0.236 — lead knee elevated.
    anim.resetToRest();
    Vector3 kneeRest = anim.jointWorldPosition("Knee_L");
    anim.applyClipNormalized(*throwClip, 0.24f);
    Vector3 knee = anim.jointWorldPosition("Knee_L");
    Vector3 hip = anim.jointWorldPosition("Hips");
    expect(knee.y > kneeRest.y + 0.12f, "high kick elevates lead knee");
    expect(knee.y > hip.y - 0.35f, "high kick knee near hip height");

    // Plant / max ER ~ t=1.04 / 2.20 ≈ 0.47 — elbow near/above shoulder.
    anim.applyClipNormalized(*throwClip, 0.47f);
    Vector3 elbowLay = anim.jointWorldPosition("Elbow_R");
    Vector3 shoulderLay = anim.jointWorldPosition("Shoulder_R");
    expect(elbowLay.y > shoulderLay.y - 0.12f, "plant elbow near shoulder height");

    // Release ~ t=1.22 / 2.20 ≈ 0.555 — arm extended, hand high.
    anim.applyClipNormalized(*throwClip, 0.555f);
    Vector3 wrist = anim.jointWorldPosition("Wrist_R");
    Vector3 elbow = anim.jointWorldPosition("Elbow_R");
    Vector3 palm = anim.jointWorldPosition("Palm_R");
    expect((wrist - elbow).magnitude() > 0.18f, "arm extended at release");
    expect(palm.y > 0.95f, "release hand high (¾ slot)");

    // Limb lengths stay rigid (no noodle melt).
    Vector3 sh = anim.jointWorldPosition("Shoulder_R");
    expect(std::abs((elbow - sh).magnitude() - 0.325f) < 0.04f, "upper arm length stable");
    expect(std::abs((wrist - elbow).magnitude() - 0.265f) < 0.04f, "forearm length stable");

    Mesh3D posed = pitcher.skinToMesh(anim.skinMatrices());
    expect(posed.vertices.size() == rest.vertices.size(), "posed vert count");

    Matrix4 world = Matrix4::translation(Vector3(0.0f, 0.0f, 1.0f));
    Vector3 hand = anim.throwHandWorld(world);
    expect(std::isfinite(hand.x) && std::isfinite(hand.y) && std::isfinite(hand.z), "hand finite");
    expect(pitcher.findJoint("Ball") >= 0, "Ball joint for glue");

    SkinnedModel3D catcher = CharacterModel3D::build(
        CharacterModel3D::Role::Catcher,
        CharacterModel3D::Detail::Medium
    );
    expect(catcher.findJoint("Head") >= 0, "catcher head");
    expect(catcher.findClip("catcher_idle") != nullptr, "catcher_idle");
    expect(catcher.findClip("receive") != nullptr, "receive");

    // Factory wrappers route to CharacterModel3D.
    SkinnedModel3D viaFactory = SkinnedModel3D::makeProceduralPitcher(1);
    expect(viaFactory.findClip("throw_preview") != nullptr, "factory has throw_preview");

    if (gFails == 0) {
        std::cout << "skinned_model_test OK\n";
        return 0;
    }
    std::cerr << gFails << " failure(s)\n";
    return 1;
}
