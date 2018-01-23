#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "common/network/utility.h"
#include "common/upstream/load_balancer_impl.h"
#include "common/upstream/upstream_impl.h"

#include "test/common/upstream/utility.h"
#include "test/mocks/runtime/mocks.h"
#include "test/mocks/upstream/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::ElementsAre;
using testing::NiceMock;
using testing::Return;

namespace Envoy {
namespace Upstream {

class LoadBalancerTestBase : public ::testing::TestWithParam<bool> {
protected:
  // Run all tests aginst both priority 0 and priority 1 host sets, to ensure
  // all the load balancers have equivalent functonality for failover host sets.
  MockHostSet& hostSet() { return GetParam() ? host_set_ : failover_host_set_; }

  LoadBalancerTestBase() : stats_(ClusterInfoImpl::generateStats(stats_store_)) {}
  Stats::IsolatedStoreImpl stats_store_;
  ClusterStats stats_;
  NiceMock<Runtime::MockLoader> runtime_;
  NiceMock<Runtime::MockRandomGenerator> random_;
  NiceMock<MockPrioritySet> priority_set_;
  MockHostSet& host_set_ = *priority_set_.getMockHostSet(0);
  MockHostSet& failover_host_set_ = *priority_set_.getMockHostSet(1);
  std::shared_ptr<MockClusterInfo> info_{new NiceMock<MockClusterInfo>()};
};

class TestLb : public LoadBalancerBase {
public:
  TestLb(const PrioritySet& priority_set, ClusterStats& stats, Runtime::Loader& runtime,
         Runtime::RandomGenerator& random)
      : LoadBalancerBase(priority_set, stats, runtime, random) {}
  using LoadBalancerBase::chooseHostSet;
  using LoadBalancerBase::percentageLoad;
};

class LoadBalancerBaseTest : public LoadBalancerTestBase {
public:
  TestLb lb_{priority_set_, stats_, runtime_, random_};

  void updateHostSet(MockHostSet& host_set, uint32_t num_hosts, uint32_t num_healthy_hosts) {
    ASSERT(num_healthy_hosts <= num_hosts);

    host_set.hosts_.clear();
    host_set.healthy_hosts_.clear();
    for (uint32_t i = 0; i < num_hosts; ++i) {
      host_set.hosts_.push_back(makeTestHost(info_, "tcp://127.0.0.1:80"));
    }
    for (uint32_t i = 0; i < num_healthy_hosts; ++i) {
      host_set.healthy_hosts_.push_back(host_set.hosts_[i]);
    }
    host_set.runCallbacks({}, {});
  }

