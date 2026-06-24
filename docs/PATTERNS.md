# Patterns & Gotchas — PS3 / PSL1GHT homebrew

Hard-won conventions for porting games to **PS3 homebrew (PSL1GHT)**, accumulated across
ports and meant to be reused. Each entry is **what to do**, **why**, and the **trap it
avoids**.

> **This repo (ps3-mega-mario)** is a **2D C++/SFML platformer**, so the 2D rendering,
> input, audio, UI, build and packaging sections apply directly. The 3D-camera bits
> (§3.2–§3.4) are reference from a prior **3D** port — skip them for a 2D game. The
> fighting-game examples in §6 are illustrative; the principles transfer. See §7 for the
> SFML-port specifics.

> Rule of thumb that underlies most of this: **you cannot run the build on the dev
> host** — only the user can, on a real PS3 / RPCS3. So favour code you can fully
> reason about, change in small reversible steps, build green in the toolchain image,
> and mark on-hardware behaviour as *unverified* until someone plays it.

---

## 1. Build & verify workflow

- **Build through the Docker toolchain**, never assume a local PSL1GHT:
  ```bash
  DOCKER_DEFAULT_PLATFORM=linux/amd64 ./scripts/build.sh        # -> src.self
  PS3_IP=192.168.x.x ./scripts/deploy.sh                        # ps3load to console
  ```
  `DOCKER_DEFAULT_PLATFORM` is read by the Docker CLI itself, so the scripts need no
  `--platform` flag (the image is x86_64; Apple Silicon emulates it).
- **Output is named after the mount dir.** Mounted at `/src`, the Makefile's
  `TARGET := $(notdir $(CURDIR))` makes `src.elf` / `src.self`. Deploy/CI match by glob
  or `src.self`, not a repo-named file.
- **Keep `-Wall` clean.** The build uses `-Wall`; treat warnings as failures. Common
  one: `-Wmisleading-indentation` from `if (a) x; if (b) y;` on one line — split it.
- **Small, reversible increments.** Build green after every change; a wrong matrix/
  color/sign often shows only as a black or garbled screen the user has to catch.
- **Transient toolchain segfaults are normal under emulation.** The PPU gcc 7.2
  (`cc1` / `collect2`) sporadically crashes with "internal compiler error: Segmentation
  fault" when the x86_64 image runs emulated on Apple Silicon. It's not your code —
  just re-run. `scripts/build.sh` retries automatically; don't go hunting a code cause
  unless the failure is *deterministic* (same error every run).

## 2. Input — DualShock via the PSL1GHT pad API

### 2.1 `padData.len == 0` means "no new data" — retain the last packet ⚠️

This bit this project **three times** (phantom flailing, false "disconnected", ki
charge stalling while still). `cellPadGetData` sets `len = 0` on frames with no input
change (e.g. a button held while the sticks are still). If you zero the struct every
frame and use it as-is, held buttons read as released on those frames.

**Do:** read into a temp; refresh a retained per-port copy only when `len > 0`; reuse
it otherwise so held inputs stay held.

```c
static padData held[2];                 /* persists across frames */
padData pd[2]; int conn[2] = {0,0};
ioPadGetInfo(&pad_info);
for (int i = 0; i < 2; i++) {
    padData tmp; memset(&tmp, 0, sizeof tmp);          /* zero-init: no stack garbage */
    if (pad_info.status[i] && ioPadGetData(i, &tmp) == 0) {
        conn[i] = 1;
        if (tmp.len > 0) held[i] = tmp;                /* fresh data: remember it     */
        pd[i] = held[i];                               /* else reuse last known state */
    } else {
        conn[i] = 0; memset(&held[i], 0, sizeof held[i]);  /* forget on disconnect    */
    }
}
```

**Don't:** gate *connection* on `len > 0` (a motionless pad would read as disconnected),
and **don't** read into an uninitialized struct (a phantom/unconfigured port — common on
RPCS3 — leaves stack garbage that sends a fighter flailing into a corner).

⚠️ **Only ONE reader per port per frame.** `ioPadGetData` returns the fresh packet
(`len > 0`) to the *first* caller in a frame; a second call the same frame gets `len == 0`.
So if two places read port 0 (e.g. a `start_pressed()` helper *and* an event-poll backend),
the second is permanently starved — it never sees `len > 0`, so its retained state never
initializes and it produces no input (symptom: "only Start works, nothing else"). Have a
single pad-read site and share its state.

