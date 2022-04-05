#!/bin/env python3
import subprocess
import os
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--llvm-version")
parser.add_argument("--build-dir", default="build")
parser.add_argument("--diffkemp-dir", default=".")
parser.add_argument("--image", default="viktormalik/diffkemp-devel")
args = parser.parse_args()

build_dir = os.path.abspath(args.build_dir)
diffkemp_dir = os.path.abspath(args.diffkemp_dir)

# Use podman if docker is not available
check_docker = subprocess.run(
    ["command", "-v", "docker"],
    check=False,
    text=True,
    capture_output=True
)
if check_docker.returncode == 0 and os.access(check_docker.stdout, os.X_OK):
    command = "docker"
else:
    command = "podman"

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
