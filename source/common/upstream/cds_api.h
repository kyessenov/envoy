#pragma once

#include "envoy/event/dispatcher.h"
#include "envoy/json/json_object.h"
#include "envoy/upstream/cluster_manager.h"

namespace Upstream {

class CdsApi;
typedef std::unique_ptr<CdsApi> CdsApiPtr;

/**
 * fixfix
 */
class CdsApi {
public:
  static CdsApiPtr create(const Json::Object& config, ClusterManager& cm,
                          Event::Dispatcher& dispatcher, const std::string& local_service_cluster,
                          const std::string& local_service_node);

private:
  CdsApi(const Json::Object& config, ClusterManager& cm, Event::Dispatcher& dispatcher,
         const std::string& local_service_cluster, const std::string& local_service_node);

  ClusterManager& cm_;
  const std::string remote_cluster_name_;
  const std::chrono::milliseconds refresh_interval_;
  const std::string local_service_cluster_;
  const std::string local_service_node_;
  Event::TimerPtr refresh_timer_;
};

} // Upstream