  std::vector<uint32_t> getLoadPercentage() {
    std::vector<uint32_t> ret;
    for (size_t i = 0; i < priority_set_.host_sets_.size(); ++i) {
      ret.push_back(lb_.percentageLoad(i));
    }
    return ret;
  }
};

INSTANTIATE_TEST_CASE_P(PrimaryOrFailover, LoadBalancerBaseTest, ::testing::Values(true));

// Basic test of host set selection.
TEST_P(LoadBalancerBaseTest, PrioritySelecton) {
  updateHostSet(host_set_, 1 /* num_hosts */, 0 /* num_healthy_hosts */);
  updateHostSet(failover_host_set_, 1, 0);

  // With both the primary and failover hosts unhealthy, we should select an
  // unhealthy primary host.
  EXPECT_EQ(100, lb_.percentageLoad(0));
  EXPECT_EQ(0, lb_.percentageLoad(1));
  EXPECT_EQ(&host_set_, &lb_.chooseHostSet());

  // Update the priority set with a new priority level P=2 and ensure the host
  // is chosen
  MockHostSet& tertiary_host_set_ = *priority_set_.getMockHostSet(2);
  updateHostSet(tertiary_host_set_, 1 /* num_hosts */, 1 /* num_healthy_hosts */);
  EXPECT_EQ(0, lb_.percentageLoad(0));
  EXPECT_EQ(0, lb_.percentageLoad(1));
  EXPECT_EQ(100, lb_.percentageLoad(2));
  EXPECT_EQ(&tertiary_host_set_, &lb_.chooseHostSet());

  // Now add a healthy host in P=0 and make sure it is immediately selected.
  updateHostSet(host_set_, 1 /* num_hosts */, 1 /* num_healthy_hosts */);
  host_set_.healthy_hosts_ = host_set_.hosts_;
  host_set_.runCallbacks({}, {});
  EXPECT_EQ(100, lb_.percentageLoad(0));
  EXPECT_EQ(0, lb_.percentageLoad(2));
  EXPECT_EQ(&host_set_, &lb_.chooseHostSet());

  // Remove the healthy host and ensure we fail back over to tertiary_host_set_
  updateHostSet(host_set_, 1 /* num_hosts */, 0 /* num_healthy_hosts */);
  EXPECT_EQ(0, lb_.percentageLoad(0));
  EXPECT_EQ(100, lb_.percentageLoad(2));
  EXPECT_EQ(&tertiary_host_set_, &lb_.chooseHostSet());
}

TEST_P(LoadBalancerBaseTest, GentleFailover) {
  // With 100% of P=0 hosts healthy, P=0 gets all the load.
  updateHostSet(host_set_, 1, 1);
  updateHostSet(failover_host_set_, 1, 1);
  ASSERT_THAT(getLoadPercentage(), ElementsAre(100, 0));

  // Health P=0 == 50*1.4 == 70
  updateHostSet(host_set_, 2 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(failover_host_set_, 2 /* num_hosts */, 1 /* num_healthy_hosts */);
  ASSERT_THAT(getLoadPercentage(), ElementsAre(70, 30));

  // Health P=0 == 25*1.4 == 35   P=1 is healthy so takes all spillover.
  updateHostSet(host_set_, 4 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(failover_host_set_, 2 /* num_hosts */, 2 /* num_healthy_hosts */);
  ASSERT_THAT(getLoadPercentage(), ElementsAre(35, 65));

  // Health P=0 == 25*1.4 == 35   P=1 == 35
  // Health is then scaled up by (100 / (35 + 35) == 50)
  updateHostSet(host_set_, 4 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(failover_host_set_, 4 /* num_hosts */, 1 /* num_healthy_hosts */);
  ASSERT_THAT(getLoadPercentage(), ElementsAre(50, 50));
}

TEST_P(LoadBalancerBaseTest, GentleFailoverWithExtraLevels) {
  // Add a third host set. Again with P=0 healthy, all traffic goes there.
  MockHostSet& tertiary_host_set_ = *priority_set_.getMockHostSet(2);
  updateHostSet(host_set_, 1, 1);
  updateHostSet(failover_host_set_, 1, 1);
  updateHostSet(tertiary_host_set_, 1, 1);
  ASSERT_THAT(getLoadPercentage(), ElementsAre(100, 0, 0));

  // Health P=0 == 50*1.4 == 70
  // Health P=0 == 50, so can take the 30% spillover.
  updateHostSet(host_set_, 2 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(failover_host_set_, 2 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(tertiary_host_set_, 2 /* num_hosts */, 1 /* num_healthy_hosts */);
  ASSERT_THAT(getLoadPercentage(), ElementsAre(70, 30, 0));

  // Health P=0 == 25*1.4 == 35   P=1 is healthy so takes all spillover.
  updateHostSet(host_set_, 4 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(failover_host_set_, 2 /* num_hosts */, 2 /* num_healthy_hosts */);
  updateHostSet(tertiary_host_set_, 2 /* num_hosts */, 1 /* num_healthy_hosts */);
  ASSERT_THAT(getLoadPercentage(), ElementsAre(35, 65, 0));

  // This is the first test where health (P=0 + P=1 < 100)
  // Health P=0 == 25*1.4 == 35   P=1 == 35  P=2 == 35
  updateHostSet(host_set_, 4 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(failover_host_set_, 4 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(tertiary_host_set_, 4 /* num_hosts */, 1 /* num_healthy_hosts */);
  ASSERT_THAT(getLoadPercentage(), ElementsAre(35, 35, 30));

  // This is the first test where (health P=0 + P=1 < 100)
  // Health P=0 == 25*1.4 == 35   P=1 == 35  P=2 == 35
  updateHostSet(host_set_, 4 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(failover_host_set_, 4 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(tertiary_host_set_, 4 /* num_hosts */, 1 /* num_healthy_hosts */);
  ASSERT_THAT(getLoadPercentage(), ElementsAre(35, 35, 30));

  // Now all health is (20% * 1.5 == 28). 28 * 3 < 100 so we have to scale.
  // Each Priority level gets 33% of the load, with P=0 picking up the rounding error.
  updateHostSet(host_set_, 5 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(failover_host_set_, 5 /* num_hosts */, 1 /* num_healthy_hosts */);
  updateHostSet(tertiary_host_set_, 5 /* num_hosts */, 1 /* num_healthy_hosts */);
  ASSERT_THAT(getLoadPercentage(), ElementsAre(34, 33, 33));
}

TEST_P(LoadBalancerBaseTest, BoundaryConditions) {
  TestRandomGenerator rand;
  uint32_t num_priorities = rand.random() % 10;

  for (uint32_t i = 0; i < num_priorities; ++i) {
    uint32_t num_hosts = rand.random() % 100;
    uint32_t healthy_hosts = std::min<uint32_t>(num_hosts, rand.random() % 100);
    // Make sure random health situations don't trigger the assert in recalculatePerPriorityState
    updateHostSet(*priority_set_.getMockHostSet(i), num_hosts, healthy_hosts);
  }
}

class RoundRobinLoadBalancerTest : public LoadBalancerTestBase {
public:
  void init(bool need_local_cluster) {
    if (need_local_cluster) {
      local_priority_set_.reset(new PrioritySetImpl());
      local_host_set_ = reinterpret_cast<HostSetImpl*>(&local_priority_set_->getOrCreateHostSet(0));
    }
    lb_.reset(new RoundRobinLoadBalancer(priority_set_, local_priority_set_.get(), stats_, runtime_,
                                         random_));
  }

  std::shared_ptr<PrioritySetImpl> local_priority_set_;
  HostSetImpl* local_host_set_{nullptr};
  std::shared_ptr<LoadBalancer> lb_;
  std::shared_ptr<const std::vector<std::vector<HostSharedPtr>>> empty_locality_;
  std::vector<HostSharedPtr> empty_host_vector_;
};

// For the tests which mutate primary and failover host sets explicitly, only
// run once.
typedef RoundRobinLoadBalancerTest FailoverTest;

// Ensure if all the hosts with priority 0 unhealthy, the next priority hosts are used.
TEST_P(FailoverTest, BasicFailover) {
  host_set_.hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80")};
  failover_host_set_.healthy_hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:82")};
  failover_host_set_.hosts_ = failover_host_set_.healthy_hosts_;
  init(false);
  EXPECT_EQ(failover_host_set_.healthy_hosts_[0], lb_->chooseHost(nullptr));
}

// Test that extending the priority set with an existing LB causes the correct updates.
TEST_P(FailoverTest, PriorityUpdatesWithLocalHostSet) {
  host_set_.hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80")};
  failover_host_set_.hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:81")};
  init(false);
  // With both the primary and failover hosts unhealthy, we should select an
  // unhealthy primary host.
  EXPECT_EQ(host_set_.hosts_[0], lb_->chooseHost(nullptr));

  // Update the priority set with a new priority level P=2 and ensure the host
  // is chosen
  MockHostSet& tertiary_host_set_ = *priority_set_.getMockHostSet(2);
  HostVectorSharedPtr hosts(
      new std::vector<HostSharedPtr>({makeTestHost(info_, "tcp://127.0.0.1:82")}));
  tertiary_host_set_.hosts_ = *hosts;
  tertiary_host_set_.healthy_hosts_ = tertiary_host_set_.hosts_;
  std::vector<HostSharedPtr> add_hosts;
  add_hosts.push_back(tertiary_host_set_.hosts_[0]);
  tertiary_host_set_.runCallbacks(add_hosts, {});
  EXPECT_EQ(tertiary_host_set_.hosts_[0], lb_->chooseHost(nullptr));

  // Now add a healthy host in P=0 and make sure it is immediately selected.
  host_set_.healthy_hosts_ = host_set_.hosts_;
  host_set_.runCallbacks(add_hosts, {});
  EXPECT_EQ(host_set_.hosts_[0], lb_->chooseHost(nullptr));

  // Remove the healthy host and ensure we fail back over to tertiary_host_set_
  host_set_.healthy_hosts_ = {};
  host_set_.runCallbacks({}, {});
  EXPECT_EQ(tertiary_host_set_.hosts_[0], lb_->chooseHost(nullptr));
}

// Test extending the priority set.
TEST_P(FailoverTest, ExtendPrioritiesUpdatingPrioritySet) {
  init(true);
  host_set_.hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80")};
  failover_host_set_.hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:81")};
  // With both the primary and failover hosts unhealthy, we should select an
  // unhealthy primary host.
  EXPECT_EQ(host_set_.hosts_[0], lb_->chooseHost(nullptr));

  // Update the priority set with a new priority level P=2
  // As it has healthy hosts, it should be selected.
  MockHostSet& tertiary_host_set_ = *priority_set_.getMockHostSet(2);
  HostVectorSharedPtr hosts(
      new std::vector<HostSharedPtr>({makeTestHost(info_, "tcp://127.0.0.1:82")}));
  tertiary_host_set_.hosts_ = *hosts;
  tertiary_host_set_.healthy_hosts_ = tertiary_host_set_.hosts_;
  std::vector<HostSharedPtr> add_hosts;
  add_hosts.push_back(tertiary_host_set_.hosts_[0]);
  tertiary_host_set_.runCallbacks(add_hosts, {});
  EXPECT_EQ(tertiary_host_set_.hosts_[0], lb_->chooseHost(nullptr));

  // Now add a healthy host in P=0 and make sure it is immediately selected.
  host_set_.healthy_hosts_ = host_set_.hosts_;
  host_set_.runCallbacks(add_hosts, {});
  EXPECT_EQ(host_set_.hosts_[0], lb_->chooseHost(nullptr));
}

TEST_P(FailoverTest, ExtendPrioritiesWithLocalPrioritySet) {
  init(true);
  host_set_.hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80")};
  failover_host_set_.hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:81")};
  // With both the primary and failover hosts unhealthy, we should select an
  // unhealthy primary host.
  EXPECT_EQ(host_set_.hosts_[0], lb_->chooseHost(nullptr));

  // Update the host set with a new priority level. We should start selecting
  // hosts from that level as it has viable hosts.
  MockHostSet& tertiary_host_set_ = *priority_set_.getMockHostSet(2);
  HostVectorSharedPtr hosts2(
      new std::vector<HostSharedPtr>({makeTestHost(info_, "tcp://127.0.0.1:84")}));
  tertiary_host_set_.hosts_ = *hosts2;
  tertiary_host_set_.healthy_hosts_ = tertiary_host_set_.hosts_;
  std::vector<HostSharedPtr> add_hosts;
  add_hosts.push_back(tertiary_host_set_.hosts_[0]);
  tertiary_host_set_.runCallbacks(add_hosts, {});
  EXPECT_EQ(tertiary_host_set_.hosts_[0], lb_->chooseHost(nullptr));

  // Update the local hosts. We're not doing locality based routing in this
  // test, but it should at least do no harm.
  HostVectorSharedPtr hosts(
      new std::vector<HostSharedPtr>({makeTestHost(info_, "tcp://127.0.0.1:82")}));
  local_priority_set_->getOrCreateHostSet(0).updateHosts(
      hosts, hosts, empty_locality_, empty_locality_, empty_host_vector_, empty_host_vector_);
  EXPECT_EQ(tertiary_host_set_.hosts_[0], lb_->chooseHost(nullptr));
}

INSTANTIATE_TEST_CASE_P(PrimaryOrFailover, FailoverTest, ::testing::Values(true));

TEST_P(RoundRobinLoadBalancerTest, NoHosts) {
  init(false);
  EXPECT_EQ(nullptr, lb_->chooseHost(nullptr));
}

TEST_P(RoundRobinLoadBalancerTest, SingleHost) {
  hostSet().healthy_hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80")};
  hostSet().hosts_ = hostSet().healthy_hosts_;
  init(false);
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_->chooseHost(nullptr));
}

TEST_P(RoundRobinLoadBalancerTest, Normal) {
  hostSet().healthy_hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80"),
                              makeTestHost(info_, "tcp://127.0.0.1:81")};
  hostSet().hosts_ = hostSet().healthy_hosts_;
  init(false);
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_->chooseHost(nullptr));
}

TEST_P(RoundRobinLoadBalancerTest, MaxUnhealthyPanic) {
  hostSet().healthy_hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80"),
                              makeTestHost(info_, "tcp://127.0.0.1:81")};
  hostSet().hosts_ = {
      makeTestHost(info_, "tcp://127.0.0.1:80"), makeTestHost(info_, "tcp://127.0.0.1:81"),
      makeTestHost(info_, "tcp://127.0.0.1:82"), makeTestHost(info_, "tcp://127.0.0.1:83"),
      makeTestHost(info_, "tcp://127.0.0.1:84"), makeTestHost(info_, "tcp://127.0.0.1:85")};

  init(false);
  EXPECT_EQ(hostSet().hosts_[0], lb_->chooseHost(nullptr));
  EXPECT_EQ(hostSet().hosts_[1], lb_->chooseHost(nullptr));
  EXPECT_EQ(hostSet().hosts_[2], lb_->chooseHost(nullptr));

  // Take the threshold back above the panic threshold.
  hostSet().healthy_hosts_ = {
      makeTestHost(info_, "tcp://127.0.0.1:80"), makeTestHost(info_, "tcp://127.0.0.1:81"),
      makeTestHost(info_, "tcp://127.0.0.1:82"), makeTestHost(info_, "tcp://127.0.0.1:83")};

  EXPECT_EQ(hostSet().healthy_hosts_[3], lb_->chooseHost(nullptr));
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_->chooseHost(nullptr));

  EXPECT_EQ(3UL, stats_.lb_healthy_panic_.value());
}

TEST_P(RoundRobinLoadBalancerTest, ZoneAwareSmallCluster) {
  HostVectorSharedPtr hosts(new std::vector<HostSharedPtr>(
      {makeTestHost(info_, "tcp://127.0.0.1:80"), makeTestHost(info_, "tcp://127.0.0.1:81"),
       makeTestHost(info_, "tcp://127.0.0.1:82")}));
  HostListsSharedPtr hosts_per_locality(
      new std::vector<std::vector<HostSharedPtr>>({{makeTestHost(info_, "tcp://127.0.0.1:81")},
                                                   {makeTestHost(info_, "tcp://127.0.0.1:80")},
                                                   {makeTestHost(info_, "tcp://127.0.0.1:82")}}));

  hostSet().hosts_ = *hosts;
  hostSet().healthy_hosts_ = *hosts;
  hostSet().healthy_hosts_per_locality_ = *hosts_per_locality;
  init(true);
  local_host_set_->updateHosts(hosts, hosts, hosts_per_locality, hosts_per_locality,
                               empty_host_vector_, empty_host_vector_);

  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.healthy_panic_threshold", 50))
      .WillRepeatedly(Return(50));
  EXPECT_CALL(runtime_.snapshot_, featureEnabled("upstream.zone_routing.enabled", 100))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.zone_routing.min_cluster_size", 6))
      .WillRepeatedly(Return(6));

  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_->chooseHost(nullptr));
  EXPECT_EQ(hostSet().healthy_hosts_[1], lb_->chooseHost(nullptr));
  EXPECT_EQ(hostSet().healthy_hosts_[2], lb_->chooseHost(nullptr));

  if (&hostSet() == &host_set_) {
    // Cluster size is computed once at zone aware struct regeneration point.
    EXPECT_EQ(1U, stats_.lb_zone_cluster_too_small_.value());
  } else {
    EXPECT_EQ(0U, stats_.lb_zone_cluster_too_small_.value());
    return;
  }
  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.zone_routing.min_cluster_size", 6))
      .WillRepeatedly(Return(1));
  // Trigger reload.
  local_host_set_->updateHosts(hosts, hosts, hosts_per_locality, hosts_per_locality,
                               empty_host_vector_, empty_host_vector_);
  EXPECT_EQ(hostSet().healthy_hosts_per_locality_[0][0], lb_->chooseHost(nullptr));
}

TEST_P(RoundRobinLoadBalancerTest, NoZoneAwareDifferentZoneSize) {
  if (&hostSet() == &failover_host_set_) { // P = 1 does not support zone-aware routing.
    return;
  }
  HostVectorSharedPtr hosts(new std::vector<HostSharedPtr>(
      {makeTestHost(info_, "tcp://127.0.0.1:80"), makeTestHost(info_, "tcp://127.0.0.1:81"),
       makeTestHost(info_, "tcp://127.0.0.1:82")}));
  HostListsSharedPtr upstream_hosts_per_locality(
      new std::vector<std::vector<HostSharedPtr>>({{makeTestHost(info_, "tcp://127.0.0.1:81")},
                                                   {makeTestHost(info_, "tcp://127.0.0.1:80")},
                                                   {makeTestHost(info_, "tcp://127.0.0.1:82")}}));
  HostListsSharedPtr local_hosts_per_locality(new std::vector<std::vector<HostSharedPtr>>(
      {{makeTestHost(info_, "tcp://127.0.0.1:81")}, {makeTestHost(info_, "tcp://127.0.0.1:80")}}));

  hostSet().healthy_hosts_ = *hosts;
  hostSet().hosts_ = *hosts;
  hostSet().healthy_hosts_per_locality_ = *upstream_hosts_per_locality;
  init(true);
  local_host_set_->updateHosts(hosts, hosts, local_hosts_per_locality, local_hosts_per_locality,
                               empty_host_vector_, empty_host_vector_);

  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.healthy_panic_threshold", 50))
      .WillRepeatedly(Return(50));
  EXPECT_CALL(runtime_.snapshot_, featureEnabled("upstream.zone_routing.enabled", 100))
      .WillRepeatedly(Return(true));

  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_->chooseHost(nullptr));
  EXPECT_EQ(1U, stats_.lb_zone_number_differs_.value());
}

TEST_P(RoundRobinLoadBalancerTest, ZoneAwareRoutingLargeZoneSwitchOnOff) {
  if (&hostSet() == &failover_host_set_) { // P = 1 does not support zone-aware routing.
    return;
  }
  HostVectorSharedPtr hosts(new std::vector<HostSharedPtr>(
      {makeTestHost(info_, "tcp://127.0.0.1:80"), makeTestHost(info_, "tcp://127.0.0.1:81"),
       makeTestHost(info_, "tcp://127.0.0.1:82")}));
  HostListsSharedPtr hosts_per_locality(
      new std::vector<std::vector<HostSharedPtr>>({{makeTestHost(info_, "tcp://127.0.0.1:81")},
                                                   {makeTestHost(info_, "tcp://127.0.0.1:80")},
                                                   {makeTestHost(info_, "tcp://127.0.0.1:82")}}));

  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.healthy_panic_threshold", 50))
      .WillRepeatedly(Return(50));
  EXPECT_CALL(runtime_.snapshot_, featureEnabled("upstream.zone_routing.enabled", 100))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.zone_routing.min_cluster_size", 6))
      .WillRepeatedly(Return(3));

  hostSet().healthy_hosts_ = *hosts;
  hostSet().hosts_ = *hosts;
  hostSet().healthy_hosts_per_locality_ = *hosts_per_locality;
  init(true);
  local_host_set_->updateHosts(hosts, hosts, hosts_per_locality, hosts_per_locality,
                               empty_host_vector_, empty_host_vector_);

  // There is only one host in the given zone for zone aware routing.
  EXPECT_EQ(hostSet().healthy_hosts_per_locality_[0][0], lb_->chooseHost(nullptr));
  EXPECT_EQ(1U, stats_.lb_zone_routing_all_directly_.value());
  EXPECT_EQ(hostSet().healthy_hosts_per_locality_[0][0], lb_->chooseHost(nullptr));
  EXPECT_EQ(2U, stats_.lb_zone_routing_all_directly_.value());

  // Disable runtime global zone routing.
  EXPECT_CALL(runtime_.snapshot_, featureEnabled("upstream.zone_routing.enabled", 100))
      .WillRepeatedly(Return(false));
  EXPECT_EQ(hostSet().healthy_hosts_[2], lb_->chooseHost(nullptr));
}

TEST_P(RoundRobinLoadBalancerTest, ZoneAwareRoutingSmallZone) {
  if (&hostSet() == &failover_host_set_) { // P = 1 does not support zone-aware routing.
    return;
  }
  HostVectorSharedPtr upstream_hosts(new std::vector<HostSharedPtr>(
      {makeTestHost(info_, "tcp://127.0.0.1:80"), makeTestHost(info_, "tcp://127.0.0.1:81"),
       makeTestHost(info_, "tcp://127.0.0.1:82"), makeTestHost(info_, "tcp://127.0.0.1:83"),
       makeTestHost(info_, "tcp://127.0.0.1:84")}));
  HostVectorSharedPtr local_hosts(new std::vector<HostSharedPtr>(
      {makeTestHost(info_, "tcp://127.0.0.1:0"), makeTestHost(info_, "tcp://127.0.0.1:1"),
       makeTestHost(info_, "tcp://127.0.0.1:2")}));

  HostListsSharedPtr upstream_hosts_per_locality(new std::vector<std::vector<HostSharedPtr>>(
      {{makeTestHost(info_, "tcp://127.0.0.1:81")},
       {makeTestHost(info_, "tcp://127.0.0.1:80"), makeTestHost(info_, "tcp://127.0.0.1:82")},
       {makeTestHost(info_, "tcp://127.0.0.1:83"), makeTestHost(info_, "tcp://127.0.0.1:84")}}));

  HostListsSharedPtr local_hosts_per_locality(
      new std::vector<std::vector<HostSharedPtr>>({{makeTestHost(info_, "tcp://127.0.0.1:0")},
                                                   {makeTestHost(info_, "tcp://127.0.0.1:1")},
                                                   {makeTestHost(info_, "tcp://127.0.0.1:2")}}));

  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.healthy_panic_threshold", 50))
      .WillRepeatedly(Return(50));
  EXPECT_CALL(runtime_.snapshot_, featureEnabled("upstream.zone_routing.enabled", 100))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.zone_routing.min_cluster_size", 6))
      .WillRepeatedly(Return(5));

  hostSet().healthy_hosts_ = *upstream_hosts;
  hostSet().hosts_ = *upstream_hosts;
  hostSet().healthy_hosts_per_locality_ = *upstream_hosts_per_locality;
  init(true);
  local_host_set_->updateHosts(local_hosts, local_hosts, local_hosts_per_locality,
                               local_hosts_per_locality, empty_host_vector_, empty_host_vector_);

  // There is only one host in the given zone for zone aware routing.
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(100));
  EXPECT_EQ(hostSet().healthy_hosts_per_locality_[0][0], lb_->chooseHost(nullptr));
  EXPECT_EQ(1U, stats_.lb_zone_routing_sampled_.value());

  // Force request out of small zone.
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(9999)).WillOnce(Return(2));
  EXPECT_EQ(hostSet().healthy_hosts_per_locality_[1][1], lb_->chooseHost(nullptr));
  EXPECT_EQ(1U, stats_.lb_zone_routing_cross_zone_.value());
}

TEST_P(RoundRobinLoadBalancerTest, LowPrecisionForDistribution) {
  if (&hostSet() == &failover_host_set_) { // P = 1 does not support zone-aware routing.
    return;
  }
  // upstream_hosts and local_hosts do not matter, zone aware routing is based on per zone hosts.
  HostVectorSharedPtr upstream_hosts(
      new std::vector<HostSharedPtr>({makeTestHost(info_, "tcp://127.0.0.1:80")}));
  hostSet().healthy_hosts_ = *upstream_hosts;
  hostSet().hosts_ = *upstream_hosts;
  HostVectorSharedPtr local_hosts(
      new std::vector<HostSharedPtr>({makeTestHost(info_, "tcp://127.0.0.1:0")}));

  HostListsSharedPtr upstream_hosts_per_locality(new std::vector<std::vector<HostSharedPtr>>());
  HostListsSharedPtr local_hosts_per_locality(new std::vector<std::vector<HostSharedPtr>>());

  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.healthy_panic_threshold", 50))
      .WillRepeatedly(Return(50));
  EXPECT_CALL(runtime_.snapshot_, featureEnabled("upstream.zone_routing.enabled", 100))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.zone_routing.min_cluster_size", 6))
      .WillRepeatedly(Return(1));

  // The following host distribution with current precision should lead to the no_capacity_left
  // situation.
  // Reuse the same host in all of the structures below to reduce time test takes and this does not
  // impact load balancing logic.
  HostSharedPtr host = makeTestHost(info_, "tcp://127.0.0.1:80");
  std::vector<HostSharedPtr> current(45000);

  for (int i = 0; i < 45000; ++i) {
    current[i] = host;
  }
  local_hosts_per_locality->push_back(current);

  current.resize(55000);
  for (int i = 0; i < 55000; ++i) {
    current[i] = host;
  }
  local_hosts_per_locality->push_back(current);

  current.resize(44999);
  for (int i = 0; i < 44999; ++i) {
    current[i] = host;
  }
  upstream_hosts_per_locality->push_back(current);

  current.resize(55001);
  for (int i = 0; i < 55001; ++i) {
    current[i] = host;
  }
  upstream_hosts_per_locality->push_back(current);

  hostSet().healthy_hosts_per_locality_ = *upstream_hosts_per_locality;
  init(true);

  // To trigger update callback.
  local_host_set_->updateHosts(local_hosts, local_hosts, local_hosts_per_locality,
                               local_hosts_per_locality, empty_host_vector_, empty_host_vector_);

  // Force request out of small zone and to randomly select zone.
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(9999)).WillOnce(Return(2));
  lb_->chooseHost(nullptr);
  EXPECT_EQ(1U, stats_.lb_zone_no_capacity_left_.value());
}

TEST_P(RoundRobinLoadBalancerTest, NoZoneAwareRoutingOneZone) {
  if (&hostSet() == &failover_host_set_) { // P = 1 does not support zone-aware routing.
    return;
  }
  HostVectorSharedPtr hosts(
      new std::vector<HostSharedPtr>({makeTestHost(info_, "tcp://127.0.0.1:80")}));
  HostListsSharedPtr hosts_per_locality(
      new std::vector<std::vector<HostSharedPtr>>({{makeTestHost(info_, "tcp://127.0.0.1:81")}}));

  hostSet().healthy_hosts_ = *hosts;
  hostSet().hosts_ = *hosts;
  hostSet().healthy_hosts_per_locality_ = *hosts_per_locality;
  init(true);
  local_host_set_->updateHosts(hosts, hosts, hosts_per_locality, hosts_per_locality,
                               empty_host_vector_, empty_host_vector_);
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_->chooseHost(nullptr));
}

TEST_P(RoundRobinLoadBalancerTest, NoZoneAwareRoutingNotHealthy) {
  HostVectorSharedPtr hosts(new std::vector<HostSharedPtr>(
      {makeTestHost(info_, "tcp://127.0.0.1:80"), makeTestHost(info_, "tcp://127.0.0.2:80")}));
  HostListsSharedPtr hosts_per_locality(new std::vector<std::vector<HostSharedPtr>>(
      {{},
       {makeTestHost(info_, "tcp://127.0.0.1:80"), makeTestHost(info_, "tcp://127.0.0.2:80")}}));

  hostSet().healthy_hosts_ = *hosts;
  hostSet().hosts_ = *hosts;
  hostSet().healthy_hosts_per_locality_ = *hosts_per_locality;
  init(true);
  local_host_set_->updateHosts(hosts, hosts, hosts_per_locality, hosts_per_locality,
                               empty_host_vector_, empty_host_vector_);

  // local zone has no healthy hosts, take from the all healthy hosts.
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_->chooseHost(nullptr));
  EXPECT_EQ(hostSet().healthy_hosts_[1], lb_->chooseHost(nullptr));
}

TEST_P(RoundRobinLoadBalancerTest, NoZoneAwareRoutingLocalEmpty) {
  if (&hostSet() == &failover_host_set_) { // P = 1 does not support zone-aware routing.
    return;
  }
  HostVectorSharedPtr upstream_hosts(new std::vector<HostSharedPtr>(
      {makeTestHost(info_, "tcp://127.0.0.1:80"), makeTestHost(info_, "tcp://127.0.0.1:81")}));
  HostVectorSharedPtr local_hosts(new std::vector<HostSharedPtr>({}, {}));

  HostListsSharedPtr upstream_hosts_per_locality(new std::vector<std::vector<HostSharedPtr>>(
      {{makeTestHost(info_, "tcp://127.0.0.1:80")}, {makeTestHost(info_, "tcp://127.0.0.1:81")}}));
  HostListsSharedPtr local_hosts_per_locality(
      new std::vector<std::vector<HostSharedPtr>>({{}, {}}));

  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.healthy_panic_threshold", 50))
      .WillOnce(Return(50))
      .WillOnce(Return(50));
  EXPECT_CALL(runtime_.snapshot_, featureEnabled("upstream.zone_routing.enabled", 100))
      .WillOnce(Return(true));
  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.zone_routing.min_cluster_size", 6))
      .WillOnce(Return(1));

  hostSet().healthy_hosts_ = *upstream_hosts;
  hostSet().hosts_ = *upstream_hosts;
  hostSet().healthy_hosts_per_locality_ = *upstream_hosts_per_locality;
  init(true);
  local_host_set_->updateHosts(local_hosts, local_hosts, local_hosts_per_locality,
                               local_hosts_per_locality, empty_host_vector_, empty_host_vector_);

  // Local cluster is not OK, we'll do regular routing.
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_->chooseHost(nullptr));
  EXPECT_EQ(0U, stats_.lb_healthy_panic_.value());
  EXPECT_EQ(1U, stats_.lb_local_cluster_not_ok_.value());
}

