#pragma once

#include "Scene.h"

class Scene_Play : public Scene {
  struct PlayerConfig {
    float X, Y, CX, CY, SPEED, MAXSPEED, JUMP, GRAVITY;
    std::string WEAPON;
  };

protected:
  std::shared_ptr<Entity> m_player;
  std::string m_levelPath;
  PlayerConfig m_playerConfing;
  bool m_drawTextures = true;
  bool m_drawCollision = false;
  bool m_drawGrid = false;

  bool m_playerIsInGround = false;

  const Vec2 m_gridSize = {64, 64};
  sf::Text m_gridText;
  void init(const std::string &levelPath);

  void onEnd();

public:
  Scene_Play(GameEngine *gameEngine, const std::string levelPath);
  void update();
  void sDoAction(const Action &action);
  void sRender();
  void sAnimation();
  void sMovement();
  void sCollision();
  void sLifeSpan();

  void mapGridToPixels(int &x, int &y);

  void addPlayer();
  void addTile(int x, int y, std::string name);
  void addDecoration(int x, int y, std::string name);

  float getPlayerDirection();
  void flipPlayer();
  void playerMovement();
  void Shoot();

  void spawnExplosion(Vec2 pos);
  void spawnCoin(Vec2 pos);

  void resetLevel();
};
