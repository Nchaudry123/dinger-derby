#include "rendering/BaseballAnims.h"
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
    SkinnedModel3D pitcher = SkinnedModel3D::makeProceduralPitcher(1);
    expect(!pitcher.joints.empty(), "has joints");
    expect(!pitcher.bindVertices.empty(), "has verts");
    expect(!pitcher.triangles.empty(), "has tris");
    expect(pitcher.findJoint("Wrist_R") >= 0, "has Wrist_R");
    expect(pitcher.findJoint("Palm_R") >= 0, "has Palm_R");
    expect(pitcher.findJoint("Knee_L") >= 0, "has Knee_L");

    SkeletonAnimator anim;
    anim.setModel(pitcher);
    anim.resetToRest();
    Mesh3D rest = pitcher.skinToMesh(anim.skinMatrices());
    expect(rest.vertices.size() == pitcher.bindVertices.size(), "skin vert count");
    expect(!rest.triangles.empty(), "skin tris");

    // No NaNs in rest pose.
    for (const Vector3& v : rest.vertices) {
        expect(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z), "finite vert");
    }

    AnimationClip windup = BaseballAnims::yamamotoWindup(pitcher);
    expect(windup.duration > 1.0f, "windup duration");
    expect(!windup.channels.empty(), "windup channels");

    // Peak kick ~ t=0.34 — knee clearly elevated vs rest; hands still high.
    anim.resetToRest();
    Vector3 kneeRest = anim.jointWorldPosition("Knee_L");
    anim.applyClipNormalized(windup, 0.34f);
    Vector3 knee = anim.jointWorldPosition("Knee_L");
    Vector3 hip = anim.jointWorldPosition("Hips");
    Vector3 wristKick = anim.jointWorldPosition("Wrist_R");
    Vector3 gloveKick = anim.jointWorldPosition("Wrist_L");
    expect(knee.y > kneeRest.y + 0.15f, "high kick elevates lead knee");
    expect(knee.y > hip.y - 0.20f, "high kick knee near hip height");
    expect((wristKick - gloveKick).magnitude() < 0.55f, "hands stay near each other on kick");

    // Layback ~ t=0.58 — throwing elbow high (near/above shoulder).
    anim.applyClipNormalized(windup, 0.58f);
    Vector3 elbowLay = anim.jointWorldPosition("Elbow_R");
    Vector3 shoulderLay = anim.jointWorldPosition("Shoulder_R");
    expect(elbowLay.y > shoulderLay.y - 0.08f, "layback elbow near shoulder height");

    // Release ~ t=0.66 — arm extended, hand high, ball path toward plate.
    anim.applyClipNormalized(windup, 0.66f);
    Vector3 wrist = anim.jointWorldPosition("Wrist_R");
    Vector3 elbow = anim.jointWorldPosition("Elbow_R");
    Vector3 palm = anim.jointWorldPosition("Palm_R");
    expect((wrist - elbow).magnitude() > 0.18f, "arm extended at release");
    expect(palm.y > 1.15f, "release hand high (3/4 slot)");
    expect(palm.z > elbow.z - 0.05f, "palm at or ahead of elbow toward plate");

    Mesh3D posed = pitcher.skinToMesh(anim.skinMatrices());
    expect(posed.vertices.size() == rest.vertices.size(), "posed vert count");

    // Hand helper
    Matrix4 world = Matrix4::translation(Vector3(0.0f, 0.0f, 1.0f));
    Vector3 hand = anim.throwHandWorld(world);
    expect(std::isfinite(hand.x) && std::isfinite(hand.y) && std::isfinite(hand.z), "hand finite");

    SkinnedModel3D catcher = SkinnedModel3D::makeProceduralCatcher(1);
    expect(catcher.findJoint("Head") >= 0, "catcher head");

    if (gFails == 0) {
        std::cout << "skinned_model_test OK\n";
        return 0;
    }
    std::cerr << gFails << " failure(s)\n";
    return 1;
}
