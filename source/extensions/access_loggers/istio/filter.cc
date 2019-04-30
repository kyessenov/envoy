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

#include "source/extensions/access_loggers/istio/filter.h"

#include "source/extensions/access_loggers/istio/http_builder.h"

#include "common/common/base64.h"
#include "common/http/header_map_impl.h"
#include "common/protobuf/utility.h"

#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"

using ::google::protobuf::util::Status;

namespace istio {
namespace context {

void Filter::log(const ::Envoy::Http::HeaderMap* request_headers,
                 const ::Envoy::Http::HeaderMap* response_headers,
                 const ::Envoy::Http::HeaderMap* response_trailers,
                 const ::Envoy::StreamInfo::StreamInfo& stream_info) {
  static ::Envoy::Http::HeaderMapImpl empty_headers;
  if (!request_headers) {
    request_headers = &empty_headers;
  }
  if (!response_headers) {
    response_headers = &empty_headers;
  }
  if (!response_trailers) {
    response_trailers = &empty_headers;
  }
  Context context;
  Request request;
  Response response;
  ExtractRequest(request, *request_headers);
  ExtractContext(context, *request_headers);
  ExtractReportData(request, response, context, *response_headers, *response_trailers, stream_info);

  /* the block below should be moved to the filter factory */
  google::api::expr::v1alpha1::SourceInfo source_info;
  std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder> builder =
      google::api::expr::runtime::CreateCelExpressionBuilder();
  google::api::expr::runtime::RegisterBuiltinFunctions(builder->GetRegistry());
  auto cel_expression_status = builder->CreateExpression(&config_.expr(), &source_info);
  if (!cel_expression_status.ok()) {
    std::cout << "CEL failed to build expression" << std::endl;
    return;
  }
  auto cel_expression = std::move(cel_expression_status.ValueOrDie());
  /* end of block */

  google::api::expr::runtime::Activation activation;
  google::protobuf::Arena arena;

  activation.InsertValue("context",
                         google::api::expr::runtime::CelValue::CreateMessage(&context, &arena));
  activation.InsertValue("request",
                         google::api::expr::runtime::CelValue::CreateMessage(&request, &arena));
  activation.InsertValue("response",
                         google::api::expr::runtime::CelValue::CreateMessage(&response, &arena));
  auto eval_status = cel_expression->Evaluate(activation, &arena);
  if (!eval_status.ok()) {
    std::cout << "CEL eval failed" << std::endl;
    return;
  }
  auto result = eval_status.ValueOrDie();
  if (result.IsString()) {
    std::cout << "CEL: " << result.StringOrDie().value() << std::endl;
  } else if (result.IsError()) {
    std::cout << "CEL error: " << result.ErrorOrDie()->message() << std::endl;
  }
}

} // namespace context
} // namespace istio
