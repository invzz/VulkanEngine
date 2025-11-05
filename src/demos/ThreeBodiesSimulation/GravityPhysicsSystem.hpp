#pragma once

#include <glm/vec2.hpp>
#include <vector>

#include "2dEngine/GameObject.hpp"

namespace engine {

    class GravityPhysicsSystem
    {
      public:
        explicit GravityPhysicsSystem(float strength);

        void      update(std::vector<GameObject>& objs, float dt, unsigned int substeps = 1) const;
        glm::vec2 computeForce(GameObject& fromObj, GameObject& toObj) const;

      private:
        void stepSimulation(std::vector<GameObject>& physicsObjs, float dt) const;
        void applyBoundaryDamping(GameObject& obj, float dt) const;

        float strengthGravity;
    };

} // namespace engine
