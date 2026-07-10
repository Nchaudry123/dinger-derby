#pragma once

#include <SFML/Graphics/Color.hpp>

#include "../math/Vector3.h"
#include "FrameBuffer.h"

class Rasterizer3D {
public:
    static void setAntiAliasingEnabled(bool enabled);
    static bool isAntiAliasingEnabled();

    static void drawTriangle(
        FrameBuffer& frameBuffer,
        const Vector3& a,
        const Vector3& b,
        const Vector3& c,
        sf::Color color
    );

    // Gouraud: interpolate vertex colors with the same coverage AA path.
    static void drawTriangle(
        FrameBuffer& frameBuffer,
        const Vector3& a,
        const Vector3& b,
        const Vector3& c,
        sf::Color colorA,
        sf::Color colorB,
        sf::Color colorC
    );

private:
    static float edgeFunction(
        const Vector3& a,
        const Vector3& b,
        float x,
        float y
    );
};