### 2.2 Two controllers = two ports

`padInfo.status[i]` flags each connected port; `ioPadGetData(i, &data)` reads a specific
one. Port 0 = player 1, port 1 = player 2 — same layout each. Accept menu/quit buttons
from *either* pad. On RPCS3, mapping the *same* physical pad to two players makes both
fighters move together; that's expected, not a bug.

### 2.3 Edge-trigger actions, level-read holds

One-shot actions (fire, melee, menu select) must be edge-detected so a held button does
not repeat; continuous actions (charge, move) read the current level.

```c
u32 cur = (pd.BTN_SQUARE?1:0) | (pd.BTN_CIRCLE?2:0);
u32 pressed = cur & ~prev;   prev = cur;   /* pressed = this frame's new presses */
```

### 2.4 Analog deadzone + the "(0,0) = no data" guard

Map a stick byte (0..255, centre 128) through a deadzone. Treat **both axes exactly 0**
as "no analog data this frame" (digital pad / not read), not full down-left — otherwise
the character drifts on startup.

```c
if (!(pd.ANA_L_H == 0 && pd.ANA_L_V == 0)) { mx = axis(pd.ANA_L_H); mz = -axis(pd.ANA_L_V); }
```

## 3. Rendering (Tiny3D + ya2d)

### 3.1 Colour format mismatch ⚠️

- `tiny3d_Clear()` takes **ARGB** (`0xAARRGGBB`).
- `ya2d_*` fills and `tiny3d_VertexColor()` take **RGBA** (`0xRRGGBBAA`).

Mixing them silently produces wrong colours (e.g. a "blue" clear coming out red). Keep
clear constants separate and comment the byte order at the define.

### 3.2 Prefer a deterministic software camera when you can't iterate on-device

Tiny3D has a full matrix pipeline (`tiny3d_Project3D`, `SetProjectionMatrix`,
`SetMatrixModelView`, `matrix.h`), but its multiply order / FOV conventions are easy to
get subtly wrong, and a wrong camera = a black screen you can only debug on hardware.
A small, fully-understood **pinhole projection** (lookAt basis + perspective divide)
feeding Tiny3D's 2D primitives is predictable and reviewable:

```c
/* basis: f = norm(target-eye); r = norm(cross(up,f)); u = cross(f,r) */
float d[3]; v_sub(p, eye, d);
float vz = v_dot(d, f); if (vz < 0.05f) return 0;        /* behind camera */
*sx = CX + FOCAL * v_dot(d, r) / vz;
*sy = CY - FOCAL * v_dot(d, u) / vz;
*scale = FOCAL / vz;                                     /* px per world unit at depth */
```

General principle: **when feedback loops are slow/expensive, choose the approach you can
verify by reading, not by running.**

### 3.3 2D draw order = paint order (via the depth test), not "no z-buffer"

For the **3D** scene, draw far-to-near: sort objects by camera-forward depth
(`dot(pos - eye, forward)`), draw the larger-depth one first. Fine for a handful of actors.

> **2D correction (verified by disassembling `libtiny3d`):** `tiny3d_Project2D` is *not*
> z-less — it enables the depth test with func **`LEQUAL`**, depth-write **on**, and clears
> depth to **far**. So overlapping 2D quads compose in **draw order for free** *as long as
> they all use one constant `z`* (a later draw at the same `z` passes `≤` and overwrites).
> ⚠️ The trap: don't bias `z` per-sprite to "fix" ordering — a later, *larger* `z` then
> **fails** `≤` and the sprite vanishes wherever it overlaps an earlier one. Draw
> back-to-front at a single `z` and let LEQUAL do the rest.

### 3.4 Clamp the footprint edge, not the centre

Clamp `pos ± half-extent` to the bounds so the body stops at the wall, not its centre.
A camera-facing **billboard is flat in Z**, so its Z footprint is 0 (it can reach the
front/back edge); give X a real half-width. Revisit when real meshes replace billboards.

### 3.5 Prefer Clay for ALL 2D UI/HUD — it avoids render glitches ⚠️

**Build every HUD / menu / overlay with the Clay layout engine (`extern/clay-ps3`), not
hand-drawn `ya2d`/`ttf` calls.** Reserve raw `ya2d`/`tiny3d` primitives for the 3D scene
(floor, sprites, projectiles) only.

