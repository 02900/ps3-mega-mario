/*
 * SFML shim backend (PS3): the real ya2d / tiny3d-backed implementations of the
 * shim methods that touch the platform. The shim *headers* stay pure C++ (opaque
 * void* handles); this TU is the only place that touches ya2d / tiny3d / sysutil.
 *
 * C++<->C interop (see docs/PATTERNS.md §7): pre-include the C system headers so
 * ya2d.h's transitive includes don't land inside the extern "C" wrap.
 *
 * Coordinate model (Phase 5): the game works in a virtual 1920x1080 window
 * (sf::RenderWindow::getSize) with SFML's Y-down convention; tiny3d's 2D canvas
 * is a fixed 848x512. draw() maps world -> canvas: screen = (world - camera) *
 * (canvas / window). The sf::View supplies the scrolling camera offset.
 */
#include "ya2d_lite.h"   // ya2d (minus controls) + tiny3d + C++/C interop boilerplate
#include <io/pad.h>      // input (has its own extern "C" guard)
#include <sysutil/sysutil.h>
#include "ttf_render.h"
#include <SFML/Graphics.hpp>
#include "asset_registry.h"
#include "clay_menu.h"   // Clay UI (menu) — extern "C" hooks
#include "audio.h"       // MikMod music + SFX — extern "C" hooks

// Identifies this backend in the benchmark output file (Scene_Benchmark.cpp).
const char *BACKEND_NAME = "tiny3d";

// tiny3d's 2D canvas is a fixed 848x512 regardless of output resolution
// (tiny3d_Project2D); the game renders in a 1920x1080 virtual window.
#define T3D_CANVAS_W 848.0f
#define T3D_CANVAS_H 512.0f

// ---- window / frame lifecycle + XMB exit --------------------------------
namespace {

bool g_exit_requested = false;
bool g_platform_ready = false;

void sys_callback(u64 status, u64 /*param*/, void * /*userdata*/)
{
	if (status == SYSUTIL_EXIT_GAME)
		g_exit_requested = true;
}

void platform_init()
{
	if (g_platform_ready)
		return;
	sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, nullptr);
	// Larger vertex buffer: tiny3d reuses this ring across frames; if it wraps
	// while the RSX is still reading it, a single frame's sprites corrupt (a quad
	// jumps / others vanish for one frame, then settle). More headroom = far rarer.
	tiny3d_Init(8 * 1024 * 1024);
	ya2d_init();
	// PS3 system fonts (present on console + RPCS3); kept for the Clay HUD/menu
	// in Phase 7. The game scene itself draws only sprites.
	TTFLoadFont(0, (char *)"/dev_flash/data/font/SCE-PS3-SR-R-LATIN2.TTF", nullptr, 0);
	TTFLoadFont(1, (char *)"/dev_flash/data/font/SCE-PS3-DH-R-CGB.TTF", nullptr, 0);
	u32 *ttf_tex = (u32 *)init_ttf_table((u16 *)ya2d_texturePointer);
	ya2d_texturePointer = ttf_tex;
	set_ttf_window(0, 0, (int)T3D_CANVAS_W, (int)T3D_CANVAS_H, 0);
	clay_backend_init((int)T3D_CANVAS_W, (int)T3D_CANVAS_H);  // UI is rendered in Clay
	audio_init();                                            // music + SFX (defensive)
	ioPadInit(7);
	g_platform_ready = true;
}

}  // namespace

