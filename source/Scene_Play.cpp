#include "Scene_Play.h"

#include <fstream>

#include "Components.h"
#include "GameEngine.h"
#include "Physics.h"
#include "Scene_Menu.h"

Scene_Play::Scene_Play(GameEngine *gameEngine, std::string levelPath) {
  m_game = gameEngine;
  init(levelPath);
}

void Scene_Play::init(const std::string &levelPath) {
  registerAction(sf::Keyboard::Escape, "Quit");
  registerAction(sf::Keyboard::D, "Right");
  registerAction(sf::Keyboard::A, "Left");
  registerAction(sf::Keyboard::W, "Jump");
  registerAction(sf::Keyboard::Space, "Shoot");
  registerAction(sf::Keyboard::F, "Draw Grid");
  registerAction(sf::Keyboard::J, "Draw Collision");
  registerAction(sf::Keyboard::P, "Pause");

  m_gridText.setFont(m_game->assets().getFont("TechFont"));
  m_gridText.setCharacterSize(12);

  m_currentFrame = 0;

  m_levelPath = levelPath;

  std::ifstream fin(levelPath);
  std::string type;
  while (fin >> type) {
    if (type == "Dec") {
      std::string name;
      int x, y;
      fin >> name >> x >> y;
      addDecoration(x, y, name);
    } else if (type == "Tile") {
      std::string name;
      int x, y;
      fin >> name >> x >> y;
      addTile(x, y, name);
    } else if (type == "Player") {
      fin >> m_playerConfing.X >> m_playerConfing.Y >> m_playerConfing.CX >>
          m_playerConfing.CY >> m_playerConfing.SPEED >>
          m_playerConfing.MAXSPEED >> m_playerConfing.JUMP >>
          m_playerConfing.GRAVITY >> m_playerConfing.WEAPON;
      addPlayer();
    }
  }
}

void Scene_Play::addDecoration(int x, int y, std::string name) {
  auto dec = m_entityManager.addEntity("decoration");
  auto &anim =
      dec->addComponent<CAnimation>(m_game->assets().getAnimation(name));

  auto &sprite = anim.animation.getSprite();

  mapGridToPixels(x, y);
  x += sprite.getOrigin().x * sprite.getScale().x;
  y -= sprite.getOrigin().y * sprite.getScale().y;

  dec->addComponent<CTransform>(Vec2(x, y));
}

void Scene_Play::addTile(int x, int y, std::string name) {
  auto tile = m_entityManager.addEntity("tile");
  auto &anim =
      tile->addComponent<CAnimation>(m_game->assets().getAnimation(name));

  auto &sprite = anim.animation.getSprite();

  mapGridToPixels(x, y);
  x += sprite.getOrigin().x * sprite.getScale().x;
  y -= sprite.getOrigin().y * sprite.getScale().y;

  tile->addComponent<CTransform>(Vec2(x, y));

  float cx = sprite.getOrigin().x * sprite.getScale().x * 2.0;
  float cy = sprite.getOrigin().y * sprite.getScale().y * 2.0;

  tile->addComponent<CBoundingBox>(Vec2(cx, cy));
}

void Scene_Play::addPlayer() {
  m_player = m_entityManager.addEntity("player");
  m_player->addComponent<CInput>();
  auto &anim = m_player->addComponent<CAnimation>(
      m_game->assets().getAnimation("Stand"));

  int x = m_playerConfing.X, y = m_playerConfing.Y;
  mapGridToPixels(x, y);
  x += anim.animation.getSprite().getOrigin().x *
       anim.animation.getSprite().getScale().x;
  y -= anim.animation.getSprite().getOrigin().y *
       anim.animation.getSprite().getScale().y;
  m_player->addComponent<CTransform>(Vec2(x, y));
  m_player->addComponent<CBoundingBox>(
      Vec2(m_playerConfing.CX, m_playerConfing.CY));
  m_player->addComponent<CGravity>(m_playerConfing.GRAVITY);
}

