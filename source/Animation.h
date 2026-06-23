#pragma once

#include <SFML/Graphics.hpp>

#include "Texture.h"
#include "Vec2.h"

class Animation {
    sf::Sprite m_sprite;
    size_t m_frameCount = 1;
    size_t m_currentFrame = 0;
    size_t m_speed = 1;
    Vec2 m_size = {1, 1};
    std::string m_name = "none";

   public:
    Animation();
    Animation(const std::string name, const Texture& t);
    Animation(const std::string name, const Texture& t, size_t frameCount,
              size_t speed);

    void update();
    bool hasEnded() const;
    const std::string& getName() const;
    const Vec2& getSize() const;
    sf::Sprite& getSprite();
};