INSTANTIATE_TEST_CASE_P(PrimaryOrFailover, RoundRobinLoadBalancerTest,
                        ::testing::Values(true, false));

class LeastRequestLoadBalancerTest : public LoadBalancerTestBase {
public:
  LeastRequestLoadBalancer lb_{priority_set_, nullptr, stats_, runtime_, random_};
};

TEST_P(LeastRequestLoadBalancerTest, NoHosts) { EXPECT_EQ(nullptr, lb_.chooseHost(nullptr)); }

TEST_P(LeastRequestLoadBalancerTest, SingleHost) {
  hostSet().healthy_hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80")};
  hostSet().hosts_ = hostSet().healthy_hosts_;
  hostSet().runCallbacks({}, {}); // Trigger callbacks. The added/removed lists are not relevant.

  // Host weight is 1.
  {
    EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(2)).WillOnce(Return(3));
    stats_.max_host_weight_.set(1UL);
    EXPECT_EQ(hostSet().healthy_hosts_[0], lb_.chooseHost(nullptr));
  }

  // Host weight is 100.
  {
    EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(2));
    stats_.max_host_weight_.set(100UL);
    EXPECT_EQ(hostSet().healthy_hosts_[0], lb_.chooseHost(nullptr));
  }

  std::vector<HostSharedPtr> empty;
  {
    hostSet().runCallbacks(empty, empty);
    EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(2));
    EXPECT_EQ(hostSet().healthy_hosts_[0], lb_.chooseHost(nullptr));
  }

  {
    std::vector<HostSharedPtr> remove_hosts;
    remove_hosts.push_back(hostSet().hosts_[0]);
    hostSet().runCallbacks(empty, remove_hosts);
    EXPECT_CALL(random_, random()).WillOnce(Return(0));
    hostSet().healthy_hosts_.clear();
    hostSet().hosts_.clear();
    EXPECT_EQ(nullptr, lb_.chooseHost(nullptr));
  }
}

