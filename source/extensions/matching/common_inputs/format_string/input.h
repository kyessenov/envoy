#pragma once

#include "envoy/formatter/substitution_formatter.h"
#include "envoy/http/filter.h"
#include "envoy/matcher/matcher.h"
#include "envoy/network/filter.h"

#include "source/common/http/header_map_impl.h"

namespace Envoy {
namespace Extensions {
namespace Matching {
namespace CommonInputs {
namespace FormatString {

class Input : public Matcher::DataInput<Network::MatchingData>,
              public Matcher::DataInput<Http::HttpMatchingData> {
public:
  explicit Input(const Formatter::FormatterConstSharedPtr& formatter) : formatter_(formatter) {}

  Matcher::DataInputGetResult get(const Network::MatchingData& data) const override {
    return {Matcher::DataInputGetResult::DataAvailability::AllDataAvailable,
            formatter_->format(*Http::StaticEmptyHeaders::get().request_headers,
                               *Http::StaticEmptyHeaders::get().response_headers,
                               *Http::StaticEmptyHeaders::get().response_trailers,
                               data.streamInfo(), "")};
  }
  Matcher::DataInputGetResult get(const Http::HttpMatchingData& data) const override {
    const StreamInfo::StreamInfo& info = data.streamInfo();
    const auto* request_headers = info.getRequestHeaders();
    return {Matcher::DataInputGetResult::DataAvailability::AllDataAvailable,
            formatter_->format(request_headers ? *request_headers
                                               : *Http::StaticEmptyHeaders::get().request_headers,
                               *Http::StaticEmptyHeaders::get().response_headers,
                               *Http::StaticEmptyHeaders::get().response_trailers, info, "")};
  }

private:
  const Formatter::FormatterConstSharedPtr formatter_;
};
} // namespace FormatString
} // namespace CommonInputs
} // namespace Matching
} // namespace Extensions
} // namespace Envoy
