#!/usr/bin/env python3

from subprocess import check_output
from subprocess import check_call
import glob
import os
import shutil
import sys
import re

TARGETS = '@envoy_api//...'
IMPORT_BASE = 'github.com/envoyproxy/go-control-plane'
OUTPUT_BASE = 'build_go'
REPO_BASE = 'go-control-plane'
BRANCH = 'master'
MIRROR_MSG = 'Mirrored from envoyproxy/envoy @ '
USER_NAME = 'Kuat Yessenov'
USER_EMAIL = 'kuat@google.com'


def generateProtobufs(output):
  bazel_bin = check_output(['bazel', 'info', 'bazel-bin']).decode().strip()
  go_protos = check_output([
      'bazel',
      'query',
      'kind("go_proto_library", %s)' % TARGETS,
  ]).split()

  # Each rule has the form @envoy_api//foo/bar:baz_go_proto.
  # First build all the rules to ensure we have the output files.
  check_call(['bazel', 'build', '-c', 'fastbuild'] + go_protos)

  shutil.rmtree(output, ignore_errors=True)
  for rule in go_protos:
    # Example rule:
    # @envoy_api//envoy/config/bootstrap/v2:pkg_go_proto
    #
    # Example generated directory:
    # bazel-bin/external/envoy_api/envoy/config/bootstrap/v2/linux_amd64_stripped/pkg_go_proto%/github.com/envoyproxy/go-control-plane/envoy/config/bootstrap/v2/
    #
    # Example output directory:
    # go_out/envoy/config/bootstrap/v2
    rule_dir, proto = rule.decode()[len('@envoy_api//'):].rsplit(':', 1)

    input_dir = os.path.join(bazel_bin, 'external', 'envoy_api', rule_dir, 'linux_amd64_stripped',
                             proto + '%', IMPORT_BASE, rule_dir)
    input_files = glob.glob(os.path.join(input_dir, '*.go'))
    output_dir = os.path.join(output, rule_dir)

    # Ensure the output directory exists
    os.makedirs(output_dir, 0o755, exist_ok=True)
    for generated_file in input_files:
      shutil.copy(generated_file, output_dir)
  print('Go artifacts placed into: ' + output)


def cloneGoProtobufs(repo):
  # Create a local clone of go-control-plane
  shutil.rmtree(repo, ignore_errors=True)
  check_call(['git', 'clone', 'git@git:envoyproxy/go-control-plane', repo])
  check_call(['git', '-C', repo, 'fetch'])
  check_call(['git', '-C', repo, 'checkout', '-B', BRANCH, 'origin/master'])


def findLastSyncSHA(repo):
  # Determine last envoyproxy/envoy SHA in envoyproxy/go-control-plane
  last_commit = check_output(
      ['git', '-C', repo, 'log', '--grep=' + MIRROR_MSG, '-n', '1',
       '--format=%B']).decode().strip()
  # Initial SHA from which the APIs start syncing. Prior to that it was done manually.
  if last_commit == "":
    return 'e7f0b7176efdc65f96eb1697b829d1e6187f4502'
  m = re.search(MIRROR_MSG + '(\w+)', last_commit)
  return m.group(1)


def updatedSinceSHA(repo, last_sha):
  # Determine if there are changes to api since last SHA
  changes = check_output(['git', 'rev-list', '%s..HEAD' % last_sha, 'api/envoy']).decode().split()
  return changes


def syncGoProtobufs(output, repo):
  # Sync generated content against repo and return true if there is a commit necessary
  dst = os.path.join(repo, 'envoy')
  # Remove subtree at envoy in repo
  shutil.rmtree(dst, ignore_errors=True)
  # Copy subtree at envoy from output to repo
  shutil.copytree(os.path.join(output, 'envoy'), dst)


def publishGoProtobufs(repo, sha):
  # Publish generated files with the last SHA changes to api
  check_call(['git', '-C', repo, 'config', 'user.name', USER_NAME])
  check_call(['git', '-C', repo, 'config', 'user.email', USER_EMAIL])
  check_call(['git', '-C', repo, 'add', 'envoy'])
  check_call(['git', '-C', repo, 'commit', '-s', '-m', MIRROR_MSG + sha])
  check_call(['git', '-C', repo, 'push', 'origin', BRANCH])


if __name__ == "__main__":
  workspace = check_output(['bazel', 'info', 'workspace']).decode().strip()
  output = os.path.join(workspace, OUTPUT_BASE)
  generateProtobufs(output)
  repo = os.path.join(workspace, REPO_BASE)
  cloneGoProtobufs(repo)
  last_sha = findLastSyncSHA(repo)
  changes = updatedSinceSHA(repo, last_sha)
  if changes:
    print('Changes detected: %s' % changes)
    syncGoProtobufs(output, repo)
    publishGoProtobufs(repo, changes[0])