void Scene_Play::update() {
  m_entityManager.update();
  if (!m_paused) {
    sAnimation();
    sMovement();
    sCollision();
    sLifeSpan();
  }
  sRender();
  m_currentFrame++;
}

void Scene_Play::sDoAction(const Action &action) {
  if (action.name() == "Quit") {
    onEnd();
    return;
  }

  if (action.type() == "START") {
    if (action.name() == "Right")
      m_player->getComponent<CInput>().right = true;

    else if (action.name() == "Left")
      m_player->getComponent<CInput>().left = true;

    else if (action.name() == "Jump") {
      if (m_playerIsInGround) {
        m_player->getComponent<CInput>().up = true;
      }
    } else if (action.name() == "Draw Grid")
      m_drawGrid = !m_drawGrid;

    else if (action.name() == "Draw Collision")
      m_drawCollision = !m_drawCollision;

    else if (action.name() == "Pause")
      m_paused = !m_paused;
  }

  else {
    if (action.name() == "Right")
      m_player->getComponent<CInput>().right = false;

    else if (action.name() == "Left")
      m_player->getComponent<CInput>().left = false;

    else if (action.name() == "Jump")
      m_player->getComponent<CInput>().up = false;

    else if (action.name() == "Shoot")
      Shoot();
  }
}

void Scene_Play::Shoot() {
  auto bullet = m_entityManager.addEntity("bullet");
  auto playerTransform = m_player->getComponent<CTransform>();
  bullet->addComponent<CTransform>(
      playerTransform.pos,
      Vec2(m_playerConfing.SPEED * getPlayerDirection() * 2, 0));
  bullet->addComponent<CAnimation>(m_game->assets().getAnimation("Bullet"));

  auto sprite = bullet->getComponent<CAnimation>().animation.getSprite();

  float cx = sprite.getOrigin().x * sprite.getScale().x * 2.0;
  float cy = sprite.getOrigin().y * sprite.getScale().y * 2.0;

  bullet->addComponent<CBoundingBox>(Vec2(cx, cy));

  bullet->addComponent<CLifeSpan>(60);
}

float Scene_Play::getPlayerDirection() {
  float flip =
      m_player->getComponent<CAnimation>().animation.getSprite().getScale().x;
  flip /= abs(flip);
  return flip;
}

void Scene_Play::flipPlayer() {
  auto &anim = m_player->getComponent<CAnimation>();
  auto spriteScale = anim.animation.getSprite().getScale();
  spriteScale.x *= -1;
  anim.animation.getSprite().setScale(spriteScale);
}

void Scene_Play::sAnimation() {
  auto &input = m_player->getComponent<CInput>();
  auto &anim = m_player->getComponent<CAnimation>();
  float flip = getPlayerDirection();

  if (!m_playerIsInGround)
    anim.animation = m_game->assets().getAnimation("Jump");

  else if (!input.right && !input.left)
    anim.animation = m_game->assets().getAnimation("Stand");

  else if (anim.animation.getName() != "Run")
    anim.animation = m_game->assets().getAnimation("Run");

  auto spriteScale = anim.animation.getSprite().getScale();
  anim.animation.getSprite().setScale(abs(spriteScale.x) * flip, spriteScale.y);
  if (input.right) {
    if (flip < 0)
      flipPlayer();
  } else if (input.left) {
    if (flip > 0)
      flipPlayer();
  }

  for (auto &e : m_entityManager.getEntities()) {
    if (e->hasComponent<CAnimation>()) {
      auto &anim = e->getComponent<CAnimation>();
      anim.animation.update();
      if (!anim.loop && anim.animation.hasEnded())
        e->destroy();
    }
  }
}

