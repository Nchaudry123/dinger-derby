#include <cassert>
#include <cmath>

#include "rendering/BaseballPlayer3D.h"
#include "rendering/CharacterModel3D.h"
#include "rendering/SkeletonAnimator.h"
#include "rendering/Mesh3D.h"

namespace {

bool nearlyEqual(float a, float b, float tolerance = 0.001f) {
    return std::abs(a - b) <= tolerance;
}

void testCubeMeshShape() {
    Mesh3D cube = Mesh3D::cube(2.0f);

    assert(cube.vertices.size() == 8);
    assert(cube.edges.size() == 12);
    assert(cube.triangles.size() == 12);
    assert(cube.triangleColors.size() == cube.triangles.size());
    assert(cube.triangleNormals.size() == cube.triangles.size());
    assert(nearlyEqual(cube.triangleNormals[0].magnitude(), 1.0f));
    assert(nearlyEqual(cube.vertices[0].x, -1.0f));
    assert(nearlyEqual(cube.vertices[0].y, -1.0f));
    assert(nearlyEqual(cube.vertices[0].z, -1.0f));
    assert(cube.edges[0].start == 0);
    assert(cube.edges[0].end == 1);
    assert(cube.triangles[0].a == 0);
    assert(cube.triangles[0].b == 2);
    assert(cube.triangles[0].c == 1);
}

void testAxesMeshShape() {
    Mesh3D axes = Mesh3D::axes(3.0f);

    assert(axes.vertices.size() == 4);
    assert(axes.edges.size() == 3);
    assert(axes.triangles.empty());
    assert(axes.triangleColors.empty());
    assert(nearlyEqual(axes.vertices[1].x, 3.0f));
    assert(nearlyEqual(axes.vertices[2].y, 3.0f));
    assert(nearlyEqual(axes.vertices[3].z, 3.0f));
}

void testCubeBoundingSphere() {
    Mesh3D cube = Mesh3D::cube(2.0f);
    BoundingSphere3D sphere = cube.localBoundingSphere();

    assert(nearlyEqual(sphere.center.x, 0.0f));
    assert(nearlyEqual(sphere.center.y, 0.0f));
    assert(nearlyEqual(sphere.center.z, 0.0f));
    assert(nearlyEqual(sphere.radius, std::sqrt(3.0f)));
}

void testSphereMeshShape() {
    Mesh3D sphere = Mesh3D::sphere(1.0f, 6, 8);

    assert(!sphere.vertices.empty());
    assert(!sphere.triangles.empty());
    assert(sphere.triangleColors.size() == sphere.triangles.size());
    assert(sphere.triangleNormals.size() == sphere.triangles.size());

    BoundingSphere3D bounds = sphere.localBoundingSphere();
    assert(nearlyEqual(bounds.radius, 1.0f));
}

void testBoxAndPlayerMeshes() {
    Mesh3D box = Mesh3D::box(1.0f, 2.0f, 0.5f);
    assert(box.vertices.size() == 8);
    assert(box.triangles.size() == 12);

    // Canonical characters: CharacterModel3D skinned meshes.
    SkinnedModel3D pitcherModel = CharacterModel3D::build(
        CharacterModel3D::Role::Pitcher, CharacterModel3D::Detail::Low
    );
    SkinnedModel3D catcherModel = CharacterModel3D::build(
        CharacterModel3D::Role::Catcher, CharacterModel3D::Detail::Low
    );
    SkeletonAnimator pAnim;
    SkeletonAnimator cAnim;
    pAnim.setModel(pitcherModel);
    cAnim.setModel(catcherModel);
    pAnim.resetToRest();
    cAnim.resetToRest();
    Mesh3D pitcher = pitcherModel.skinToMesh(pAnim.skinMatrices());
    Mesh3D catcher = catcherModel.skinToMesh(cAnim.skinMatrices());
    assert(pitcher.triangles.size() > 20);
    assert(catcher.triangles.size() > 20);
    assert(pitcher.vertexNormals.size() == pitcher.vertices.size());
    assert(catcher.vertexNormals.size() == catcher.vertices.size());

    BoundingSphere3D pitcherBounds = pitcher.localBoundingSphere();
    BoundingSphere3D catcherBounds = catcher.localBoundingSphere();
    assert(pitcherBounds.radius > 0.5f);
    assert(catcherBounds.radius > 0.4f);
    // Standing pitcher should reach higher than crouching catcher.
    assert(pitcherBounds.center.y + pitcherBounds.radius >
        catcherBounds.center.y + catcherBounds.radius * 0.5f);

    const AnimationClip* throwClip = pitcherModel.findClip("throw_preview");
    assert(throwClip != nullptr);
    pAnim.applyClipNormalized(*throwClip, 0.55f);
    Mesh3D midDelivery = pitcherModel.skinToMesh(pAnim.skinMatrices());
    assert(midDelivery.triangles.size() > 20);
    Vector3 hand = pAnim.throwHandWorld(Matrix4::identity());
    assert(hand.y > 0.8f);
    assert(hand.magnitude() > 0.5f);

    // Legacy rigid pose path still builds (deprecated, coverage only).
    Mesh3D legacyPitcher = BaseballPlayer3D::pitcher(0);
    assert(legacyPitcher.triangles.size() > 20);
    PitcherPose delivery = BaseballPlayer3D::pitcherDeliveryPose(0.55f);
    assert(delivery.throwShoulderPitch != 0.0f || delivery.stride != 0.0f);
}

}

int main() {
    testCubeMeshShape();
    testAxesMeshShape();
    testCubeBoundingSphere();
    testSphereMeshShape();
    testBoxAndPlayerMeshes();

    return 0;
}
