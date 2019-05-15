FROM fedora:27
MAINTAINER Viktor Malik <vmalik@redhat.com>
RUN dnf install -y \
      autoconf \
      automake \
      bc \
      bison \
      bzip2 \
      clang \
      clang-devel \
      cmake \
      cpio \
      cscope \
      curl \
      elfutils-libelf-devel \
      findutils \
      flex \
      gcc-c++ \
      gcc \
      gdb \
      gettext \
      gettext-devel \
      git \
      gmp-devel \
      gperf \
      help2man \
      llvm \
      llvm-devel \
      module-init-tools \
      ninja-build \
      openssl-devel \
      patch \
      python \
      python-cffi \
      rsync \
      texinfo \
      vim \
      wget \
      xz \
      z3 \
      z3-devel
RUN pip install \
      --upgrade pip \
      llvmcpy \
      enum34 \
      progressbar \
      pytest \
  pyyaml
ENV PYTHONPATH /diffkemp:/usr/lib/python2.7:/usr/lib/python3.6

