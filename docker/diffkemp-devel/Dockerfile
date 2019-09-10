FROM ubuntu:xenial
MAINTAINER Viktor Malik <vmalik@redhat.com>
RUN apt-get update && apt-get install -y software-properties-common wget
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    add-apt-repository "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-7 main" && \
    add-apt-repository "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-8 main"
RUN apt-get update && \
    apt-get install -y \
      autoconf \
      bc \
      bison \
      bzip2 \
      clang-5.0 \
      clang-6.0 \
      clang-7 \
      clang-8 \
      cmake \
      cpio \
      cscope \
      curl \
      libelf-dev \
      libgmp-dev \
      libssl-dev \
      libz3-dev \
      flex \
      gcc-7 \
      g++-7 \
      gdb \
      git \
      gperf \
      help2man \
      llvm-5.0 \
      llvm-6.0 \
      llvm-7 \
      llvm-8 \
      llvm-5.0-dev \
      llvm-6.0-dev \
      llvm-7-dev \
      llvm-8-dev \
      make \
      module-init-tools \
      ninja-build \
      python3 \
      python3-cffi \
      python3-pip \
      python3-pytest \
      rpm2cpio \
      vim \
      xz-utils \
      z3
# Install alternatives for all LLVM versions and for GCC 7
RUN update-alternatives --install /usr/local/bin/llvm-config llvm-config /usr/bin/llvm-config-5.0 10 && \
    update-alternatives --install /usr/local/bin/clang clang /usr/bin/clang-5.0 10 && \
    update-alternatives --install /usr/local/bin/opt opt /usr/bin/opt-5.0 10 && \
    update-alternatives --install /usr/local/bin/llvm-link llvm-link /usr/bin/llvm-link-5.0 10 && \
    update-alternatives --install /usr/local/bin/llvm-config llvm-config /usr/bin/llvm-config-6.0 20 && \
    update-alternatives --install /usr/local/bin/clang clang /usr/bin/clang-6.0 20 && \
    update-alternatives --install /usr/local/bin/opt opt /usr/bin/opt-6.0 20 && \
    update-alternatives --install /usr/local/bin/llvm-link llvm-link /usr/bin/llvm-link-6.0 20 && \
    update-alternatives --install /usr/local/bin/llvm-config llvm-config /usr/bin/llvm-config-7 30 && \
    update-alternatives --install /usr/local/bin/clang clang /usr/bin/clang-7 30 && \
    update-alternatives --install /usr/local/bin/opt opt /usr/bin/opt-7 30 && \
    update-alternatives --install /usr/local/bin/llvm-link llvm-link /usr/bin/llvm-link-7 30 && \
    update-alternatives --install /usr/local/bin/llvm-config llvm-config /usr/bin/llvm-config-8 40 && \
    update-alternatives --install /usr/local/bin/clang clang /usr/bin/clang-8 40 && \
    update-alternatives --install /usr/local/bin/opt opt /usr/bin/opt-8 40 && \
    update-alternatives --install /usr/local/bin/llvm-link llvm-link /usr/bin/llvm-link-8 40 && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 100 --slave /usr/bin/g++ g++ /usr/bin/g++-7
# Copy script for changing default LLVM version
COPY switch-llvm.sh /switch-llvm.sh
RUN chmod +x /switch-llvm.sh
# Default LLVM is 8
RUN /switch-llvm.sh 8
RUN pip3 install \
      --upgrade pip \
      llvmcpy \
      pytest \
      pyyaml
RUN git clone https://github.com/viktormalik/rhel-kernel-get.git && \
    pip3 install -r rhel-kernel-get/requirements.txt
ENTRYPOINT pip3 install -e .; /bin/bash
