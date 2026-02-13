#ifndef SYSTEMREGISTRY_HPP
#define SYSTEMREGISTRY_HPP

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/GridRenderSystem.hpp"
#include "Engine/Systems/InputSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/ObjectSelectionSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "RenderContext.hpp"
#include "app.hpp"

namespace engine {

class SystemRegistry {
 public:
  SystemRegistry(Device&, Renderer&, RenderContext&, ResourceManager&);

  void initialize();
  GameLoopState buildGameLoopState();

 private:
  std::unique_ptr<ObjectSelectionSystem> objectSelection;
  std::unique_ptr<InputSystem> input;
  std::unique_ptr<CameraSystem> camera;
  std::unique_ptr<AnimationSystem> animation;
  std::unique_ptr<LODSystem> lod;
  std::unique_ptr<ModelRenderSystem> model;
  std::unique_ptr<LightSystem> light;
  std::unique_ptr<ShadowSystem> shadow;
  std::unique_ptr<SkyboxRenderSystem> skybox;
  std::unique_ptr<GridRenderSystem> grid;
  std::unique_ptr<DustRenderSystem> dust;
  std::unique_ptr<PostProcessingSystem> post;
};

}  // namespace engine
#endif  // SYSTEMREGISTRY_HPP