#include "Vec2.h"

Vec2::Vec2() {}

Vec2::Vec2(float _x, float _y) { x = _x, y = _y; }

bool Vec2::operator==(const Vec2 v) const { return x == v.x && y == v.y; }
bool Vec2::operator!=(const Vec2 v) const { return x != v.x || y != v.y; }

Vec2 Vec2::operator+(const Vec2 v) const { return Vec2(x + v.x, y + v.y); }
Vec2 Vec2::operator-(const Vec2 v) const { return Vec2(x - v.x, y - v.y); }
Vec2 Vec2::operator*(const float scale) const {
    return Vec2(x * scale, y * scale);
}
Vec2 Vec2::operator/(const float scale) const {
    return Vec2(x / scale, y / scale);
}

void Vec2::operator+=(const Vec2 v) { x += v.x, y += v.y; }
void Vec2::operator-=(const Vec2 v) { x -= v.x, y -= v.y; }
void Vec2::operator*=(const float scale) { x *= scale, y *= scale; }
void Vec2::operator/=(const float scale) { x /= scale, y /= scale; }

float Vec2::norm() { return sqrtf(x * x + y * y); }

float Vec2::dist(Vec2 v) { return (*this - v).norm(); }

Vec2 Vec2::normalize() {
    if (norm() < EPS) return *this;
    return *this / norm();
}
