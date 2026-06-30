#include "Engine/Systems/JoltPhysicsSystem.hpp"

#include <entt/entt.hpp>

#include "Jolt/Physics/Collision/Shape/MeshShape.h"

#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"

namespace engine {

    // ============================================================================
    // Helpers
    // ============================================================================

    JPH::Vec3 JoltPhysicsSystem::toJolt(const glm::vec3& v) {
        return {v.x, v.y, v.z};
    }

    JPH::Quat JoltPhysicsSystem::toJolt(const glm::quat& q) {
        return {q.x, q.y, q.z, q.w};
    }

    glm::vec3 JoltPhysicsSystem::toGlm(const JPH::Vec3& v) {
        return {v.GetX(), v.GetY(), v.GetZ()};
    }

    glm::quat JoltPhysicsSystem::toGlm(const JPH::Quat& q) {
        return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
    }

    // ============================================================================
    // Constructor / Destructor
    // ============================================================================

    JoltPhysicsSystem::JoltPhysicsSystem()
        : broadPhaseLayerInterface_(nullptr),
          objectLayerPairFilter_(nullptr),
          objectVsBroadPhaseLayerFilter_(nullptr) {
        // Jolt requires global allocator registration before any JPH allocation.
        JPH::RegisterDefaultAllocator();

        factory_                  = std::make_unique<JPH::Factory>();
        tempAllocator_            = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
        jobSystem_                = std::make_unique<JPH::JobSystemThreadPool>(1024, 64);
        physicsSystem_            = std::make_unique<JPH::PhysicsSystem>();
        broadPhaseLayerInterface_ = std::make_unique<JPH::BroadPhaseLayerInterfaceTable>(cNumObjectLayers, cNumBroadPhaseLayers);
        objectLayerPairFilter_    = std::make_unique<JPH::ObjectLayerPairFilterTable>(cNumObjectLayers);

        JPH::Factory::sInstance = factory_.get();
        JPH::RegisterTypes();
        typesRegistered_ = true;

        broadPhaseLayerInterface_->MapObjectToBroadPhaseLayer(cNonMovingObjectLayer, JPH::BroadPhaseLayer(0));
        broadPhaseLayerInterface_->MapObjectToBroadPhaseLayer(cMovingObjectLayer, JPH::BroadPhaseLayer(1));

        objectLayerPairFilter_->EnableCollision(cNonMovingObjectLayer, cNonMovingObjectLayer);
        objectLayerPairFilter_->EnableCollision(cNonMovingObjectLayer, cMovingObjectLayer);
        objectLayerPairFilter_->EnableCollision(cMovingObjectLayer, cNonMovingObjectLayer);
        objectLayerPairFilter_->EnableCollision(cMovingObjectLayer, cMovingObjectLayer);

        objectVsBroadPhaseLayerFilter_ = std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(
            *broadPhaseLayerInterface_, cNumBroadPhaseLayers, *objectLayerPairFilter_, cNumObjectLayers);

        physicsSystem_->Init(1024, 0, 1024, 1024,
            *broadPhaseLayerInterface_,
            *objectVsBroadPhaseLayerFilter_,
            *objectLayerPairFilter_);

        JPH::PhysicsSettings physicsSettings       = physicsSystem_->GetPhysicsSettings();
        physicsSettings.mMinVelocityForRestitution = 0.0f;
        physicsSystem_->SetPhysicsSettings(physicsSettings);

        // Y-down convention: positive Y is downward.
        physicsSystem_->SetGravity(JPH::Vec3(0.0f, 9.81f, 0.0f));

        createGroundBody();
    }

    JoltPhysicsSystem::~JoltPhysicsSystem() {
        clear();

        if (typesRegistered_) {
            JPH::UnregisterTypes();
            typesRegistered_ = false;
        }

        JPH::Factory::sInstance = nullptr;
        physicsSystem_.reset();
        jobSystem_.reset();
        tempAllocator_.reset();
        factory_.reset();
    }

