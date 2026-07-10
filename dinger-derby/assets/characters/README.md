# Character assets (glTF)

The pitching demo prefers skinned glTF characters when present:

- `pitcher.gltf` (+ optional `.bin`)
- `catcher.gltf` (+ optional `.bin`)

If a file is missing or fails to load, the engine falls back to
**CharacterModel3D** (procedural multi-bone athlete) with built-in clips:

| Clip | Role |
|------|------|
| `throw_preview` | RHP delivery (ball in glove → windup → throw) |
| `idle` | Breathing / weight shift |
| `crouch` | Catcher set |
| `rest`, `tpose`, `arms_out`, `wave`, `walk` | Workshop / debug |

## Expected joint names

For ball attach and delivery retargeting:

| Joint | Role |
|-------|------|
| `Root`, `Hips`, `Spine`, `Chest`, `Neck`, `Head` | Core |
| `Clavicle_L/R` | Shoulder girdle |
| `Shoulder_L/R`, `UpperArm_L/R`, `HumTwist_L/R` | Upper arm |
| `Elbow_L/R`, `Forearm_L/R`, `ProTwist_L/R` | Forearm |
| `Wrist_L/R`, `Palm_L/R` | Hands |
| `Ball` | Pitch glue (child of Palm_R; preferred over palm) |
| `Hip_L/R`, `Knee_L/R`, `Ankle_L/R`, `Toe_L/R` | Legs (L = lead for RHP) |

Workshop demo: `character_viewer_demo --clip throw_preview`
