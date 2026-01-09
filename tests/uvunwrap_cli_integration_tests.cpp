#include <gtest/gtest.h>

#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

TEST(UVUnwrapCLI, GeneratesManifestFromInput)
{
  namespace fs = std::filesystem;
  fs::create_directories("assets/scenes");

  nlohmann::json in;
  in["meshes"] = nlohmann::json::array();
  nlohmann::json m;
  m["id"]        = "mesh0";
  m["positions"] = {0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
  m["indices"]   = {0, 1, 2};
  in["meshes"].push_back(m);

  in["instances"] = nlohmann::json::array();
  nlohmann::json inst;
  inst["id"]        = "object_01";
  inst["mesh"]      = "mesh0";
  inst["transform"] = {{"translation", {0.0, 0.0, 0.0}}, {"rotation", {0.0, 0.0, 0.0, 1.0}}, {"scale", {1.0, 1.0, 1.0}}};
  in["instances"].push_back(inst);

  std::ofstream out("assets/scenes/uv_unwrap_input.json");
  out << in.dump(2) << std::endl;
  out.close();

  // Run the CLI binary (use build-time path when available)
#ifndef UVUNWRAP_CLI_PATH
#define UVUNWRAP_CLI_PATH "./tools/UVUnwrapCLI"
#endif
  std::string cliExec = UVUNWRAP_CLI_PATH;
  // Trim leading whitespace (some build flags inject a leading space)
  while (!cliExec.empty() && std::isspace(static_cast<unsigned char>(cliExec.front())))
    cliExec.erase(0, 1);
  // Ensure the CLI binary exists before invoking it (gives clearer test failures)
  namespace fs = std::filesystem;
  ASSERT_TRUE(fs::exists(cliExec)) << "UVUnwrapCLI not found at: '" << cliExec << "'";
  std::string cliCmd = cliExec + " assets/scenes/uv_unwrap_input.json assets/scenes/uv_unwrap_output.json > /dev/null 2>&1";
  int         ret    = system(cliCmd.c_str());
  ASSERT_EQ(ret, 0) << "UVUnwrapCLI failed (exit " << ret << "), binary: " << cliExec;

  std::ifstream got("assets/scenes/uv_unwrap_output.json");
  ASSERT_TRUE(got.good());
  nlohmann::json lm;
  got >> lm;
  ASSERT_TRUE(lm.contains("lightmapBindings"));
  ASSERT_TRUE(lm["lightmapBindings"].contains("object_01"));
}
