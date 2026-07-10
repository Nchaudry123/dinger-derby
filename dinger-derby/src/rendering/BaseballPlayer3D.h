#pragma once

#include "Mesh3D.h"
#include "../math/Vector3.h"

// Joint offsets for procedural animation (radians unless noted).
struct PitcherPose {
    float torsoTwist = 0.0f;
    float torsoLean = 0.0f;
    float torsoSide = 0.0f;
    float headTurn = 0.0f;
    float headNod = 0.0f;
    float throwShoulderPitch = 0.0f;
    float throwShoulderYaw = 0.0f;
    float throwElbow = 0.0f;
    float throwWrist = 0.0f;
    float gloveShoulderPitch = 0.0f;
    float gloveShoulderYaw = 0.0f;
    float gloveElbow = 0.0f;
    float frontLegLift = 0.0f;
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

class BaseballPlayer3D {
public:
    static Mesh3D pitcher(int detail = 1, const PitcherPose& pose = PitcherPose());
    static Mesh3D catcher(int detail = 1, const CatcherPose& pose = CatcherPose());

    // Throw-hand position in model space (feet origin, +Z toward plate).
    static Vector3 throwHandLocal(const PitcherPose& pose);

    // t in [0,1]: load → lift → drive → release(~0.58) → follow-through.
    static PitcherPose pitcherDeliveryPose(float t);
    static PitcherPose pitcherIdlePose(float timeSeconds);
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
