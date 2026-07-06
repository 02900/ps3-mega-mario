/*
 * SFML shim backend (PS3) — raylib implementation.
 *
 * Branch `raylib-backend`: the same SFML shim methods, but drawn with raylib
 * (the ghcr.io/02900/ps3-toolchain-raylib image: raylib over RSXGL) instead of
 * Tiny3D/ya2d. raylib owns the whole frame (it can't share the RSX with Tiny3D),
 * so sprites, the window lifecycle and input all go through raylib here; audio
 * (MikMod) is independent and kept. The Clay menu is stubbed in this stage
 * (source/clay_stub.c) and ported to a raylib Clay renderer in stage 2.
 *
 * Coordinate model (unchanged): the game works in a virtual 1920x1080 window with
 * SFML's Y-down convention. world -> screen = (world - camera) * (screen / window),
 * where the sf::View supplies the scrolling camera. raylib's Y is also down in 2D,
 * so the mapping is direct.
 */
#include "raylib.h"
#include "rlgl.h"        // rlDrawRenderBatchActive() — force-flush the render batch

#include <SFML/Graphics.hpp>
#include "asset_registry.h"
#include "audio.h"       // MikMod music + SFX — extern "C" hooks
#include "clay_menu.h"   // Clay menu (rendered via raylib in clay_renderer_raylib.c)

#include <cstring>
#include <cstddef>

// ---- raylib-facing helpers (global scope: bare names are raylib's) -------
namespace {

bool g_ready = false;

void platform_init()
{
	if (g_ready)
		return;
	// The game works in a 1920x1080 virtual window; on PS3 raylib creates a
	// fullscreen surface and renders this logical space onto the panel.
	InitWindow(1920, 1080, "Mega Mario (PS3, raylib)");
	SetTargetFPS(60);
	clay_backend_init(848, 512);   // Clay arena + raylib measure-text (layout space)
	audio_init();          // music + SFX (defensive; independent of the GPU)
	g_ready = true;
}

// Decode an embedded PNG into a heap Texture2D (nearest filtering = crisp pixels).
void *load_png(const unsigned char *buf, unsigned size, unsigned *w, unsigned *h)
{
	Image img = LoadImageFromMemory(".png", buf, (int)size);
	if (!img.data)
		return nullptr;
	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img);
	if (tex.id == 0)
		return nullptr;
	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	*w = (unsigned)tex.width;
	*h = (unsigned)tex.height;
	return new Texture2D(tex);
}

// A textured quad: src sub-rect (px), dest rect (screen px), origin (dest px),
// horizontal flip, RGBA tint. Reproduces SFML's pos+(local-origin)*scale.
void draw_sprite(void *handle,
                 float sl, float st, float sw, float sh,
                 float dx, float dy, float dw, float dh,
                 float ox, float oy, bool flipX,
                 unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	Texture2D *t = (Texture2D *)handle;
	Rectangle src = { sl, st, flipX ? -sw : sw, sh };
	Rectangle dst = { dx, dy, dw, dh };
	Vector2 org   = { ox, oy };
	DrawTexturePro(*t, src, dst, org, 0.0f, (Color){ r, g, b, a });
}

void draw_fill_rect(float x, float y, float w, float h,
                    unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	DrawRectangle((int)x, (int)y, (int)w, (int)h, (Color){ r, g, b, a });
}

void draw_line(float x0, float y0, float x1, float y1,
               unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	DrawLineV((Vector2){ x0, y0 }, (Vector2){ x1, y1 }, (Color){ r, g, b, a });
}

void draw_text(const char *s, float x, float y, float size,
               unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	DrawTextEx(GetFontDefault(), s, (Vector2){ x, y }, size, size / 10.0f,
	           (Color){ r, g, b, a });
	// Consecutive text all shares the font atlas texture, so rlgl merges it into one
	// huge glDrawElements that RSXGL corrupts (stray glyph vertices -> white streaks).
	// Flush after each string so every text is its own tiny draw call (~a few quads),
	// which RSXGL renders cleanly (like sprites). This cycles the batch VBO once per
	// string, so callers must keep the number of strings per frame under the image's
	// batch-buffer count (RL_DEFAULT_BATCH_BUFFERS) or the RSX reuses a VBO mid-frame
	// while still reading it -> flicker. The debug grid is thinned (label every 4th
	// cell) to stay well under that.
	rlDrawRenderBatchActive();
}

}  // namespace