    void JoltPhysicsSystem::createGroundBody() {
        if (!physicsSystem_) {
            return;
        }

        if (!groundEnabled_) {
            return;
        }

        if (!groundBodyID_.IsInvalid()) {
            return;
        }

        auto& bodyInterface = physicsSystem_->GetBodyInterface();

        // Large static box aligned with the grid plane at Y = 0.
        // In Y-down convention, floor thickness extends toward +Y (downward).
        auto                      groundShape = std::make_unique<JPH::BoxShape>(JPH::Vec3(1000.0f, 10.0f, 1000.0f));
        JPH::BodyCreationSettings groundSettings(
            groundShape.release(),
            JPH::RVec3(0.0, 10.0, 0.0),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            cNonMovingObjectLayer);
        groundSettings.mFriction    = 1.0f;
        groundSettings.mRestitution = 0.2f;

        groundBodyID_ = bodyInterface.CreateAndAddBody(groundSettings, JPH::EActivation::Activate);
    }

    void JoltPhysicsSystem::setGroundEnabled(bool enabled) {
        groundEnabled_ = enabled;

        if (!physicsSystem_) {
            return;
        }

        auto& bodyInterface = physicsSystem_->GetBodyInterface();

        if (!enabled) {
            if (!groundBodyID_.IsInvalid()) {
                if (bodyInterface.IsAdded(groundBodyID_)) {
                    bodyInterface.RemoveBody(groundBodyID_);
                }
                bodyInterface.DestroyBody(groundBodyID_);
                groundBodyID_ = JPH::BodyID();
            }
            return;
        }

        if (groundBodyID_.IsInvalid()) {
            createGroundBody();
        }
    }

    // ============================================================================
    // update
    // ============================================================================

    void JoltPhysicsSystem::update(float frameTime, int maxSubSteps, float subStepTime) {
        if (!physicsSystem_) {
            return;
        }

        // Step the physics world
        (void) subStepTime;
        physicsSystem_->Update(frameTime, maxSubSteps, tempAllocator_.get(), jobSystem_.get());
    }

    // ============================================================================
    // syncToEntities
    // ============================================================================

    void JoltPhysicsSystem::syncToEntities(Scene* scene) {
        if (!physicsSystem_ || scene == nullptr) {
            return;
        }

        auto& registry = scene->getRegistry();
        auto  view     = registry.view<RigidBodyComponent, TransformComponent>();
        for (auto entity : view) {
            syncEntity(registry, entity);
        }
    }

    // ============================================================================
    // syncEntity
    // ============================================================================