TEST_P(LeastRequestLoadBalancerTest, Normal) {
  hostSet().healthy_hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80"),
                              makeTestHost(info_, "tcp://127.0.0.1:81")};
  stats_.max_host_weight_.set(1UL);
  hostSet().hosts_ = hostSet().healthy_hosts_;
  hostSet().runCallbacks({}, {}); // Trigger callbacks. The added/removed lists are not relevant.
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(2)).WillOnce(Return(3));
  EXPECT_EQ(hostSet().healthy_hosts_[1], lb_.chooseHost(nullptr));

  hostSet().healthy_hosts_[0]->stats().rq_active_.set(1);
  hostSet().healthy_hosts_[1]->stats().rq_active_.set(2);
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(2)).WillOnce(Return(3));
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_.chooseHost(nullptr));

  hostSet().healthy_hosts_[0]->stats().rq_active_.set(2);
  hostSet().healthy_hosts_[1]->stats().rq_active_.set(1);
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(2)).WillOnce(Return(3));
  EXPECT_EQ(hostSet().healthy_hosts_[1], lb_.chooseHost(nullptr));
}

TEST_P(LeastRequestLoadBalancerTest, WeightImbalanceRuntimeOff) {
  // Disable weight balancing.
  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.weight_enabled", 1))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.healthy_panic_threshold", 50))
      .WillRepeatedly(Return(50));

  hostSet().healthy_hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80", 1),
                              makeTestHost(info_, "tcp://127.0.0.1:81", 3)};
  stats_.max_host_weight_.set(3UL);

  hostSet().hosts_ = hostSet().healthy_hosts_;
  hostSet().healthy_hosts_[0]->stats().rq_active_.set(1);
  hostSet().healthy_hosts_[1]->stats().rq_active_.set(2);
  hostSet().runCallbacks({}, {}); // Trigger callbacks. The added/removed lists are not relevant.

  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(0)).WillOnce(Return(1));
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_.chooseHost(nullptr));

  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(1)).WillOnce(Return(0));
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_.chooseHost(nullptr));
}

