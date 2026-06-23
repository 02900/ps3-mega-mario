#include "GameEngine.h"
#include "Scene_Menu.h"
#include "Scene_Play.h"
#include <fstream>

GameEngine::GameEngine(const std::string path) { init(path); }

void GameEngine::init(const std::string &path) {
  std::ifstream fin(path);
  std::string type;
  while (fin >> type) {
    if (type == "Texture") {
      std::string name, path;
      Vec2 size;
      fin >> name >> path >> size.x >> size.y;
      m_assets.addTexture(name, path, size);
    } else if (type == "Animation") {
      std::string animName, texName;
      size_t frameCount, speed;
      fin >> animName >> texName >> frameCount >> speed;
      m_assets.addAnimation(animName, texName, frameCount, speed);
    } else {
      std::string name, path;
      fin >> name >> path;
      m_assets.addFont(name, path);
    }
  }
  m_window.create(sf::VideoMode(1920, 1080), "mega mario",
                  sf::Style::Fullscreen);

  m_window.setFramerateLimit(60);

  m_sceneMap["play"] = std::make_shared<Scene_Play>(this, "../bin/level1.txt");
  m_sceneMap["menu"] = std::make_shared<Scene_Menu>(this);
  changeScene("menu", m_sceneMap["menu"]);
}

void GameEngine::run() {
  while (m_window.isOpen()) {
    update();
  }
}

void GameEngine::sUserInput() {
  sf::Event event;
  while (m_window.pollEvent(event)) {
    if (event.type == sf::Event::Closed)
      m_window.close();
    if (currentScene()->getActionMap().find(event.key.code) ==
        currentScene()->getActionMap().end())
      continue;

    const std::string actionType =
        (event.type == sf::Event::KeyPressed) ? "START" : "END";
    currentScene()->doAction(
        Action(currentScene()->getActionMap().at(event.key.code), actionType));
  }
}

void GameEngine::update() {
  sUserInput();
  currentScene()->update();
}

void GameEngine::changeScene(const std::string sceneName,
                             std::shared_ptr<Scene> scene,
                             bool endCurrentScene) {
  if (endCurrentScene)
    m_sceneMap.erase(m_currentScene);
  m_currentScene = sceneName;
  m_sceneMap[m_currentScene] = scene;
}

void GameEngine::quit() {
  m_window.close();
  m_running = false;
}

sf::RenderWindow &GameEngine::window() { return m_window; }

const Assets &GameEngine::assets() { return m_assets; }

const std::shared_ptr<Scene> GameEngine::currentScene() {
  return m_sceneMap[m_currentScene];
}

bool GameEngine::isRunning() const { return m_running; }
