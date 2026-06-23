#pragma once

#include "Animation.h"
#include "Vec2.h"

class Component {
   public:
    bool has = false;
};

class CTransform : public Component {
   public:
    Vec2 pos = {0, 0};
    Vec2 prevPos = {0, 0};
    Vec2 scale = {1, 1};
    Vec2 velocity = {0, 0};
    float angle = 0;
    CTransform() {}
    CTransform(const Vec2 _pos) : pos(_pos), prevPos(_pos) {}
    CTransform(const Vec2 _pos, const Vec2 _velocity)
        : pos(_pos), velocity(_velocity) {}
};

class CLifeSpan : public Component {
   public:
    int total = 0;
    int remaining = 0;
    CLifeSpan() {}
    CLifeSpan(const int _total) : total(_total), remaining(_total) {}
};

class CInput : public Component {
   public:
    bool up = false;
    bool down = false;
    bool right = false;
    bool left = false;
    bool shot = false;
    CInput() {}
};

class CBoundingBox : public Component {
   public:
    Vec2 size, halfSize;
    CBoundingBox() {}
    CBoundingBox(const Vec2 _size) : size(_size), halfSize(_size / 2.0) {}
};

class CGravity : public Component {
   public:
    float gravity = 0;
    float totalTime = 0;
    CGravity() {}
    CGravity(float _gravity) : gravity(_gravity) {}
};

class CAnimation : public Component {
   public:
    Animation animation;
    bool loop = true;
    CAnimation() {}
    CAnimation(Animation _animation, bool _loop = true)
        : animation(_animation), loop(_loop) {}
};