    void JoltPhysicsSystem::syncEntity(entt::registry& registry, entt::entity entity) {
        if (!physicsSystem_ || !registry.all_of<RigidBodyComponent, TransformComponent>(entity)) {
            return;
        }

        auto& rigidBody = registry.get<RigidBodyComponent>(entity);
        auto& transform = registry.get<TransformComponent>(entity);
        auto* collider  = registry.try_get<ColliderComponent>(entity);
        auto* material  = registry.try_get<PhysicsMaterialComponent>(entity);

        const uint32_t      entityKey            = static_cast<uint32_t>(entt::to_integral(entity));
        JPH::BodyInterface& mutableBodyInterface = physicsSystem_->GetBodyInterface();

        JPH::EMotionType desiredMotionType = JPH::EMotionType::Dynamic;
        if (rigidBody.isStatic || rigidBody.mode == RigidBodyComponent::PhysicsMode::Static) {
            desiredMotionType = JPH::EMotionType::Static;
        } else if (rigidBody.mode == RigidBodyComponent::PhysicsMode::Kinematic) {
            desiredMotionType = JPH::EMotionType::Kinematic;
        }

        auto                               bodyIt                  = bodyMap_.find(entityKey);
        const ColliderComponent::ShapeType desiredShapeType        = (collider != nullptr) ? collider->shape : ColliderComponent::ShapeType::Box;
        const bool                         colliderRequestsRebuild = collider != nullptr && collider->pendingShapeRebuild;
        const bool                         needsNewBody            = bodyIt == bodyMap_.end() || bodyIt->second.shapeType != desiredShapeType || colliderRequestsRebuild;

        if (needsNewBody) {
            if (bodyIt != bodyMap_.end()) {
                if (mutableBodyInterface.IsAdded(bodyIt->second.bodyID)) {
                    mutableBodyInterface.RemoveBody(bodyIt->second.bodyID);
                }
                mutableBodyInterface.DestroyBody(bodyIt->second.bodyID);
                bodyMap_.erase(bodyIt);
            }

            std::unique_ptr<JPH::BodyCreationSettings> bodySettings;
            if (collider != nullptr) {
                if (collider->shape == ColliderComponent::ShapeType::Mesh) {
                    auto* modelComp = registry.try_get<ModelComponent>(entity);
                    if (modelComp == nullptr || modelComp->model == nullptr) {
                        return;
                    }

                    const auto& srcVertices = modelComp->model->getCollisionVertices();
                    const auto& srcIndices  = modelComp->model->getCollisionIndices();
                    if (srcVertices.size() < 3 || srcIndices.size() < 3) {
                        return;
                    }

                    JPH::VertexList vertices;
                    vertices.reserve(srcVertices.size());
                    for (const auto& v : srcVertices) {
                        vertices.emplace_back(
                            v.x * transform.scale.x,
                            v.y * transform.scale.y,
                            v.z * transform.scale.z);
                    }

                    JPH::IndexedTriangleList triangles;
                    triangles.reserve(srcIndices.size() / 3);
                    for (size_t i = 0; i + 2 < srcIndices.size(); i += 3) {
                        triangles.emplace_back(srcIndices[i], srcIndices[i + 1], srcIndices[i + 2], 0);
                    }

                    JPH::MeshShapeSettings meshSettings(std::move(vertices), std::move(triangles));
                    auto                   shapeResult = meshSettings.Create();
                    if (!shapeResult.IsValid()) {
                        return;
                    }

                    bodySettings = std::make_unique<JPH::BodyCreationSettings>(
                        shapeResult.Get().GetPtr(),
                        JPH::RVec3(transform.translation.x, transform.translation.y, transform.translation.z),
                        toJolt(glm::quat(transform.rotation)),
                        desiredMotionType,
                        desiredMotionType == JPH::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer);
                    if (desiredMotionType == JPH::EMotionType::Dynamic) {
                        bodySettings->mMotionQuality = JPH::EMotionQuality::LinearCast;
                    }
                } else {
                    bodySettings = createCollisionShape(transform, rigidBody, *collider, material);
                }
            } else {
                auto const halfExtents = glm::max(transform.scale * 0.5f, glm::vec3(0.05f));
                auto       shape       = std::make_unique<JPH::BoxShape>(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));

                bodySettings = std::make_unique<JPH::BodyCreationSettings>(
                    shape.release(),
                    JPH::RVec3(transform.translation.x, transform.translation.y, transform.translation.z),
                    toJolt(glm::quat(transform.rotation)),
                    desiredMotionType,
                    desiredMotionType == JPH::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer);
                if (desiredMotionType == JPH::EMotionType::Dynamic) {
                    bodySettings->mMotionQuality = JPH::EMotionQuality::LinearCast;
                }

                bodySettings->mFriction       = rigidBody.friction;
                bodySettings->mRestitution    = rigidBody.restitution;
                bodySettings->mLinearDamping  = 0.0f;
                bodySettings->mAngularDamping = 0.0f;
                bodySettings->mGravityFactor  = rigidBody.useGravity ? 1.0f : 0.0f;
                bodySettings->mIsSensor       = false;
                bodySettings->mUserData       = static_cast<uint64_t>(entityKey);
            }

            if (!bodySettings) {
                return;
            }

            glm::vec3 bodyPosition = transform.translation;
            if (collider != nullptr) {
                bodyPosition += glm::quat(transform.rotation) * collider->centerOffset;
            }

            bodySettings->mPosition    = JPH::RVec3(bodyPosition.x, bodyPosition.y, bodyPosition.z);
            bodySettings->mRotation    = toJolt(glm::quat(transform.rotation));
            bodySettings->mMotionType  = desiredMotionType;
            bodySettings->mObjectLayer = desiredMotionType == JPH::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer;
            if (collider != nullptr && collider->shape == ColliderComponent::ShapeType::Mesh && desiredMotionType != JPH::EMotionType::Static) {
                bodySettings->mMotionType  = JPH::EMotionType::Static;
                bodySettings->mObjectLayer = cNonMovingObjectLayer;
            }
            bodySettings->mLinearVelocity  = toJolt(rigidBody.velocity);
            bodySettings->mAngularVelocity = toJolt(rigidBody.angularVelocity);
            bodySettings->mGravityFactor   = rigidBody.useGravity ? 1.0f : 0.0f;
            bodySettings->mFriction        = material != nullptr ? material->friction : rigidBody.friction;
            bodySettings->mRestitution     = material != nullptr ? material->restitution : rigidBody.restitution;
            bodySettings->mLinearDamping   = material != nullptr ? material->damping : 0.0f;
            bodySettings->mAngularDamping  = material != nullptr ? material->angularDamping : 0.0f;
            bodySettings->mIsSensor        = collider != nullptr ? collider->isTrigger : false;
            bodySettings->mUserData        = static_cast<uint64_t>(entityKey);

            JPH::BodyID bodyID = mutableBodyInterface.CreateAndAddBody(*bodySettings, JPH::EActivation::Activate);
            if (bodyID.IsInvalid()) {
                return;
            }

            bodyMap_.emplace(entityKey, BodySyncInfo{bodyID, desiredShapeType});
            bodyIt = bodyMap_.find(entityKey);
            if (collider != nullptr) {
                collider->pendingShapeRebuild = false;
            }
        }

