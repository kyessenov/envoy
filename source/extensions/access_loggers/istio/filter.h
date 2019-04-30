/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "source/extensions/access_loggers/istio/context.pb.h"

#include "common/common/logger.h"
#include "envoy/access_log/access_log.h"
#include "envoy/http/filter.h"

namespace istio {
namespace context {

class Filter : public ::Envoy::AccessLog::Instance,
               public ::Envoy::Logger::Loggable<::Envoy::Logger::Id::filter> {
public:
  Filter(const FilterConfig& config) : config_(config) {}

  // Called when the request is completed.
  virtual void log(const ::Envoy::Http::HeaderMap* request_headers,
                   const ::Envoy::Http::HeaderMap* response_headers,
                   const ::Envoy::Http::HeaderMap* response_trailers,
                   const ::Envoy::StreamInfo::StreamInfo& stream_info) override;

private:
  const FilterConfig config_;
};

} // namespace context
} // namespace istio
