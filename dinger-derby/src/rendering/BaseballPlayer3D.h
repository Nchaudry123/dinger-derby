#pragma once

#include "Mesh3D.h"
#include "../math/Vector3.h"

// Joint channels for procedural animation.
// Angles are loosely radian-scaled; frontLegLift / elbow are 0..1-ish.
struct PitcherPose {
    float torsoTwist = 0.0f;
    float torsoLean = 0.0f;
    float torsoSide = 0.0f;
    float headTurn = 0.0f;
    float headNod = 0.0f;
    // Throw arm (RHP right). Pitch: layback <0 · set ~0.5 · release >1.
    // Yaw: cocked closed <0 · open/finish >0. Elbow: 0 straight · 1 bent.
    float throwShoulderPitch = 0.0f;
    float throwShoulderYaw = 0.0f;
    float throwElbow = 0.0f;
    float throwWrist = 0.0f;
    float gloveShoulderPitch = 0.0f;
    float gloveShoulderYaw = 0.0f;
    float gloveElbow = 0.0f;
    float frontLegLift = 0.0f; // 0..1 vertical kick
    float frontKneeBend = 0.0f;
    float plantKneeBend = 0.0f;
    float hipOpen = 0.0f;
    float stride = 0.0f;
};

struct CatcherPose {
    float torsoSway = 0.0f;
    float torsoLean = 0.0f;
    float crouchBob = 0.0f;
    float headTurn = 0.0f;
    float mittSide = 0.0f;
    float mittHeight = 0.0f;
    float mittReach = 0.0f;
    float gloveOpen = 0.0f;
    float freeArmBrace = 0.0f;
};

// DEPRECATED: rigid pose-mesh path. Prefer CharacterModel3D + SkeletonAnimator
// for gameplay characters. Kept for legacy mesh3d_test and offline experiments.
class BaseballPlayer3D {
public:
    // detail: 0 performance · 1 default · 2 high (recommended).
    static Mesh3D pitcher(int detail = 2, const PitcherPose& pose = PitcherPose());
    static Mesh3D catcher(int detail = 2, const CatcherPose& pose = CatcherPose());

    // Model-space palm where the baseball sits (+Z toward plate).
    static Vector3 throwHandLocal(const PitcherPose& pose);

    static PitcherPose pitcherIdlePose(float timeSeconds);
    // t∈[0,1] Yamamoto windup (FanGraphs slow-mo):
    // set → rocker → high kick (hands together) → balance → hand break →
    // plant/layback → high 3/4 release at ~0.66 → finish.
    static PitcherPose pitcherDeliveryPose(float t);
    static CatcherPose catcherIdlePose(float timeSeconds);
    static CatcherPose catcherReceivePose(
        float timeSeconds,
        float aimX,
        float aimY,
        float catcherWorldX,
        float catcherWorldY
    );

    static PitcherPose blend(const PitcherPose& a, const PitcherPose& b, float t);
    static CatcherPose blend(const CatcherPose& a, const CatcherPose& b, float t);
};
