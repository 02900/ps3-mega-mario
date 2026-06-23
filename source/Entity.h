#pragma once

#include <string>
#include <tuple>

#include "Components.h"

typedef std::tuple<CTransform, CLifeSpan, CInput, CBoundingBox, CGravity,
                   CAnimation>
    ComponentTuple;

class Entity {
    friend class EntityManager;

    bool m_active = true;
    size_t m_id = 0;
    std::string m_tag = "default";
    ComponentTuple m_components;

    Entity(const size_t id, std::string tag);

   public:
    void destroy();
    bool isActive();
    const size_t& id();
    const std::string& tag();

    template <typename T>
    T& getComponent() {
        return std::get<T>(m_components);
    }

    template <typename T>
    const T& getComponent() const {
        return std::get<T>(m_components);
    }

    template <typename T>
    bool hasComponent() const {
        return getComponent<T>().has;
    }

    template <typename T, typename... TArgs>
    T& addComponent(TArgs&&... m_args) {
        auto& component = getComponent<T>();
        component = T(std::forward<TArgs>(m_args)...);
        component.has = true;
        return component;
    }

    template <typename T>
    void removeComponent() {
        getComponent<T>() = T();
    }
};
