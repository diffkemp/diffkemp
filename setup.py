from setuptools import setup, find_packages
from diffkemp.simpll.utils import get_simpll_build_dir
import subprocess

setup(name="diffkemp",
      version="0.3.0",
      description="A tool for semantic difference analysis of kernel functions",
      author="Viktor Malik",
      author_email="vmalik@redhat.com",
      url="https://github.com/viktormalik/diffkemp",
      packages=find_packages(),
      setup_requires=["cffi"],
      install_requires=["pyyaml", "cffi"],
      cffi_modules=["./diffkemp/simpll/simpll_build.py:ffibuilder"])

build_dir = get_simpll_build_dir()
subprocess.run(["mv", "diffkemp/simpll/_simpll.abi3.so", build_dir])
