#!/bin/env python3
import subprocess
import os
import argparse
import errno
import shutil
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--llvm-version")
parser.add_argument("--build-dir", default="build")
parser.add_argument("--diffkemp-dir", default=".")
parser.add_argument("--image", default="viktormalik/diffkemp-devel")
args = parser.parse_args()

build_dir = os.path.abspath(args.build_dir)
diffkemp_dir = os.path.abspath(args.diffkemp_dir)

# Use podman if docker is not available
if shutil.which("docker", os.X_OK):
    command = "docker"
elif shutil.which("podman", os.X_OK):
    command = "podman"
else:
    sys.stderr.write("Error: You need to have docker or podman installed!\n")
    sys.exit(errno.ENOPKG)

cwd = os.getcwd()
to_execute = [command, "run",
              "-ti",
              "--security-opt", "seccomp=unconfined",
              "--security-opt", "label=disable",
              "--memory", "8g",
              "--cpus", "3",
              "--volume", f"{cwd}:{cwd}:Z",
              "--workdir", f"{cwd}",
              "--env", f"SIMPLL_BUILD_DIR={build_dir}",
              "--env", f"DIFFKEMP_DIR={diffkemp_dir}"]
if args.llvm_version is not None:
    to_execute.extend(["--env", f"LLVM_VERSION={args.llvm_version}"])
to_execute.append(args.image)

subprocess.run(to_execute)
