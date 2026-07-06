#include "Scene_Benchmark.h"

#include "GameEngine.h"
#include "Scene_Menu.h"

#include <ppu-types.h>
#include <lv2/sysfs.h>
#include <memory>
#include <string>

// Each backend defines its own name (source/sfml_backend.cpp): "tiny3d" / "raylib"
// / "rsxgl". Used to tag the output file so the three runs don't overwrite.
extern const char *BACKEND_NAME;

namespace {

// Cheap SINGLE-FRAME sprites (no animation cost), several textures so the draw
// load doesn't trivially batch to ~1 call. Names from assets.txt.
const char *SPRITES[] = {"Bullet", "Brick", "Block", "Ground", "Question2"};
const int   NUM_SPRITES = 5;

// fixed(2) float -> string without <iomanip> / printf floats.
std::string f2(double v) {
  if (v < 0) v = 0;
  long whole = (long)v;
  int  frac = (int)((v - (double)whole) * 100.0 + 0.5);
  std::string s = std::to_string(whole) + ".";
  if (frac < 10) s += "0";
  return s + std::to_string(frac);
}

}  // namespace

Scene_Benchmark::Scene_Benchmark(GameEngine *gameEngine) {
  m_game = gameEngine;
  init();
}

void Scene_Benchmark::init() {
  registerAction(sf::Keyboard::Escape, "Quit");
  openFile();
  writeLine(
      "backend,stage,target,live,frames,avg_ms,avg_fps,min_fps,max_fps,spawns,"
      "destroys\n");
}

void Scene_Benchmark::onEnd() {
  if (m_fd >= 0) {
    sysFsClose(m_fd);
    m_fd = -1;
  }
  m_game->changeScene("menu", std::make_shared<Scene_Menu>(m_game), true);
}

// Deterministic LCG -> 0..32767. Identical sequence on every backend.
unsigned int Scene_Benchmark::rnd() {
  m_rng = m_rng * 1103515245u + 12345u;
  return (m_rng >> 16) & 0x7fff;
}

void Scene_Benchmark::spawnOne() {
  auto e = m_entityManager.addEntity("b");
  float x = (float)(rnd() % 1920);
  float y = (float)(rnd() % 1080);
  float vx = (float)(rnd() % 1000) / 100.0f - 5.0f;  // ~ -5..+5 px/frame
  float vy = (float)(rnd() % 1000) / 100.0f - 5.0f;
  e->addComponent<CTransform>(Vec2(x, y), Vec2(vx, vy));
  e->addComponent<CAnimation>(
      m_game->assets().getAnimation(SPRITES[rnd() % NUM_SPRITES]), false);
  e->addComponent<CLifeSpan>(60 + (int)(rnd() % 121));  // 60..180 frames
  m_spawns++;
}

void Scene_Benchmark::sMovement() {
  for (auto &e : m_entityManager.getEntities()) {
    if (!e->hasComponent<CTransform>()) continue;
    auto &t = e->getComponent<CTransform>();
    t.pos.x += t.velocity.x;
    t.pos.y += t.velocity.y;
    if (t.pos.x < 0)    { t.pos.x = 0;    t.velocity.x = -t.velocity.x; }
    if (t.pos.x > 1920) { t.pos.x = 1920; t.velocity.x = -t.velocity.x; }
    if (t.pos.y < 0)    { t.pos.y = 0;    t.velocity.y = -t.velocity.y; }
    if (t.pos.y > 1080) { t.pos.y = 1080; t.velocity.y = -t.velocity.y; }
  }
}

void Scene_Benchmark::endStage() {
  int    live   = (int)m_entityManager.getEntities().size();
  double avgMs  = m_measured > 0 ? m_sumMs / m_measured : 0.0;
  double avgFps = avgMs   > 0 ? 1000.0 / avgMs   : 0.0;
  double minFps = m_maxMs > 0 ? 1000.0 / m_maxMs : 0.0;  // slowest frame
  double maxFps = m_minMs > 0 ? 1000.0 / m_minMs : 0.0;  // fastest frame

  writeLine(std::string(BACKEND_NAME) + "," + std::to_string(m_stage) + "," +
            std::to_string(m_target[m_stage]) + "," + std::to_string(live) + "," +
            std::to_string(m_measured) + "," + f2(avgMs) + "," + f2(avgFps) + "," +
            f2(minFps) + "," + f2(maxFps) + "," + std::to_string(m_spawns) + "," +
            std::to_string(m_destroys) + "\n");

  m_sumMs = 0; m_measured = 0; m_minMs = 1e9; m_maxMs = 0;
  m_spawns = 0; m_destroys = 0;
  m_stage++;
  m_stageFrame = 0;
  if (m_stage >= NUM_STAGES) {
    m_done = true;
    if (m_fd >= 0) { sysFsClose(m_fd); m_fd = -1; }
    for (auto &e : m_entityManager.getEntities()) e->destroy();
  }
}

