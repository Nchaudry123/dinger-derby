#include "Rasterizer3D.h"

#include <algorithm>
#include <cmath>

void Rasterizer3D::drawTriangle(
    FrameBuffer& frameBuffer,
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    sf::Color color
) {
    float minX = std::min({a.x, b.x, c.x});
    float maxX = std::max({a.x, b.x, c.x});
    float minY = std::min({a.y, b.y, c.y});
    float maxY = std::max({a.y, b.y, c.y});

    int startX = std::max(0, static_cast<int>(std::floor(minX)));
    int endX = std::min(
        frameBuffer.getWidth() - 1,
        static_cast<int>(std::ceil(maxX))
    );
    int startY = std::max(0, static_cast<int>(std::floor(minY)));
    int endY = std::min(
        frameBuffer.getHeight() - 1,
        static_cast<int>(std::ceil(maxY))
    );

    float area = edgeFunction(a, b, c.x, c.y);

    if (area == 0.0f) {
        return;
    }

    float inverseArea = 1.0f / area;
    bool positiveArea = area > 0.0f;

    float stepAX = c.y - b.y;
    float stepAY = -(c.x - b.x);
    float stepBX = a.y - c.y;
    float stepBY = -(a.x - c.x);
    float stepCX = b.y - a.y;
    float stepCY = -(b.x - a.x);

    float rowStartX = startX + 0.5f;
    float rowStartY = startY + 0.5f;
    float rowA = edgeFunction(b, c, rowStartX, rowStartY);
    float rowB = edgeFunction(c, a, rowStartX, rowStartY);
    float rowC = edgeFunction(a, b, rowStartX, rowStartY);

    for (int y = startY; y <= endY; y++) {
        float edgeA = rowA;
        float edgeB = rowB;
        float edgeC = rowC;

        for (int x = startX; x <= endX; x++) {
            bool inside = positiveArea
                ? edgeA >= 0.0f && edgeB >= 0.0f && edgeC >= 0.0f
                : edgeA <= 0.0f && edgeB <= 0.0f && edgeC <= 0.0f;

            if (!inside) {
                edgeA += stepAX;
                edgeB += stepBX;
                edgeC += stepCX;
                continue;
            }

            float depth =
                (a.z * edgeA + b.z * edgeB + c.z * edgeC) * inverseArea;

            frameBuffer.setPixelFast(x, y, color, depth);

            edgeA += stepAX;
            edgeB += stepBX;
            edgeC += stepCX;
        }

        rowA += stepAY;
        rowB += stepBY;
        rowC += stepCY;
    }
}

float Rasterizer3D::edgeFunction(
    const Vector3& a,
    const Vector3& b,
    float x,
    float y
) {
    return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}
