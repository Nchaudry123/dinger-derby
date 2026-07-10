#pragma once

#include "SkinnedModel3D.h"

// Authored animation clips for baseball characters (bone-space).
namespace BaseballAnims {

// Yamamoto windup (FanGraphs slow-mo timing). Duration 1.55s; release ~1.023s (t=0.66).
AnimationClip yamamotoWindup(const SkinnedModel3D& model);

// Quiet set / breathing on the rubber.
AnimationClip pitcherIdle(const SkinnedModel3D& model);

// Simple catcher crouch idle.
AnimationClip catcherIdle(const SkinnedModel3D& model);

} // namespace BaseballAnims