namespace sf {

// sf::Texture::loadFromFile -> look the basename up in the embedded registry and
// decode the PNG into an RSX texture. (The original passes file paths; there is
// no filesystem on PS3, so we map path -> embedded buffer.)
bool Texture::loadFromFile(const std::string &path)
{
	// GameEngine::init() loads every texture *before* it calls window.create(),
	// so ensure tiny3d/ya2d are up before decoding a PNG into VRAM (idempotent).
	platform_init();

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

// GameEngine::init -> m_window.create(): bring up the PS3 2D stack once.
void RenderWindow::create(VideoMode m, const std::string &, unsigned int)
{
	m_w = m.width;
	m_h = m.height;
	// SFML's window view defaults to the *centered* default view (center =
	// size/2). Scene_Play::sRender relies on this: it sets the camera Y to
	// `height() - getView().getCenter().y`, which is only stable when the
	// previous center.y is height/2 (1080-540 == 540). Our View() default
	// constructs center=(0,0), which would make that feedback oscillate
	// 1080<->0 every frame (the whole scene jumps vertically). Center it.
	m_view = getDefaultView();
	platform_init();
	m_open = true;
}

// Begin a tiny3d 2D frame (the game calls this at the top of each sRender()).
void RenderWindow::clear(const Color &c)
{
	u32 argb = ((u32)c.a << 24) | ((u32)c.r << 16) | ((u32)c.g << 8) | (u32)c.b;
	tiny3d_Clear(argb, TINY3D_CLEAR_ALL);
	tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);
	tiny3d_BlendFunc(1,
		(blend_src_func)(TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA),
		(blend_dst_func)(TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO),
		(blend_func)(TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD));
	tiny3d_Project2D();
	reset_ttf_frame();
}

// End the frame (the game calls this at the bottom of each sRender()).
void RenderWindow::display()
{
	tiny3d_Flip();
	audio_update();        // drive MikMod's software mixer once per frame
	sysUtilCheckCallback();
	if (g_exit_requested)
		m_open = false;   // XMB "Quit Game" -> drop out of GameEngine::run()
}

// sf::RenderWindow::draw(Sprite): a textured tiny3d quad honoring the sprite's
// texture sub-rect (animation frame / sheet cell), origin, scale (negative
// scale.x = facing flip), and the sf::View camera. Nearest filtering keeps the
// pixel art crisp. This is the core of the scene render.
void RenderWindow::draw(const Sprite &s)
{
	if (!s.m_tex || !s.m_tex->handle)
		return;
	ya2d_Texture *t = (ya2d_Texture *)s.m_tex->handle;

	// Source sub-rect in PNG pixels; a zero rect means "the whole image".
	float rl = s.m_rect.width  ? (float)s.m_rect.left  : 0.0f;
	float rt = s.m_rect.height ? (float)s.m_rect.top   : 0.0f;
	float rw = s.m_rect.width  ? (float)s.m_rect.width : (float)t->imageWidth;
	float rh = s.m_rect.height ? (float)s.m_rect.height: (float)t->imageHeight;

	const float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	const float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;
	const float wsx = T3D_CANVAS_W / (float)m_w;
	const float wsy = T3D_CANVAS_H / (float)m_h;
	const u32 col = ((u32)s.m_color.r << 24) | ((u32)s.m_color.g << 16) |
	                ((u32)s.m_color.b << 8)  | (u32)s.m_color.a;

	// SFML transform: world = pos + (local - origin) * scale (local in source px),
	// then world -> canvas. Compute the 4 corners up front so we can cull.
	auto toScreen = [&](float lx, float ly, float &sx, float &sy) {
		float wx = s.m_pos.x + (lx - s.m_origin.x) * s.m_scale.x;
		float wy = s.m_pos.y + (ly - s.m_origin.y) * s.m_scale.y;
		sx = (wx - camX) * wsx;
		sy = (wy - camY) * wsy;
	};
	float sx[4], sy[4];
	toScreen(0,  0,  sx[0], sy[0]);   // top-left
	toScreen(rw, 0,  sx[1], sy[1]);   // top-right
	toScreen(rw, rh, sx[2], sy[2]);   // bottom-right
	toScreen(0,  rh, sx[3], sy[3]);   // bottom-left

	// Off-screen cull: skip if the sprite's screen AABB misses the 848x512 canvas.
	float minx = sx[0], maxx = sx[0], miny = sy[0], maxy = sy[0];
	for (int i = 1; i < 4; i++) {
		minx = sx[i] < minx ? sx[i] : minx;  maxx = sx[i] > maxx ? sx[i] : maxx;
		miny = sy[i] < miny ? sy[i] : miny;  maxy = sy[i] > maxy ? sy[i] : maxy;
	}
	if (maxx < 0 || minx > T3D_CANVAS_W || maxy < 0 || miny > T3D_CANVAS_H)
		return;

	// UVs normalized over the allocated (possibly padded) VRAM texture.
	const float u0 = rl / (float)t->textureWidth,  u1 = (rl + rw) / (float)t->textureWidth;
	const float v0 = rt / (float)t->textureHeight, v1 = (rt + rh) / (float)t->textureHeight;
	const float u[4] = { u0, u1, u1, u0 };
	const float v[4] = { v0, v0, v1, v1 };

	// tiny3d 2D uses depth func LEQUAL + clear-to-far, so equal-z later draws win
	// (painter's order). The player is the last entity, so z=0 for all is correct.
	tiny3d_SetTextureWrap(0, t->textureOffset, t->textureWidth, t->textureHeight,
	                      t->rowBytes, (text_format)t->format,
	                      TEXTWRAP_CLAMP, TEXTWRAP_CLAMP, TEXTURE_NEAREST);
	tiny3d_SetPolygon(TINY3D_QUADS);
	for (int i = 0; i < 4; i++) {
		tiny3d_VertexPos(sx[i], sy[i], 0);
		tiny3d_VertexColor(col);
		tiny3d_VertexTexture(u[i], v[i]);
	}
	tiny3d_End();
}

// ---- debug overlays (off by default: 'J' AABBs, 'F' grid) ----------------
// Both honor the same world->canvas camera mapping as sprites.
void RenderWindow::draw(const RectangleShape &s)
{
	const float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	const float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;
	const float wsx = T3D_CANVAS_W / (float)m_w;
	const float wsy = T3D_CANVAS_H / (float)m_h;
	const Color &c = s.m_outlineThickness > 0 ? s.m_outline : s.m_fill;
	const u32 col = ((u32)c.r << 24) | ((u32)c.g << 16) | ((u32)c.b << 8) | (u32)c.a;

	float x = (s.m_pos.x - s.m_origin.x - camX) * wsx;
	float y = (s.m_pos.y - s.m_origin.y - camY) * wsy;
	float w = s.m_size.x * wsx, h = s.m_size.y * wsy;

	if (s.m_outlineThickness > 0) {   // outline: four 1px edges
		ya2d_drawFillRectZ((int)x, (int)y, 0, (int)w, 1, col);
		ya2d_drawFillRectZ((int)x, (int)(y + h), 0, (int)w, 1, col);
		ya2d_drawFillRectZ((int)x, (int)y, 0, 1, (int)h, col);
		ya2d_drawFillRectZ((int)(x + w), (int)y, 0, 1, (int)h, col);
	} else {
		ya2d_drawFillRectZ((int)x, (int)y, 0, (int)w, (int)h, col);
	}
}

void RenderWindow::draw(const Vertex *v, std::size_t n, PrimitiveType)
{
	if (n < 2)
		return;
	const float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	const float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;
	const float wsx = T3D_CANVAS_W / (float)m_w;
	const float wsy = T3D_CANVAS_H / (float)m_h;
	// The game only emits axis-aligned grid segments; draw each as a 1px rect.
	for (std::size_t i = 0; i + 1 < n; i += 2) {
		float x0 = (v[i].position.x   - camX) * wsx, y0 = (v[i].position.y   - camY) * wsy;
		float x1 = (v[i+1].position.x - camX) * wsx, y1 = (v[i+1].position.y - camY) * wsy;
		const Color &c = v[i].color;
		u32 col = ((u32)c.r << 24) | ((u32)c.g << 16) | ((u32)c.b << 8) | (u32)c.a;
		int rx = (int)(x0 < x1 ? x0 : x1), ry = (int)(y0 < y1 ? y0 : y1);
		int rw = (int)(x0 < x1 ? x1 - x0 : x0 - x1) + 1;
		int rh = (int)(y0 < y1 ? y1 - y0 : y0 - y1) + 1;
		ya2d_drawFillRectZ(rx, ry, 0, rw, rh, col);
	}
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
bool      g_held[11] = { false };

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

	bool cur[11];
	cur[0]  = p.BTN_CROSS;                           // W  (Jump = X only)
	cur[1]  = p.BTN_LEFT  || sx < 0;                 // A  (Left)
	cur[2]  = p.BTN_DOWN  || sy > 0;                 // Down arrow (menu down)
	cur[3]  = p.BTN_RIGHT || sx > 0;                 // D  (Right)
	cur[4]  = p.BTN_CIRCLE;                          // Enter (menu select)
	cur[5]  = p.BTN_SQUARE;                          // Space (Shoot)
	cur[6]  = p.BTN_START;                           // Escape (Quit / back)
	cur[7]  = p.BTN_TRIANGLE;                        // P (Pause)
	cur[8]  = p.BTN_L1;                              // J (debug: draw collision boxes)
	cur[9]  = p.BTN_R1;                              // F (debug: draw grid)
	cur[10] = p.BTN_UP   || sy < 0;                  // Up arrow (menu up; unused in play)

	static const sf::Keyboard::Key keys[11] = {
		sf::Keyboard::W, sf::Keyboard::A, sf::Keyboard::Down, sf::Keyboard::D,
		sf::Keyboard::Enter, sf::Keyboard::Space, sf::Keyboard::Escape, sf::Keyboard::P,
		sf::Keyboard::J, sf::Keyboard::F, sf::Keyboard::Up
	};
	for (int i = 0; i < 11; i++)
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
