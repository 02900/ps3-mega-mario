#include "EntityManager.h"

EntityManager::EntityManager() {}

void EntityManager::removeDeadEntities(EntityVec& entities) {
    EntityVec activeEntities;
    for (size_t i = 0; i < entities.size(); i++) {
        if (entities[i] && entities[i]->isActive())
            activeEntities.push_back(entities[i]);
    }
    entities = activeEntities;
}

void EntityManager::update() {
    for (auto& e : m_entities2add) {
        m_entities.push_back(e);
        m_entityMap[e->tag()].push_back(e);
    }
    m_entities2add.clear();
    removeDeadEntities(m_entities);
    for (auto& [tag, e] : m_entityMap) {
        removeDeadEntities(e);
    }
}

std::shared_ptr<Entity> EntityManager::addEntity(std::string tag) {
    auto e = std::shared_ptr<Entity>(new Entity(m_totalEntities++, tag));
    m_entities2add.push_back(e);
    return e;
}

const EntityVec& EntityManager::getEntities() { return m_entities; }

const EntityVec& EntityManager::getEntities(const std::string& tag) {
    return m_entityMap[tag];
}

const EntityMap& EntityManager::getEntityMap() { return m_entityMap; }
