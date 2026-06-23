#include "Physics.h"

Vec2 Physics::GetOverlap(std::shared_ptr<Entity> a, std::shared_ptr<Entity> b) {
    auto& ca = a->getComponent<CBoundingBox>();
    auto& cb = b->getComponent<CBoundingBox>();
    Vec2 posA = a->getComponent<CTransform>().pos;
    Vec2 posB = b->getComponent<CTransform>().pos;

    Vec2 overlap(0, 0);

    overlap.x = ca.halfSize.x + cb.halfSize.x - abs(posA.x - posB.x);
    overlap.y = ca.halfSize.y + cb.halfSize.y - abs(posA.y - posB.y);

    return overlap;
}

Vec2 Physics::GetPreviousOverlap(std::shared_ptr<Entity> a, std::shared_ptr<Entity> b) {
    auto& ca = a->getComponent<CBoundingBox>();
    auto& cb = b->getComponent<CBoundingBox>();
    Vec2 posA = a->getComponent<CTransform>().prevPos;
    Vec2 posB = b->getComponent<CTransform>().prevPos;

    Vec2 overlap(0, 0);

    overlap.x = ca.halfSize.x + cb.halfSize.x - abs(posA.x - posB.x);
    overlap.y = ca.halfSize.y + cb.halfSize.y - abs(posA.y - posB.y);

    return overlap;
}
