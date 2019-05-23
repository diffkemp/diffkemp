from distutils.core import setup

setup(name="DiffKemp",
      version="0.1",
      description="A tool for semantic difference analysis of kernel functions.",
      author="Viktor Malik",
      author_email="vmalik@redhat.com",
      url="https://github.com/viktormalik/diffkemp",
      packages=["diffkemp"],
      install_requires=["enum", "llvmcpy", "progressbar", "pyyaml"])
