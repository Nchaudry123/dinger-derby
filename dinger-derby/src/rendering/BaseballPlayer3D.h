#pragma once

#include "Mesh3D.h"
#include "../math/Vector3.h"

// Procedural joint channels (radians / normalized as noted).
struct PitcherPose {
    float torsoTwist = 0.0f;
    float torsoLean = 0.0f;
    float torsoSide = 0.0f;
    float headTurn = 0.0f;
    float headNod = 0.0f;
    // Throw arm (right for RHP). Pitch ~ elevation/slot; yaw ~ cocking vs finish.
    float throwShoulderPitch = 0.0f;
    float throwShoulderYaw = 0.0f;
    float throwElbow = 0.0f;   // 0 extended, ~1 fully bent
    float throwWrist = 0.0f;
    // Glove arm (left).
    float gloveShoulderPitch = 0.0f;
    float gloveShoulderYaw = 0.0f;
    float gloveElbow = 0.0f;
    float frontLegLift = 0.0f; // 0..1 knee height
    float frontKneeBend = 0.0f;
    float plantKneeBend = 0.0f;
    float hipOpen = 0.0f;      // hips rotate open toward plate
    float stride = 0.0f;       // front foot travel toward plate (meters-ish)
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

// Continuous game-character pitcher/catcher + keyframed delivery.
class BaseballPlayer3D {
public:
    // detail: 0 performance, 1 default, 2 high poly.
    static Mesh3D pitcher(int detail = 2, const PitcherPose& pose = PitcherPose());
    static Mesh3D catcher(int detail = 2, const CatcherPose& pose = CatcherPose());

    // Model-space palm where the ball sits (feet origin, +Z toward plate).
    static Vector3 throwHandLocal(const PitcherPose& pose);

    static PitcherPose pitcherIdlePose(float timeSeconds);
    // t∈[0,1]: Yamamoto windup — set → high kick (hands together) →
    // plant/layback → high 3/4 release(~0.66) → finish.
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
