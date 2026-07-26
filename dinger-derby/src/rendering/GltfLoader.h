#pragma once

#include <optional>
#include <string>

#include "CharacterModel3D.h"
#include "Mesh3D.h"
#include "SkinnedModel3D.h"

struct GltfLoadResult {
    bool ok = false;
    std::string error;
    SkinnedModel3D model;
};

// Minimal glTF 2.0 loader (.gltf + external .bin).
// Supports: POSITION/NORMAL/JOINTS_0/WEIGHTS_0, indices, skins, node TRS,
// LINEAR animation channels (translation/rotation/scale).
// No: Draco, morphs, textures, sparse accessors, .glb (yet).
GltfLoadResult loadGltfFile(const std::string& path);

// Resolve character asset with fallbacks:
// 1) assets/characters/<name>.gltf from CWD and parents
// 2) CharacterModel3D procedural build for the given role
SkinnedModel3D loadCharacterOrProcedural(
    const std::string& name,
    CharacterModel3D::Role role,
    int detail = 2
);

// Load a static (non-skinned) prop mesh: assets/stadium/<name>.gltf from
// CWD and parents. Returns nullopt if no matching file is found or it
// fails to parse — callers fall back to their own procedural geometry.
std::optional<Mesh3D> loadStaticProp(const std::string& name);
