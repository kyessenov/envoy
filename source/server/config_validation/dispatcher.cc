#include "source/server/config_validation/dispatcher.h"

#include "source/common/common/assert.h"
#include "source/server/config_validation/connection.h"

namespace Envoy {
namespace Event {

Network::ClientConnectionPtr ValidationDispatcher::createClientConnection(
    Network::Address::InstanceConstSharedPtr remote_address,
    Network::Address::InstanceConstSharedPtr source_address,
    Network::TransportSocketPtr&& transport_socket,
    const Network::ConnectionSocket::OptionsSharedPtr& options) {
  return std::make_unique<Network::ConfigValidateConnection>(*this, remote_address, source_address,
                                                             std::move(transport_socket), options);
}

Network::ListenerPtr ValidationDispatcher::createListener(Network::SocketSharedPtr&&,
                                                          Network::TcpListenerCallbacks&,
                                                          Runtime::Loader&, bool, bool) {
  return nullptr;
}

} // namespace Event
} // namespace Envoy
