#include "common/network/utility.h"

#include "extensions/filters/common/expr/context.h"

#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

#include "absl/container/flat_hash_map.h"
#include "benchmark/benchmark.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Expr {
namespace {

void BM_Context(benchmark::State& state) {
  size_t output_bytes = 0;
  Http::TestHeaderMapImpl request_headers{{"x-request-id", "blah"}};
  NiceMock<StreamInfo::MockStreamInfo> info;
  RequestWrapper request(request_headers, info);

  for (auto _ : state) {
    auto value = request[CelValue::CreateString(ID)];
    output_bytes += value.value().StringOrDie().value().size();
  }

  benchmark::DoNotOptimize(output_bytes);
}

BENCHMARK(BM_Context);

static absl::flat_hash_map<absl::string_view,
                           std::function<absl::optional<CelValue>(
                               const Http::HeaderMap& headers, const StreamInfo::StreamInfo& info)>>
    functions = {{Path,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {UrlPath,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {Host,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {Scheme,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {Method,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {Referer,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {Headers,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {Time,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {ID,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {UserAgent,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {Size,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {TotalSize,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 {Duration,
                  [](const Http::HeaderMap& headers,
                     const StreamInfo::StreamInfo&) -> absl::optional<CelValue> {
                    auto header = headers.RequestId();
                    if (header == nullptr) {
                      return {};
                    }
                    return CelValue::CreateString(header->value().getStringView());
                  }},
                 };

class TestWrapper : public BaseWrapper {
public:
  TestWrapper(const Http::HeaderMap& headers, const StreamInfo::StreamInfo& info)
      : headers_(headers), info_(info) {}
  absl::optional<CelValue> operator[](CelValue key) const override {
    if (!key.IsString()) {
      return {};
    }
    auto value = key.StringOrDie().value();
    if (value == ID) {
      auto header = headers_.RequestId();
      if (header == nullptr) {
        return {};
      }
      return CelValue::CreateString(header->value().getStringView());
    } else if (value == Time) {
      return CelValue::CreateTimestamp(absl::FromChrono(info_.startTime()));
    }
    return {};
  }

private:
  const Http::HeaderMap& headers_;
  const StreamInfo::StreamInfo& info_;
};

void BM_TestContext(benchmark::State& state) {
  size_t output_bytes = 0;
  Http::TestHeaderMapImpl request_headers{{"x-request-id", "blah"}};
  NiceMock<StreamInfo::MockStreamInfo> info;
  TestWrapper request(request_headers, info);

  for (auto _ : state) {
    auto value = request[CelValue::CreateString(ID)];
    output_bytes += value.value().StringOrDie().value().size();
  }

  benchmark::DoNotOptimize(output_bytes);
}

BENCHMARK(BM_TestContext);

} // namespace
} // namespace Expr
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy

// Boilerplate main(), which discovers benchmarks in the same file and runs them.
int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);

  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  benchmark::RunSpecifiedBenchmarks();
}
