#include "extensions/access_loggers/istio/config.h"
#include "source/extensions/access_loggers/istio/context.pb.h"
#include "source/extensions/access_loggers/istio/filter.h"

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"

#include "common/common/assert.h"
#include "common/common/macros.h"
#include "common/protobuf/protobuf.h"

namespace istio {
namespace context {

::Envoy::AccessLog::InstanceSharedPtr
IstioAccessLogFactory::createAccessLogInstance(const google::protobuf::Message& config,
                                               ::Envoy::AccessLog::FilterPtr&&,
                                               ::Envoy::Server::Configuration::FactoryContext&) {
  const auto& proto_config = dynamic_cast<const FilterConfig&>(config);
  return std::make_shared<Filter>(proto_config);
}

std::unique_ptr<google::protobuf::Message> IstioAccessLogFactory::createEmptyConfigProto() {
  return std::make_unique<FilterConfig>();
}

std::string IstioAccessLogFactory::name() const { return "istio"; }

REGISTER_FACTORY(IstioAccessLogFactory, ::Envoy::Server::Configuration::AccessLogInstanceFactory);

} // namespace context
} // namespace istio
