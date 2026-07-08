#include <cassert>
#include <vector>

#include "../src/rendering/SpatialGrid3D.h"

namespace {

bool contains(const std::vector<int>& values, int expected) {
    for (int value : values) {
        if (value == expected) {
            return true;
        }
    }

    return false;
}

void testQueryReturnsInsertedObject() {
    SpatialGrid3D grid(4.0f);
    std::vector<int> results;

    grid.insertSphere(7, Vector3(1.0f, 2.0f, 3.0f), 0.5f);
    grid.queryAabb(Vector3(0.0f, 0.0f, 0.0f), Vector3(2.0f, 3.0f, 4.0f), results);

    assert(results.size() == 1);
    assert(results[0] == 7);
}

void testQuerySkipsObjectsOutsideAabb() {
    SpatialGrid3D grid(4.0f);
    std::vector<int> results;

    grid.insertSphere(1, Vector3(1.0f, 0.0f, 0.0f), 0.5f);
    grid.insertSphere(2, Vector3(30.0f, 0.0f, 0.0f), 0.5f);

    grid.queryAabb(Vector3(-2.0f, -2.0f, -2.0f), Vector3(3.0f, 2.0f, 2.0f), results);

    assert(results.size() == 1);
    assert(results[0] == 1);
}

void testQueryDeduplicatesObjectsAcrossCells() {
    SpatialGrid3D grid(2.0f);
    std::vector<int> results;

    grid.insertSphere(3, Vector3(1.9f, 0.0f, 0.0f), 1.5f);
    grid.queryAabb(Vector3(-2.0f, -2.0f, -2.0f), Vector3(4.0f, 2.0f, 2.0f), results);

    assert(results.size() == 1);
    assert(results[0] == 3);
    assert(grid.getObjectReferenceCount() > 1);
}

void testQueryHandlesNegativeCells() {
    SpatialGrid3D grid(4.0f);
    std::vector<int> results;

    grid.insertSphere(9, Vector3(-5.0f, 0.0f, -5.0f), 0.75f);
    grid.queryAabb(Vector3(-7.0f, -1.0f, -7.0f), Vector3(-3.0f, 1.0f, -3.0f), results);

    assert(contains(results, 9));
}

}

int main() {
    testQueryReturnsInsertedObject();
    testQuerySkipsObjectsOutsideAabb();
    testQueryDeduplicatesObjectsAcrossCells();
    testQueryHandlesNegativeCells();

    return 0;
}
