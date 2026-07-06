/*
 * SFML shim backend (PS3) — raw RSXGL / OpenGL implementation (branch rsxgl-backend).
 *
 * The same SFML shim methods, but drawn directly on RSXGL/OpenGL (like ps3-gl-test,
 * but 2D-ortho) instead of raylib or Tiny3D. WE control the batching: every sprite,
 * rect, line and text glyph-run is its own small draw call, which sidesteps the
 * big-single-texture-draw corruption that RSXGL inflicts on raylib's merged text.
 *
 * One 2D shader draws everything as a textured quad (solid fills use a 1x1 white
 * texture x vertex colour). Text uses a built-in 8x8 bitmap font atlas (no TTF).
 * Audio (MikMod) and input (raw ioPad) are GPU-independent and carried over.
 *
 * Coordinate model (unchanged): the game works in a virtual 1920x1080 window,
 * Y-down; world -> screen px = (world - camera) * (screen / window), the sf::View
 * supplies the camera. The ortho projection maps screen px straight to NDC.
 */
#include <EGL/egl.h>
#define GL3_PROTOTYPES
#include <GL3/gl3.h>
#include <GL3/rsxgl.h>

#include <ppu-types.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <sysmodule/sysmodule.h>
#include <png.h>

#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <csetjmp>
#include <sys/time.h>
extern "C" int usleep(unsigned long microseconds);

#include <SFML/Graphics.hpp>
#include "asset_registry.h"
#include "audio.h"        // MikMod music + SFX — extern "C" hooks
#include "clay_menu.h"    // Clay menu (rendered via gl2d in clay_renderer_raylib.c)
#include "gl2d.h"         // shared 2D helpers (this TU implements them)
#include "font8x8.h"      // built-in 8x8 bitmap ASCII font

// Identifies this backend in the benchmark's output file (Scene_Benchmark.cpp).
const char *BACKEND_NAME = "rsxgl";

// ---- GL state -----------------------------------------------------------
namespace {

EGLDisplay g_dpy = 0;
EGLSurface g_surf = 0;
EGLContext g_ctx = 0;
int   g_w = 1280, g_h = 720;        // real framebuffer size
GLuint g_prog = 0;
GLint  g_uProj = -1, g_uTex = -1;
GLuint g_vao = 0, g_vbo = 0;
GLuint g_white = 0, g_font = 0;
bool  g_ready = false;
bool  g_exit = false;

struct GLTex { GLuint id; int w, h; };

const char *VS =
  "#version 130\n"
  "attribute vec2 position;\n"
  "attribute vec2 texcoord;\n"
  "attribute vec4 color;\n"
  "uniform mat4 Proj;\n"
  "varying vec2 v_uv;\n"
  "varying vec4 v_col;\n"
  "void main(void) { gl_Position = Proj * vec4(position, 0.0, 1.0); v_uv = texcoord; v_col = color; }\n";

const char *FS =
  "#version 130\n"
  "varying vec2 v_uv;\n"
  "varying vec4 v_col;\n"
  "uniform sampler2D tex;\n"
  "void main(void) { gl_FragColor = texture2D(tex, v_uv) * v_col; }\n";

void mat_ortho(float *m, float l, float r, float b, float t, float n, float f)
{
	memset(m, 0, 16 * sizeof(float));
	m[0]  = 2.0f / (r - l);
	m[5]  = 2.0f / (t - b);
	m[10] = -2.0f / (f - n);
	m[12] = -(r + l) / (r - l);
	m[13] = -(t + b) / (t - b);
	m[14] = -(f + n) / (f - n);
	m[15] = 1.0f;
}

GLuint compile(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	return s;
}

GLuint make_tex(const unsigned char *rgba, int w, int h)
{
	GLuint t;
	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_2D, t);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return t;
}

