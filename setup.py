from setuptools import setup, find_packages

setup(name="diffkemp",
      version="0.1",
      description="A tool for semantic difference analysis of kernel functions",
      author="Viktor Malik",
      author_email="vmalik@redhat.com",
      url="https://github.com/viktormalik/diffkemp",
      packages=find_packages(),
      install_requires=["llvmcpy", "pyyaml"])
