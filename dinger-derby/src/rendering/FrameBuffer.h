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

        pixels[dIndex] = packColor(color);

        return true;
    }

    int getWidth() const;
    int getHeight() const;
    const std::vector<std::uint32_t>& getPixels() const;
    sf::Color getPixelColor(int x, int y) const;
    float getDepth(int x, int y) const;
    void present(sf::RenderWindow& window);

private:
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
    std::vector<float> depthBuffer;
    sf::Texture texture;

    static std::uint32_t packColor(sf::Color color);
    static sf::Color unpackColor(std::uint32_t color);
    int pixelIndex(int x, int y) const;
    int depthIndex(int x, int y) const;
};
