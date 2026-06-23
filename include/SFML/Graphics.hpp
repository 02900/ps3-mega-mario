// Minimal SFML Graphics shim for the PS3 port. Header-only stubs: the scene
// types (Texture/Sprite/View/RenderWindow) get a ya2d backend in later phases;
// Text/Font stay minimal because the menu/HUD are rebuilt in Clay (PATTERNS §3.5).
#pragma once
#include "System.hpp"
#include "Window.hpp"
#include <cstddef>
#include <string>

namespace sf {

// ---- Color --------------------------------------------------------------
struct Color {
	uint8_t r, g, b, a;
	Color() : r(0), g(0), b(0), a(255) {}
	Color(uint8_t R, uint8_t G, uint8_t B, uint8_t A = 255) : r(R), g(G), b(B), a(A) {}

	bool operator==(const Color &c) const { return r == c.r && g == c.g && b == c.b && a == c.a; }

	static const Color Black, White, Red, Green, Blue, Yellow, Magenta, Cyan, Transparent;
};
inline const Color Color::Black{0, 0, 0};
inline const Color Color::White{255, 255, 255};
inline const Color Color::Red{255, 0, 0};
inline const Color Color::Green{0, 255, 0};
inline const Color Color::Blue{0, 0, 255};
inline const Color Color::Yellow{255, 255, 0};
inline const Color Color::Magenta{255, 0, 255};
inline const Color Color::Cyan{0, 255, 255};
inline const Color Color::Transparent{0, 0, 0, 0};

// ---- Rect ---------------------------------------------------------------
template <typename T>
struct Rect {
	T left, top, width, height;
	Rect() : left(0), top(0), width(0), height(0) {}
	Rect(T l, T t, T w, T h) : left(l), top(t), width(w), height(h) {}
	bool contains(T x, T y) const { return x >= left && x < left + width && y >= top && y < top + height; }
	bool contains(const Vector2<T> &p) const { return contains(p.x, p.y); }
};
typedef Rect<int>   IntRect;
typedef Rect<float> FloatRect;

// ---- Texture ------------------------------------------------------------
class Texture {
public:
	void    *handle = nullptr;   // ya2d_Texture* (filled by the backend)
	Vector2u sz{1, 1};
	bool loadFromFile(const std::string &path);   // backend: sfml_backend.cpp
	Vector2u getSize() const { return sz; }
};

// ---- Sprite -------------------------------------------------------------
class Sprite {
public:
	const Texture *m_tex = nullptr;
	IntRect  m_rect;
	Vector2f m_pos, m_scale{1, 1}, m_origin;
	Color    m_color{255, 255, 255};

	Sprite() {}
	explicit Sprite(const Texture &t) : m_tex(&t) {}

	void setTexture(const Texture &t, bool = false) { m_tex = &t; }
	void setTextureRect(const IntRect &r) { m_rect = r; }
	void setPosition(const Vector2f &p) { m_pos = p; }
	void setPosition(float x, float y) { m_pos = Vector2f(x, y); }
	void setScale(const Vector2f &s) { m_scale = s; }
	void setScale(float x, float y) { m_scale = Vector2f(x, y); }
	void setOrigin(const Vector2f &o) { m_origin = o; }
	void setOrigin(float x, float y) { m_origin = Vector2f(x, y); }
	void setColor(const Color &c) { m_color = c; }
	const Vector2f &getPosition() const { return m_pos; }
	const Vector2f &getScale() const { return m_scale; }
	const Vector2f &getOrigin() const { return m_origin; }
	const Texture  *getTexture() const { return m_tex; }
	FloatRect getGlobalBounds() const {
		float w = m_rect.width * m_scale.x, h = m_rect.height * m_scale.y;
		return FloatRect(m_pos.x - m_origin.x * m_scale.x, m_pos.y - m_origin.y * m_scale.y, w, h);
	}
};

// ---- Font / Text (UI -> Clay; minimal so it links) ----------------------
class Font {
public:
	bool loadFromFile(const std::string & /*path*/) { return true; }
};

class Text {
public:
	std::string    m_str;
	const Font    *m_font = nullptr;
	unsigned int   m_size = 30;
	Color          m_color{255, 255, 255};
	Vector2f       m_pos, m_origin;

