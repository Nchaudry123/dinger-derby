#pragma once

#include "Mesh3D.h"

// Joint offsets in radians / unit offsets for procedural poses.
// Zero = default set / crouch rest pose.
struct PitcherPose {
    float torsoTwist = 0.0f;      // Y rotation of upper body
    float torsoLean = 0.0f;       // X lean toward plate
    float throwShoulderPitch = 0.0f;
    float throwShoulderYaw = 0.0f;
    float throwElbow = 0.0f;
    float gloveShoulderPitch = 0.0f;
    float gloveShoulderRoll = 0.0f;
    float frontLegLift = 0.0f;     // lead knee raise (0..1-ish scale)
    float stride = 0.0f;          // forward weight shift along +Z
};

struct CatcherPose {
    float torsoSway = 0.0f;
    float crouchBob = 0.0f;
    float mittSide = 0.0f;        // local X (model faces -Z when placed)
    float mittHeight = 0.0f;      // local Y
    float mittReach = 0.0f;       // local Z toward pitcher
    float gloveOpen = 0.0f;       // 0 closed pocket, 1 more open
};

// Procedural low-poly baseball players (no external assets).
class BaseballPlayer3D {
public:
    // Standing set, facing +Z (toward plate). Origin at feet center.
    static Mesh3D pitcher(int detail = 1, const PitcherPose& pose = PitcherPose());

    // Crouch, facing -Z (toward mound) in model space. Origin at feet center.
    static Mesh3D catcher(int detail = 1, const CatcherPose& pose = CatcherPose());

    // Sampled pitch-delivery poses (t in [0,1]: 0 idle set → windup → release ~0.55 → follow-through 1).
    static PitcherPose pitcherDeliveryPose(float t);
    // Idle breathing / sway.
    static PitcherPose pitcherIdlePose(float timeSeconds);
    static CatcherPose catcherIdlePose(float timeSeconds);
    // Mitt tracks a plate-plane aim point (world), given catcher world position.
    static CatcherPose catcherReceivePose(
        float timeSeconds,
        float aimX,
        float aimY,
        float catcherWorldX,
        float catcherWorldY
    );
};
