#pragma once

#include "envoy/common/hashable.h"
#include "envoy/network/filter.h"
#include "envoy/stream_info/filter_state.h"

#include "source/common/common/logger.h"
#include "source/common/singleton/const_singleton.h"

namespace Envoy {
namespace Extensions {
namespace ListenerFilters {
namespace OriginalDst {

class FilterNameValues {
public:
  const std::string Name = "envoy.filters.listener.original_dst";
  const std::string LocalField = "local";
  const std::string LocalFilterStateKey = "envoy.filters.listener.original_dst.local_ip";
  const std::string RemoteFilterStateKey = "envoy.filters.listener.original_dst.remote_ip";
};

using FilterNames = ConstSingleton<FilterNameValues>;

class AddressObject : public StreamInfo::FilterState::Object, public Hashable {
public:
  AddressObject(const Network::Address::InstanceConstSharedPtr& address) : address_(address) {}
  absl::optional<std::string> serializeAsString() const override { return address_->asString(); }
  // Implements hashing interface because the value is applied once per the internal connection.
  // Multiple streams sharing the internal upstream connection must have the same address object.
  absl::optional<uint64_t> hash() const override;
  const Network::Address::InstanceConstSharedPtr& address() const { return address_; }

private:
  const Network::Address::InstanceConstSharedPtr address_;
};

/**
 * Implementation of an original destination listener filter.
 */
class OriginalDstFilter : public Network::ListenerFilter, Logger::Loggable<Logger::Id::filter> {
public:
  OriginalDstFilter(const envoy::config::core::v3::TrafficDirection& traffic_direction)
      : traffic_direction_(traffic_direction) {
    // [[maybe_unused]] attribute is not supported on GCC for class members. We trivially use the
    // parameter here to silence the warning.
    (void)traffic_direction_;
  }

  virtual Network::Address::InstanceConstSharedPtr getOriginalDst(Network::Socket& sock);

  // Network::ListenerFilter
  Network::FilterStatus onAccept(Network::ListenerFilterCallbacks& cb) override;

  size_t maxReadBytes() const override { return 0; }

  Network::FilterStatus onData(Network::ListenerFilterBuffer&) override {
    return Network::FilterStatus::Continue;
  };

private:
  envoy::config::core::v3::TrafficDirection traffic_direction_;
};

} // namespace OriginalDst
} // namespace ListenerFilters
} // namespace Extensions
} // namespace Envoy
