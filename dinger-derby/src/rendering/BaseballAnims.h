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

// RHB plate stance — classic high hands at rear ear/shoulder, bat tip overhead
// (~45°) behind the head. Quiet rhythm, shoulder-width+ base.
// Model faces +Z (world rotY(π) → looks at mound / −Z).
// Refs: Hitting Vault stance cues; MLB high-hand set (not flat across the zone).
AnimationClip batterStance(const SkinnedModel3D& model);

// RHB swing keyed to 1-2-3-4 kinematic sequence: pelvis → torso → arm → hand.
// Phases: load/coil · toe-tap · stride · early plant · hip fire · contact ·
// extend · wrap. Duration 0.60s; contact at t_norm ≈ 0.42 (BatPose.swingT).
// Drive with applyClipNormalized(clip, bat.swingT).
// Refs: Welch et al. JOSPT 1995; Fortenbaugh 2011 phases; K-Vest/Driveline/RPP
// sequencing; Ohtani MLB toe-tap (not NPB high kick) + early foot down.
AnimationClip batterSwing(const SkinnedModel3D& model);

} // namespace BaseballAnims
