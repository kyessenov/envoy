workspace(name = "envoy")

load("//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("//bazel:repositories.bzl", "GO_VERSION", "envoy_dependencies")
load("//bazel:cc_configure.bzl", "cc_configure")

envoy_dependencies()

load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

cc_configure()

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(go_version = GO_VERSION)

# CEL-CPP additions
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# glog
http_archive(
    name = "com_google_glog",
    sha256 = "1ee310e5d0a19b9d584a855000434bb724aa744745d5b8ab1855c85bff8a8e21",
    strip_prefix = "glog-028d37889a1e80e8a07da1b8945ac706259e5fd8",
    urls = [
        "https://mirror.bazel.build/github.com/google/glog/archive/028d37889a1e80e8a07da1b8945ac706259e5fd8.tar.gz",
        "https://github.com/google/glog/archive/028d37889a1e80e8a07da1b8945ac706259e5fd8.tar.gz",
    ],
)

# Google RE2 (Regular Expression) C++ Library
http_archive(
    name = "com_google_re2",
    strip_prefix = "re2-master",
    urls = ["https://github.com/google/re2/archive/master.zip"],
)

http_archive(
    name = "com_google_googleapis",
    build_file_content = """
cc_proto_library(
    name = 'cc_rpc_status',
    deps = ['//google/rpc:status_proto'],
    visibility = ['//visibility:public'],
)
cc_proto_library(
    name = 'cc_rpc_code',
    deps = ['//google/rpc:code_proto'],
    visibility = ['//visibility:public'],
)
cc_proto_library(
    name = 'cc_type_money',
    deps = ['//google/type:money_proto'],
    visibility = ['//visibility:public'],
)
cc_proto_library(
    name = 'cc_expr_v1alpha1',
    deps = ['//google/api/expr/v1alpha1:syntax_proto'],
    visibility = ['//visibility:public'],
)
cc_proto_library(
    name = 'cc_expr_v1beta1',
    deps = ['//google/api/expr/v1beta1:eval_proto'],
    visibility = ['//visibility:public'],
)
""",
    strip_prefix = "googleapis-9a02c5acecb43f38fae4fa52c6420f21c335b888",
    urls = ["https://github.com/googleapis/googleapis/archive/9a02c5acecb43f38fae4fa52c6420f21c335b888.tar.gz"],
)

http_archive(
    name = "com_google_api_codegen",
    urls = ["https://github.com/googleapis/gapic-generator/archive/96c3c5a4c8397d4bd29a6abce861547a271383e1.zip"],
    strip_prefix = "gapic-generator-96c3c5a4c8397d4bd29a6abce861547a271383e1",
    sha256 = "c8ff36df84370c3a0ffe141ec70c3633be9b5f6cc9746b13c78677e9d5566915",
)

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "io_grpc_grpc_java",
    remote = "https://github.com/grpc/grpc-java.git",
    tag = "v1.13.1",
)

#
# java_gapic artifacts runtime dependencies (gax-java)
# @unused
# buildozer: disable=load
load("@com_google_api_codegen//rules_gapic/java:java_gapic_repositories.bzl", "java_gapic_repositories")


# gflags
http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "6e16c8bc91b1310a44f3965e616383dbda48f83e8c1eaa2370a215057b00cabe",
    strip_prefix = "gflags-77592648e3f3be87d6c7123eb81cbad75f9aef5a",
    urls = [
        "https://mirror.bazel.build/github.com/gflags/gflags/archive/77592648e3f3be87d6c7123eb81cbad75f9aef5a.tar.gz",
        "https://github.com/gflags/gflags/archive/77592648e3f3be87d6c7123eb81cbad75f9aef5a.tar.gz",
    ],
)

http_archive(
    name = "com_google_cel_spec",
    strip_prefix = "cel-spec-master",
    urls = ["https://github.com/google/cel-spec/archive/master.zip"],
)


GOOGLE_CEL_CPP_SHA = "c384f6ea9acbb67cad70ced7d5606b9d97dc02a3"

http_archive(
    name = "com_google_cel_cpp",
    strip_prefix = "cel-cpp-" + GOOGLE_CEL_CPP_SHA,
    url = "https://github.com/google/cel-cpp/archive/" + GOOGLE_CEL_CPP_SHA + ".tar.gz",
)
