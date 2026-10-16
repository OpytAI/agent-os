"""Repository-aware wrappers for first-party formatters."""

load("@rules_shell//shell:sh_binary.bzl", "sh_binary")

def source_formatter(name, language, tool, data = []):
    """Creates matching write and check commands for one first-party language."""
    sh_binary(
        name = name,
        srcs = ["//bazel/tools/format:runner.sh"],
        args = [
            "write",
            "$(rlocationpath {})".format(tool),
            language,
        ],
        data = [tool] + data,
        deps = ["@bazel_tools//tools/bash/runfiles"],
    )
    sh_binary(
        name = name + ".check",
        srcs = ["//bazel/tools/format:runner.sh"],
        args = [
            "check",
            "$(rlocationpath {})".format(tool),
            language,
        ],
        data = [tool] + data,
        deps = ["@bazel_tools//tools/bash/runfiles"],
    )
