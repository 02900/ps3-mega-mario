// Minimal SFML System shim for the PS3 port. See docs/PATTERNS.md §7.
#pragma once
#include <cstdint>
#include <string>

namespace sf {

template <typename T>
struct Vector2 {
	T x, y;
	Vector2() : x(0), y(0) {}
	Vector2(T X, T Y) : x(X), y(Y) {}
	template <typename U>
	explicit Vector2(const Vector2<U> &v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)) {}

	Vector2 operator+(const Vector2 &v) const { return Vector2(x + v.x, y + v.y); }
	Vector2 operator-(const Vector2 &v) const { return Vector2(x - v.x, y - v.y); }
	Vector2 operator*(T s) const { return Vector2(x * s, y * s); }
	Vector2 operator/(T s) const { return Vector2(x / s, y / s); }
	Vector2 &operator+=(const Vector2 &v) { x += v.x; y += v.y; return *this; }
	Vector2 &operator-=(const Vector2 &v) { x -= v.x; y -= v.y; return *this; }
	bool operator==(const Vector2 &v) const { return x == v.x && y == v.y; }
	bool operator!=(const Vector2 &v) const { return !(*this == v); }
};

typedef Vector2<float>        Vector2f;
typedef Vector2<int>          Vector2i;
typedef Vector2<unsigned int> Vector2u;

}  // namespace sf
