from setuptools import setup, find_packages

setup(name="diffkemp",
      version="0.2.2",
      description="A tool for semantic difference analysis of kernel functions",
      author="Viktor Malik",
      author_email="vmalik@redhat.com",
      url="https://github.com/viktormalik/diffkemp",
      packages=find_packages(),
      setup_requires=["cffi"],
      install_requires=["llvmcpy", "pyyaml", "cffi"],
      cffi_modules=["./diffkemp/simpll/simpll_build.py:ffibuilder"])
