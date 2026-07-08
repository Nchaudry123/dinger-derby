#include <cassert>
#include <cmath>

#include "../src/rendering/Mesh3D.h"

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

}

int main() {
    testCubeMeshShape();
    testAxesMeshShape();

    return 0;
}