	Text() {}
	void setFont(const Font &f) { m_font = &f; }
	void setString(const std::string &s) { m_str = s; }
	void setCharacterSize(unsigned int s) { m_size = s; }
	void setFillColor(const Color &c) { m_color = c; }
	void setPosition(const Vector2f &p) { m_pos = p; }
	void setPosition(float x, float y) { m_pos = Vector2f(x, y); }
	void setOrigin(const Vector2f &o) { m_origin = o; }
	void setOrigin(float x, float y) { m_origin = Vector2f(x, y); }
	const std::string &getString() const { return m_str; }
	FloatRect getLocalBounds() const {
		return FloatRect(0, 0, (float)(m_str.size() * m_size) * 0.5f, (float)m_size);
	}
};

// ---- RectangleShape -----------------------------------------------------
class RectangleShape {
public:
	Vector2f m_size, m_pos, m_origin;
	Color    m_fill{255, 255, 255}, m_outline{0, 0, 0, 0};
	float    m_outlineThickness = 0;

	RectangleShape() {}
	explicit RectangleShape(const Vector2f &s) : m_size(s) {}
	void setSize(const Vector2f &s) { m_size = s; }
	void setPosition(const Vector2f &p) { m_pos = p; }
	void setPosition(float x, float y) { m_pos = Vector2f(x, y); }
	void setOrigin(const Vector2f &o) { m_origin = o; }
	void setOrigin(float x, float y) { m_origin = Vector2f(x, y); }
	void setFillColor(const Color &c) { m_fill = c; }
	void setOutlineColor(const Color &c) { m_outline = c; }
	void setOutlineThickness(float t) { m_outlineThickness = t; }
	const Vector2f &getSize() const { return m_size; }
	const Vector2f &getOrigin() const { return m_origin; }
};

// ---- Vertices -----------------------------------------------------------
enum PrimitiveType { Points, Lines, LineStrip, Triangles, TriangleStrip, TriangleFan, Quads };

struct Vertex {
	Vector2f position;
	Color    color;
	Vector2f texCoords;
	Vertex() : color(255, 255, 255) {}
	explicit Vertex(const Vector2f &p) : position(p), color(255, 255, 255) {}
	Vertex(const Vector2f &p, const Color &c) : position(p), color(c) {}
};

// ---- View ---------------------------------------------------------------
class View {
public:
	Vector2f m_center, m_size{1920, 1080};
	View() {}
	View(const Vector2f &c, const Vector2f &s) : m_center(c), m_size(s) {}
	void setCenter(const Vector2f &c) { m_center = c; }
	void setCenter(float x, float y) { m_center = Vector2f(x, y); }
	void setSize(const Vector2f &s) { m_size = s; }
	void setSize(float x, float y) { m_size = Vector2f(x, y); }
	const Vector2f &getCenter() const { return m_center; }
	const Vector2f &getSize() const { return m_size; }
};

// ---- RenderWindow -------------------------------------------------------
class RenderWindow {
public:
	View         m_view;
	bool         m_open = true;
	unsigned int m_w = 1920, m_h = 1080;

	void create(VideoMode m, const std::string &, unsigned int = Style::Default) { m_w = m.width; m_h = m.height; }
	bool isOpen() const { return m_open; }
	void close() { m_open = false; }
	void clear(const Color & = Color()) {}
	void display() {}
	void draw(const Sprite &s);             // backend: sfml_backend.cpp (ya2d)
	void draw(const Text &) {}              // UI -> Clay (Phase 7)
	void draw(const RectangleShape &) {}    // backend: Phase 5
	void draw(const Vertex *, std::size_t, PrimitiveType) {}  // backend: Phase 5
	void setView(const View &v) { m_view = v; }
	const View &getView() const { return m_view; }
	View getDefaultView() const { return View(Vector2f(m_w / 2.0f, m_h / 2.0f), Vector2f((float)m_w, (float)m_h)); }
	bool pollEvent(Event &e);               // backend: sfml_backend.cpp (DualShock -> keys)
	void setFramerateLimit(unsigned int) {}
	Vector2u getSize() const { return Vector2u(m_w, m_h); }
};

}  // namespace sf
