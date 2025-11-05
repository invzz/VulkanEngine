#pragma once

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

#include "3dEngine/Device.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/Model.hpp"
#include "3dEngine/Renderer.hpp"
#include "3dEngine/SwapChain.hpp"
#include "3dEngine/Window.hpp"

namespace engine {

  class App
  {
  public:
    static int width() { return 800; }
    static int height() { return 600; }

    App();
    ~App();

    // delete copy operations
    App(const App&)            = delete;
    App& operator=(const App&) = delete;

    void run();

  private:
    void                    loadGameObjects();
    Window                  window{width(), height(), "Engine App"};
    Device                  device{window};
    std::vector<GameObject> gameObjects;
    Renderer                renderer{window, device};
  };
} // namespace engine