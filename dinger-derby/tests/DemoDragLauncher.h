#pragma once

#include <SFML/Graphics.hpp>

#include "../src/physics/Body2D.h"

class DemoDragLauncher {
public:
    DemoDragLauncher(Body2D& body, float hitboxRadius, float powerScale)
        : body(body), hitboxRadius(hitboxRadius), powerScale(powerScale) {}

    void handleMousePressed(const sf::Event::MouseButtonPressed& mouse) {
        if (mouse.button != sf::Mouse::Button::Left) {
            return;
        }

        Vector2 mousePosition(mouse.position.x, mouse.position.y);

        if ((mousePosition - body.position).magnitude() <= hitboxRadius) {
            dragging = true;
            dragStart = mousePosition;
            dragCurrent = mousePosition;
        }
    }

    void handleMouseMoved(const sf::Event::MouseMoved& move) {
        if (dragging) {
            dragCurrent = Vector2(move.position.x, move.position.y);
        }
    }

    void handleMouseReleased(const sf::Event::MouseButtonReleased& mouse) {
        if (mouse.button != sf::Mouse::Button::Left || !dragging) {
            return;
        }

        Vector2 releasePosition(mouse.position.x, mouse.position.y);
        body.wakeUp();
        body.velocity = (dragStart - releasePosition) * powerScale;
        dragging = false;
    }

    bool isDragging() const {
        return dragging;
    }

    Vector2 currentDragVector() const {
        return dragStart - dragCurrent;
    }

    Vector2 currentMousePosition() const {
        return dragCurrent;
    }

    void updateLine(sf::VertexArray& line) const {
        line[0].position = sf::Vector2f(body.position.x, body.position.y);
        line[0].color = sf::Color::Green;

        line[1].position = sf::Vector2f(dragCurrent.x, dragCurrent.y);
        line[1].color = sf::Color::Green;
    }

private:
    Body2D& body;
    float hitboxRadius;
    float powerScale;
    bool dragging = false;
    Vector2 dragStart;
    Vector2 dragCurrent;
};