// PNG (in memory) -> malloc'd RGBA8 buffer. NULL on failure.
struct PngSrc { const unsigned char *p; size_t left; };
void png_read_mem(png_structp png, png_bytep out, png_size_t n)
{
	PngSrc *s = (PngSrc *)png_get_io_ptr(png);
	size_t k = n < s->left ? n : s->left;
	memcpy(out, s->p, k);
	s->p += k;
	s->left -= k;
}
unsigned char *decode_png(const unsigned char *buf, unsigned size, int *w, int *h)
{
	if (size < 8 || png_sig_cmp((png_bytep)buf, 0, 8)) return NULL;
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) return NULL;
	png_infop info = png_create_info_struct(png);
	if (!info) { png_destroy_read_struct(&png, NULL, NULL); return NULL; }
	unsigned char *out = NULL;
	png_bytep *rows = NULL;
	if (setjmp(png_jmpbuf(png))) { free(out); free(rows); png_destroy_read_struct(&png, &info, NULL); return NULL; }
	PngSrc src = { buf, size };
	png_set_read_fn(png, &src, png_read_mem);
	png_read_info(png, info);
	int bw = png_get_image_width(png, info), bh = png_get_image_height(png, info);
	int ct = png_get_color_type(png, info), bd = png_get_bit_depth(png, info);
	if (bd == 16) png_set_strip_16(png);
	if (ct == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
	if (ct == PNG_COLOR_TYPE_GRAY && bd < 8) png_set_expand_gray_1_2_4_to_8(png);
	if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
	if (ct == PNG_COLOR_TYPE_GRAY || ct == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
	png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);   // ensure 4 channels
	png_read_update_info(png, info);
	out = (unsigned char *)malloc((size_t)bw * bh * 4);
	rows = (png_bytep *)malloc(sizeof(png_bytep) * bh);
	for (int y = 0; y < bh; y++) rows[y] = out + (size_t)y * bw * 4;
	png_read_image(png, rows);
	free(rows);
	png_destroy_read_struct(&png, &info, NULL);

	// RSXGL's texture upload has a channel/endianness quirk: the RGBA bytes we
	// hand it come back rotated when sampled (orange -> magenta). raylib-ps3 hits
	// the same "buggy PS3 opengl driver" and fixes it by byte-swapping each pixel
	// (rtextures.c LoadTextureFromImage). Do the same: [R,G,B,A] -> [A,B,G,R].
	uint32_t *px = (uint32_t *)out;
	for (size_t i = 0, n = (size_t)bw * bh; i < n; i++)
		px[i] = __builtin_bswap32(px[i]);

	*w = bw; *h = bh;
	return out;
}

// Expand the 8x8 font into a 128x64 (16x8 grid) RGBA atlas: white where set.
void build_font_atlas()
{
	const int AW = 128, AH = 64;
	unsigned char *px = (unsigned char *)calloc((size_t)AW * AH * 4, 1);
	for (int c = 0; c < 128; c++) {
		int gx = (c % 16) * 8, gy = (c / 16) * 8;
		for (int r = 0; r < 8; r++) {
			unsigned char bits = FONT8X8[c][r];
			for (int x = 0; x < 8; x++)
				if ((bits >> x) & 1) {
					size_t o = ((size_t)(gy + r) * AW + (gx + x)) * 4;
					px[o] = px[o + 1] = px[o + 2] = px[o + 3] = 0xFF;
				}
		}
	}
	g_font = make_tex(px, AW, AH);
	free(px);
}