        if (bodyIt == bodyMap_.end()) {
            return;
        }

        const JPH::BodyID      bodyID = bodyIt->second.bodyID;
        const JPH::EMotionType effectiveMotionType =
            (collider != nullptr && collider->shape == ColliderComponent::ShapeType::Mesh)
                ? JPH::EMotionType::Static
                : desiredMotionType;

        const JPH::ObjectLayer targetObjectLayer =
            effectiveMotionType == JPH::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer;

        if (mutableBodyInterface.GetMotionType(bodyID) != effectiveMotionType) {
            mutableBodyInterface.SetMotionType(bodyID, effectiveMotionType, JPH::EActivation::Activate);
        }
        if (mutableBodyInterface.GetObjectLayer(bodyID) != targetObjectLayer) {
            mutableBodyInterface.SetObjectLayer(bodyID, targetObjectLayer);
        }
        if (effectiveMotionType == JPH::EMotionType::Dynamic &&
            mutableBodyInterface.GetMotionQuality(bodyID) != JPH::EMotionQuality::LinearCast) {
            mutableBodyInterface.SetMotionQuality(bodyID, JPH::EMotionQuality::LinearCast);
        }
        mutableBodyInterface.SetGravityFactor(bodyID, rigidBody.useGravity ? 1.0f : 0.0f);
        mutableBodyInterface.SetFriction(bodyID, material != nullptr ? material->friction : rigidBody.friction);
        mutableBodyInterface.SetRestitution(bodyID, material != nullptr ? material->restitution : rigidBody.restitution);
        mutableBodyInterface.SetIsSensor(bodyID, collider != nullptr ? collider->isTrigger : false);