TEST_P(LeastRequestLoadBalancerTest, WeightImbalance) {
  hostSet().healthy_hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80", 1),
                              makeTestHost(info_, "tcp://127.0.0.1:81", 3)};
  stats_.max_host_weight_.set(3UL);

  hostSet().hosts_ = hostSet().healthy_hosts_;
  hostSet().runCallbacks({}, {}); // Trigger callbacks. The added/removed lists are not relevant.
  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.healthy_panic_threshold", 50))
      .WillRepeatedly(Return(50));

  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.weight_enabled", 1))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(runtime_.snapshot_, getInteger("upstream.healthy_panic_threshold", 50))
      .WillRepeatedly(Return(50));

  // As max weight higher then 1 we do random host pick and keep it for weight requests.
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(1));
  EXPECT_EQ(hostSet().healthy_hosts_[1], lb_.chooseHost(nullptr));

  // Same host stays as we have to hit it 3 times.
  hostSet().healthy_hosts_[0]->stats().rq_active_.set(2);
  hostSet().healthy_hosts_[1]->stats().rq_active_.set(1);
  EXPECT_CALL(random_, random()).Times(0);
  EXPECT_EQ(hostSet().healthy_hosts_[1], lb_.chooseHost(nullptr));

  // Same host stays as we have to hit it 3 times.
  EXPECT_CALL(random_, random()).Times(0);
  EXPECT_EQ(hostSet().healthy_hosts_[1], lb_.chooseHost(nullptr));

  // Get random host after previous one was selected 3 times in a row.
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(2));
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_.chooseHost(nullptr));

  // Select second host again.
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(1));
  EXPECT_EQ(hostSet().healthy_hosts_[1], lb_.chooseHost(nullptr));

  // Set weight to 1, we will switch to the two random hosts mode.
  stats_.max_host_weight_.set(1UL);
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(2)).WillOnce(Return(3));
  EXPECT_EQ(hostSet().healthy_hosts_[1], lb_.chooseHost(nullptr));

  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(2)).WillOnce(Return(2));
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_.chooseHost(nullptr));
}

