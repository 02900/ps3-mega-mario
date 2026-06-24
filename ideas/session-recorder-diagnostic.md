# Idea: Instant-replay session recorder (video + draw-log) to diagnose the 1-frame glitch

Status: **proposed (not implemented)** · captured 2026-06-23

## Context / problem

The port has an **intermittent 1-frame rendering glitch**: every few seconds, for a single
frame, a block appears out of nowhere over the player and unrelated tiles (even far from the
player) vanish, then everything settles. It is **not** gameplay (not brick-breaking) and
**not** the camera (it happens while jumping in place, where the camera Y is locked). Static
tiles compute identical vertices every frame, so a 1-frame corruption of *some* of them points
at the **RSX vertex/command buffer** being read while reused — but we can't confirm CPU-vs-GPU
from screen-recordings alone, and a 1-frame event is nearly impossible to scrub to.

Goal: a **frame-exact capture** triggered the moment the glitch is seen, capturing **both** the
rendered framebuffer (→ MP4) **and** a per-frame log of what the CPU submitted to draw.
Cross-referencing answers the root cause: video shows the glitch but the log shows correct
submitted rects → RSX/emulator; log is wrong → CPU-side bug located.

Model chosen: **instant-replay** — always keep the last ~8–10 s in RAM (zero disk I/O during
play, so recording can't itself cause hitches/pollution), and **dump on a button press**.

## Feasibility (verified)

- tiny3d exposes the display framebuffers in **CPU-readable main memory**:
  `extern u32 *Video_buffer[2]; extern int Video_currentBuffer; extern int Video_pitch;
  extern videoResolution Video_Resolution;` (tiny3d.h ~L535–545). Format **A8R8G8B8**, dims
  `Video_Resolution.width/height`, row stride `Video_pitch` (≥ width*4, may be padded). All
  confirmed exported in `libtiny3d.a`.
- File I/O: `sysFsOpen/sysFsWrite/sysFsClose` (`lv2/sysfs.h`, flags
  `SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC`); Makefile already links `-lsysfs`.
- RPCS3 (macOS) maps `/dev_hdd0/` → `~/Library/Application Support/rpcs3/dev_hdd0/`.
- Free pad button for the trigger: **SELECT** (`BTN_SELECT`); also free: L2, R2, L3, R3.

## Approach

New isolated TU **`source/session_record.{h,cpp}`** (Makefile auto-globs `source/*.cpp`),
guarded by a compile flag so release builds pay nothing:

```c
#define ENABLE_SESSION_RECORDING 1   // in session_record.h
```

### Ring buffers (RAM, allocated once)
- `REC_SECONDS = 8` → `REC_FRAMES = 480` slots. Each slot = a downscaled RGB frame + that
  frame's draw-log.
- Downscale: decimate to ~256 px wide by `step = max(1, Video_Resolution.width/256)` (nearest),
  store **rgb24** via `r=(px>>16)&0xff; g=(px>>8)&0xff; b=px&0xff` (endian-safe, drops unused
  alpha). ~256×144×3 ≈ 110 KB/frame × 480 ≈ **~53 MB** (fits 256 MB; constants tunable).
- Draw-log: per frame up to `REC_MAX_DRAWS` (~64) records `{u16 texW,texH; i16 sx,sy,sw,sh}`
  (sprite size identifies the tile type; screen rect = where it was submitted). ~370 KB total.
- Plain circular array; `g_rec_head` advances each frame, wrapping.

### Hooks in `source/sfml_backend.cpp`
- `RenderWindow::clear()` → `record_begin_frame()` (reset current-frame draw count).
- `RenderWindow::draw(const Sprite&)` → after the screen AABB (`minx/miny/maxx/maxy`, already
  computed) and before the cull `return`, call
  `record_log_draw(t->imageWidth, t->imageHeight, minx, miny, maxx-minx, maxy-miny, culled)`.
- `RenderWindow::display()` → after `tiny3d_Flip()` (rendering complete) call
  `record_capture_frame()`: read the just-presented buffer (`Video_buffer[<front index>]`,
  resolve the post-flip index on-device — likely `Video_currentBuffer ^ 1`), downscale into the
  ring slot, copy the draw-log, advance the head.
- `build_pad_events()` → **SELECT** calls `record_trigger_dump()` directly (not routed as an
  sf:: key, so it never reaches game logic).
- `platform_init()` → `record_init()` (allocate ring via `memalign(128, …)`).

### Dump on SELECT (`record_trigger_dump()`)
Writes to **`/dev_hdd0/tmp/`** (host: `~/Library/Application Support/rpcs3/dev_hdd0/tmp/`),
incrementing per-boot counter so repeats don't clobber:
- `mmrec_<NN>.rgb` — ASCII header `MMREC <w> <h> <fps> <nframes>\n` then the ring frames in
  chronological order (oldest first from `g_rec_head`).
- `mmrec_<NN>.log` — text: per frame `frame <i>:` then one line per draw
  `tex WxH  scr x,y wxh  [culled]`.
Heavy I/O here is fine (one-time, after the glitch already happened).

### On-screen indicator (recording build only)
`display_ttf_string` "● REC" + live ring frame counter; flash "SAVED n" for a few frames after
a dump.

## Host-side post-processing (when a dump exists)
1. Read `mmrec_<NN>.rgb`, parse the header, strip the header bytes.
2. `ffmpeg -f rawvideo -pixel_format rgb24 -video_size <w>x<h> -framerate 60 -i frames.raw out.mp4`
   (confirm `rgb24` vs `bgr24` against one frame — RPCS3 endianness can surprise).
3. Step to the glitch frame; cross-reference `mmrec_<NN>.log` at that frame — were the vanished
   tiles' rects submitted correctly? → isolates RSX/GPU vs CPU.

## Critical files
- **New**: `source/session_record.h`, `source/session_record.cpp` (everything behind
  `ENABLE_SESSION_RECORDING`).
- **Edit**: `source/sfml_backend.cpp` — include the header + the 5 hook calls above; reuse the
  `Video_*` externs (via `#include <tiny3d.h>`, already pulled in by `ya2d_lite.h`) and the
  screen-AABB already computed in `draw(const Sprite&)`.

## Caveats
- **RPCS3 must have "Write Color Buffers" enabled** (Settings → GPU) or framebuffer readback is
  blank/stale (real PS3 always has it in main memory).
- Press **SELECT within ~8 s** of the glitch so it's still in the ring.
- If `Video_buffer` reads blank on RPCS3 even with WCB, fall back to capturing **before** the
  next frame's `tiny3d_Flip()`, or rely on `mmrec_<NN>.log` alone (still decisive for CPU-vs-GPU).

## Verification
1. `DOCKER_DEFAULT_PLATFORM=linux/amd64 ./scripts/build.sh` → green `src.self` (flag on).
2. Run on RPCS3 (WCB on), Level 1, play until the glitch flickers, press **SELECT**.
3. Confirm `mmrec_00.rgb` + `mmrec_00.log` under `~/Library/Application Support/rpcs3/dev_hdd0/tmp/`.
4. Convert `.rgb` → MP4, verify it shows the session (ideally the glitch frame); check the `.log`.
5. Compare video vs log → RSX-vs-CPU, then fix the actual rendering bug (separate follow-up).
