#include "Engine/Systems/JoltPhysicsSystem.hpp"

#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"

#include <entt/entt.hpp>

#include "Jolt/Physics/Collision/Shape/MeshShape.h"

namespace Jolt = JPH;

namespace engine {

// ============================================================================
// Helpers
// ============================================================================

Jolt::Vec3 JoltPhysicsSystem::toJolt(const glm::vec3& v) {
    return Jolt::Vec3(v.x, v.y, v.z);
}

Jolt::Quat JoltPhysicsSystem::toJolt(const glm::quat& q) {
    return Jolt::Quat(q.x, q.y, q.z, q.w);
}

glm::vec3 JoltPhysicsSystem::toGlm(const Jolt::Vec3& v) {
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

glm::quat JoltPhysicsSystem::toGlm(const Jolt::Quat& q) {
    return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

JoltPhysicsSystem::JoltPhysicsSystem()
        : broadPhaseLayerInterface_(nullptr),
            objectLayerPairFilter_(nullptr),
            objectVsBroadPhaseLayerFilter_(nullptr) {

    // Jolt requires global allocator registration before any JPH allocation.
    Jolt::RegisterDefaultAllocator();

    factory_ = std::make_unique<Jolt::Factory>();
    tempAllocator_ = std::make_unique<Jolt::TempAllocatorImpl>(10 * 1024 * 1024);
    jobSystem_ = std::make_unique<Jolt::JobSystemThreadPool>(1024, 64);
    physicsSystem_ = std::make_unique<Jolt::PhysicsSystem>();
    broadPhaseLayerInterface_ = std::make_unique<Jolt::BroadPhaseLayerInterfaceTable>(cNumObjectLayers, cNumBroadPhaseLayers);
    objectLayerPairFilter_ = std::make_unique<Jolt::ObjectLayerPairFilterTable>(cNumObjectLayers);
    objectVsBroadPhaseLayerFilter_ = std::make_unique<Jolt::ObjectVsBroadPhaseLayerFilterTable>(
        *broadPhaseLayerInterface_, cNumBroadPhaseLayers, *objectLayerPairFilter_, cNumObjectLayers);

    Jolt::Factory::sInstance = factory_.get();
    Jolt::RegisterTypes();
    typesRegistered_ = true;

    broadPhaseLayerInterface_->MapObjectToBroadPhaseLayer(cNonMovingObjectLayer, Jolt::BroadPhaseLayer(0));
    broadPhaseLayerInterface_->MapObjectToBroadPhaseLayer(cMovingObjectLayer, Jolt::BroadPhaseLayer(1));

    objectLayerPairFilter_->EnableCollision(cNonMovingObjectLayer, cNonMovingObjectLayer);
    objectLayerPairFilter_->EnableCollision(cNonMovingObjectLayer, cMovingObjectLayer);
    objectLayerPairFilter_->EnableCollision(cMovingObjectLayer, cNonMovingObjectLayer);
    objectLayerPairFilter_->EnableCollision(cMovingObjectLayer, cMovingObjectLayer);

    physicsSystem_->Init(1024, 0, 1024, 1024,
                         *broadPhaseLayerInterface_,
                         *objectVsBroadPhaseLayerFilter_,
                         *objectLayerPairFilter_);

    Jolt::PhysicsSettings physicsSettings = physicsSystem_->GetPhysicsSettings();
    physicsSettings.mMinVelocityForRestitution = 0.0f;
    physicsSystem_->SetPhysicsSettings(physicsSettings);

    // Y-down convention: positive Y is downward.
    physicsSystem_->SetGravity(Jolt::Vec3(0.0f, 9.81f, 0.0f));

    createGroundBody();
}

JoltPhysicsSystem::~JoltPhysicsSystem() {
    clear();

    if (typesRegistered_) {
        Jolt::UnregisterTypes();
        typesRegistered_ = false;
    }

    Jolt::Factory::sInstance = nullptr;
    physicsSystem_.reset();
    jobSystem_.reset();
    tempAllocator_.reset();
    factory_.reset();
}

void JoltPhysicsSystem::createGroundBody() {
    if (!physicsSystem_) {
        return;
    }

    auto& bodyInterface = physicsSystem_->GetBodyInterface();

    // Large static box aligned with the grid plane at Y = 0.
    // In Y-down convention, floor thickness extends toward +Y (downward).
    auto groundShape = std::make_unique<Jolt::BoxShape>(Jolt::Vec3(1000.0f, 10.0f, 1000.0f));
    Jolt::BodyCreationSettings groundSettings(
        groundShape.release(),
        Jolt::RVec3(0.0, 10.0, 0.0),
        Jolt::Quat::sIdentity(),
        Jolt::EMotionType::Static,
        cNonMovingObjectLayer);
    groundSettings.mFriction = 1.0f;
    groundSettings.mRestitution = 0.2f;

    groundBodyID_ = bodyInterface.CreateAndAddBody(groundSettings, Jolt::EActivation::Activate);
}

// ============================================================================
// update
// ============================================================================

void JoltPhysicsSystem::update(float frameTime, int maxSubSteps, float subStepTime) {
    if (!physicsSystem_) {
        return;
    }

    // Step the physics world
    (void)subStepTime;
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
    auto view = registry.view<RigidBodyComponent, TransformComponent>();
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
    auto* collider = registry.try_get<ColliderComponent>(entity);
    auto* material = registry.try_get<PhysicsMaterialComponent>(entity);

    const uint32_t entityKey = static_cast<uint32_t>(entt::to_integral(entity));
    Jolt::BodyInterface& mutableBodyInterface = physicsSystem_->GetBodyInterface();

    Jolt::EMotionType desiredMotionType = Jolt::EMotionType::Dynamic;
    if (rigidBody.isStatic || rigidBody.mode == RigidBodyComponent::PhysicsMode::Static) {
        desiredMotionType = Jolt::EMotionType::Static;
    } else if (rigidBody.mode == RigidBodyComponent::PhysicsMode::Kinematic) {
        desiredMotionType = Jolt::EMotionType::Kinematic;
    }

    auto bodyIt = bodyMap_.find(entityKey);
    const ColliderComponent::ShapeType desiredShapeType = (collider != nullptr) ? collider->shape : ColliderComponent::ShapeType::Box;
    const bool colliderRequestsRebuild = collider != nullptr && collider->pendingShapeRebuild;
    const bool needsNewBody = bodyIt == bodyMap_.end() || bodyIt->second.shapeType != desiredShapeType || colliderRequestsRebuild;

    if (needsNewBody) {
        if (bodyIt != bodyMap_.end()) {
            if (mutableBodyInterface.IsAdded(bodyIt->second.bodyID)) {
                mutableBodyInterface.RemoveBody(bodyIt->second.bodyID);
            }
            mutableBodyInterface.DestroyBody(bodyIt->second.bodyID);
            bodyMap_.erase(bodyIt);
        }

        std::unique_ptr<Jolt::BodyCreationSettings> bodySettings;
        if (collider != nullptr) {
            if (collider->shape == ColliderComponent::ShapeType::Mesh) {
                auto* modelComp = registry.try_get<ModelComponent>(entity);
                if (modelComp == nullptr || modelComp->model == nullptr) {
                    return;
                }

                const auto& srcVertices = modelComp->model->getCollisionVertices();
                const auto& srcIndices = modelComp->model->getCollisionIndices();
                if (srcVertices.size() < 3 || srcIndices.size() < 3) {
                    return;
                }

                Jolt::VertexList vertices;
                vertices.reserve(srcVertices.size());
                for (const auto& v : srcVertices) {
                    vertices.emplace_back(
                        v.x * transform.scale.x,
                        v.y * transform.scale.y,
                        v.z * transform.scale.z);
                }

                Jolt::IndexedTriangleList triangles;
                triangles.reserve(srcIndices.size() / 3);
                for (size_t i = 0; i + 2 < srcIndices.size(); i += 3) {
                    triangles.emplace_back(srcIndices[i], srcIndices[i + 1], srcIndices[i + 2], 0);
                }

                Jolt::MeshShapeSettings meshSettings(std::move(vertices), std::move(triangles));
                auto shapeResult = meshSettings.Create();
                if (!shapeResult.IsValid()) {
                    return;
                }

                bodySettings = std::make_unique<Jolt::BodyCreationSettings>(
                    shapeResult.Get().GetPtr(),
                    Jolt::RVec3(transform.translation.x, transform.translation.y, transform.translation.z),
                    toJolt(glm::quat(transform.rotation)),
                    desiredMotionType,
                    desiredMotionType == Jolt::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer);
                if (desiredMotionType == Jolt::EMotionType::Dynamic) {
                    bodySettings->mMotionQuality = Jolt::EMotionQuality::LinearCast;
                }
            } else {
                bodySettings = createCollisionShape(transform, rigidBody, *collider, material);
            }
        } else {
            auto const halfExtents = glm::max(transform.scale * 0.5f, glm::vec3(0.05f));
            auto shape = std::make_unique<Jolt::BoxShape>(Jolt::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));

            bodySettings = std::make_unique<Jolt::BodyCreationSettings>(
                shape.release(),
                Jolt::RVec3(transform.translation.x, transform.translation.y, transform.translation.z),
                toJolt(glm::quat(transform.rotation)),
                desiredMotionType,
                desiredMotionType == Jolt::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer);
            if (desiredMotionType == Jolt::EMotionType::Dynamic) {
                bodySettings->mMotionQuality = Jolt::EMotionQuality::LinearCast;
            }

            bodySettings->mFriction = rigidBody.friction;
            bodySettings->mRestitution = rigidBody.restitution;
            bodySettings->mLinearDamping = 0.0f;
            bodySettings->mAngularDamping = 0.0f;
            bodySettings->mGravityFactor = rigidBody.useGravity ? 1.0f : 0.0f;
            bodySettings->mIsSensor = false;
            bodySettings->mUserData = static_cast<uint64_t>(entityKey);
        }

        if (!bodySettings) {
            return;
        }

        glm::vec3 bodyPosition = transform.translation;
        if (collider != nullptr) {
            bodyPosition += glm::quat(transform.rotation) * collider->centerOffset;
        }

        bodySettings->mPosition = Jolt::RVec3(bodyPosition.x, bodyPosition.y, bodyPosition.z);
        bodySettings->mRotation = toJolt(glm::quat(transform.rotation));
        bodySettings->mMotionType = desiredMotionType;
        bodySettings->mObjectLayer = desiredMotionType == Jolt::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer;
        if (collider != nullptr && collider->shape == ColliderComponent::ShapeType::Mesh && desiredMotionType != Jolt::EMotionType::Static) {
            bodySettings->mMotionType = Jolt::EMotionType::Static;
            bodySettings->mObjectLayer = cNonMovingObjectLayer;
        }
        bodySettings->mLinearVelocity = toJolt(rigidBody.velocity);
        bodySettings->mAngularVelocity = toJolt(rigidBody.angularVelocity);
        bodySettings->mGravityFactor = rigidBody.useGravity ? 1.0f : 0.0f;
        bodySettings->mFriction = material != nullptr ? material->friction : rigidBody.friction;
        bodySettings->mRestitution = material != nullptr ? material->restitution : rigidBody.restitution;
        bodySettings->mLinearDamping = material != nullptr ? material->damping : 0.0f;
        bodySettings->mAngularDamping = material != nullptr ? material->angularDamping : 0.0f;
        bodySettings->mIsSensor = collider != nullptr ? collider->isTrigger : false;
        bodySettings->mUserData = static_cast<uint64_t>(entityKey);

        Jolt::BodyID bodyID = mutableBodyInterface.CreateAndAddBody(*bodySettings, Jolt::EActivation::Activate);
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

    const Jolt::BodyID bodyID = bodyIt->second.bodyID;
    const Jolt::EMotionType effectiveMotionType =
        (collider != nullptr && collider->shape == ColliderComponent::ShapeType::Mesh)
            ? Jolt::EMotionType::Static
            : desiredMotionType;

    const Jolt::ObjectLayer targetObjectLayer =
        effectiveMotionType == Jolt::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer;

    if (mutableBodyInterface.GetMotionType(bodyID) != effectiveMotionType) {
        mutableBodyInterface.SetMotionType(bodyID, effectiveMotionType, Jolt::EActivation::Activate);
    }
    if (mutableBodyInterface.GetObjectLayer(bodyID) != targetObjectLayer) {
        mutableBodyInterface.SetObjectLayer(bodyID, targetObjectLayer);
    }
    if (effectiveMotionType == Jolt::EMotionType::Dynamic &&
        mutableBodyInterface.GetMotionQuality(bodyID) != Jolt::EMotionQuality::LinearCast) {
        mutableBodyInterface.SetMotionQuality(bodyID, Jolt::EMotionQuality::LinearCast);
    }
    mutableBodyInterface.SetGravityFactor(bodyID, rigidBody.useGravity ? 1.0f : 0.0f);
    mutableBodyInterface.SetFriction(bodyID, material != nullptr ? material->friction : rigidBody.friction);
    mutableBodyInterface.SetRestitution(bodyID, material != nullptr ? material->restitution : rigidBody.restitution);
    mutableBodyInterface.SetIsSensor(bodyID, collider != nullptr ? collider->isTrigger : false);

    if (effectiveMotionType == Jolt::EMotionType::Dynamic && !needsNewBody) {
        // Dynamic bodies are usually Jolt-authoritative, but explicit ECS edits from the UI
        // (while simulation is paused) should be applied exactly once on resume.
        if (rigidBody.pendingBodyStateOverride) {
            glm::vec3 bodyPosition = transform.translation;
            if (collider != nullptr) {
                bodyPosition += glm::quat(transform.rotation) * collider->centerOffset;
            }
            mutableBodyInterface.SetPositionRotationAndVelocity(
                bodyID,
                Jolt::RVec3(bodyPosition.x, bodyPosition.y, bodyPosition.z),
                toJolt(glm::quat(transform.rotation)),
                toJolt(rigidBody.velocity),
                toJolt(rigidBody.angularVelocity));
            mutableBodyInterface.ActivateBody(bodyID);
            rigidBody.pendingBodyStateOverride = false;
        }

        // Apply one-shot ECS acceleration as force.
        if (glm::dot(rigidBody.acceleration, rigidBody.acceleration) > 0.0f) {
            mutableBodyInterface.AddForce(bodyID, toJolt(rigidBody.acceleration * rigidBody.mass), Jolt::EActivation::Activate);
            rigidBody.acceleration = glm::vec3(0.0f);
        }
    } else if (effectiveMotionType != Jolt::EMotionType::Static || needsNewBody || rigidBody.pendingBodyStateOverride) {
        // Kinematic bodies are ECS-driven every frame. Static bodies are only
        // re-driven when newly created or explicitly overridden by UI edits.
        glm::vec3 bodyPosition = transform.translation;
        if (collider != nullptr) {
            bodyPosition += glm::quat(transform.rotation) * collider->centerOffset;
        }
        mutableBodyInterface.SetPositionRotationAndVelocity(
            bodyID,
            Jolt::RVec3(bodyPosition.x, bodyPosition.y, bodyPosition.z),
            toJolt(glm::quat(transform.rotation)),
            toJolt(rigidBody.velocity),
            toJolt(rigidBody.angularVelocity));
        rigidBody.pendingBodyStateOverride = false;
    }

    Jolt::RVec3 position = mutableBodyInterface.GetPosition(bodyID);
    Jolt::Quat rotation = mutableBodyInterface.GetRotation(bodyID);
    Jolt::Vec3 linearVelocity = mutableBodyInterface.GetLinearVelocity(bodyID);
    Jolt::Vec3 angularVelocity = mutableBodyInterface.GetAngularVelocity(bodyID);

    const glm::quat bodyRotation = toGlm(rotation);
    transform.rotation = glm::eulerAngles(bodyRotation);
    transform.translation = toGlm(Jolt::Vec3(position.GetX(), position.GetY(), position.GetZ()));
    if (collider != nullptr) {
        transform.translation -= bodyRotation * collider->centerOffset;
    }
    rigidBody.velocity = toGlm(linearVelocity);
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
    auto bodyIt = bodyMap_.find(entityKey);
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
            groundBodyID_ = Jolt::BodyID();
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

    const Jolt::RRayCast rayCast(
        Jolt::RVec3(origin.x, origin.y, origin.z),
        toJolt(direction) * maxDistance);
    Jolt::RayCastResult result;
    physicsSystem_->GetNarrowPhaseQuery().CastRay(rayCast, result);
    if (result.mBodyID.IsInvalid()) {
        return glm::vec3(0.0f);
    }

    const Jolt::RVec3 hitPoint = rayCast.GetPointOnRay(result.mFraction);
    return glm::vec3(hitPoint.GetX(), hitPoint.GetY(), hitPoint.GetZ());
}

// ============================================================================
// createCollisionShape
// ============================================================================

std::unique_ptr<Jolt::BodyCreationSettings> JoltPhysicsSystem::createCollisionShape(
    const TransformComponent& transform,
    const RigidBodyComponent& rigidBody,
    const ColliderComponent& collider,
    const PhysicsMaterialComponent* material) {
    (void)transform;
    (void)rigidBody;
    (void)material;

    std::unique_ptr<Jolt::Shape> shape;
    switch (collider.shape) {
        case ColliderComponent::ShapeType::Sphere:
            shape = std::make_unique<Jolt::SphereShape>(collider.radius);
            break;
        case ColliderComponent::ShapeType::Box:
            shape = std::make_unique<Jolt::BoxShape>(Jolt::Vec3(collider.size.x * 0.5f, collider.size.y * 0.5f, collider.size.z * 0.5f));
            break;
        case ColliderComponent::ShapeType::Capsule:
            shape = std::make_unique<Jolt::CapsuleShape>(collider.size.y * 0.5f, collider.radius);
            break;
        case ColliderComponent::ShapeType::Mesh:
            return nullptr;
    }

    if (!shape) {
        return nullptr;
    }

    Jolt::EMotionType motionType = Jolt::EMotionType::Dynamic;
    if (rigidBody.isStatic || rigidBody.mode == RigidBodyComponent::PhysicsMode::Static) {
        motionType = Jolt::EMotionType::Static;
    } else if (rigidBody.mode == RigidBodyComponent::PhysicsMode::Kinematic) {
        motionType = Jolt::EMotionType::Kinematic;
    }

    auto settings = std::make_unique<Jolt::BodyCreationSettings>(
        shape.release(),
        Jolt::RVec3(transform.translation.x, transform.translation.y, transform.translation.z),
        toJolt(glm::quat(transform.rotation)),
        motionType,
        motionType == Jolt::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer);
    if (motionType == Jolt::EMotionType::Dynamic) {
        settings->mMotionQuality = Jolt::EMotionQuality::LinearCast;
    }

    settings->mFriction = material != nullptr ? material->friction : rigidBody.friction;
    settings->mRestitution = material != nullptr ? material->restitution : rigidBody.restitution;
    settings->mLinearDamping = material != nullptr ? material->damping : 0.0f;
    settings->mAngularDamping = material != nullptr ? material->angularDamping : 0.0f;
    settings->mGravityFactor = rigidBody.useGravity ? 1.0f : 0.0f;
    settings->mIsSensor = collider.isTrigger;
    settings->mUserData = 0;

    return settings;
}

} // namespace engine
