#pragma once

class Vector2 {
public:
    float x;
    float y;

    Vector2();
    Vector2(float x, float y);

    Vector2 operator+(const Vector2& other) const;
    Vector2 operator-(const Vector2& other) const;
    Vector2 operator*(float scalar) const;
    Vector2 operator/(float scalar) const;

    Vector2& operator+=(const Vector2& other);
    Vector2& operator-=(const Vector2& other);

    float dot(const Vector2& other) const;
    float magnitude() const;
    Vector2 normalized() const;
};