TEST_P(LeastRequestLoadBalancerTest, WeightImbalanceCallbacks) {
  hostSet().healthy_hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80", 1),
                              makeTestHost(info_, "tcp://127.0.0.1:81", 3)};
  stats_.max_host_weight_.set(3UL);

  hostSet().hosts_ = hostSet().healthy_hosts_;
  hostSet().runCallbacks({}, {}); // Trigger callbacks. The added/removed lists are not relevant.

  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(1));
  EXPECT_EQ(hostSet().healthy_hosts_[1], lb_.chooseHost(nullptr));

  // Same host stays as we have to hit it 3 times, but we remove it and fire callback.
  std::vector<HostSharedPtr> empty;
  std::vector<HostSharedPtr> hosts_removed;
  hosts_removed.push_back(hostSet().hosts_[1]);
  hostSet().hosts_.erase(hostSet().hosts_.begin() + 1);
  hostSet().healthy_hosts_.erase(hostSet().healthy_hosts_.begin() + 1);
  hostSet().runCallbacks(empty, hosts_removed);

  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(1));
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_.chooseHost(nullptr));
}

INSTANTIATE_TEST_CASE_P(PrimaryOrFailover, LeastRequestLoadBalancerTest,
                        ::testing::Values(true, false));

class RandomLoadBalancerTest : public LoadBalancerTestBase {
public:
  RandomLoadBalancer lb_{priority_set_, nullptr, stats_, runtime_, random_};
};

