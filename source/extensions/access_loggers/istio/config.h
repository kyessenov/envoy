#pragma once

#include <string>

#include "envoy/server/access_log_config.h"

namespace istio {
namespace context {

class IstioAccessLogFactory : public ::Envoy::Server::Configuration::AccessLogInstanceFactory {
public:
  ::Envoy::AccessLog::InstanceSharedPtr
  createAccessLogInstance(const google::protobuf::Message& config,
                          ::Envoy::AccessLog::FilterPtr&& filter,
                          ::Envoy::Server::Configuration::FactoryContext& context) override;

  std::unique_ptr<google::protobuf::Message> createEmptyConfigProto() override;

  std::string name() const override;
};

} // namespace context
} // namespace istio
