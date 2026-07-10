#pragma once

#include <string>

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
// 1) explicit path if non-empty and loadable
// 2) assets/characters/<name>.gltf from CWD and parents
// 3) procedural factory
SkinnedModel3D loadCharacterOrProcedural(
    const std::string& name,
    bool catcher,
    int detail = 2
);
