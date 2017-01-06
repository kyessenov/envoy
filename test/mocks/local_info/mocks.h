#pragma once

namespace LocalInfo {

class MockLocalInfo : public LocalInfo {
public:
  MockLocalInfo();
  ~MockLocalInfo();

  MOCK_CONST_METHOD0(address, std::string&());
  MOCK_CONST_METHOD0(zoneName, std::string&());
  MOCK_CONST_METHOD0(clusterName, std::string&());
  MOCK_CONST_METHOD0(nodeName, std::string&());
};

} // LocalInfo
