#include "FrameBuffer.h"

#include <SFML/Graphics/Sprite.hpp>
#include <algorithm>
#include <limits>

FrameBuffer::FrameBuffer() = default;

FrameBuffer::FrameBuffer(int width, int height) {
    resize(width, height);
}

void FrameBuffer::resize(int newWidth, int newHeight) {
    width = newWidth;
    height = newHeight;
    pixels.resize(width * height);
    depthBuffer.resize(width * height);
    bool resized = texture.resize(sf::Vector2u(width, height));
    (void)resized;
    clear(sf::Color::Black);
    clearDepth(std::numeric_limits<float>::infinity());
}

void FrameBuffer::clear(sf::Color color) {
    std::fill(pixels.begin(), pixels.end(), packColor(color));
}

void FrameBuffer::clearDepth(float value) {
    std::fill(depthBuffer.begin(), depthBuffer.end(), value);
}

bool FrameBuffer::setPixel(int x, int y, sf::Color color, float depth) {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return false;
    }

    int dIndex = depthIndex(x, y);

    if (depth >= depthBuffer[dIndex]) {
        return false;
    }

    depthBuffer[dIndex] = depth;

    pixels[dIndex] = packColor(color);

    return true;
}

int FrameBuffer::getWidth() const {
    return width;
}

int FrameBuffer::getHeight() const {
    return height;
}

const std::vector<std::uint32_t>& FrameBuffer::getPixels() const {
    return pixels;
}

sf::Color FrameBuffer::getPixelColor(int x, int y) const {
    return unpackColor(pixels[depthIndex(x, y)]);
}

float FrameBuffer::getDepth(int x, int y) const {
    return depthBuffer[depthIndex(x, y)];
}

void FrameBuffer::present(sf::RenderWindow& window) {
    texture.update(reinterpret_cast<const std::uint8_t*>(pixels.data()));
    sf::Sprite sprite(texture);

    sf::Vector2u windowSize = window.getSize();
    sprite.setScale(sf::Vector2f(
        static_cast<float>(windowSize.x) / width,
        static_cast<float>(windowSize.y) / height
    ));

    window.draw(sprite);
}

int FrameBuffer::pixelIndex(int x, int y) const {
    return y * width + x;
}

int FrameBuffer::depthIndex(int x, int y) const {
    return y * width + x;
}

std::uint32_t FrameBuffer::packColor(sf::Color color) {
    return
        static_cast<std::uint32_t>(color.r) |
        (static_cast<std::uint32_t>(color.g) << 8) |
        (static_cast<std::uint32_t>(color.b) << 16) |
        (static_cast<std::uint32_t>(color.a) << 24);
}

sf::Color FrameBuffer::unpackColor(std::uint32_t color) {
    return sf::Color(
        static_cast<std::uint8_t>(color & 0xff),
        static_cast<std::uint8_t>((color >> 8) & 0xff),
        static_cast<std::uint8_t>((color >> 16) & 0xff),
        static_cast<std::uint8_t>((color >> 24) & 0xff)
    );
}
