#pragma once

#include "Mesh3D.h"

// Procedural low-poly baseball players (no external assets).
// Units match the pitching field (adult ~1.85–1.95 tall when standing).
class BaseballPlayer3D {
public:
    // Standing set position, facing +Z (toward the plate). Origin at feet center.
    static Mesh3D pitcher(int detail = 1);

    // Crouch behind the plate, facing -Z (toward the mound). Origin at feet center.
    static Mesh3D catcher(int detail = 1);
};
