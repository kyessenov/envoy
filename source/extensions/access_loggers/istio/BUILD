# Copyright 2019 Istio Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
#

licenses(["notice"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

proto_library(
    name = "context_proto",
    srcs = ["context.proto"],
    deps = [
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_proto",
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:descriptor_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:empty_proto",
        "@com_google_protobuf//:struct_proto",
        "@com_google_protobuf//:timestamp_proto",
        "@com_google_protobuf//:wrappers_proto",
    ],
)

cc_proto_library(
    name = "context_proto_cc",
    deps = ["context_proto"],
)

envoy_cc_library(
    name = "context_lib",
    srcs = [
        "http_builder.cc",
        "http_builder.h",
    ],
    repository = "@envoy",
    deps = [
        ":context_proto_cc",
        "//source/common/http:header_map_lib",
        "//source/common/network:utility_lib",
        "//source/common/grpc:common_lib",
        "//source/common/stream_info:utility_lib",
        "//source/common/common:base64_lib",
    ],
)

envoy_cc_library(
    name = "filter",
    srcs = [
        "filter.cc",
        "filter.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":context_lib",
        "//include/envoy/server:access_log_config_interface",
        "@com_google_cel_cpp//eval/public:activation",
        "@com_google_cel_cpp//eval/public:builtin_func_registrar",
        "@com_google_cel_cpp//eval/public:cel_expr_builder_factory",
    ],
)

envoy_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":filter",
        "//include/envoy/registry",
        "//include/envoy/server:access_log_config_interface",
        "//source/common/common:assert_lib",
        "//source/common/protobuf",
    ],
)