void Scene_Play::playerMovement() {
  auto &input = m_player->getComponent<CInput>();
  auto &gravity = m_player->getComponent<CGravity>();
  auto &playerTransform = m_player->getComponent<CTransform>();

  Vec2 in(0, 0), grav(0, 0);
  if (input.right)
    in.x++;
  else if (input.left)
    in.x--;

  if (input.up)
    in.y++;
  else if (playerTransform.prevPos.y >= playerTransform.pos.y)
    gravity.totalTime = 0.1;

  if (!m_playerIsInGround) {
    grav.y = gravity.gravity * gravity.totalTime;
    gravity.totalTime += 0.1;
  } else
    gravity.totalTime = 0;

  in.x *= m_playerConfing.SPEED;
  in.y *= -m_playerConfing.JUMP;

  playerTransform.velocity = in + grav;

  if (playerTransform.velocity.y >= 0 && input.up)
    input.up = false, gravity.totalTime = 0.1;

  if (playerTransform.velocity.y > 0)
    playerTransform.velocity.y =
        std::min(playerTransform.velocity.y, m_playerConfing.MAXSPEED);
  else
    playerTransform.velocity.y =
        std::max(playerTransform.velocity.y, -m_playerConfing.MAXSPEED);
}

void Scene_Play::sMovement() {
  playerMovement();

  for (auto &e : m_entityManager.getEntities()) {
    if (e->hasComponent<CTransform>()) {
      auto &transform = e->getComponent<CTransform>();
      transform.prevPos = transform.pos;
      transform.pos += transform.velocity;
      transform.pos.x = std::max((float)0, transform.pos.x);
    }
  }
}

void Scene_Play::sCollision() {
  m_playerIsInGround = false;

  for (auto &e : m_entityManager.getEntities("tile")) {
    Vec2 overlap = Physics::GetOverlap(m_player, e);
    Vec2 prevOverlap = Physics::GetPreviousOverlap(m_player, e);
    Vec2 &playerPos = m_player->getComponent<CTransform>().pos;
    Vec2 &tilePos = e->getComponent<CTransform>().pos;

    if (prevOverlap.x >= 0 && overlap.y >= -1) {
      if (playerPos.y < tilePos.y)
        m_playerIsInGround = true;
    }

    if (overlap.x < 0 || overlap.y < 0)
      continue;

    std::string tileName = e->getComponent<CAnimation>().animation.getName();
    if (tileName == "Flag" || tileName == "FlagPole")
      resetLevel();

    if (prevOverlap.x >= 0) {
      if (playerPos.y > tilePos.y) {
        playerPos.y += overlap.y + 1;
        m_player->getComponent<CInput>().up = false;

        if (tileName == "Brick") {
          e->destroy();
          spawnExplosion(tilePos);
        } else if (tileName == "QShade") {
          e->getComponent<CAnimation>().animation =
              m_game->assets().getAnimation("Question2");
          spawnCoin(tilePos);
        }
      } else
        playerPos.y -= overlap.y + 1;
    } else {
      if (playerPos.x > tilePos.x)
        playerPos.x += overlap.x + 1;
      else
        playerPos.x -= overlap.x + 1;
    }
  }
  for (auto &bullet : m_entityManager.getEntities("bullet")) {
    for (auto &e : m_entityManager.getEntities("tile")) {
      Vec2 overlap = Physics::GetOverlap(bullet, e);
      Vec2 prevOverlap = Physics::GetPreviousOverlap(bullet, e);
      Vec2 &bulletPos = bullet->getComponent<CTransform>().pos;
      Vec2 &tilePos = e->getComponent<CTransform>().pos;

      if (overlap.x < 0 || overlap.y < 0)
        continue;

      bullet->destroy();
      if (e->getComponent<CAnimation>().animation.getName() == "Brick") {
        e->destroy();
        spawnExplosion(tilePos);
      }
    }
  }
  if (m_player->getComponent<CTransform>().pos.y > height())
    resetLevel();
}

void Scene_Play::sLifeSpan() {
  for (auto &e : m_entityManager.getEntities()) {
    if (e->hasComponent<CLifeSpan>()) {
      e->getComponent<CLifeSpan>().remaining--;
      if (e->getComponent<CLifeSpan>().remaining == 0)
        e->destroy();
    }
  }
}