        if (effectiveMotionType == JPH::EMotionType::Dynamic) {
            // Dynamic bodies are usually Jolt-authoritative, but explicit ECS edits from the UI
            // (while simulation is paused) should be applied exactly once on resume.
            if (!needsNewBody && rigidBody.pendingBodyStateOverride) {
                glm::vec3 bodyPosition = transform.translation;
                if (collider != nullptr) {
                    bodyPosition += glm::quat(transform.rotation) * collider->centerOffset;
                }
                mutableBodyInterface.SetPositionRotationAndVelocity(
                    bodyID,
                    JPH::RVec3(bodyPosition.x, bodyPosition.y, bodyPosition.z),
                    toJolt(glm::quat(transform.rotation)),
                    toJolt(rigidBody.velocity),
                    toJolt(rigidBody.angularVelocity));
                mutableBodyInterface.ActivateBody(bodyID);
                rigidBody.pendingBodyStateOverride = false;
            }

            // Apply one-shot ECS acceleration as force.
            if (glm::dot(rigidBody.acceleration, rigidBody.acceleration) > 0.0f) {
                mutableBodyInterface.AddForce(bodyID, toJolt(rigidBody.acceleration * rigidBody.mass), JPH::EActivation::Activate);
                rigidBody.acceleration = glm::vec3(0.0f);
            }
        } else if (effectiveMotionType != JPH::EMotionType::Static || needsNewBody || rigidBody.pendingBodyStateOverride) {
            // Kinematic bodies are ECS-driven every frame. Static bodies are only
            // re-driven when newly created or explicitly overridden by UI edits.
            glm::vec3 bodyPosition = transform.translation;
            if (collider != nullptr) {
                bodyPosition += glm::quat(transform.rotation) * collider->centerOffset;
            }
            mutableBodyInterface.SetPositionRotationAndVelocity(
                bodyID,
                JPH::RVec3(bodyPosition.x, bodyPosition.y, bodyPosition.z),
                toJolt(glm::quat(transform.rotation)),
                toJolt(rigidBody.velocity),
                toJolt(rigidBody.angularVelocity));
            rigidBody.pendingBodyStateOverride = false;
        }

        JPH::RVec3 position        = mutableBodyInterface.GetPosition(bodyID);
        JPH::Quat  rotation        = mutableBodyInterface.GetRotation(bodyID);
        JPH::Vec3  linearVelocity  = mutableBodyInterface.GetLinearVelocity(bodyID);
        JPH::Vec3  angularVelocity = mutableBodyInterface.GetAngularVelocity(bodyID);

