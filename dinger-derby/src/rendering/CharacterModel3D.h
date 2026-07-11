#pragma once

#include <string>
#include <vector>

#include "SkinnedModel3D.h"

// Procedural baseball athlete v3 — bold silhouette, team colors, solid volumes.
// ~1.78 m male, game-readable head/shoulders; natural stand (arms hang −Y).
// Fluid game-rig (same joint names for BaseballAnims / optional glTF):
//   Spine: Hips→Spine→Spine2→Chest→Neck→Head
//   Arms:  Clavicle→Shoulder→UpperArm→HumTwist→Elbow→Forearm→ProTwist→Wrist→Palm
//   Legs:  Hip→Thigh→ThighTwist→Knee→Shin→ShinTwist→Ankle→Toe
// Roles: Athlete (blue sleeves + number), Pitcher (white jersey/navy cap),
//        Catcher (navy gear + helmet cage).
namespace CharacterModel3D {

enum class Role {
    Athlete, // plain athletic body
    Pitcher, // jersey, cap, glove L, bare R hand
    Catcher  // gear + helmet + crouch-capable rest
};

enum class Detail {
    Low = 0,
    Medium = 1,
    High = 2
};

struct BuildInfo {
    int jointCount = 0;
    int vertexCount = 0;
    int triangleCount = 0;
    float heightMeters = 0.0f;
    std::string summary;
};

// Clips attached after build:
//   rest, idle, tpose, arms_out, wave, throw_preview, crouch, walk
// Catcher also: catcher_idle, receive
// Batter plate anims (BaseballAnims): batter_stance, batter_swing
SkinnedModel3D build(Role role = Role::Pitcher, Detail detail = Detail::High);

BuildInfo inspect(const SkinnedModel3D& model);
const AnimationClip* findClip(const SkinnedModel3D& model, const std::string& name);
std::vector<std::string> clipNames(const SkinnedModel3D& model);
const char* roleName(Role role);
const char* detailName(Detail detail);

} // namespace CharacterModel3D
