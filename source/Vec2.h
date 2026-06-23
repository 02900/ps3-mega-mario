#pragma once

#include <math.h>

const float EPS = 1e-9;

class Vec2 {
   public:
    float x, y;

    Vec2();
    Vec2(float _x, float _y);

    bool operator==(const Vec2 v) const;
    bool operator!=(const Vec2 v) const;

    Vec2 operator+(const Vec2 v) const;
    Vec2 operator-(const Vec2 v) const;
    Vec2 operator*(const float scale) const;
    Vec2 operator/(const float scale) const;

    void operator+=(const Vec2 v);
    void operator-=(const Vec2 v);
    void operator*=(const float scale);
    void operator/=(const float scale);

    float norm();
    float dist(Vec2 v);
    Vec2 normalize();
};
