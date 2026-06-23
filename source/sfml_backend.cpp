/*
 * SFML shim backend (PS3): the real ya2d-backed implementations of the shim
 * methods that need rendering / asset access. The shim *headers* stay pure C++
 * (opaque void* handles); this TU is the only place that touches ya2d.
 *
 * C++<->C interop (see docs/PATTERNS.md §7): pre-include the C system headers so
 * ya2d.h's transitive includes don't land inside the extern "C" wrap.
 */
#include "ya2d_lite.h"   // ya2d (minus controls) + C++/C interop boilerplate
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
