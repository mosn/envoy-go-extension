#include <chrono>

#include "source/common/router/config_impl.h"
#include "src/envoy/http/cluster/golang_cluster.h"

namespace Envoy {
namespace Router {
namespace Golang {

ClusterConfig::ClusterConfig(const GolangClusterProto& config)
    : so_id_(config.so_id()), default_cluster_(config.default_cluster()), config_(config.config()) {
}

Dso::DsoInstance* ClusterConfig::getDsoLib() {
  if (dynamicLib_ != nullptr) {
    return dynamicLib_;
  }
  auto dynamicLib_ = Dso::DsoInstanceManager::getDsoInstanceByID(so_id_);
  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "golang dynamicLib is nullPtr.");
  }
  return dynamicLib_;
}

uint64_t ClusterConfig::getConfigId() {
  if (config_id_ != 0) {
    return config_id_;
  }

  auto dlib = getDsoLib();
  if (dlib == nullptr) {
    return 0;
  }

  std::string str;
  if (!config_.SerializeToString(&str)) {
    ENVOY_LOG(error, "failed to serialize any pb to string");
    return 0;
  }
  auto ptr = reinterpret_cast<unsigned long long>(str.data());
  auto len = str.length();
  config_id_ = dlib->moeNewClusterConfig(ptr, len);
  if (config_id_ == 0) {
    ENVOY_LOG(error, "invalid golang plugin config");
  }
  return config_id_;
}

RouteConstSharedPtr
GolangClusterSpecifierPlugin::route(const RouteEntry& parent,
                                    const Http::RequestHeaderMap& headers) const {
  ASSERT(dynamic_cast<const RouteEntryImplBase*>(&parent) != nullptr);
  int buffer_len = 256;
  std::string buffer;
  std::string cluster;
  auto dlib = config_->getDsoLib();

again:
  buffer.reserve(buffer_len);
  auto config_id = config_->getConfigId();
  auto header_ptr = reinterpret_cast<uint64_t>(&headers);
  auto buffer_ptr = reinterpret_cast<uint64_t>(buffer.data());
  auto new_len = dlib != nullptr
                     ? dlib->moeOnClusterSpecify(header_ptr, config_id, buffer_ptr, buffer_len)
                     : 0;
  if (new_len == 0) {
    ENVOY_LOG(debug, "golang choose the default cluster");
    cluster = config_->defaultCluster();
  } else if (new_len < 0) {
    ENVOY_LOG(error, "error happened while golang choose cluster, using the default cluster");
    cluster = config_->defaultCluster();
  } else if (new_len <= buffer_len) {
    ENVOY_LOG(debug, "buffer size fit the cluster name from golang");
    cluster = std::string(buffer.data(), new_len);
  } else {
    ENVOY_LOG(debug, "need larger size of buffer to save the cluster name in golang, try again");
    buffer_len = new_len;
    goto again;
  }

  // TODO: add cache for it?
  return std::make_shared<RouteEntryImplBase::DynamicRouteEntry>(
      dynamic_cast<const RouteEntryImplBase*>(&parent), cluster);
}

} // namespace Golang
} // namespace Router
} // namespace Envoy
