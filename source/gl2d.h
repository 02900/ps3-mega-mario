/*
 * gl2d.h — tiny 2D-renderer helpers shared between the C++ SFML backend
 * (sfml_backend.cpp, which implements them over RSXGL/OpenGL) and the C Clay
 * renderer (clay_renderer_raylib.c). Coordinates are screen pixels (Y-down).
 */
#ifndef GL2D_H
#define GL2D_H

#ifdef __cplusplus
extern "C" {
#endif

/* Real framebuffer size, set by the backend once the GL context is up. */
extern int gl2d_screen_w, gl2d_screen_h;

/* Filled rectangle (solid colour) in screen px. */
void gl2d_rect(float x, float y, float w, float h,
               unsigned char r, unsigned char g, unsigned char b, unsigned char a);

/* Text via the built-in 8x8 bitmap font; `size` = glyph cell height in px
 * (monospace advance == size). */
void gl2d_text(const char *s, float x, float y, float size,
               unsigned char r, unsigned char g, unsigned char b, unsigned char a);

/* Rendered width of the first `n` chars at `size` (monospace). */
float gl2d_text_width(const char *s, int n, float size);

#ifdef __cplusplus
}
#endif

#endif /* GL2D_H */
