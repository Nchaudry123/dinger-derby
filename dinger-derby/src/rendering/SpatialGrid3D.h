#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "../math/Vector3.h"

struct SpatialGridCell3D {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const SpatialGridCell3D& other) const;
};

struct SpatialGridCell3DHash {
    std::size_t operator()(const SpatialGridCell3D& cell) const;
};

class SpatialGrid3D {
public:
    explicit SpatialGrid3D(float cellSize = 8.0f);

    void clear();
    void insertSphere(int objectId, const Vector3& center, float radius);
    void queryAabb(const Vector3& minimum, const Vector3& maximum, std::vector<int>& results);

    float getCellSize() const;
    int getCellCount() const;
    int getObjectReferenceCount() const;

private:
    float cellSize;
    std::unordered_map<SpatialGridCell3D, std::vector<int>, SpatialGridCell3DHash> cells;
    std::vector<int> queryMarks;
    int queryStamp = 0;
    int objectReferenceCount = 0;

    SpatialGridCell3D cellForPoint(const Vector3& point) const;
    void markObject(int objectId);
};
