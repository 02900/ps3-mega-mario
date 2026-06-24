/*
 * Audio (Phase 8) — MikMod music + SFX, all synthesized in code.
 *
 * mega-mario has no audio assets, so each sound is generated as 16-bit mono PCM
 * (square / noise waveforms), wrapped in a little-endian WAV (the PPU is
 * big-endian, so the header + samples are emitted byte-by-byte — docs/PATTERNS.md
 * §5.3), and handed to MikMod through an in-memory MREADER (§5.2). Init is
 * defensive: any failure leaves audio_ok = 0 and every call becomes a no-op (§5.5).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ppu-types.h>
#include <mikmod.h>

#include "audio.h"

#define SR 22050   /* synthesis sample rate */

static int     audio_ok = 0;
static SAMPLE *s_jump = NULL, *s_coin = NULL, *s_brick = NULL,
              *s_shoot = NULL, *s_clear = NULL, *s_music = NULL;
static SBYTE   music_voice = -1;

/* ---- in-memory MREADER (lets MikMod parse a WAV from a RAM buffer) -------- */
typedef struct { MREADER core; const unsigned char *data; long size, pos; } MemReader;

static BOOL mr_eof(MREADER *r)  { MemReader *m = (MemReader *)r; return m->pos >= m->size; }
static long mr_tell(MREADER *r) { return ((MemReader *)r)->pos; }
static int  mr_get(MREADER *r)
{
	MemReader *m = (MemReader *)r;
	return (m->pos >= m->size) ? EOF : m->data[m->pos++];
}
static BOOL mr_read(MREADER *r, void *dst, size_t n)
{
	MemReader *m = (MemReader *)r;
	long rem = m->size - m->pos;
	if ((long)n > rem) { if (rem > 0) { memcpy(dst, m->data + m->pos, rem); m->pos += rem; } return 0; }
	memcpy(dst, m->data + m->pos, n); m->pos += n; return 1;
}
static BOOL mr_seek(MREADER *r, long off, int whence)
{
	MemReader *m = (MemReader *)r;
	long base = (whence == SEEK_SET) ? 0 : (whence == SEEK_CUR) ? m->pos : m->size;
	long np = base + off;
	if (np < 0 || np > m->size) return -1;
	m->pos = np; return 0;
}
static SAMPLE *load_wav(const void *data, long size)
{
	MemReader mr;
	mr.core.Seek = mr_seek; mr.core.Tell = mr_tell; mr.core.Read = mr_read;
	mr.core.Get = mr_get;   mr.core.Eof = mr_eof;
	mr.data = (const unsigned char *)data; mr.size = size; mr.pos = 0;
	return Sample_LoadGeneric(&mr.core);
}

/* ---- PCM synthesis ------------------------------------------------------- */
static unsigned g_rng = 0x1234567u;
static int rnd(void) { g_rng = g_rng * 1103515245u + 12345u; return (int)((g_rng >> 16) & 0x7fff); }

/* Append a segment to buf (cap samples). f0->f1 sweeps the frequency; wave 0 =
 * square, 1 = noise; f0<=0 => silence. A short attack/release ramp avoids clicks.
 * phase carries across calls so swept/contiguous tones stay continuous. */
static int seg(SWORD *buf, int cap, int pos, double f0, double f1,
               int ms, int amp, int wave, double *phase)
{
	int n = (int)((long)ms * SR / 1000); if (n < 1) n = 1;
	int atk = SR * 3 / 1000, rel = SR * 10 / 1000;
	for (int i = 0; i < n && pos < cap; i++, pos++) {
		if (f0 <= 0 && f1 <= 0) { buf[pos] = 0; continue; }
		double frac = (double)i / n;
		double f = f0 + (f1 - f0) * frac;
		*phase += f / SR;
		if (*phase >= 1.0) *phase -= (double)(long)*phase;   /* wrap to [0,1), no libm */
		double e = 1.0;
		if (i < atk)            e = (double)i / atk;
		else if (i > n - rel)   e = (double)(n - i) / rel;
		double v = (wave == 1) ? ((rnd() & 1) ? amp : -amp)
		                       : ((*phase < 0.5) ? amp : -amp);
		int s = (int)(v * e);
		buf[pos] = (SWORD)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
	}
	return pos;
}

