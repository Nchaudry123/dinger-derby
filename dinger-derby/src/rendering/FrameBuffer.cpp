#include "FrameBuffer.h"

#include <SFML/Graphics/Sprite.hpp>
#include <algorithm>
#include <cstdint>
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

    if (depth > depthBuffer[dIndex] + depthEpsilon) {
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
    // Smooth only when scaling so native 1:1 blits stay crisp while supersamples
    // and any residual resize filter cleanly.
    const bool scaling =
        windowSize.x != static_cast<unsigned int>(width) ||
        windowSize.y != static_cast<unsigned int>(height);
    texture.setSmooth(scaling);

    sprite.setScale(sf::Vector2f(
        static_cast<float>(windowSize.x) / static_cast<float>(width),
        static_cast<float>(windowSize.y) / static_cast<float>(height)
    ));

    window.draw(sprite);
}

void FrameBuffer::blitDownsampleTo(
    FrameBuffer& dest,
    int destX,
    int destY,
    int destW,
    int destH
) const {
    if (destW <= 0 || destH <= 0 || width <= 0 || height <= 0) {
        return;
    }

    for (int dy = 0; dy < destH; dy++) {
        int outY = destY + dy;
        if (outY < 0 || outY >= dest.getHeight()) {
            continue;
        }

        int srcY0 = dy * height / destH;
        int srcY1 = std::max(srcY0 + 1, (dy + 1) * height / destH);
        srcY1 = std::min(srcY1, height);

        for (int dx = 0; dx < destW; dx++) {
            int outX = destX + dx;
            if (outX < 0 || outX >= dest.getWidth()) {
                continue;
            }

            int srcX0 = dx * width / destW;
            int srcX1 = std::max(srcX0 + 1, (dx + 1) * width / destW);
            srcX1 = std::min(srcX1, width);

            int count = 0;
            int red = 0;
            int green = 0;
            int blue = 0;
            float nearestDepth = std::numeric_limits<float>::infinity();

            for (int sy = srcY0; sy < srcY1; sy++) {
                for (int sx = srcX0; sx < srcX1; sx++) {
                    int index = sy * width + sx;
                    float depth = depthBuffer[index];
                    if (depth >= std::numeric_limits<float>::infinity() * 0.5f) {
                        continue;
                    }

                    sf::Color color = unpackColor(pixels[index]);
                    red += color.r;
                    green += color.g;
                    blue += color.b;
                    nearestDepth = std::min(nearestDepth, depth);
                    count++;
                }
            }

            if (count == 0) {
                continue;
            }

            sf::Color averaged(
                static_cast<std::uint8_t>(red / count),
                static_cast<std::uint8_t>(green / count),
                static_cast<std::uint8_t>(blue / count)
            );
            dest.setPixelFast(outX, outY, averaged, nearestDepth);
        }
    }
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
