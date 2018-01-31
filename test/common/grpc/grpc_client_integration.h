#include "common/common/assert.h"

#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Grpc {

// Support paramaterizing over gRPC client type.
enum class ClientType { EnvoyGrpc, GoogleGrpc };

class GrpcClientIntegrationParamTest
    : public testing::TestWithParam<std::tuple<Network::Address::IpVersion, ClientType>> {
public:
  Network::Address::IpVersion ipVersion() const { return std::get<0>(GetParam()); }
  ClientType clientType() const { return std::get<1>(GetParam()); }

  void setGrpcService(envoy::api::v2::core::GrpcService& grpc_service, const std::string& cluster_name,
                      Network::Address::InstanceConstSharedPtr address) {
    switch (clientType()) {
    case ClientType::EnvoyGrpc:
      grpc_service.mutable_envoy_grpc()->set_cluster_name(cluster_name);
      break;
    case ClientType::GoogleGrpc: {
      auto* google_grpc = grpc_service.mutable_google_grpc();
      google_grpc->set_target_uri(address->asString());
      google_grpc->set_stat_prefix(cluster_name);
      break;
    }
    default:
      NOT_REACHED;
    }
  }
};

// Skip tests based on gRPC client type.
#define SKIP_IF_GRPC_CLIENT(client_type)                                                           \
  if (clientType() == (client_type)) {                                                             \
    return;                                                                                        \
  }

#ifdef ENVOY_GOOGLE_GRPC
#define GRPC_CLIENT_INTEGRATION_PARAMS                                                             \
  testing::Combine(testing::ValuesIn(TestEnvironment::getIpVersionsForTest()),                     \
                   testing::Values(Grpc::ClientType::EnvoyGrpc, Grpc::ClientType::GoogleGrpc))
#else
#define GRPC_CLIENT_INTEGRATION_PARAMS                                                             \
  testing::Combine(testing::ValuesIn(TestEnvironment::getIpVersionsForTest()),                     \
                   testing::Values(Grpc::ClientType::EnvoyGrpc))
#endif

} // namespace Grpc
} // namespace Envoy
