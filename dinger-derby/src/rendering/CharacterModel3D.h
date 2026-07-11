#pragma once

#include <string>
#include <vector>

#include "SkinnedModel3D.h"

// Realistic procedural baseball athlete rebuilt from human proportions.
// ~1.78 m male, 8-head canon, natural standing rest (arms hang straight).
// Multi-bone arms (Shoulder→UpperArm→HumTwist→Elbow→Forearm→ProTwist→Wrist→Palm)
// for fluid throw skinning. throw_preview = RHP sideways set → windup → plate.
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
