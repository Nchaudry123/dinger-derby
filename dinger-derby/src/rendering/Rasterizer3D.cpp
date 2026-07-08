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

    for (int y = startY; y <= endY; y++) {
        for (int x = startX; x <= endX; x++) {
            float sampleX = x + 0.5f;
            float sampleY = y + 0.5f;

            float weightA = edgeFunction(b, c, sampleX, sampleY) / area;
            float weightB = edgeFunction(c, a, sampleX, sampleY) / area;
            float weightC = edgeFunction(a, b, sampleX, sampleY) / area;

            if (weightA < 0.0f || weightB < 0.0f || weightC < 0.0f) {
                continue;
            }

            float depth =
                a.z * weightA +
                b.z * weightB +
                c.z * weightC;

            frameBuffer.setPixel(x, y, color, depth);
        }
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