**Why (hard-won):** mixing hand-drawn `display_ttf_string` / `ya2d_drawRectZ` HUD draws
with the 3D scene produced **transient single-frame render glitches** — warped bars,
scattered text, stray geometry across the frame — that were maddening to chase (a `LINES`
→ quads attempt didn't fix it). The glitches **disappeared the moment the HUD was moved
into Clay** (`clay_render` issues its draws through one consistent path). So a split HUD
(some Clay, some hand-drawn) is the worst case; go all-Clay.

Two patterns cover almost any game HUD purely in Clay:
- **Progress bar** = a fixed-size track with a `CLAY_SIZING_PERCENT(frac)` fill child + a
  `CLAY_SIZING_GROW(0)` remainder (e.g. the blue/red balance bar, the ki bars).
- **Tick marks / centered markers** = `CLAY_FLOATING` children
  (`attachTo = CLAY_ATTACH_TO_PARENT`, `.attachPoints`, `.offset = {px,0}`) so they overlay
  a bar without disturbing its fill layout.

Text, borders, images and semi-transparent backgrounds all render (alpha blends).

**Build the Clay UI as a C TU even from a C++ game.** The `CLAY(...)` / `CLAY_TEXT_CONFIG(...)`
macros are C compound-literals — happiest under `-std=gnu99` (the renderer's own language) and
finicky in C++. Keep each screen's layout in a `.c` file and call it from the C++ game through a
tiny `extern "C"` bridge (`clay_render_menu(title, items, n, sel)`), passing plain C strings/ints.
Keep `CLAY_IMPLEMENTATION` in exactly one TU (the renderer).

⚠️ **`CLAY_IDI(label, i)` / `CLAY_ID(label)` need a string *literal*** — the macro
stringifies it, so a ternary (`CLAY_IDI(cond ? "A" : "B", 0)`) fails to compile. Use a
fixed literal label and disambiguate with the index: `CLAY_IDI("KiTick", side*3 + k)`.
(Note: the PS3 Clay renderer ignores `cornerRadius` — borders are square.)

### 3.6 Fonts come from `/dev_flash`

System TTFs (`/dev_flash/data/font/SCE-PS3-*.TTF`) are present on real consoles and
RPCS3. Load them via the `ttf_render` helper; don't ship your own for basic UI.

## 4. Game loop & simulation

- **Clean XMB exit:** `sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, cb, NULL)` once, then
  `sysUtilCheckCallback()` **every frame**; set a `running = 0` flag on `SYSUTIL_EXIT_GAME`.
  Without the per-frame check the console can't reclaim the app.
- **Fixed timestep.** The loop runs at vsync; use a constant `FRAME_DT = 1/60` for
  rates (charge/sec, speeds) instead of measuring wall time. Simple and deterministic.
- **Fixed pools for transient objects** (projectiles, explosions): a small `struct{int active; …}` array
  with an `active` flag. No per-frame allocation, trivial lifetime, cache-friendly.
- **Embed assets with `bin2o`.** Files in `data/*.png|jpg|bin|mod|s3m` become extern
  symbols (`foo_png` / `foo_png_size`) linked into the `.self`, so they work over
  `ps3load` with no PKG install. Bulky assets go in `pkgfiles/assets/` (PKG only).

## 5. Audio (MikMod)

MikMod plays tracker **modules** and **samples** (PCM) — it **cannot decode OGG/MP3**.
Drive it with `MikMod_Update()` once per frame and reserve voices with
`MikMod_SetNumVoices(music, sfx)` (sample playback needs sfx voices reserved).

### 5.1 Synthesize SFX in code — no assets needed

You can generate sound effects procedurally: build a PCM waveform with math (a decaying
sine for a "thud", a falling sweep for a "pew", noise for an explosion), wrap it as a
little-endian WAV in memory, and load it as a MikMod sample. Self-contained (no asset
files), great for prototyping, placeholders, or a stylized retro feel.

```c
#define SR 22050
short pcm[SR/2];
int n = SR * 8 / 100;                       /* 80 ms */
for (int i = 0; i < n; i++) {
    float t = (float)i / SR, env = expf(-t * 22.0f);     /* fast decay */
    float s = sinf(2*M_PI*150.0f*t) * 0.7f + noisef() * 0.3f;
    pcm[i] = (short)(env * s * 22000.0f);
}
/* -> make_wav(pcm, n) into a byte buffer, then Sample_LoadGeneric (see 5.2/5.3) */
```

`noisef()` is a tiny LCG mapped to −1..1. Vary frequency/decay/noise mix per effect.

### 5.2 Load audio from memory via an MREADER

MikMod's loaders (`Sample_LoadGeneric`, `Player_LoadGeneric`) take an **`MREADER`**, not a
raw pointer. Implement a ~5-function in-memory reader over an embedded buffer (from
`bin2o`) and pass `&reader.core`:

```c
typedef struct { MREADER core; const unsigned char *data; long size, pos; } MemReader;
/* Get -> byte or EOF; Read -> 1 on FULL read else 0; Seek -> 0 on success (non-zero
 * error); Tell -> pos; Eof -> pos>=size.  NOTE the inverted Read vs Seek conventions. */
SAMPLE *s = Sample_LoadGeneric(&mr.core);   /* NULL on failure */
```

Embed the `.wav` with `bin2o` by naming the file `*.bin` in `data/` (the Makefile's
`bin2o` rule keys on extension; MikMod parses the RIFF header, not the filename).

Convert real assets to a clean canonical WAV first (strips odd chunks that loaders trip
on), e.g. with ffmpeg:
```bash
ffmpeg -i in.ogg -ac 1 -ar 22050 -c:a pcm_s16le -map_metadata -1 -f wav data/music.bin
```

### 5.3 Hand-built WAV bytes must be little-endian (PPU is big-endian) ⚠️

WAV is little-endian by spec. If you **build** a WAV yourself (e.g. for synthesized SFX),
write the header fields *and* the int16 PCM as explicit LE bytes — a naive `u32`/`s16`
store is big-endian on the PPU and MikMod will misread it. (Real `.wav` files from ffmpeg
are already LE, so this only bites hand-built buffers.)

```c
static void put_u16le(unsigned char *b, unsigned v){ b[0]=v; b[1]=v>>8; }
static void put_u32le(unsigned char *b, unsigned v){ b[0]=v; b[1]=v>>8; b[2]=v>>16; b[3]=v>>24; }
```

### 5.4 Loop long music as a sample (the OGG/MP3 way)

Since MikMod can't play OGG/MP3 and a module would mean re-authoring, decode the track to
PCM WAV on the host (ffmpeg) and play it as **one long looping sample**: set `SF_LOOP` +
`loopend`, and play with `SFX_CRITICAL` so transient SFX voices can't steal it.

```c
music->flags |= SF_LOOP; music->loopstart = 0; music->loopend = music->length;
SBYTE v = Sample_Play(music, 0, SFX_CRITICAL);
if (v >= 0) Voice_SetVolume(v, 150);        /* duck under SFX */
```

### 5.5 Init audio defensively — it can hang the console ⚠️

A bad audio init can **freeze a PS3** (double-init is a known hang), and you can't hear it
on the dev host. Guard every step; on any failure set an `audio_ok = 0` flag and make all
calls no-ops (degrade to silence). Isolate audio in its own file so it's easy to disable.

## 6. Porting from a Unity (or other engine) game

- **Read the original mechanic before assuming it.** This game's "health" is a
  *power tug-of-war* (a hit transfers power; you win by filling the bar to 100, with a
  single shared balance bar), not a health-to-zero race. Porting it as the latter would
  be wrong. Read the source script, don't infer from the genre.
- **Map engine input to the pad explicitly.** Unity `CrossPlatformInput` axes/buttons →
  concrete DualShock buttons; write the mapping down (header comment + HUD hint).
- **Replace `Rigidbody`/`Collider` with hand-rolled math.** Overlap = XZ distance vs a
  radius; "raycast" = step + proximity test. PS3 has no physics engine to lean on.
- **State machines port cleanly** (round/match flow, AI FSM) — keep them explicit.
- **Record every deviation.** Where the port simplifies the original (e.g. a single
  damage base instead of an inner/outer split, or skipping explosion knockback), say so
  in the commit and the roadmap so it's a known choice, not a silent bug.

## 7. Porting a C++/SFML game (the framework-shim approach)

When the original is **already C++ on a portable framework** (SFML, raylib, SDL…), don't
rewrite the game logic — **wrap the framework behind a thin shim** backed by PS3 APIs, so
the game code compiles nearly unchanged.

- **One shim header per framework namespace.** Provide just the `sf::` types the game
  actually uses (grep the source — it's usually a small set): `sf::Texture` →
  `ya2d_Texture`, `sf::Sprite` → a textured-quad draw with position/scale/`IntRect`,
  `sf::RenderWindow` → tiny3d clear/flip, `sf::View` → a 2D camera offset applied to
  draws, `sf::Keyboard` → the pad mapping, `sf::Font`/`sf::Text` → `ttf_render` or Clay,
  `sf::Music`/`sf::Sound` → MikMod, plus value types (`sf::Vector2`, `sf::Color`,
  `sf::IntRect`, `sf::RectangleShape`). Implement behaviour incrementally — stub first so
  it **links**, then fill in rendering/input/audio.
- **C++↔C header interop.** The PS3 libs are C. Headers with their own
  `#ifdef __cplusplus extern "C"` guards (`tiny3d.h`, `io/pad.h`, `sysutil.h`) include
  normally; **un-guarded ones (`ya2d.h`) must be wrapped** in `extern "C"` from C++, or the
  C++ compiler mangles the names → undefined references. ⚠️ But wrapping a header that
  transitively pulls **system** headers (`ya2d.h` → `machine/malloc.h`) drags those into the
  block and clashes ("conflicting declaration … with 'C' linkage"). Fix: **pre-include the C
  system headers** (`<stdlib.h>`, `<malloc.h>`, `<string.h>`) *before* the `extern "C"` wrap
  so their include guards are already set. Give any C helper you vendor (e.g. `ttf_render.h`)
  its own `extern "C"` guard.
- **`ppu-g++` is gcc 7.2 → no C++20.** Build the source at `-std=gnu++17` (set `CXXFLAGS`
  separately from the C `-std=gnu99`; don't let `CXXFLAGS = CFLAGS` apply gnu99 to C++).
  Replace C++20-only constructs (concepts, ranges, `<bit>`, designated-init quirks) as you
  hit them. `-fno-exceptions -fno-rtti` keeps the binary lean if the game doesn't need them.
- **newlib's libstdc++ lacks `std::to_string` / `std::stoi` / `std::stof`** ("'to_string'
  is not a member of 'std'"). Provide them in a tiny `ps3_compat.h` (snprintf / strtol
  based) and **force-include it everywhere** via `CXXFLAGS += -include ps3_compat.h` — no
  edits to the original source needed.
- **⚠️ A slim shim won't pull the transitive includes the original leaned on.** The game
  called `abs()` on **floats**. Upstream, `<cmath>` arrived transitively (via SFML headers)
  so it bound to the float overload; our lean shim didn't include it, so `abs()` resolved to
  C's **`abs(int)`** and silently **truncated every float** — it wrecked AABB-overlap
  collisions and divided by zero in `flip /= abs(flip)`. No compile error; wrong results at
  runtime. Use `std::fabs` explicitly and `#include <cmath>` wherever the game does float
  math; never trust unqualified `abs`/`round`/`min`/`max` to resolve as they did upstream.
- **Build against a stub shim first.** A header-only shim with the ~20 methods the game
  actually calls is enough to get the whole codebase *compiling & linking* (stub bodies),
  before any real rendering exists — a huge, verifiable milestone that de-risks the rest.
  Keep the texture handle an opaque `void*` in the header so it stays pure C++ (no ya2d
  include); the real backend `.cpp` fills it in later.
- **2D camera = subtract the view offset at draw time.** A side-scroller's `sf::View`
  becomes `screen = world - camera`; cull sprites outside the screen. No 3D pipeline to set
  up — ya2d/tiny3d quads in `tiny3d_Project2D`, all drawn at a constant `z` (its depth test is
  `LEQUAL`, so draw order = paint order; see §3.3).
- **⚠️ Map the game's virtual resolution onto tiny3d's fixed 848×512 canvas.**
  `tiny3d_Project2D()` gives a **fixed 848×512** 2D space whatever the output mode, but the
  game thinks in its own window size (mega-mario: 1920×1080, SFML Y-down). So the draw
  transform is two steps: `world → (apply sprite pos/origin/scale, then − camera) → · (848/winW, 512/winH)`.
  Bake the `canvas/window` factor into the vertex positions; don't fight it by resizing the
  window. `getSize()` must return the *virtual* size the game positions against (1920×1080),
  not 848×512, or the camera math (`width()/2` centering, `mapGridToPixels`) drifts.
- **⚠️ `ya2d_drawTextureEx` is full-image only — sub-rects need a raw tiny3d quad.**
  Sprite-sheet animation (`setTextureRect`) and texture atlases require partial UVs, which
  ya2d's helpers don't expose. Emit the quad yourself: `tiny3d_SetTextureWrap(0, t->textureOffset,
  t->textureWidth, t->textureHeight, t->rowBytes, (text_format)t->format, …, TEXTURE_NEAREST)`
  then `SetPolygon(TINY3D_QUADS)` + 4 × `VertexPos/Color/Texture`. **Normalize UVs by
  `textureWidth/Height`** (the allocated, possibly padded VRAM size), *not* `imageWidth`.
  `ya2d_Texture.format` already holds the tiny3d `text_format` enum (RGBA → `A8R8G8B8`).
  `TEXTURE_NEAREST` (=0) keeps pixel art crisp; building the corners from the full SFML
  transform `world = pos + (local − origin)·scale` makes negative `scale.x` (facing flip)
  fall out for free.
- **⚠️ Match SFML default-value semantics, or feedback loops oscillate.** A
  `RenderWindow`'s view defaults to the *centered* default view (center = size/2), not
  `(0,0)`. Side-scrollers often write the camera as `view.setCenter(x, height() -
  getView().getCenter().y)` — that's only stable because `height - height/2 == height/2` is a
  fixed point. If your `sf::View()` default-constructs center `(0,0)`, the term oscillates
  `height<->0` every frame and the whole scene jumps vertically (looked exactly like a
  GPU/buffer flicker — half the sprites cull out each frame, alternating). Initialize the
  window view to `getDefaultView()` in `create()`. Lesson: when a shim feeds its own getters
  back into setters, the *default* value must match the real framework's.
- **Cull off-screen sprites at draw time.** A wide level draws far more entities than fit on
  screen (here ~210 tiles wide, ~30 visible). Compute the sprite's screen AABB and skip if it
  misses the 848×512 canvas — both a perf win and it keeps the per-frame tiny3d draw count in
  the range the renderer is proven at.
- **Tilemaps / level configs** are usually plain `.txt`. Embed them with `bin2o` (name
  them `*.bin`) and parse from memory (`std::istringstream` over the embedded buffer — the
  game's `fin >> token` loops work unchanged), or ship them in the PKG `assets/`.
- **Asset paths → embedded buffers.** The game loads textures by file path; there's no
  filesystem, so map **path basename → bin2o buffer** in a small generated registry and
  have the shim's `loadFromFile` do the lookup → `ya2d_loadPNGfromBuffer`. No edits to the
  game's asset code.
- **⚠️ Bring the platform up lazily if the game loads assets before the window.** mega-mario
  loads every texture in its ctor *before* `window.create()` — but on PS3 it's `create()`
  that inits tiny3d/ya2d, so the first `loadFromFile` decoded a PNG into an uninitialised RSX
  and crashed on boot. Fix: make platform bring-up **idempotent + lazy** — call it from both
  `create()` *and* the first `loadFromFile()`, guarded by a `ready` flag — so whichever the
  game reaches first wins.
- **⚠️ A C header with non-`extern` globals breaks multi-TU C++ links.** `ya2d_controls.h`
  has `padData ya2d_paddata[7];` (no `extern`): in C that's a tentative def (merges); in
  **C++ it's a real definition**, so two C++ TUs including `<ya2d/ya2d.h>` → "multiple
  definition". Fix: include only the sub-headers you need (skip the offending one — we use
  `io/pad.h` for input, not ya2d's), e.g. via a private `ya2d_lite.h`.

---

## TL;DR checklist for a new input/render feature

1. Read input from the **retained** pad packet, never assume fresh data each frame.
2. **Edge-detect** one-shot actions; level-read holds.
3. Watch the **colour format** (clear = ARGB, ya2d/vertex = RGBA).
4. Build **all 2D UI/HUD in Clay** (not hand-drawn ya2d/ttf) — a split HUD causes
   transient render glitches; keep raw primitives for the 3D scene only.
5. Keep the **camera math deterministic**; sort draws far-to-near.
6. Build **green under `-Wall`** in the toolchain image; mark on-hardware as unverified.
7. **Guard audio init defensively** — a bad init can hang the console, and you can't hear
   it on the dev host.
8. **Document deviations** from the source game.
