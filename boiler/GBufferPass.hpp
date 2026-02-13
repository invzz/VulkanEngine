class GBufferPass : public IRenderPass {
 public:
  GBufferPass(SystemRegistry& systems, Renderer& renderer) : systems(systems), renderer(renderer) {}

  const char* getName() const override {
    return "GBuffer";
  }

  void execute(FrameInfo& frameInfo) override {
    systems.model().beginFrame(frameInfo.frameIndex);

    renderer.beginGbufferRenderPass(frameInfo.commandBuffer, systems.renderSettings().multithreaded);

    systems.model().renderGbuffer(frameInfo);

    renderer.endOffscreenRenderPass(frameInfo.commandBuffer);
  }

 private:
  SystemRegistry& systems;
  Renderer& renderer;
};
