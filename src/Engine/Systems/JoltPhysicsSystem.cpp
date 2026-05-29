#include "Engine/Systems/JoltPhysicsSystem.hpp"

#include "Engine/Scene/Scene.hpp"

#include <entt/entt.hpp>

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

    objectLayerPairFilter_->EnableCollision(cNonMovingObjectLayer, cMovingObjectLayer);
    objectLayerPairFilter_->EnableCollision(cMovingObjectLayer, cMovingObjectLayer);

    physicsSystem_->Init(1024, 0, 1024, 1024,
                         *broadPhaseLayerInterface_,
                         *objectVsBroadPhaseLayerFilter_,
                         *objectLayerPairFilter_);
    physicsSystem_->SetGravity(Jolt::Vec3(0.0f, -9.81f, 0.0f));
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

// ============================================================================
// Registration
// ============================================================================

static void registerJoltTypes() {
    Jolt::RegisterTypes();
}

// ============================================================================
// update
// ============================================================================

void JoltPhysicsSystem::update(float frameTime, int maxSubSteps, float subStepTime) {
    if (!physicsSystem_)
        return;

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

    const auto desiredMotionType = rigidBody.isStatic || rigidBody.mode == RigidBodyComponent::PhysicsMode::Static
        ? Jolt::EMotionType::Static
        : (rigidBody.mode == RigidBodyComponent::PhysicsMode::Kinematic ? Jolt::EMotionType::Kinematic : Jolt::EMotionType::Dynamic);

    auto bodyIt = bodyMap_.find(entityKey);
    const bool needsNewBody = collider == nullptr || bodyIt == bodyMap_.end() || bodyIt->second.shapeType != collider->shape;

    if (needsNewBody) {
        if (bodyIt != bodyMap_.end()) {
            if (mutableBodyInterface.IsAdded(bodyIt->second.bodyID)) {
                mutableBodyInterface.RemoveBody(bodyIt->second.bodyID);
            }
            mutableBodyInterface.DestroyBody(bodyIt->second.bodyID);
            bodyMap_.erase(bodyIt);
        }

        if (collider == nullptr) {
            return;
        }

        auto bodySettings = createCollisionShape(transform, rigidBody, *collider, material);
        if (!bodySettings) {
            return;
        }

        bodySettings->mPosition = Jolt::RVec3(transform.translation.x, transform.translation.y, transform.translation.z);
        bodySettings->mRotation = toJolt(glm::quat(transform.rotation));
        bodySettings->mMotionType = desiredMotionType;
        bodySettings->mObjectLayer = desiredMotionType == Jolt::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer;
        bodySettings->mLinearVelocity = toJolt(rigidBody.velocity);
        bodySettings->mAngularVelocity = toJolt(rigidBody.angularVelocity);
        bodySettings->mGravityFactor = rigidBody.useGravity ? 1.0f : 0.0f;
        bodySettings->mFriction = material ? material->friction : rigidBody.friction;
        bodySettings->mRestitution = material ? material->restitution : rigidBody.restitution;
        bodySettings->mLinearDamping = material ? material->damping : 0.0f;
        bodySettings->mAngularDamping = material ? material->angularDamping : 0.0f;
        bodySettings->mIsSensor = collider->isTrigger;
        bodySettings->mUserData = static_cast<uint64_t>(entityKey);

        Jolt::BodyID bodyID = mutableBodyInterface.CreateAndAddBody(*bodySettings, Jolt::EActivation::Activate);
        if (bodyID.IsInvalid()) {
            return;
        }

        bodyMap_.emplace(entityKey, BodySyncInfo{bodyID, collider->shape});
        bodyIt = bodyMap_.find(entityKey);
    }

    if (bodyIt == bodyMap_.end()) {
        return;
    }

    const Jolt::BodyID bodyID = bodyIt->second.bodyID;

    mutableBodyInterface.SetMotionType(bodyID, desiredMotionType, Jolt::EActivation::Activate);
    mutableBodyInterface.SetObjectLayer(bodyID, desiredMotionType == Jolt::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer);
    mutableBodyInterface.SetGravityFactor(bodyID, rigidBody.useGravity ? 1.0f : 0.0f);
    mutableBodyInterface.SetFriction(bodyID, material ? material->friction : rigidBody.friction);
    mutableBodyInterface.SetRestitution(bodyID, material ? material->restitution : rigidBody.restitution);
    mutableBodyInterface.SetIsSensor(bodyID, collider->isTrigger);

    if (desiredMotionType == Jolt::EMotionType::Dynamic && !needsNewBody) {
        // Keep dynamic body state authoritative in Jolt. Only apply one-shot ECS acceleration as force.
        if (glm::dot(rigidBody.acceleration, rigidBody.acceleration) > 0.0f) {
            mutableBodyInterface.AddForce(bodyID, toJolt(rigidBody.acceleration * rigidBody.mass), Jolt::EActivation::Activate);
            rigidBody.acceleration = glm::vec3(0.0f);
        }
    } else {
        // Static/kinematic (and freshly-created bodies) are driven from ECS.
        mutableBodyInterface.SetPositionRotationAndVelocity(
            bodyID,
            Jolt::RVec3(transform.translation.x, transform.translation.y, transform.translation.z),
            toJolt(glm::quat(transform.rotation)),
            toJolt(rigidBody.velocity),
            toJolt(rigidBody.angularVelocity));
    }

    Jolt::RVec3 position = mutableBodyInterface.GetPosition(bodyID);
    Jolt::Quat rotation = mutableBodyInterface.GetRotation(bodyID);
    Jolt::Vec3 linearVelocity = mutableBodyInterface.GetLinearVelocity(bodyID);
    Jolt::Vec3 angularVelocity = mutableBodyInterface.GetAngularVelocity(bodyID);

    transform.translation = toGlm(Jolt::Vec3(position.GetX(), position.GetY(), position.GetZ()));
    transform.rotation = glm::eulerAngles(toGlm(rotation));
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
    const PhysicsMaterialComponent* material) const {
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

    const Jolt::EMotionType motionType = rigidBody.isStatic || rigidBody.mode == RigidBodyComponent::PhysicsMode::Static
        ? Jolt::EMotionType::Static
        : (rigidBody.mode == RigidBodyComponent::PhysicsMode::Kinematic ? Jolt::EMotionType::Kinematic : Jolt::EMotionType::Dynamic);

    auto settings = std::make_unique<Jolt::BodyCreationSettings>(
        shape.release(),
        Jolt::RVec3(transform.translation.x, transform.translation.y, transform.translation.z),
        toJolt(glm::quat(transform.rotation)),
        motionType,
        motionType == Jolt::EMotionType::Static ? cNonMovingObjectLayer : cMovingObjectLayer);

    settings->mFriction = material ? material->friction : rigidBody.friction;
    settings->mRestitution = material ? material->restitution : rigidBody.restitution;
    settings->mLinearDamping = material ? material->damping : 0.0f;
    settings->mAngularDamping = material ? material->angularDamping : 0.0f;
    settings->mGravityFactor = rigidBody.useGravity ? 1.0f : 0.0f;
    settings->mIsSensor = collider.isTrigger;
    settings->mUserData = 0;

    return settings;
}

} // namespace engine
