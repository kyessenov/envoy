load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@envoy//bazel/toolchains:rbe_toolchains_config.bzl", "rbe_toolchains_config")

def envoy_dependency_imports(go_version = "1.12.5"):
    rules_foreign_cc_dependencies()
    go_rules_dependencies()
    go_register_toolchains(go_version)
    rbe_toolchains_config()