// --- quad batching ---------------------------------------------------------
// The naive "glBufferData + glDrawArrays per texture change" costs one buffer
// REALLOCATION per draw run — ~65us each on RSXGL — so a multi-texture scene of N
// sprites pays N reallocations/frame (the benchmark measured rsxgl ~15x slower than
// raylib/tiny3d here). Instead accumulate the WHOLE frame's geometry into one CPU
// buffer, recording "runs" (a texture + a vertex range), upload it ONCE per frame
// (a single orphaning glBufferData, no stall), then issue one glDrawArrays per run.
// Cost drops to 1 upload + N cheap draws/frame. Painter's order is preserved (runs
// are emitted in submission order); a run also breaks on demand (text) so a big
// merged font-atlas draw — the RSXGL glyph corruption — never forms.
const int BATCH_MAX_VERTS = 131072;  // ~21845 quads/frame; 4 MB CPU buffer (fits the 16k-sprite benchmark stage in one upload)
const int BATCH_MAX_RUNS  = 32768;   // worst case: one run per quad (multi-texture)
float  g_verts[BATCH_MAX_VERTS * 8];
int    g_vcount = 0;                 // vertices accumulated this frame
struct DrawRun { GLuint tex; int start; int count; };
DrawRun g_runs[BATCH_MAX_RUNS];
int    g_run_count = 0;
bool   g_force_break = false;        // start a new run on the next quad (text boundary)

// Upload the frame's geometry once and draw every run. Called at display() (and as a
// mid-frame fallback if the CPU buffer fills). One glBufferData => orphan => no stall.
void frame_flush()
{
	if (g_vcount == 0) { g_run_count = 0; g_force_break = false; return; }
	glBindVertexArray(g_vao);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)g_vcount * 8 * sizeof(float), g_verts, GL_STREAM_DRAW);
	for (int i = 0; i < g_run_count; i++) {
		glBindTexture(GL_TEXTURE_2D, g_runs[i].tex);
		glDrawArrays(GL_TRIANGLES, g_runs[i].start, g_runs[i].count);
	}
	g_vcount = 0;
	g_run_count = 0;
	g_force_break = false;
}

// Append one textured quad (screen px, uv 0..1) to the current frame's buffer.
void gl_quad(GLuint tex, float x, float y, float w, float h,
             float u0, float v0, float u1, float v1,
             float cr, float cg, float cb, float ca)
{
	if (g_vcount + 6 > BATCH_MAX_VERTS || g_run_count >= BATCH_MAX_RUNS)
		frame_flush();   // buffer full mid-frame: draw what we have, keep going

	// New run on texture change or a forced break (else extend the current run).
	if (g_run_count == 0 || g_runs[g_run_count - 1].tex != tex || g_force_break) {
		g_runs[g_run_count].tex   = tex;
		g_runs[g_run_count].start = g_vcount;
		g_runs[g_run_count].count = 0;
		g_run_count++;
		g_force_break = false;
	}

	float x1 = x + w, y1 = y + h;
	const float v[6 * 8] = {
		x,  y,  u0, v0, cr, cg, cb, ca,   x1, y,  u1, v0, cr, cg, cb, ca,   x1, y1, u1, v1, cr, cg, cb, ca,
		x,  y,  u0, v0, cr, cg, cb, ca,   x1, y1, u1, v1, cr, cg, cb, ca,   x,  y1, u0, v1, cr, cg, cb, ca,
	};
	memcpy(&g_verts[g_vcount * 8], v, sizeof(v));
	g_vcount += 6;
	g_runs[g_run_count - 1].count += 6;
}

void sys_callback(u64 status, u64 /*param*/, void * /*userdata*/)
{
	if (status == SYSUTIL_EXIT_GAME) g_exit = true;
}