void Scene_Play::sRender() {
  m_game->window().clear(sf::Color(0, 87, 217));

  auto &pos = m_player->getComponent<CTransform>().pos;
  float windowCenterX = std::max(width() / 2.0f, pos.x);
  sf::View view = m_game->window().getView();
  view.setCenter(windowCenterX, height() - view.getCenter().y);
  m_game->window().setView(view);

  for (auto e : m_entityManager.getEntities()) {
    if (e->hasComponent<CTransform>() && e->hasComponent<CAnimation>()) {
      auto &animation = e->getComponent<CAnimation>().animation;
      auto &transform = e->getComponent<CTransform>();
      animation.getSprite().setPosition(transform.pos.x, transform.pos.y);
      m_game->window().draw(
          e->getComponent<CAnimation>().animation.getSprite());
    }
  }

  if (m_drawGrid) {
    float leftX = m_game->window().getView().getCenter().x - width() / 2.0;
    float rightX = leftX + width() + m_gridSize.x;
    float nextGridX = leftX - ((int)leftX % (int)m_gridSize.x);
    for (float x = nextGridX; x < rightX; x += m_gridSize.x)
      drawLine(Vec2(x, 0), Vec2(x, height()));
    for (float y = 0; y < height(); y += m_gridSize.y) {
      drawLine(Vec2(leftX, height() - y), Vec2(rightX, height() - y));
      for (float x = nextGridX; x < rightX; x += m_gridSize.x) {
        std::string xCell = std::to_string((int)x / (int)m_gridSize.x);
        std::string yCell = std::to_string((int)y / (int)m_gridSize.y);
        m_gridText.setString("(" + xCell + "," + yCell + ")");
        m_gridText.setPosition(x + 3, height() - y - m_gridSize.y + 6);
        m_game->window().draw(m_gridText);
      }
    }
  }

  if (m_drawCollision) {
    for (auto &e : m_entityManager.getEntities()) {
      if (!e->hasComponent<CBoundingBox>())
        continue;
      auto &box = e->getComponent<CBoundingBox>();
      auto &transform = e->getComponent<CTransform>();
      sf::RectangleShape rect;
      rect.setSize(sf::Vector2f(box.size.x, box.size.y));
      rect.setOrigin(box.halfSize.x, box.halfSize.y);
      rect.setPosition(transform.pos.x, transform.pos.y);
      rect.setFillColor(sf::Color(0, 0, 0, 0));
      rect.setOutlineColor(sf::Color(255, 255, 255, 255));
      rect.setOutlineThickness(1);
      m_game->window().draw(rect);
    }
  }

  m_game->window().display();
}

void Scene_Play::mapGridToPixels(int &x, int &y) {
  x *= m_gridSize.x;
  y = m_game->window().getSize().y - m_gridSize.y * y;
}

void Scene_Play::spawnExplosion(Vec2 pos) {
  auto e = m_entityManager.addEntity("explosion");
  e->addComponent<CTransform>(pos);
  e->addComponent<CAnimation>(m_game->assets().getAnimation("Explosion"),
                              false);
}

void Scene_Play::spawnCoin(Vec2 pos) {
  auto e = m_entityManager.addEntity("coin");
  e->addComponent<CTransform>(Vec2(pos.x, pos.y - 64));
  e->addComponent<CAnimation>(m_game->assets().getAnimation("Coin"), true);
  e->addComponent<CLifeSpan>(30);
}

void Scene_Play::resetLevel() {
  for (auto &e : m_entityManager.getEntities()) {
    e->destroy();
  }
  init(m_levelPath);
}

void Scene_Play::onEnd() {
  sf::View view = m_game->window().getView();
  view.setCenter(width() / 2, height() - view.getCenter().y);
  m_game->window().setView(view);
  std::shared_ptr<Scene_Menu> scene = std::make_shared<Scene_Menu>(m_game);
  m_game->changeScene("menu", scene, false);
}
