#include "source/extensions/transport_sockets/internal/internal.h"

#include "envoy/buffer/buffer.h"
#include "envoy/network/connection.h"
#include "envoy/upstream/upstream.h"

#include "source/common/common/assert.h"
#include "source/common/protobuf/utility.h"
#include "source/extensions/io_socket/user_space/io_handle.h"

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Internal {

InternalSocket::InternalSocket(
    Network::TransportSocketPtr inner_socket,
    std::unique_ptr<envoy::config::core::v3::Metadata> metadata,
    std::unique_ptr<IoSocket::UserSpace::FilterStateObjects> filter_state_objects)
    : PassthroughSocket(std::move(inner_socket)), metadata_(std::move(metadata)),
      filter_state_objects_(std::move(filter_state_objects)) {}

void InternalSocket::setTransportSocketCallbacks(Network::TransportSocketCallbacks& callbacks) {
  transport_socket_->setTransportSocketCallbacks(callbacks);
  auto* io_handle = dynamic_cast<IoSocket::UserSpace::IoHandle*>(&callbacks.ioHandle());
  if (io_handle != nullptr && io_handle->passthroughState()) {
    io_handle->passthroughState()->initialize(std::move(metadata_),
                                              std::move(filter_state_objects_));
    metadata_ = nullptr;
    filter_state_objects_ = nullptr;
  }
}

} // namespace Internal
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
