#include <cassert>
#include <cmath>

#include "../src/rendering/FrameBuffer.h"

namespace {

bool nearlyEqual(float a, float b, float tolerance = 0.001f) {
    return std::abs(a - b) <= tolerance;
}

void testClearWritesAllPixels() {
    FrameBuffer buffer(2, 2);

    buffer.clear(sf::Color(10, 20, 30, 255));

    const std::vector<std::uint8_t>& pixels = buffer.getPixels();
    assert(pixels.size() == 16);

    for (int i = 0; i < 4; i++) {
        int index = i * 4;
        assert(pixels[index] == 10);
        assert(pixels[index + 1] == 20);
        assert(pixels[index + 2] == 30);
        assert(pixels[index + 3] == 255);
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

    const std::vector<std::uint8_t>& pixels = buffer.getPixels();
    int index = (1 * buffer.getWidth() + 1) * 4;
    assert(pixels[index] == sf::Color::Green.r);
    assert(pixels[index + 1] == sf::Color::Green.g);
    assert(pixels[index + 2] == sf::Color::Green.b);
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
    testSetPixelRejectsOutOfBoundsWrites();

    return 0;
}
