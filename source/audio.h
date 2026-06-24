/*
 * Audio (Phase 8) — MikMod-backed music + SFX for the PS3 port.
 *
 * The original mega-mario ships no audio, so every sound here is *synthesized in
 * code* (simple square/noise waveforms wrapped in little-endian WAVs and loaded
 * as MikMod samples — docs/PATTERNS.md §5.1/§5.3). Init is fully defensive: if
 * anything fails, audio_ok stays 0 and every entry point is a no-op, so a bad
 * audio init can never hang the console (§5.5).
 *
 * C TU; the C++ game code (Scene_Play / GameEngine) calls these via extern "C".
 */
#ifndef AUDIO_H
#define AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

void audio_init(void);       /* once at startup (after the PS3 stack is up)   */
void audio_update(void);     /* every frame (drives MikMod's software mixer)  */
void audio_shutdown(void);   /* on exit                                        */

void audio_play_jump(void);
void audio_play_coin(void);
void audio_play_brick(void);     /* brick break / block bump                  */
void audio_play_shoot(void);
void audio_play_levelclear(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H */
