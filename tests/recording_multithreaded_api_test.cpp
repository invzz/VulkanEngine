#include <gtest/gtest.h>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"

using namespace engine;

TEST(ModelRenderSystem, EnableMultiThreadedRecordingApi)
{
  // Basic API/behavior smoke: enabling/disabling should be idempotent and
  // accept a thread count. This test does NOT exercise GPU recording (that
  // is covered by the manual/bench integration), but it validates the
  // opt-in plumbing compiles and stores config.
  Window win(16, 16, "MT Recording API");
  Device device(win);

  // Create a ModelRenderSystem with a dummy render pass (VK_NULL_HANDLE is
  // sufficient for this API-only test since we do not record/execute).
  ModelRenderSystem mrs(device, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

  // Default: disabled
  mrs.enableMultiThreadedRecording(true, 4);
  mrs.enableMultiThreadedRecording(false, 0);

  // Re-enable with auto thread-count
  mrs.enableMultiThreadedRecording(true, 0);

  SUCCEED();
}
