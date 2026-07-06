#pragma once

#include "Scene.h"
#include <sys/time.h>

// Cross-backend rendering benchmark. Ramps up waves of moving sprites with
// continuous spawn/destroy churn, samples per-frame time, and writes one CSV row
// per stage to /dev_hdd0/tmp/bench_<backend>.csv. The workload is deterministic
// (seeded LCG, fixed frame counts) so the three backends (tiny3d / raylib / rsxgl)
// run an identical load and the results are comparable. Launched from the menu.
// The 60fps cap stays on: light stages read ~60 for all three; the backends
// separate in the heavy, render-bound stages. See ideas/benchmark.md.
class Scene_Benchmark : public Scene {
  static const int NUM_STAGES = 6;
  int  m_target[NUM_STAGES] = {1000, 2000, 4000, 8000, 16000, 32000};  // live sprites per stage
  int  m_stage = 0;
  int  m_stageFrame = 0;
  static const int STAGE_FRAMES = 150;   // frames per stage (fixed, not wall-time)
  static const int WARMUP = 30;          // leading frames excluded from stats

  unsigned int m_rng = 0x12345678u;      // deterministic LCG (same on every backend)

  // per-stage timing / churn accumulators
  double m_sumMs = 0.0, m_minMs = 1e9, m_maxMs = 0.0;
  int    m_measured = 0;
  int    m_spawns = 0, m_destroys = 0;

  struct timeval m_lastTv;
  bool m_haveLast = false;
  bool m_done = false;
  int  m_fd = -1;                         // sysFs file descriptor

  void init();
  void onEnd();
  unsigned int rnd();
  void spawnOne();
  void sMovement();
  void endStage();
  void openFile();
  void writeLine(const std::string &s);

 public:
  Scene_Benchmark(GameEngine *gameEngine = nullptr);
  void update();
  void sDoAction(const Action &action);
  void sRender();
};
