#include <cassert>

#include "../src/rendering/FrameBuffer.h"
#include "../src/rendering/Rasterizer3D.h"

namespace {

sf::Color colorAt(const FrameBuffer& buffer, int x, int y) {
    return buffer.getPixelColor(x, y);
}

void testRasterizerFillsTriangleInterior() {
    FrameBuffer buffer(16, 16);
    buffer.clear(sf::Color::Black);
    buffer.clearDepth(100.0f);

    Rasterizer3D::drawTriangle(
        buffer,
        Vector3(2.0f, 2.0f, 5.0f),
        Vector3(12.0f, 2.0f, 5.0f),
        Vector3(2.0f, 12.0f, 5.0f),
        sf::Color::Red
    );

    sf::Color filled = colorAt(buffer, 4, 4);
    sf::Color empty = colorAt(buffer, 14, 14);

    assert(filled.r == sf::Color::Red.r);
    assert(filled.g == sf::Color::Red.g);
    assert(filled.b == sf::Color::Red.b);
    assert(empty == sf::Color::Black);
}

void testRasterizerUsesDepthBuffer() {
    FrameBuffer buffer(16, 16);
    buffer.clear(sf::Color::Black);
    buffer.clearDepth(100.0f);

    Rasterizer3D::drawTriangle(
        buffer,
        Vector3(2.0f, 2.0f, 8.0f),
        Vector3(12.0f, 2.0f, 8.0f),
        Vector3(2.0f, 12.0f, 8.0f),
        sf::Color::Blue
    );

    Rasterizer3D::drawTriangle(
        buffer,
        Vector3(2.0f, 2.0f, 4.0f),
        Vector3(12.0f, 2.0f, 4.0f),
        Vector3(2.0f, 12.0f, 4.0f),
        sf::Color::Green
    );

    sf::Color front = colorAt(buffer, 4, 4);
    assert(front == sf::Color::Green);
}

}

int main() {
    testRasterizerFillsTriangleInterior();
    testRasterizerUsesDepthBuffer();

    return 0;
}
