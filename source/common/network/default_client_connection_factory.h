#pragma once

#include "envoy/common/pure.h"
#include "envoy/network/client_connection_factory.h"
#include "envoy/network/connection.h"

namespace Envoy {

namespace Network {

/**
 * This client connection factory handles the connection if the remote address type is either ip or
 * pipe.
 */
class DefaultClientConnectionFactory : public ClientConnectionFactory {
public:
  ~DefaultClientConnectionFactory() override = default;

  // Config::UntypedFactory
  std::string name() const override { return "default"; }

  // Network::ClientConnectionFactory
  Network::ClientConnectionPtr createClientConnection(
      Event::Dispatcher& dispatcher, Network::Address::InstanceConstSharedPtr address,
      Network::Address::InstanceConstSharedPtr source_address,
      Network::TransportSocketPtr&& transport_socket,
      const Network::ConnectionSocket::OptionsSharedPtr& options,
      Network::TransportSocketOptionsConstSharedPtr transport_options = nullptr) override;
};

} // namespace Network
} // namespace Envoy
