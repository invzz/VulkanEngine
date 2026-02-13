lass HZBPass : public IRenderPass {
 public:
  HZBPass(Renderer & renderer) : renderer(renderer) {}

  const char* getName() const override {
    return "HZB";
  }

  void execute(FrameInfo & frameInfo) override {
    renderer.generateDepthPyramid(frameInfo.commandBuffer);
  }

 private:
  Renderer & renderer;
};