#include "SpatialGrid3D.h"

#include <algorithm>
#include <cmath>
#include <functional>

bool SpatialGridCell3D::operator==(const SpatialGridCell3D& other) const {
    return x == other.x && y == other.y && z == other.z;
}

std::size_t SpatialGridCell3DHash::operator()(const SpatialGridCell3D& cell) const {
    std::size_t hash = 0;
    auto combine = [&hash](int value) {
        hash ^= std::hash<int>{}(value) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    };

    combine(cell.x);
    combine(cell.y);
    combine(cell.z);
    return hash;
}

SpatialGrid3D::SpatialGrid3D(float cellSize) {
    this->cellSize = std::max(cellSize, 0.001f);
}

void SpatialGrid3D::clear() {
    cells.clear();
    objectReferenceCount = 0;
}

void SpatialGrid3D::insertSphere(int objectId, const Vector3& center, float radius) {
    if (objectId < 0) {
        return;
    }

    Vector3 radiusVector(radius, radius, radius);
    SpatialGridCell3D minimum = cellForPoint(center - radiusVector);
    SpatialGridCell3D maximum = cellForPoint(center + radiusVector);

    markObject(objectId);

    for (int z = minimum.z; z <= maximum.z; z++) {
        for (int y = minimum.y; y <= maximum.y; y++) {
            for (int x = minimum.x; x <= maximum.x; x++) {
                cells[{x, y, z}].push_back(objectId);
                objectReferenceCount++;
            }
        }
    }
}

void SpatialGrid3D::queryAabb(
    const Vector3& minimum,
    const Vector3& maximum,
    std::vector<int>& results
) {
    results.clear();

    queryStamp++;
    if (queryStamp == 0) {
        std::fill(queryMarks.begin(), queryMarks.end(), 0);
        queryStamp = 1;
    }

    SpatialGridCell3D minimumCell = cellForPoint(minimum);
    SpatialGridCell3D maximumCell = cellForPoint(maximum);

    for (int z = minimumCell.z; z <= maximumCell.z; z++) {
        for (int y = minimumCell.y; y <= maximumCell.y; y++) {
            for (int x = minimumCell.x; x <= maximumCell.x; x++) {
                auto found = cells.find({x, y, z});

                if (found == cells.end()) {
                    continue;
                }

                for (int objectId : found->second) {
                    if (objectId < 0) {
                        continue;
                    }

                    markObject(objectId);

                    if (queryMarks[objectId] == queryStamp) {
                        continue;
                    }

                    queryMarks[objectId] = queryStamp;
                    results.push_back(objectId);
                }
            }
        }
    }
}

float SpatialGrid3D::getCellSize() const {
    return cellSize;
}

int SpatialGrid3D::getCellCount() const {
    return static_cast<int>(cells.size());
}

int SpatialGrid3D::getObjectReferenceCount() const {
    return objectReferenceCount;
}

SpatialGridCell3D SpatialGrid3D::cellForPoint(const Vector3& point) const {
    return SpatialGridCell3D{
        static_cast<int>(std::floor(point.x / cellSize)),
        static_cast<int>(std::floor(point.y / cellSize)),
        static_cast<int>(std::floor(point.z / cellSize))
    };
}

void SpatialGrid3D::markObject(int objectId) {
    if (objectId >= static_cast<int>(queryMarks.size())) {
        queryMarks.resize(objectId + 1, 0);
    }
}
