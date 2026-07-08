#pragma once

#include <SFML/Graphics/Color.hpp>
#include <cstdint>
#include <vector>

class FrameBuffer {
public:
    FrameBuffer();
    FrameBuffer(int width, int height);

    void resize(int newWidth, int newHeight);
    void clear(sf::Color color);
    void clearDepth(float value);
    bool setPixel(int x, int y, sf::Color color, float depth);

    int getWidth() const;
    int getHeight() const;
    const std::vector<std::uint8_t>& getPixels() const;
    float getDepth(int x, int y) const;

private:
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
    std::vector<float> depthBuffer;

    int pixelIndex(int x, int y) const;
    int depthIndex(int x, int y) const;
};
