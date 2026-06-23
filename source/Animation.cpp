#include "Animation.h"

Animation::Animation() {}

Animation::Animation(const std::string name, const Texture &t) {
  m_name = name;
  m_size = Vec2(t.image.getSize().x, t.image.getSize().y);
  m_sprite = sf::Sprite(t.image);
  m_sprite.setScale(t.size.x / m_size.x, t.size.y / m_size.y);
  m_sprite.setOrigin(m_size.x / 2, m_size.y / 2);
}

Animation::Animation(const std::string name, const Texture &t,
                     size_t frameCount, size_t speed) {
  m_name = name;
  m_size = Vec2(t.image.getSize().x, t.image.getSize().y);
  m_sprite = sf::Sprite(t.image);
  m_sprite.setScale(t.size.x / m_size.x, t.size.y / m_size.y);
  m_frameCount = frameCount;
  m_speed = speed;

  int width = (m_size.x + m_frameCount - 1) / m_frameCount;
  m_sprite.setTextureRect(sf::IntRect(0, 0, width, m_size.y));
  m_sprite.setOrigin(width / 2, m_size.y / 2);
}

void Animation::update() {
  int curr = (m_currentFrame / m_speed) % m_frameCount;

  int width = (m_size.x + m_frameCount - 1) / m_frameCount;

  int sz = width;

  if (width * curr + width > m_size.x)
    sz = m_size.x - width * curr;

  m_sprite.setTextureRect(sf::IntRect(width * curr, 0, sz, m_size.y));
  m_sprite.setOrigin(sz / 2, m_size.y / 2);

  m_currentFrame++;
}

bool Animation::hasEnded() const {
  return (m_currentFrame / m_speed) % m_frameCount == m_frameCount - 1;
}

const std::string &Animation::getName() const { return m_name; }

const Vec2 &Animation::getSize() const { return m_size; }

sf::Sprite &Animation::getSprite() { return m_sprite; }