namespace sf {

// sf::Texture::loadFromFile -> embedded-registry lookup + raylib PNG decode.
bool Texture::loadFromFile(const std::string &path)
{
	platform_init();   // GameEngine loads textures before window.create(); bring raylib up first
	const unsigned char *buf;
	unsigned size;
	if (!asset_lookup(path.c_str(), &buf, &size))
		return false;
	unsigned w = 0, h = 0;
	void *t = load_png(buf, size, &w, &h);
	if (!t)
		return false;
	handle = t;
	sz = Vector2u(w, h);
	return true;
}

void RenderWindow::create(VideoMode m, const std::string &, unsigned int)
{
	m_w = m.width;
	m_h = m.height;
	m_view = getDefaultView();   // centered default view (see scroll feedback note, original backend)
	platform_init();
	m_open = true;
}

// Begin a raylib frame (top of the game's sRender()).
void RenderWindow::clear(const Color &c)
{
	BeginDrawing();
	ClearBackground((::Color){ c.r, c.g, c.b, c.a });
}

// End the frame (bottom of sRender()).
void RenderWindow::display()
{
	EndDrawing();
	audio_update();              // drive MikMod's software mixer once per frame
	if (WindowShouldClose())     // XMB "Quit Game" / close -> drop out of GameEngine::run()
		m_open = false;
}

// The core scene draw: a sprite as a textured quad honoring sub-rect, origin,
// scale (negative scale.x = facing flip) and the sf::View camera.
void RenderWindow::draw(const Sprite &s)
{
	if (!s.m_tex || !s.m_tex->handle)
		return;
	unsigned tw = s.m_tex->sz.x, th = s.m_tex->sz.y;

	float rl = s.m_rect.width  ? (float)s.m_rect.left   : 0.0f;
	float rt = s.m_rect.height ? (float)s.m_rect.top    : 0.0f;
	float rw = s.m_rect.width  ? (float)s.m_rect.width  : (float)tw;
	float rh = s.m_rect.height ? (float)s.m_rect.height : (float)th;

	const float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	const float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;
	const float wsx = (float)GetScreenWidth()  / (float)m_w;
	const float wsy = (float)GetScreenHeight() / (float)m_h;

	const float asx = s.m_scale.x < 0 ? -s.m_scale.x : s.m_scale.x;
	const float asy = s.m_scale.y < 0 ? -s.m_scale.y : s.m_scale.y;

	// dest rect at the sprite's world position; origin offset in dest px.
	float dx = (s.m_pos.x - camX) * wsx;
	float dy = (s.m_pos.y - camY) * wsy;
	float dw = rw * asx * wsx;
	float dh = rh * asy * wsy;
	float ox = s.m_origin.x * asx * wsx;
	float oy = s.m_origin.y * asy * wsy;

	// Cull if the dest AABB misses the screen.
	if (dx - ox > GetScreenWidth() || dx - ox + dw < 0 ||
	    dy - oy > GetScreenHeight() || dy - oy + dh < 0)
		return;

	draw_sprite(s.m_tex->handle, rl, rt, rw, rh, dx, dy, dw, dh, ox, oy,
	            s.m_scale.x < 0, s.m_color.r, s.m_color.g, s.m_color.b, s.m_color.a);
}

// ---- debug overlays ('J' AABBs, 'F' grid): same world->screen mapping ----
void RenderWindow::draw(const RectangleShape &s)
{
	const float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	const float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;
	const float wsx = (float)GetScreenWidth()  / (float)m_w;
	const float wsy = (float)GetScreenHeight() / (float)m_h;
	const Color &c = s.m_outlineThickness > 0 ? s.m_outline : s.m_fill;

	float x = (s.m_pos.x - s.m_origin.x - camX) * wsx;
	float y = (s.m_pos.y - s.m_origin.y - camY) * wsy;
	float w = s.m_size.x * wsx, h = s.m_size.y * wsy;

	if (s.m_outlineThickness > 0) {        // outline: four 1px edges
		draw_fill_rect(x, y, w, 1, c.r, c.g, c.b, c.a);
		draw_fill_rect(x, y + h, w, 1, c.r, c.g, c.b, c.a);
		draw_fill_rect(x, y, 1, h, c.r, c.g, c.b, c.a);
		draw_fill_rect(x + w, y, 1, h, c.r, c.g, c.b, c.a);
	} else {
		draw_fill_rect(x, y, w, h, c.r, c.g, c.b, c.a);
	}
}

void RenderWindow::draw(const Vertex *v, std::size_t n, PrimitiveType)
{
	if (n < 2)
		return;
	const float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	const float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;
	const float wsx = (float)GetScreenWidth()  / (float)m_w;
	const float wsy = (float)GetScreenHeight() / (float)m_h;
	for (std::size_t i = 0; i + 1 < n; i += 2) {
		float x0 = (v[i].position.x   - camX) * wsx, y0 = (v[i].position.y   - camY) * wsy;
		float x1 = (v[i+1].position.x - camX) * wsx, y1 = (v[i+1].position.y - camY) * wsy;
		const Color &c = v[i].color;
		draw_line(x0, y0, x1, y1, c.r, c.g, c.b, c.a);
	}
}

// sf::Text via raylib's built-in font. Used for the debug grid's per-cell "(x,y)"
// labels, which live at world positions, so apply the same view camera as sprites.
void RenderWindow::draw(const Text &t)
{
	if (t.m_str.empty())
		return;
	const float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	const float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;
	const float wsx = (float)GetScreenWidth()  / (float)m_w;
	const float wsy = (float)GetScreenHeight() / (float)m_h;

	float x = (t.m_pos.x - t.m_origin.x - camX) * wsx;
	float y = (t.m_pos.y - t.m_origin.y - camY) * wsy;
	float size = (float)t.m_size * wsy;

	draw_text(t.m_str.c_str(), x, y, size, t.m_color.r, t.m_color.g, t.m_color.b, t.m_color.a);
}

}  // namespace sf

