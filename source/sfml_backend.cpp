/*
 * SFML shim backend (PS3): the real ya2d-backed implementations of the shim
 * methods that need rendering / asset access. The shim *headers* stay pure C++
 * (opaque void* handles); this TU is the only place that touches ya2d.
 *
 * C++<->C interop (see docs/PATTERNS.md §7): pre-include the C system headers so
 * ya2d.h's transitive includes don't land inside the extern "C" wrap.
 */
#include "ya2d_lite.h"   // ya2d (minus controls) + C++/C interop boilerplate
#include <io/pad.h>      // input (has its own extern "C" guard)
#include <SFML/Graphics.hpp>
#include "asset_registry.h"

namespace sf {

// sf::Texture::loadFromFile -> look the basename up in the embedded registry and
// decode the PNG into an RSX texture. (The original passes file paths; there is
// no filesystem on PS3, so we map path -> embedded buffer.)
bool Texture::loadFromFile(const std::string &path)
{
	const unsigned char *buf;
	unsigned size;
	if (!asset_lookup(path.c_str(), &buf, &size))
		return false;
	ya2d_Texture *t = ya2d_loadPNGfromBuffer((void *)buf, size);
	if (!t)
		return false;
	handle = t;
	sz = Vector2u((unsigned)t->imageWidth, (unsigned)t->imageHeight);
	return true;
}

// sf::RenderWindow::draw(Sprite): immediate ya2d draw within the current tiny3d
// frame. Applies the sf::View as a 2D camera offset (screen = world - camera).
// NOTE: full-texture draw for now; texture-rect sub-frames (animations) and
// nearest-neighbour filtering for crisp pixel art land in Phase 5.
void RenderWindow::draw(const Sprite &s)
{
	if (!s.m_tex || !s.m_tex->handle)
		return;
	ya2d_Texture *t = (ya2d_Texture *)s.m_tex->handle;

	float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;

	float w = (float)t->imageWidth  * s.m_scale.x;
	float h = (float)t->imageHeight * s.m_scale.y;
	float x = s.m_pos.x - s.m_origin.x * s.m_scale.x - camX;
	float y = s.m_pos.y - s.m_origin.y * s.m_scale.y - camY;

	ya2d_drawTextureEx(t, x, y, 0, w, h);
}

}  // namespace sf

// ---- input: DualShock -> sf::Event(KeyPressed/Released) ------------------
// The game's sUserInput() loops `while (window.pollEvent(e))` and maps
// e.key.code (an sf::Keyboard::Key) through the scene's ActionMap. We map each
// logical key to a set of pad buttons (OR-combined) and emit press/release edges.
// Retained-packet pattern per docs/PATTERNS.md §2.1.
namespace {

sf::Event g_evq[128];
int       g_head = 0, g_tail = 0;
padData   g_prev;
bool      g_prev_valid = false;
bool      g_held[8] = { false };

void push_event(sf::Event::EventType t, sf::Keyboard::Key k)
{
	int nt = (g_tail + 1) % 128;
	if (nt == g_head) return;  // queue full -> drop
	g_evq[g_tail].type = t;
	g_evq[g_tail].key.code = k;
	g_evq[g_tail].key.alt = g_evq[g_tail].key.control =
		g_evq[g_tail].key.shift = g_evq[g_tail].key.system = false;
	g_tail = nt;
}

int axis_dir(unsigned v) { int d = (int)v - 128; return d < -40 ? -1 : (d > 40 ? 1 : 0); }

void build_pad_events()
{
	padInfo info;
	padData pd;
	memset(&pd, 0, sizeof pd);
	ioPadGetInfo(&info);
	if (info.status[0] && ioPadGetData(0, &pd) == 0)
		if (pd.len > 0) { g_prev = pd; g_prev_valid = true; }  // retain last good packet
	if (!g_prev_valid) return;

	padData &p = g_prev;
	int sx = 0, sy = 0;
	if (!(p.ANA_L_H == 0 && p.ANA_L_V == 0)) { sx = axis_dir(p.ANA_L_H); sy = axis_dir(p.ANA_L_V); }

	bool cur[8];
	cur[0] = p.BTN_UP    || p.BTN_CROSS || sy < 0;  // W  (Up / Jump)
	cur[1] = p.BTN_LEFT  || sx < 0;                 // A  (Left)
	cur[2] = p.BTN_DOWN  || sy > 0;                 // S  (Down)
	cur[3] = p.BTN_RIGHT || sx > 0;                 // D  (Right)
	cur[4] = p.BTN_CIRCLE;                          // Enter (menu select)
	cur[5] = p.BTN_SQUARE;                          // Space (Shoot)
	cur[6] = p.BTN_START;                           // Escape (Quit / back)
	cur[7] = p.BTN_TRIANGLE;                        // P (Pause)

	static const sf::Keyboard::Key keys[8] = {
		sf::Keyboard::W, sf::Keyboard::A, sf::Keyboard::S, sf::Keyboard::D,
		sf::Keyboard::Enter, sf::Keyboard::Space, sf::Keyboard::Escape, sf::Keyboard::P
	};
	for (int i = 0; i < 8; i++)
		if (cur[i] != g_held[i]) {
			push_event(cur[i] ? sf::Event::KeyPressed : sf::Event::KeyReleased, keys[i]);
			g_held[i] = cur[i];
		}
}

}  // namespace

namespace sf {

bool RenderWindow::pollEvent(Event &e)
{
	if (g_head == g_tail) {           // queue drained -> poll the pad for this frame
		build_pad_events();
		if (g_head == g_tail) return false;
	}
	e = g_evq[g_head];
	g_head = (g_head + 1) % 128;
	return true;
}

}  // namespace sf
