#include "Engine/Scene/TransformService.hpp"

#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {
    glm::vec3 TransformService::getTranslation(entt::entity e) const {
        auto& reg = scene_.getRegistry();
        return reg.all_of<TransformComponent>(e) ? reg.get<TransformComponent>(e).translation : glm::vec3{};
    }
    void TransformService::setTranslation(entt::entity e, const glm::vec3& v) {
        scene_.getRegistry().get<TransformComponent>(e).translation = v;
    }
    glm::vec3 TransformService::getRotation(entt::entity e) const {
        auto& reg = scene_.getRegistry();
        return reg.all_of<TransformComponent>(e) ? reg.get<TransformComponent>(e).rotation : glm::vec3{};
    }
    void TransformService::setRotation(entt::entity e, const glm::vec3& v) {
        scene_.getRegistry().get<TransformComponent>(e).rotation = v;
    }
    glm::vec3 TransformService::getScale(entt::entity e) const {
        auto& reg = scene_.getRegistry();
        return reg.all_of<TransformComponent>(e) ? reg.get<TransformComponent>(e).scale : glm::vec3{1};
    }
    void TransformService::setScale(entt::entity e, const glm::vec3& v) {
        scene_.getRegistry().get<TransformComponent>(e).scale = v;
    }
}  // namespace engine