void platform_init()
{
	if (g_ready) return;
	sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, NULL);

	// --- EGL / RSXGL context (from ps3-gl-test) ---
	g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLint v0 = 0, v1 = 0;
	eglInitialize(g_dpy, &v0, &v1);
	EGLint attribs[] = {
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 16, EGL_NONE
	};
	EGLConfig config; EGLint nconfig = 0;
	eglChooseConfig(g_dpy, attribs, &config, 1, &nconfig);
	g_surf = eglCreateWindowSurface(g_dpy, config, 0, 0);
	eglQuerySurface(g_dpy, g_surf, EGL_WIDTH, &g_w);
	eglQuerySurface(g_dpy, g_surf, EGL_HEIGHT, &g_h);
	g_ctx = eglCreateContext(g_dpy, config, 0, 0);
	eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx);
	gl2d_screen_w = g_w; gl2d_screen_h = g_h;

	// --- 2D shader ---
	g_prog = glCreateProgram();
	glAttachShader(g_prog, compile(GL_VERTEX_SHADER, VS));
	glAttachShader(g_prog, compile(GL_FRAGMENT_SHADER, FS));
	glBindAttribLocation(g_prog, 0, "position");
	glBindAttribLocation(g_prog, 1, "texcoord");
	glBindAttribLocation(g_prog, 2, "color");
	glLinkProgram(g_prog);
	glUseProgram(g_prog);
	g_uProj = glGetUniformLocation(g_prog, "Proj");
	g_uTex = glGetUniformLocation(g_prog, "tex");
	glUniform1i(g_uTex, 0);
	float proj[16];
	mat_ortho(proj, 0.0f, (float)g_w, (float)g_h, 0.0f, -1.0f, 1.0f);
	glUniformMatrix4fv(g_uProj, 1, GL_FALSE, proj);

	glViewport(0, 0, g_w, g_h);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// --- streaming VBO/VAO: pos(2)+uv(2)+col(4) = 8 floats/vertex ---
	glGenVertexArrays(1, &g_vao);
	glBindVertexArray(g_vao);
	glGenBuffers(1, &g_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(2 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(4 * sizeof(float)));

	unsigned char wp[4] = { 255, 255, 255, 255 };
	g_white = make_tex(wp, 1, 1);
	build_font_atlas();

	clay_backend_init(848, 512);
	audio_init();

	// Pad AFTER the GL context (RSXGL init resets lv2 IO).
	sysModuleLoad(SYSMODULE_IO);
	ioPadInit(7);

	g_ready = true;
}

}  // namespace