TEST_P(RandomLoadBalancerTest, NoHosts) { EXPECT_EQ(nullptr, lb_.chooseHost(nullptr)); }

TEST_P(RandomLoadBalancerTest, Normal) {
  hostSet().healthy_hosts_ = {makeTestHost(info_, "tcp://127.0.0.1:80"),
                              makeTestHost(info_, "tcp://127.0.0.1:81")};
  hostSet().hosts_ = hostSet().healthy_hosts_;
  hostSet().runCallbacks({}, {}); // Trigger callbacks. The added/removed lists are not relevant.
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(2));
  EXPECT_EQ(hostSet().healthy_hosts_[0], lb_.chooseHost(nullptr));
  EXPECT_CALL(random_, random()).WillOnce(Return(0)).WillOnce(Return(3));
  EXPECT_EQ(hostSet().healthy_hosts_[1], lb_.chooseHost(nullptr));
}

INSTANTIATE_TEST_CASE_P(PrimaryOrFailover, RandomLoadBalancerTest, ::testing::Values(true, false));

TEST(LoadBalancerSubsetInfoImplTest, DefaultConfigIsDiabled) {
  auto subset_info =
      LoadBalancerSubsetInfoImpl(envoy::api::v2::cluster::Cluster::LbSubsetConfig::default_instance());

  EXPECT_FALSE(subset_info.isEnabled());
  EXPECT_TRUE(subset_info.fallbackPolicy() == envoy::api::v2::cluster::Cluster::LbSubsetConfig::NO_FALLBACK);
  EXPECT_EQ(subset_info.defaultSubset().fields_size(), 0);
  EXPECT_EQ(subset_info.subsetKeys().size(), 0);
}

