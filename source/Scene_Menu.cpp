#include "Scene_Menu.h"
#include "GameEngine.h"
#include "Scene_Play.h"
#include "Scene_Benchmark.h"
#include "clay_menu.h"  // UI rendered in Clay, not hand-drawn sf::Text (PATTERNS §3.5)

Scene_Menu::Scene_Menu() {}

Scene_Menu::Scene_Menu(GameEngine *gameEngine) {
  m_game = gameEngine;
  init();
}

void Scene_Menu::init() {
  m_currentFrame = 0;
  registerAction(sf::Keyboard::Escape, "Quit");
  registerAction(sf::Keyboard::Up, "Up");
  registerAction(sf::Keyboard::Down, "Down");
  registerAction(sf::Keyboard::Enter, "Level Selected");

  m_title = "MEGA MARIO";

  m_menuStrings = std::vector<std::string>(4);
  m_menuStrings[0] = "LEVEL 1";
  m_menuStrings[1] = "LEVEL 2";
  m_menuStrings[2] = "LEVEL 3";
  m_menuStrings[3] = "BENCHMARK";

  m_levelPaths = std::vector<std::string>(4);
  m_levelPaths[0] = "../bin/level1.txt";
  m_levelPaths[1] = "../bin/level2.txt";
  m_levelPaths[2] = "../bin/level3.txt";
  m_levelPaths[3] = "";  // benchmark: no level file
}

void Scene_Menu::onEnd() { m_game->window().close(); }

void Scene_Menu::update() {
  sRender();
  m_currentFrame++;
}

void Scene_Menu::sRender() {
  m_game->window().clear(sf::Color(0, 87, 217));

  // The menu UI is built natively in Clay (docs/PATTERNS.md §3.5), not drawn with
  // sf::Text. Hand the menu state to the Clay renderer between clear() and
  // display() (the clear() set up the tiny3d 2D frame the renderer draws into).
  const char *items[16];
  int n = (int)m_menuStrings.size();
  if (n > 16) n = 16;
  for (int i = 0; i < n; i++)
    items[i] = m_menuStrings[i].c_str();
  clay_render_menu(m_title.c_str(), items, n, (int)m_selectedMenuIndex);

  m_game->window().display();
}

void Scene_Menu::sDoAction(const Action &action) {
  if (action.name() == "Quit" && m_currentFrame > 30) {
    onEnd();
    return;
  }
  if (action.type() == "START") {
    if (action.name() == "Down")
      m_selectedMenuIndex =
          (m_selectedMenuIndex + 1) % (int)m_menuStrings.size();

    else if (action.name() == "Up")
      m_selectedMenuIndex =
          (m_selectedMenuIndex - 1 + (int)m_menuStrings.size()) %
          (int)m_menuStrings.size();

    else if (action.name() == "Level Selected") {
      if (m_selectedMenuIndex == 3) {  // BENCHMARK
        m_game->changeScene("benchmark",
                            std::make_shared<Scene_Benchmark>(m_game), false);
      } else {
        std::shared_ptr<Scene_Play> scene = std::make_shared<Scene_Play>(
            m_game, m_levelPaths[m_selectedMenuIndex]);
        m_game->changeScene("play", scene, false);
      }
    }
  }
}
