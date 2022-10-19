#include <string>

#include "src/envoy/common/dso/dso.h"

#include "envoy/registry/registry.h"

#include "test/test_common/utility.h"
#include "test/test_common/environment.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;

namespace Envoy {
namespace Dso {
namespace {

std::string genSoPath(std::string name) {
  // TODO: should work without the "_go_extension" suffix
  return TestEnvironment::substitute("{{ test_rundir }}_go_extension/test/common/dso/test_data/" +
                                     name);
}

TEST(DsoInstanceTest, SimpleAPI) {
  auto path = genSoPath("simple.so");
  DsoInstance* dso = new DsoInstance(path);
  EXPECT_EQ(dso->moeNewHttpPluginConfig(0, 0), 100);
  delete dso;
}

TEST(DsoInstanceManagerTest, Pub) {
  auto id = "simple.so";
  auto path = genSoPath(id);

  // get before pub
  auto dso = DsoInstanceManager::getDsoInstanceByID(id);
  EXPECT_EQ(dso, nullptr);

  // first time pub
  auto res = DsoInstanceManager::pub(id, path);
  EXPECT_EQ(res, true);

  // get after pub
  dso = DsoInstanceManager::getDsoInstanceByID(id);
  EXPECT_NE(dso, nullptr);

  // second time pub
  res = DsoInstanceManager::pub(id, path);
  EXPECT_EQ(res, false);
}

} // namespace
} // namespace Dso
} // namespace Envoy