TEST(LoadBalancerSubsetInfoImplTest, SubsetConfig) {
  auto subset_value = ProtobufWkt::Value();
  subset_value.set_string_value("the value");

  auto subset_config = envoy::api::v2::cluster::Cluster::LbSubsetConfig::default_instance();
  subset_config.set_fallback_policy(envoy::api::v2::cluster::Cluster::LbSubsetConfig::DEFAULT_SUBSET);
  subset_config.mutable_default_subset()->mutable_fields()->insert({"key", subset_value});
  auto subset_selector = subset_config.mutable_subset_selectors()->Add();
  subset_selector->add_keys("selector_key");

  auto subset_info = LoadBalancerSubsetInfoImpl(subset_config);

  EXPECT_TRUE(subset_info.isEnabled());
  EXPECT_TRUE(subset_info.fallbackPolicy() ==
              envoy::api::v2::cluster::Cluster::LbSubsetConfig::DEFAULT_SUBSET);
  EXPECT_EQ(subset_info.defaultSubset().fields_size(), 1);
  EXPECT_EQ(subset_info.defaultSubset().fields().at("key").string_value(),
            std::string("the value"));
  EXPECT_EQ(subset_info.subsetKeys().size(), 1);
  EXPECT_EQ(subset_info.subsetKeys()[0], std::set<std::string>({"selector_key"}));
}

} // namespace Upstream
} // namespace Envoy