// ---- input: DualShock (via raylib's gamepad API) -> sf::Event ------------
// raylib already reads the pad inside its frame, so we must NOT read ioPad again
// (one-reader-per-port would starve us — see the psl1ght-input skill). Source the
// same KeyPressed/Released edges from raylib's gamepad buttons/axes instead.
namespace {

sf::Event g_evq[128];
int       g_head = 0, g_tail = 0;
bool      g_held[11] = { false };

void push_event(sf::Event::EventType t, sf::Keyboard::Key k)
{
	int nt = (g_tail + 1) % 128;
	if (nt == g_head) return;
	g_evq[g_tail].type = t;
	g_evq[g_tail].key.code = k;
	g_evq[g_tail].key.alt = g_evq[g_tail].key.control =
		g_evq[g_tail].key.shift = g_evq[g_tail].key.system = false;
	g_tail = nt;
}

void build_pad_events()
{
	float ax = IsGamepadAvailable(0) ? GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X) : 0.0f;
	float ay = IsGamepadAvailable(0) ? GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) : 0.0f;
	bool left = ax < -0.4f, right = ax > 0.4f, up = ay < -0.4f, down = ay > 0.4f;

	bool cur[11];
	cur[0]  = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);            // Cross  -> W (Jump)
	cur[1]  = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)  || left;    // Left
	cur[2]  = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)  || down;    // Down
	cur[3]  = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || right;   // Right
	cur[4]  = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);           // Circle -> Enter (select)
	cur[5]  = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);            // Square -> Space (Shoot)
	cur[6]  = IsGamepadButtonDown(0, GAMEPAD_BUTTON_MIDDLE_RIGHT);              // Start  -> Escape (Quit)
	cur[7]  = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_UP);             // Triangle -> P (Pause)
	cur[8]  = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1);            // L1 -> J (debug boxes)
	cur[9]  = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);           // R1 -> F (debug grid)
	cur[10] = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP)    || up;      // Up

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
	if (g_head == g_tail) {
		build_pad_events();
		if (g_head == g_tail) return false;
	}
	e = g_evq[g_head];
	g_head = (g_head + 1) % 128;
	return true;
}

}  // namespace sf
