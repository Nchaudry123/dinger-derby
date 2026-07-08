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
    pixels.resize(width * height * 4);
    depthBuffer.resize(width * height);
    bool resized = texture.resize(sf::Vector2u(width, height));
    (void)resized;
    clear(sf::Color::Black);
    clearDepth(std::numeric_limits<float>::infinity());
}

void FrameBuffer::clear(sf::Color color) {
    if (color.r == color.g && color.g == color.b && color.b == color.a) {
        std::fill(pixels.begin(), pixels.end(), color.r);
        return;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = pixelIndex(x, y);
            pixels[index] = color.r;
            pixels[index + 1] = color.g;
            pixels[index + 2] = color.b;
            pixels[index + 3] = color.a;
        }
    }
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

    int pIndex = pixelIndex(x, y);
    pixels[pIndex] = color.r;
    pixels[pIndex + 1] = color.g;
    pixels[pIndex + 2] = color.b;
    pixels[pIndex + 3] = color.a;

    return true;
}

int FrameBuffer::getWidth() const {
    return width;
}

int FrameBuffer::getHeight() const {
    return height;
}

const std::vector<std::uint8_t>& FrameBuffer::getPixels() const {
    return pixels;
}

float FrameBuffer::getDepth(int x, int y) const {
    return depthBuffer[depthIndex(x, y)];
}

void FrameBuffer::present(sf::RenderWindow& window) {
    texture.update(pixels.data());
    sf::Sprite sprite(texture);

    sf::Vector2u windowSize = window.getSize();
    sprite.setScale(sf::Vector2f(
        static_cast<float>(windowSize.x) / width,
        static_cast<float>(windowSize.y) / height
    ));

    window.draw(sprite);
}

int FrameBuffer::pixelIndex(int x, int y) const {
    return (y * width + x) * 4;
}

int FrameBuffer::depthIndex(int x, int y) const {
    return y * width + x;
}
