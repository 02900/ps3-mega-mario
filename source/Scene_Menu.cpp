#include "Scene_Menu.h"
#include "GameEngine.h"
#include "Scene_Play.h"

Scene_Menu::Scene_Menu() {}

Scene_Menu::Scene_Menu(GameEngine *gameEngine) {
  m_game = gameEngine;
  init();
}

void Scene_Menu::init() {
  m_currentFrame = 0;
  registerAction(sf::Keyboard::Escape, "Quit");
  registerAction(sf::Keyboard::W, "Up");
  registerAction(sf::Keyboard::S, "Down");
  registerAction(sf::Keyboard::Enter, "Level Selected");

  m_title = "MEGA MARIO";

  m_menuStrings = std::vector<std::string>(3);
  m_menuStrings[0] = "LEVEL 1";
  m_menuStrings[1] = "LEVEL 2";
  m_menuStrings[2] = "LEVEL 3";

  m_levelPaths = std::vector<std::string>(3);
  m_levelPaths[0] = "../bin/level1.txt";
  m_levelPaths[1] = "../bin/level2.txt";
  m_levelPaths[2] = "../bin/level3.txt";
}

void Scene_Menu::onEnd() { m_game->window().close(); }

void Scene_Menu::update() {
  sRender();
  m_currentFrame++;
}

void Scene_Menu::sRender() {
  m_game->window().clear(sf::Color(0, 87, 217));

  m_menuText.setFont(m_game->assets().getFont("MarioFont"));

  m_menuText.setString(m_title);
  m_menuText.setCharacterSize(60);
  m_menuText.setPosition(50, 50);
  m_menuText.setFillColor(sf::Color(255, 255, 255));
  m_game->window().draw(m_menuText);

  for (size_t i = 0; i < m_menuStrings.size(); i++) {
    m_menuText.setString(m_menuStrings[i]);
    m_menuText.setCharacterSize(30);
    m_menuText.setPosition(50, 100 + 100 * (i + 1));
    if (i != m_selectedMenuIndex)
      m_menuText.setFillColor(sf::Color(0, 0, 0));
    else
      m_menuText.setFillColor(sf::Color(255, 255, 255));

    m_game->window().draw(m_menuText);
  }

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
      std::shared_ptr<Scene_Play> scene = std::make_shared<Scene_Play>(
          m_game, m_levelPaths[m_selectedMenuIndex]);
      m_game->changeScene("play", scene, false);
    }
  }
}