void Scene_Benchmark::update() {
  // Frame-to-frame time (portable across all three backends).
  struct timeval now;
  gettimeofday(&now, NULL);
  if (m_haveLast && !m_done) {
    double ms = (now.tv_sec - m_lastTv.tv_sec) * 1000.0 +
                (now.tv_usec - m_lastTv.tv_usec) / 1000.0;
    if (m_stageFrame >= WARMUP) {
      m_sumMs += ms;
      m_measured++;
      if (ms < m_minMs) m_minMs = ms;
      if (ms > m_maxMs) m_maxMs = ms;
    }
  }
  m_lastTv = now;
  m_haveLast = true;

  m_entityManager.update();  // commit last frame's spawns + removals

  if (!m_done) {
    // Lifespan -> destroy (continuous churn); count it.
    for (auto &e : m_entityManager.getEntities()) {
      if (e->hasComponent<CLifeSpan>()) {
        if (--e->getComponent<CLifeSpan>().remaining <= 0) {
          e->destroy();
          m_destroys++;
        }
      }
    }
    sMovement();
    // Refill to the stage's target population (deferred; commits next frame).
    int live = (int)m_entityManager.getEntities().size();
    for (int i = live; i < m_target[m_stage]; i++) spawnOne();

    m_stageFrame++;
    if (m_stageFrame >= STAGE_FRAMES) endStage();
  }

  sRender();
  m_currentFrame++;
}

void Scene_Benchmark::sRender() {
  m_game->window().clear(sf::Color(0, 0, 0));
  m_game->window().setView(m_game->window().getDefaultView());

  for (auto e : m_entityManager.getEntities()) {
    if (e->hasComponent<CTransform>() && e->hasComponent<CAnimation>()) {
      auto &t = e->getComponent<CTransform>();
      auto &spr = e->getComponent<CAnimation>().animation.getSprite();
      spr.setPosition(t.pos.x, t.pos.y);
      m_game->window().draw(spr);
    }
  }

  // Progress bar (RectangleShape renders on all three backends, unlike draw(Text)
  // which is a no-op on the Tiny3D shim). The growing sprite count is the load
  // indicator; this shows how far the run is.
  float total = (float)(NUM_STAGES * STAGE_FRAMES);
  float done  = (float)(m_stage * STAGE_FRAMES + m_stageFrame);
  float frac  = m_done ? 1.0f : done / total;
  sf::RectangleShape bg(sf::Vector2f(1920, 18));
  bg.setPosition(0, 0);
  bg.setFillColor(sf::Color(40, 40, 40));
  m_game->window().draw(bg);
  sf::RectangleShape fill(sf::Vector2f(1920.0f * frac, 18));
  fill.setPosition(0, 0);
  fill.setFillColor(m_done ? sf::Color(0, 200, 0) : sf::Color(255, 209, 0));
  m_game->window().draw(fill);

  m_game->window().display();
}

void Scene_Benchmark::sDoAction(const Action &action) {
  if (action.type() == "START" && action.name() == "Quit") onEnd();
}

void Scene_Benchmark::openFile() {
  const std::string base = std::string("bench_") + BACKEND_NAME + ".csv";
  const int flags = SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC;
  if (sysFsOpen(("/dev_hdd0/tmp/" + base).c_str(), flags, &m_fd, NULL, 0) != 0) {
    m_fd = -1;  // /dev_hdd0/tmp may not exist -> fall back to hdd0 root
    if (sysFsOpen(("/dev_hdd0/" + base).c_str(), flags, &m_fd, NULL, 0) != 0)
      m_fd = -1;
  }
}

void Scene_Benchmark::writeLine(const std::string &s) {
  if (m_fd < 0) return;
  u64 written = 0;
  sysFsWrite(m_fd, s.data(), s.size(), &written);
}