/* Wrap PCM (n samples, mono 16-bit) in a little-endian WAV and load it. */
static void put_le32(unsigned char *p, u32 v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
static void put_le16(unsigned char *p, u16 v) { p[0]=v; p[1]=v>>8; }

static SAMPLE *load_pcm(const SWORD *pcm, int n)
{
	long data = (long)n * 2, total = 44 + data;
	unsigned char *w = (unsigned char *)malloc(total);
	if (!w) return NULL;
	memcpy(w, "RIFF", 4);          put_le32(w + 4, (u32)(36 + data));
	memcpy(w + 8, "WAVE", 4);
	memcpy(w + 12, "fmt ", 4);     put_le32(w + 16, 16);
	put_le16(w + 20, 1);           put_le16(w + 22, 1);          /* PCM, mono   */
	put_le32(w + 24, SR);          put_le32(w + 28, SR * 2);     /* rate, byterate */
	put_le16(w + 32, 2);           put_le16(w + 34, 16);         /* block, bits */
	memcpy(w + 36, "data", 4);     put_le32(w + 40, (u32)data);
	for (int i = 0; i < n; i++) put_le16(w + 44 + i * 2, (u16)pcm[i]);
	SAMPLE *s = load_wav(w, total);
	free(w);
	return s;
}

static SWORD g_scratch[SR * 3];   /* up to 3 s; SFX are much shorter */

static SAMPLE *synth_jump(void)
{
	double ph = 0; int n = seg(g_scratch, SR * 3, 0, 330, 920, 130, 9000, 0, &ph);
	return load_pcm(g_scratch, n);
}
static SAMPLE *synth_coin(void)
{
	double ph = 0; int n = 0;
	n = seg(g_scratch, SR * 3, n,  988,  988,  70, 9000, 0, &ph);   /* B5 */
	n = seg(g_scratch, SR * 3, n, 1319, 1319, 150, 9000, 0, &ph);   /* E6 */
	return load_pcm(g_scratch, n);
}
static SAMPLE *synth_brick(void)
{
	double ph = 0; int n = 0;
	n = seg(g_scratch, SR * 3, n, 180, 70, 60, 10000, 0, &ph);      /* low thud  */
	n = seg(g_scratch, SR * 3, n,   1,  1, 70,  8000, 1, &ph);      /* debris noise */
	return load_pcm(g_scratch, n);
}
static SAMPLE *synth_shoot(void)
{
	double ph = 0; int n = seg(g_scratch, SR * 3, 0, 1200, 300, 90, 8000, 0, &ph);
	return load_pcm(g_scratch, n);
}
static SAMPLE *synth_clear(void)
{
	double ph = 0; int n = 0;
	int notes[5] = { 523, 659, 784, 1047, 1319 };                  /* C E G C E */
	for (int k = 0; k < 5; k++) n = seg(g_scratch, SR * 3, n, notes[k], notes[k], 120, 8000, 0, &ph);
	return load_pcm(g_scratch, n);
}
static SAMPLE *synth_music(void)
{
	/* A short cheerful square-wave loop in C major (not the Mario theme). */
	static const int mf[16] = { 523, 659, 784, 1047, 784, 659, 587, 523,
	                            587, 659, 698, 784, 659, 523, 587, 0 };
	static const int md_[16] = { 150,150,150,150,150,150,150,300,
	                             150,150,150,150,150,150,300,150 };
	double ph = 0; int n = 0;
	for (int k = 0; k < 16; k++) {
		n = seg(g_scratch, SR * 3, n, mf[k], mf[k], md_[k] - 18, 5200, 0, &ph);
		n = seg(g_scratch, SR * 3, n, 0, 0, 18, 0, 0, &ph);         /* tiny gap */
	}
	return load_pcm(g_scratch, n);
}

/* ---- public API ---------------------------------------------------------- */
void audio_init(void)
{
	if (audio_ok) return;

	MikMod_RegisterAllDrivers();
	MikMod_RegisterAllLoaders();

	md_mode = DMODE_STEREO | DMODE_16BITS | DMODE_SOFT_MUSIC | DMODE_SOFT_SNDFX;
	md_mixfreq = 48000;

	if (MikMod_Init("")) return;                       /* silent on failure */
	MikMod_SetNumVoices(0, 16);
	if (MikMod_EnableOutput()) { MikMod_Exit(); return; }

	s_jump  = synth_jump();
	s_coin  = synth_coin();
	s_brick = synth_brick();
	s_shoot = synth_shoot();
	s_clear = synth_clear();

	s_music = synth_music();
	if (s_music) {
		s_music->flags    |= SF_LOOP;
		s_music->loopstart = 0;
		s_music->loopend   = s_music->length;
		music_voice = Sample_Play(s_music, 0, SFX_CRITICAL);   /* won't be stolen */
		if (music_voice >= 0) Voice_SetVolume(music_voice, 110);  /* duck under SFX */
	}

	audio_ok = 1;
}

void audio_update(void)   { if (audio_ok) MikMod_Update(); }

void audio_shutdown(void)
{
	if (!audio_ok) return;
	audio_ok = 0;
	if (music_voice >= 0) Voice_Stop(music_voice);
	if (s_music) Sample_Free(s_music);
	if (s_jump)  Sample_Free(s_jump);
	if (s_coin)  Sample_Free(s_coin);
	if (s_brick) Sample_Free(s_brick);
	if (s_shoot) Sample_Free(s_shoot);
	if (s_clear) Sample_Free(s_clear);
	MikMod_DisableOutput();
	MikMod_Exit();
}

void audio_play_jump(void)       { if (audio_ok && s_jump)  Sample_Play(s_jump,  0, 0); }
void audio_play_coin(void)       { if (audio_ok && s_coin)  Sample_Play(s_coin,  0, 0); }
void audio_play_brick(void)      { if (audio_ok && s_brick) Sample_Play(s_brick, 0, 0); }
void audio_play_shoot(void)      { if (audio_ok && s_shoot) Sample_Play(s_shoot, 0, 0); }
void audio_play_levelclear(void) { if (audio_ok && s_clear) Sample_Play(s_clear, 0, 0); }
