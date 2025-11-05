#include "GravityPhysicsSystem.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>

#include "SimulationConfig.hpp"

namespace engine {

    GravityPhysicsSystem::GravityPhysicsSystem(float strength) : strengthGravity{strength} {}

    void GravityPhysicsSystem::update(std::vector<GameObject>& objs, float dt, unsigned int substeps) const
    {
        const float stepDelta = dt / static_cast<float>(substeps);
        for (unsigned int i = 0; i < substeps; ++i)
        {
            stepSimulation(objs, stepDelta);
        }
    }

    glm::vec2 GravityPhysicsSystem::computeForce(GameObject& fromObj, GameObject& toObj) const
    {
        auto  offset          = fromObj.transform2d.translation - toObj.transform2d.translation;
        float distanceSquared = glm::dot(offset, offset);
        if (glm::abs(distanceSquared) < 1e-10f)
        {
            return {0.0f, 0.0f};
        }

        const float force = strengthGravity * toObj.rigidBody2d.mass * fromObj.rigidBody2d.mass / distanceSquared;
        return force * offset / glm::sqrt(distanceSquared);
    }

    void GravityPhysicsSystem::stepSimulation(std::vector<GameObject>& physicsObjs, float dt) const
    {
        for (auto iterA = physicsObjs.begin(); iterA != physicsObjs.end(); ++iterA)
        {
            auto& objA = *iterA;
            for (auto iterB = iterA; iterB != physicsObjs.end(); ++iterB)
            {
                if (iterA == iterB) continue;
                auto& objB = *iterB;

                const auto force = computeForce(objA, objB);
                objA.rigidBody2d.velocity += dt * -force / objA.rigidBody2d.mass;
                objB.rigidBody2d.velocity += dt * force / objB.rigidBody2d.mass;
            }
        }

        for (auto& obj : physicsObjs)
        {
            obj.transform2d.translation += dt * obj.rigidBody2d.velocity;
            applyBoundaryDamping(obj, dt);
        }
    }

    void GravityPhysicsSystem::applyBoundaryDamping(GameObject& obj, float dt) const
    {
        auto& pos = obj.transform2d.translation;
        auto& vel = obj.rigidBody2d.velocity;

        for (int axis = 0; axis < 2; ++axis)
        {
            float&      coord    = axis == 0 ? pos.x : pos.y;
            float&      velocity = axis == 0 ? vel.x : vel.y;
            const float absCoord = glm::abs(coord);
            const float side     = coord >= 0.f ? 1.f : -1.f;

            if (absCoord > BorderSettings::repulsionStart)
            {
                const float repelT =
                        glm::clamp((absCoord - BorderSettings::repulsionStart) / (BorderSettings::clamp - BorderSettings::repulsionStart), 0.f, 1.f);
                const float repelAccel = BorderSettings::repulsionStrength * (0.25f + 0.75f * repelT);
                velocity -= side * repelAccel * dt;
            }

            if (absCoord > BorderSettings::dampingStart)
            {
                const float t =
                        glm::clamp((absCoord - BorderSettings::dampingStart) / (BorderSettings::clamp - BorderSettings::dampingStart), 0.f, 1.f);
                const float dampingFactor = glm::max(0.f, 1.f - BorderSettings::dampingStrength * t * dt);
                velocity *= dampingFactor;
            }

            if (absCoord > BorderSettings::clamp)
            {
                coord = side * BorderSettings::clamp;
                if (velocity * side > 0.f)
                {
                    velocity *= BorderSettings::bounceFactor;
                }
            }
        }
    }

} // namespace engine
