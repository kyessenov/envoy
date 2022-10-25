load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_extension_package",
)

licenses(["notice"])  # Apache 2

envoy_extension_package()

envoy_cc_library(
    name = "input_lib",
    hdrs = ["input.h"],
    deps = [
        "//envoy/formatter:substitution_formatter_interface",
        "//envoy/http:filter_interface",
        "//envoy/matcher:matcher_interface",
        "//envoy/network:filter_interface",
        "//source/common/http:header_map_lib",
    ],
)

envoy_cc_extension(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":input_lib",
        "//envoy/http:filter_interface",
        "//envoy/matcher:matcher_interface",
        "//envoy/network:filter_interface",
        "//envoy/registry",
        "//envoy/server:factory_context_interface",
        "//source/common/formatter:substitution_format_string_lib",
        "@envoy_api//envoy/config/core/v3:pkg_cc_proto",
    ],
)
