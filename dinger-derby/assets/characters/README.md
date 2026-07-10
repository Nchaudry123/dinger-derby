# Character assets (glTF)

The pitching demo prefers skinned glTF characters when present:

- `pitcher.gltf` (+ optional `.bin`)
- `catcher.gltf` (+ optional `.bin`)

If a file is missing or fails to load, the engine falls back to a **procedural skinned** humanoid with the same joint names and a built-in Yamamoto windup clip.

## Expected joint names

For ball attach and `BaseballAnims::yamamotoWindup` to retarget cleanly:

| Joint | Role |
|-------|------|
| `Root`, `Hips`, `Spine`, `Chest`, `Neck`, `Head` | Core |
| `Shoulder_L/R`, `Elbow_L/R`, `Wrist_L/R` | Arms |
| `Palm_R` | Ball glue (optional; wrist used if missing) |
| `Hip_L/R`, `Knee_L/R`, `Ankle_L/R` | Legs (L = lead for RHP) |

## Export from Blender

1. Model a low/mid-poly pitcher, +Y up, facing +Z (plate).
2. Armature with the joint names above (or rename after export).
3. Parent mesh with automatic weights; apply transforms.
4. Export **glTF 2.0** (separate `.gltf` + `.bin`), include skins + animations.
5. Name the windup clip `yamamoto_windup` (optional — code also ships a clip).

## Runtime search paths

- `assets/characters/<name>.gltf`
- `../assets/characters/<name>.gltf` (when launched from `build/`)
