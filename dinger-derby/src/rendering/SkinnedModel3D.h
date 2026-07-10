#pragma once

#include <string>
#include <vector>
#include <SFML/Graphics/Color.hpp>

#include "../math/Matrix4.h"
#include "../math/Quaternion.h"
#include "../math/Vector3.h"
#include "Mesh3D.h"

struct Joint3D {
    std::string name;
    int parent = -1;
    Vector3 restTranslation;
    Quaternion restRotation = Quaternion::identity();
    Vector3 restScale = Vector3(1.0f, 1.0f, 1.0f);
    Matrix4 localRest = Matrix4::identity();
    Matrix4 inverseBind = Matrix4::identity();

    void bakeLocalRest() {
        localRest = Matrix4::fromTrs(restTranslation, restRotation, restScale);
    }
};

struct SkinVertex {
    Vector3 position;
    Vector3 normal;
    sf::Color color = sf::Color::White;
    int joints[4] = {0, 0, 0, 0};
    float weights[4] = {1.0f, 0.0f, 0.0f, 0.0f};
};

struct AnimChannel {
    int jointIndex = 0;
    enum Path { Translation, Rotation, Scale } path = Translation;
    std::vector<float> times;
    // Translation/Scale: 3 floats per key. Rotation: 4 floats (xyzw) per key.
    std::vector<float> values;
    enum Interp { Linear, Step } interp = Linear;
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimChannel> channels;
};

// CPU-skinned character: bind mesh + skeleton + optional clips.
// Skins into Mesh3D for the existing software raster path.
class SkinnedModel3D {
public:
    std::vector<SkinVertex> bindVertices;
    std::vector<Triangle3D> triangles;
    std::vector<Joint3D> joints;
    std::vector<AnimationClip> clips;

    int findJoint(const std::string& name) const;
    const AnimationClip* findClip(const std::string& name) const;

    // Linear-blend skin → Mesh3D (allocating). Prefer skinInto for hot path.
    Mesh3D skinToMesh(const std::vector<Matrix4>& skinMatrices) const;
    void skinInto(const std::vector<Matrix4>& skinMatrices, Mesh3D& out) const;

    // Compact athletic RHP / catcher with joint weights (no external file).
    static SkinnedModel3D makeProceduralPitcher(int detail = 2);
    static SkinnedModel3D makeProceduralCatcher(int detail = 2);

    // After building joints localRest, fill inverseBind from rest globals.
    void rebuildInverseBindsFromRest();
};
