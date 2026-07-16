#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_TRANSFORMSERVICE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_TRANSFORMSERVICE_HPP

#include <glm/glm.hpp>

#include <entt/entt.hpp>

namespace engine {
    class Scene;

    /**
     * @brief Narrow service for reading/writing transform components on scene
     *        entities. Extracted from EngineState to keep the composition root
     *        lean; the god-object previously exposed 12 trivial get/set accessors.
     */
    class TransformService {
       public:
        explicit TransformService(Scene& scene) : scene_(scene) {}

        glm::vec3 getTranslation(entt::entity e) const;
        void      setTranslation(entt::entity e, const glm::vec3& v);
        glm::vec3 getRotation(entt::entity e) const;
        void      setRotation(entt::entity e, const glm::vec3& v);
        glm::vec3 getScale(entt::entity e) const;
        void      setScale(entt::entity e, const glm::vec3& v);

       private:
        Scene& scene_;
    };
}  // namespace engine

#endif
