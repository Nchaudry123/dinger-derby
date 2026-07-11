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

// Ohtani-inspired RHB plate stance — upright, high hands, wide base, rhythm loop.
// Model faces +Z (apply world rotY(π) so batter looks at mound / −Z).
AnimationClip batterStance(const SkinnedModel3D& model);

// Ohtani-inspired RHB swing: coil → quick toe-tap → plant → contact → high wrap.
// Duration 0.55s; contact at t_norm ≈ 0.42 (matches BatPose). Drive with
// applyClipNormalized(clip, bat.swingT).
AnimationClip batterSwing(const SkinnedModel3D& model);

} // namespace BaseballAnims
