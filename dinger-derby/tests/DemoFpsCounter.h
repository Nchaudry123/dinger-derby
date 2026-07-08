#pragma once

#include <SFML/Graphics.hpp>
#include <iomanip>
#include <sstream>
#include <string>

class DemoFpsCounter {
public:
    explicit DemoFpsCounter(const std::string& title)
        : title(title) {}

    void frame(sf::RenderWindow& window) {
        frameCount++;

        float elapsed = clock.getElapsedTime().asSeconds();

        if (elapsed < 0.5f) {
            return;
        }

        float fps = frameCount / elapsed;
        std::ostringstream stream;
        stream << title << " | FPS: " << std::fixed << std::setprecision(1) << fps;

        window.setTitle(stream.str());
        frameCount = 0;
        clock.restart();
    }

private:
    std::string title;
    sf::Clock clock;
    int frameCount = 0;
};
