#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
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
    bool setPixelFast(int x, int y, sf::Color color, float depth) {
        int dIndex = y * width + x;

        if (depth >= depthBuffer[dIndex]) {
            return false;
        }

        depthBuffer[dIndex] = depth;

        int pIndex = dIndex * 4;
        pixels[pIndex] = color.r;
        pixels[pIndex + 1] = color.g;
        pixels[pIndex + 2] = color.b;
        pixels[pIndex + 3] = color.a;

        return true;
    }

    int getWidth() const;
    int getHeight() const;
    const std::vector<std::uint8_t>& getPixels() const;
    float getDepth(int x, int y) const;
    void present(sf::RenderWindow& window);

private:
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
    std::vector<float> depthBuffer;
    sf::Texture texture;

    int pixelIndex(int x, int y) const;
    int depthIndex(int x, int y) const;
};
