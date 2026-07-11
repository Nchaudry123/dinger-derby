# Character assets (glTF)

The pitching demo prefers skinned glTF characters when present:

- `pitcher.gltf` (+ optional `.bin`)
- `catcher.gltf` (+ optional `.bin`)
- `batter.gltf` (+ optional `.bin`) — optional future override

If a file is missing or fails to load, the engine falls back to
**CharacterModel3D v2** (procedural athletic multi-bone body) with built-in clips:

| Clip | Role |
|------|------|
| `throw_preview` | RHP delivery (ball in glove → windup → throw) |
| `idle` | Breathing / weight shift |
| `crouch` | Catcher set |
| `rest`, `tpose`, `arms_out`, `wave`, `walk` | Workshop / debug |

**Batter plate anims** (authored in `BaseballAnims`, not joint-embedded):

| Clip | Role |
|------|------|
| `batter_stance` | RHB ready set — upright, high hands, quiet rhythm |
| `batter_swing` | RHB swing — kinematic sequence, contact @ t≈0.42 |

### Animation accuracy notes

Procedural clips are timed from published biomechanics (not ripped mocap):

| Source | What we encode |
|--------|----------------|
| **Welch et al., JOSPT 1995** | Rear-foot load + trunk coil → stride → front-foot block (~123% BW) → hip peak velocity before torso/shoulder |
| **Fortenbaugh 2011** | Phases: stance · stride · coiling · initiation · acceleration · follow-through |
| **K-Vest / Driveline / RPP** | 1-2-3-4 sequence: **pelvis → torso → lead arm → hand**; each segment peaks then decelerates into the next |
| **Ohtani MLB (2018+)** | Subtler **toe-tap** vs NPB high kick (MLB.com / Scioscia); early foot down; hands quiet while lower half starts; Pujols-style timing cue |
| **Stance coaching norms** | Feet ≈ shoulder-width+, soft knees, hands near rear shoulder/ear |

**Swing timing (normalized, contact @ 0.42 for `BatPose.swingT`):**

`load → toe-tap(0.15) → stride → plant(0.28) → hip fire(0.35) → CONTACT(0.42) → extend → wrap`

**Open asset path (optional, not shipped):** Mixamo free “baseball swing” FBX →
Blender retarget → glTF with matching joint names below (Don McCurdy Mixamo→glTF).
Do **not** ship MLB The Show / broadcast / player mocap — licensing.
`batter.gltf` can override the procedural CharacterModel3D when present.

## Expected joint names

For ball attach and delivery retargeting:

| Joint | Role |
|-------|------|
| `Root`, `Hips` | Root / pelvis |
| `Spine`, `Spine2`, `Chest`, `Neck`, `Head` | 3-spine torso corkscrew |
| `Clavicle_L/R` | Shoulder girdle |
| `Shoulder_L/R`, `UpperArm_L/R`, `HumTwist_L/R` | Upper arm + humeral twist |
| `Elbow_L/R`, `Forearm_L/R`, `ProTwist_L/R` | Forearm + pronation |
| `Wrist_L/R`, `Palm_L/R` | Hands |
| `Index/Middle/Ring/Pinky/Thumb_L/R` | Finger grip (batter both hands; throw R) |
| `Ball` | Pitch glue only (child of Palm_R; no mesh — sim/viewer draw one ball) |
| `Hip_L/R`, `Thigh_L/R`, `ThighTwist_L/R` | Upper leg + femoral twist |
| `Knee_L/R`, `Shin_L/R`, `ShinTwist_L/R` | Lower leg + tibial twist |
| `Ankle_L/R`, `Toe_L/R` | Foot (L = lead for RHP / RHB) |

Workshop demo: `character_viewer_demo --clip throw_preview`  
Athlete (T) exposes `batter_stance` / `batter_swing` (BaseballAnims).  
Skeleton: **G** bones · **N** joint names (mint torso / amber arms / sky legs / pink fingers; ring = twist/Spine2).
