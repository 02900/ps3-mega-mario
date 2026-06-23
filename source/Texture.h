#pragma once

#include <SFML/Graphics.hpp>
#include "Vec2.h"

struct Texture {
    sf::Texture image;
    Vec2 size = {1, 1};
};