        const glm::quat bodyRotation = toGlm(rotation);
        transform.rotation           = glm::eulerAngles(bodyRotation);
        transform.translation        = toGlm(JPH::Vec3(position.GetX(), position.GetY(), position.GetZ()));
        if (collider != nullptr) {
            transform.translation -= bodyRotation * collider->centerOffset;
        }
        rigidBody.velocity        = toGlm(linearVelocity);
        rigidBody.angularVelocity = toGlm(angularVelocity);
    }

    // ============================================================================
    // removeEntity
    // ============================================================================

    void JoltPhysicsSystem::removeEntity(entt::entity e) {
        if (!physicsSystem_) {
            return;
        }

        const uint32_t entityKey = static_cast<uint32_t>(entt::to_integral(e));
        auto           bodyIt    = bodyMap_.find(entityKey);
        if (bodyIt == bodyMap_.end()) {
            return;
        }

        auto& bodyInterface = physicsSystem_->GetBodyInterface();
        if (bodyInterface.IsAdded(bodyIt->second.bodyID)) {
            bodyInterface.RemoveBody(bodyIt->second.bodyID);
        }
        bodyInterface.DestroyBody(bodyIt->second.bodyID);
        bodyMap_.erase(bodyIt);
    }

    // ============================================================================
    // clear
    // ============================================================================

    void JoltPhysicsSystem::clear() {
        if (physicsSystem_) {
            auto& bodyInterface = physicsSystem_->GetBodyInterface();
            if (!groundBodyID_.IsInvalid()) {
                if (bodyInterface.IsAdded(groundBodyID_)) {
                    bodyInterface.RemoveBody(groundBodyID_);
                }
                bodyInterface.DestroyBody(groundBodyID_);
                groundBodyID_ = JPH::BodyID();
            }
            for (const auto& [entityKey, syncInfo] : bodyMap_) {
                if (bodyInterface.IsAdded(syncInfo.bodyID)) {
                    bodyInterface.RemoveBody(syncInfo.bodyID);
                }
                bodyInterface.DestroyBody(syncInfo.bodyID);
            }
        }
        bodyMap_.clear();
    }

    // ============================================================================
    // raycast
    // ============================================================================

    glm::vec3 JoltPhysicsSystem::raycast(const glm::vec3& origin, const glm::vec3& direction,
        float maxDistance) const {
        if (!physicsSystem_) {
            return glm::vec3(0.0f);
        }

        const JPH::RRayCast rayCast(
            JPH::RVec3(origin.x, origin.y, origin.z),
            toJolt(direction) * maxDistance);
        JPH::RayCastResult result;
        physicsSystem_->GetNarrowPhaseQuery().CastRay(rayCast, result);
        if (result.mBodyID.IsInvalid()) {
            return glm::vec3(0.0f);
        }

        const JPH::RVec3 hitPoint = rayCast.GetPointOnRay(result.mFraction);
        return glm::vec3(hitPoint.GetX(), hitPoint.GetY(), hitPoint.GetZ());
    }

    // ============================================================================
    // createCollisionShape
    // ============================================================================

    std::unique_ptr<JPH::BodyCreationSettings> JoltPhysicsSystem::createCollisionShape(
        const TransformComponent&       transform,
        const RigidBodyComponent&       rigidBody,
        const ColliderComponent&        collider,
        const PhysicsMaterialComponent* material) {
        (void) transform;
        (void) rigidBody;
        (void) material;

        std::unique_ptr<JPH::Shape> shape;
        switch (collider.shape) {
            case ColliderComponent::ShapeType::Sphere:
                shape = std::make_unique<JPH::SphereShape>(collider.radius);
                break;
            case ColliderComponent::ShapeType::Box:
                shape = std::make_unique<JPH::BoxShape>(JPH::Vec3(collider.size.x * 0.5f, collider.size.y * 0.5f, collider.size.z * 0.5f));
                break;
            case ColliderComponent::ShapeType::Capsule:
                shape = std::make_unique<JPH::CapsuleShape>(collider.size.y * 0.5f, collider.radius);
                break;
            case ColliderComponent::ShapeType::Mesh:
                return nullptr;
        }

        if (!shape) {
            return nullptr;
        }

        JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
        if (rigidBody.isStatic || rigidBody.mode == RigidBodyComponent::PhysicsMode::Static) {
            motionType = JPH::EMotionType::Static;
        } else if (rigidBody.mode == RigidBodyComponent::PhysicsMode::Kinematic) {
            motionType = JPH::EMotionType::Kinematic;
        }

        auto settings = std::make_unique<JPH::BodyCreationSettings>(
            shape.release(),
            JPH::RVec3(transform.translation.x, transform.translation.y, transform.translation.z),
            toJolt(glm::quat(transform.rotation)),
            motionType,
            motionType == JPH::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer);
        if (motionType == JPH::EMotionType::Dynamic) {
            settings->mMotionQuality = JPH::EMotionQuality::LinearCast;
        }

        settings->mFriction       = material != nullptr ? material->friction : rigidBody.friction;
        settings->mRestitution    = material != nullptr ? material->restitution : rigidBody.restitution;
        settings->mLinearDamping  = material != nullptr ? material->damping : 0.0f;
        settings->mAngularDamping = material != nullptr ? material->angularDamping : 0.0f;
        settings->mGravityFactor  = rigidBody.useGravity ? 1.0f : 0.0f;
        settings->mIsSensor       = collider.isTrigger;
        settings->mUserData       = 0;

        return settings;
    }

}  // namespace engine
