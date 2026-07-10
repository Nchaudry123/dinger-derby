#include <cassert>
#include <cmath>

#include "rendering/FrameBuffer.h"

namespace {

bool nearlyEqual(float a, float b, float tolerance = 0.001f) {
    return std::abs(a - b) <= tolerance;
}

void testClearWritesAllPixels() {
    FrameBuffer buffer(2, 2);

    buffer.clear(sf::Color(10, 20, 30, 255));

    const std::vector<std::uint32_t>& pixels = buffer.getPixels();
    assert(pixels.size() == 4);

    for (int y = 0; y < buffer.getHeight(); y++) {
        for (int x = 0; x < buffer.getWidth(); x++) {
            sf::Color color = buffer.getPixelColor(x, y);
            assert(color.r == 10);
            assert(color.g == 20);
            assert(color.b == 30);
            assert(color.a == 255);
        }
    }
}

void testSetPixelUsesDepthTest() {
    FrameBuffer buffer(3, 3);
    buffer.clear(sf::Color::Black);
    buffer.clearDepth(100.0f);

    assert(buffer.setPixel(1, 1, sf::Color::Red, 5.0f));
    assert(nearlyEqual(buffer.getDepth(1, 1), 5.0f));
    assert(!buffer.setPixel(1, 1, sf::Color::Blue, 8.0f));
    assert(buffer.setPixel(1, 1, sf::Color::Green, 2.0f));
    assert(nearlyEqual(buffer.getDepth(1, 1), 2.0f));

    sf::Color color = buffer.getPixelColor(1, 1);
    assert(color.r == sf::Color::Green.r);
    assert(color.g == sf::Color::Green.g);
    assert(color.b == sf::Color::Green.b);
}

void testSetPixelAllowsNearlyEqualDepth() {
    FrameBuffer buffer(3, 3);
    buffer.clear(sf::Color::Black);
    buffer.clearDepth(100.0f);

    assert(buffer.setPixel(1, 1, sf::Color::Red, 5.0f));
    assert(buffer.setPixel(1, 1, sf::Color::Blue, 5.00005f));

    sf::Color color = buffer.getPixelColor(1, 1);
    assert(color.r == sf::Color::Blue.r);
    assert(color.g == sf::Color::Blue.g);
    assert(color.b == sf::Color::Blue.b);
}

void testSetPixelRejectsOutOfBoundsWrites() {
    FrameBuffer buffer(2, 2);

    assert(!buffer.setPixel(-1, 0, sf::Color::White, 1.0f));
    assert(!buffer.setPixel(0, 2, sf::Color::White, 1.0f));
}

}

int main() {
    testClearWritesAllPixels();
    testSetPixelUsesDepthTest();
    testSetPixelAllowsNearlyEqualDepth();
    testSetPixelRejectsOutOfBoundsWrites();

    return 0;
}
