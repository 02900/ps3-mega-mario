#include "Entity.h"

Entity::Entity(const size_t id, std::string tag) : m_id(id), m_tag(tag) {}

void Entity::destroy() { m_active = false; }

bool Entity::isActive() { return m_active; }

const size_t& Entity::id() { return m_id; }

const std::string& Entity::tag() { return m_tag; }
