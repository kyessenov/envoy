#pragma once

#include "envoy/local_info/local_info.h"

namespace LocalInfo {

class LocalInfoImpl : public LocalInfo {
public:
  const std::string& address() const override { return address_; }
  const std::string& zoneName() const override { return zone_name_; }
  const std::string& clusterName() const override { return cluster_name_; }
  const std::string& nodeName() const override { return node_name_; }

private:
  const std::string address_;
  const std::string zone_name_;
  const std::string cluster_name_;
  const std::string node_name_;
};

} // LocalInfo
