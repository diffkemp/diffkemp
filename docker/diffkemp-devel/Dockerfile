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
      make \
      module-init-tools \
      ninja-build \
      openssl-devel \
      patch \
      python3 \
      python3-cffi \
      python3-pytest \
      rsync \
      texinfo \
      vim \
      wget \
      xz \
      z3 \
      z3-devel
RUN pip3 install \
      --upgrade pip \
      llvmcpy \
      pytest \
      pyyaml
RUN git clone https://github.com/viktormalik/rhel-kernel-get.git && \
    pip3 install -r rhel-kernel-get/requirements.txt
ENTRYPOINT pip3 install -e .; /bin/bash