// ---- shared 2D helpers (C linkage for the Clay C renderer) --------------
extern "C" {

int gl2d_screen_w = 1280, gl2d_screen_h = 720;

void gl2d_rect(float x, float y, float w, float h,
               unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	gl_quad(g_white, x, y, w, h, 0, 0, 1, 1, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

void gl2d_text(const char *s, float x, float y, float size,
               unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	float cr = r / 255.0f, cg = g / 255.0f, cb = b / 255.0f, ca = a / 255.0f;
	float cx = x;
	for (; *s; ++s) {
		unsigned char c = (unsigned char)*s;
		if (c >= 128) c = '?';
		if (c != ' ') {
			int gx = c % 16, gy = c / 16;
			float u0 = gx / 16.0f, u1 = (gx + 1) / 16.0f;
			float v0 = gy / 8.0f,  v1 = (gy + 1) / 8.0f;
			gl_quad(g_font, cx, y, size, size, u0, v0, u1, v1, cr, cg, cb, ca);
		}
		cx += size;   // monospace advance
	}
	// End this string's run so the next text starts a separate draw call — a big
	// merged font-atlas glDrawElements is exactly what corrupts glyphs on RSXGL.
	// Just a run boundary now (no upload): the frame is still uploaded once.
	g_force_break = true;
}

float gl2d_text_width(const char *s, int n, float size)
{
	(void)s;
	return (float)n * size;
}

}  // extern "C"

// ---- sf:: shim implementations ------------------------------------------
namespace sf {

bool Texture::loadFromFile(const std::string &path)
{
	platform_init();   // textures load before window.create(); bring GL up first
	const unsigned char *buf;
	unsigned size;
	if (!asset_lookup(path.c_str(), &buf, &size))
		return false;
	int w = 0, h = 0;
	unsigned char *rgba = decode_png(buf, size, &w, &h);
	if (!rgba)
		return false;
	GLTex *t = new GLTex;
	t->id = make_tex(rgba, w, h);
	t->w = w; t->h = h;
	free(rgba);
	handle = t;
	sz = Vector2u((unsigned)w, (unsigned)h);
	return true;
}

void RenderWindow::create(VideoMode m, const std::string &, unsigned int)
{
	m_w = m.width;
	m_h = m.height;
	m_view = getDefaultView();
	platform_init();
	m_open = true;
}

void RenderWindow::clear(const Color &c)
{
	glClearColor(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(g_prog);
}

void RenderWindow::display()
{
	frame_flush();   // upload the frame's geometry once, then draw all runs
	eglSwapBuffers(g_dpy, g_surf);
	audio_update();
	sysUtilCheckCallback();
	if (g_exit)
		m_open = false;

	// Pace to ~60fps (eglSwapBuffers doesn't throttle on RSXGL).
	static struct timeval prev = { 0, 0 };
	struct timeval now;
	gettimeofday(&now, NULL);
	if (prev.tv_sec || prev.tv_usec) {
		long us = (now.tv_sec - prev.tv_sec) * 1000000L + (now.tv_usec - prev.tv_usec);
		if (us < 16666L) usleep(16666L - us);
	}
	gettimeofday(&prev, NULL);
}

// The core scene draw: a sprite as a textured quad honoring the sub-rect, origin,
// scale (negative scale.x = facing flip) and the sf::View camera.
void RenderWindow::draw(const Sprite &s)
{
	if (!s.m_tex || !s.m_tex->handle)
		return;
	GLTex *t = (GLTex *)s.m_tex->handle;
	float tw = (float)t->w, th = (float)t->h;

	float rl = s.m_rect.width  ? (float)s.m_rect.left   : 0.0f;
	float rt = s.m_rect.height ? (float)s.m_rect.top    : 0.0f;
	float rw = s.m_rect.width  ? (float)s.m_rect.width  : tw;
	float rh = s.m_rect.height ? (float)s.m_rect.height : th;

	const float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	const float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;
	const float wsx = (float)g_w / (float)m_w;
	const float wsy = (float)g_h / (float)m_h;
	const float asx = s.m_scale.x < 0 ? -s.m_scale.x : s.m_scale.x;
	const float asy = s.m_scale.y < 0 ? -s.m_scale.y : s.m_scale.y;

	float dx = (s.m_pos.x - camX) * wsx;
	float dy = (s.m_pos.y - camY) * wsy;
	float dw = rw * asx * wsx;
	float dh = rh * asy * wsy;
	float ox = s.m_origin.x * asx * wsx;
	float oy = s.m_origin.y * asy * wsy;
	float px = dx - ox, py = dy - oy;

	if (px > g_w || px + dw < 0 || py > g_h || py + dh < 0)
		return;   // off-screen cull

	float u0 = rl / tw, u1 = (rl + rw) / tw;
	float v0 = rt / th, v1 = (rt + rh) / th;
	if (s.m_scale.x < 0) { float tmp = u0; u0 = u1; u1 = tmp; }   // horizontal flip

	gl_quad(t->id, px, py, dw, dh, u0, v0, u1, v1,
	        s.m_color.r / 255.0f, s.m_color.g / 255.0f, s.m_color.b / 255.0f, s.m_color.a / 255.0f);
}

// ---- debug overlays ('J' AABBs, 'F' grid) ----
void RenderWindow::draw(const RectangleShape &s)
{
	const float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	const float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;
	const float wsx = (float)g_w / (float)m_w;
	const float wsy = (float)g_h / (float)m_h;
	const Color &c = s.m_outlineThickness > 0 ? s.m_outline : s.m_fill;

	float x = (s.m_pos.x - s.m_origin.x - camX) * wsx;
	float y = (s.m_pos.y - s.m_origin.y - camY) * wsy;
	float w = s.m_size.x * wsx, h = s.m_size.y * wsy;

	if (s.m_outlineThickness > 0) {
		gl2d_rect(x, y, w, 1, c.r, c.g, c.b, c.a);
		gl2d_rect(x, y + h, w, 1, c.r, c.g, c.b, c.a);
		gl2d_rect(x, y, 1, h, c.r, c.g, c.b, c.a);
		gl2d_rect(x + w, y, 1, h, c.r, c.g, c.b, c.a);
	} else {
		gl2d_rect(x, y, w, h, c.r, c.g, c.b, c.a);
	}
}

void RenderWindow::draw(const Vertex *v, std::size_t n, PrimitiveType)
{
	if (n < 2) return;
	const float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	const float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;
	const float wsx = (float)g_w / (float)m_w;
	const float wsy = (float)g_h / (float)m_h;
	for (std::size_t i = 0; i + 1 < n; i += 2) {
		float x0 = (v[i].position.x   - camX) * wsx, y0 = (v[i].position.y   - camY) * wsy;
		float x1 = (v[i+1].position.x - camX) * wsx, y1 = (v[i+1].position.y - camY) * wsy;
		const Color &c = v[i].color;
		// The game only emits axis-aligned grid segments; draw each as a 1px rect.
		float rx = x0 < x1 ? x0 : x1, ry = y0 < y1 ? y0 : y1;
		float rw = (x0 < x1 ? x1 - x0 : x0 - x1) + 1.0f;
		float rh = (y0 < y1 ? y1 - y0 : y0 - y1) + 1.0f;
		gl2d_rect(rx, ry, rw, rh, c.r, c.g, c.b, c.a);
	}
}

void RenderWindow::draw(const Text &t)
{
	if (t.m_str.empty()) return;
	const float camX = m_view.m_center.x - m_view.m_size.x * 0.5f;
	const float camY = m_view.m_center.y - m_view.m_size.y * 0.5f;
	const float wsx = (float)g_w / (float)m_w;
	const float wsy = (float)g_h / (float)m_h;

	float x = (t.m_pos.x - t.m_origin.x - camX) * wsx;
	float y = (t.m_pos.y - t.m_origin.y - camY) * wsy;
	float size = (float)t.m_size * wsy;
	gl2d_text(t.m_str.c_str(), x, y, size, t.m_color.r, t.m_color.g, t.m_color.b, t.m_color.a);
}

}  // namespace sf

// ---- input: DualShock -> sf::Event (raw ioPad, from the Tiny3D backend) -----
// raylib owned the pad read; on raw GL we read ioPad ourselves. Retained-packet
// pattern (docs/PATTERNS.md §2.1). Button -> key mapping unchanged.
namespace {

sf::Event g_evq[128];
int       g_head = 0, g_tail = 0;
padData   g_prev;
bool      g_prev_valid = false;
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

int axis_dir(unsigned v) { int d = (int)v - 128; return d < -40 ? -1 : (d > 40 ? 1 : 0); }

void build_pad_events()
{
	padInfo info;
	padData pd;
	memset(&pd, 0, sizeof pd);
	ioPadGetInfo(&info);
	if (info.status[0] && ioPadGetData(0, &pd) == 0)
		if (pd.len > 0) { g_prev = pd; g_prev_valid = true; }
	if (!g_prev_valid) return;

	padData &p = g_prev;
	int sx = 0, sy = 0;
	if (!(p.ANA_L_H == 0 && p.ANA_L_V == 0)) { sx = axis_dir(p.ANA_L_H); sy = axis_dir(p.ANA_L_V); }

	bool cur[11];
	cur[0]  = p.BTN_CROSS;                    // W  (Jump)
	cur[1]  = p.BTN_LEFT  || sx < 0;          // A  (Left)
	cur[2]  = p.BTN_DOWN  || sy > 0;          // Down
	cur[3]  = p.BTN_RIGHT || sx > 0;          // D  (Right)
	cur[4]  = p.BTN_CIRCLE;                   // Enter (select)
	cur[5]  = p.BTN_SQUARE;                   // Space (Shoot)
	cur[6]  = p.BTN_START;                    // Escape (Quit)
	cur[7]  = p.BTN_TRIANGLE;                 // P (Pause)
	cur[8]  = p.BTN_L1;                       // J (debug boxes)
	cur[9]  = p.BTN_R1;                       // F (debug grid)
	cur[10] = p.BTN_UP   || sy < 0;           // Up

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
