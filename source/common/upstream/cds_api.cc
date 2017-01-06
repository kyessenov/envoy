#include "cds_api.h"

#include "common/common/assert.h"

namespace Upstream {

CdsApiPtr CdsApi::create(const Json::Object& config, ClusterManager& cm,
                         Event::Dispatcher& dispatcher, const std::string& local_service_cluster,
                         const std::string& local_service_node) {
  if (!config.hasObject("cds")) {
    return nullptr;
  }

  return CdsApiPtr{new CdsApi(config, cm, dispatcher, local_service_cluster, local_service_node)};
}

CdsApi::CdsApi(const Json::Object& config, ClusterManager& cm, Event::Dispatcher& dispatcher,
               const std::string& local_service_cluster, const std::string& local_service_node)
    : cm_(cm), remote_cluster_name_(config.getString("cluster_name")),
      refresh_interval_(config.getInteger("refresh_interval_ms", 30000)),
      local_service_cluster_(local_service_cluster), local_service_node_(local_service_node),
      refresh_timer_(dispatcher.createTimer([this]() -> void {})) {
  if (!cm.get(remote_cluster_name_)) {
    ASSERT(false); // fixfix
  }
}

} // Upstream
