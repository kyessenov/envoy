#include "source/extensions/matching/common_inputs/format_string/config.h"

#include <memory>

#include "envoy/http/filter.h"
#include "envoy/matcher/matcher.h"
#include "envoy/network/filter.h"

namespace Envoy {
namespace Extensions {
namespace Matching {
namespace CommonInputs {
namespace FormatString {

class TcpInputFactory : public BaseFactory<Network::MatchingData> {};
class HttpInputFactory : public BaseFactory<Http::HttpMatchingData> {};

REGISTER_FACTORY(TcpInputFactory, Matcher::DataInputFactory<Network::MatchingData>);
REGISTER_FACTORY(HttpInputFactory, Matcher::DataInputFactory<Http::HttpMatchingData>);

} // namespace FormatString
} // namespace CommonInputs
} // namespace Matching
} // namespace Extensions
} // namespace Envoy
