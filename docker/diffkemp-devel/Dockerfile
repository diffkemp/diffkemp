FROM ubuntu:xenial
MAINTAINER Viktor Malik <vmalik@redhat.com>
RUN apt-get update && \
    apt-get install -y \
      apt-transport-https \
      software-properties-common \
      wget
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    add-apt-repository -y ppa:ubuntu-toolchain-r/test && \
    add-apt-repository -y "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-7 main" && \
    add-apt-repository -y "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-8 main" && \
    add-apt-repository -y "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-9 main" && \
    add-apt-repository -y "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-10 main" && \
    add-apt-repository -y "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-11 main" && \
    add-apt-repository -y "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-12 main" && \
    add-apt-repository -y ppa:deadsnakes/ppa
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
      clang-9 \
      clang-10 \
      clang-11 \
      clang-12 \
      clang-format-8 \
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
      llvm-9 \
      llvm-10 \
      llvm-11 \
      llvm-12 \
      llvm-5.0-dev \
      llvm-6.0-dev \
      llvm-7-dev \
      llvm-8-dev \
      llvm-9-dev \
      llvm-10-dev \
      llvm-11-dev \
      llvm-12-dev \
      make \
      module-init-tools \
      ninja-build \
      python3.8 \
      python3.8-dev \
      python3.8-distutils \
      python3-setuptools \
      rpm2cpio \
      vim \
      xz-utils \
      z3
# Remove conflicting automatically installed GCC versions
RUN apt-get remove -y cpp cpp-5 gcc gcc-5 g++ g++-5
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
    update-alternatives --install /usr/local/bin/llvm-config llvm-config /usr/bin/llvm-config-9 40 && \
    update-alternatives --install /usr/local/bin/clang clang /usr/bin/clang-9 40 && \
    update-alternatives --install /usr/local/bin/opt opt /usr/bin/opt-9 40 && \
    update-alternatives --install /usr/local/bin/llvm-link llvm-link /usr/bin/llvm-link-9 40 && \
    update-alternatives --install /usr/local/bin/llvm-config llvm-config /usr/bin/llvm-config-10 50 && \
    update-alternatives --install /usr/local/bin/clang clang /usr/bin/clang-10 50 && \
    update-alternatives --install /usr/local/bin/opt opt /usr/bin/opt-10 50 && \
    update-alternatives --install /usr/local/bin/llvm-link llvm-link /usr/bin/llvm-link-10 50 && \
    update-alternatives --install /usr/local/bin/llvm-config llvm-config /usr/bin/llvm-config-11 60 && \
    update-alternatives --install /usr/local/bin/clang clang /usr/bin/clang-11 60 && \
    update-alternatives --install /usr/local/bin/opt opt /usr/bin/opt-11 60 && \
    update-alternatives --install /usr/local/bin/llvm-link llvm-link /usr/bin/llvm-link-11 60 && \
    update-alternatives --install /usr/local/bin/llvm-config llvm-config /usr/bin/llvm-config-12 70 && \
    update-alternatives --install /usr/local/bin/clang clang /usr/bin/clang-12 70 && \
    update-alternatives --install /usr/local/bin/opt opt /usr/bin/opt-12 70 && \
    update-alternatives --install /usr/local/bin/llvm-link llvm-link /usr/bin/llvm-link-12 70 && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 100 --slave /usr/bin/g++ g++ /usr/bin/g++-7 && \
    update-alternatives --install /usr/bin/x86_64-linux-gnu-gcc x86_64-linux-gnu-gcc /usr/bin/x86_64-linux-gnu-gcc-7 100 \
                        --slave   /usr/bin/x86_64-linux-gnu-g++ x86_64-linux-gnu-g++ /usr/bin/x86_64-linux-gnu-g++-7
# Configure links for the clang-format checker script
RUN update-alternatives --install /usr/local/bin/clang-format clang-format /usr/bin/clang-format-8 40 && \
    ln -s /diffkemp/tools/check-clang-format.sh /usr/local/bin/check-clang-format
# Copy script for changing default LLVM version
COPY switch-llvm.sh /switch-llvm.sh
RUN chmod +x /switch-llvm.sh
# Default LLVM is 11
RUN /switch-llvm.sh 11
# Setup Python 3.8 and install pip packages
RUN update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.8 100
RUN curl https://bootstrap.pypa.io/get-pip.py | python3.8
RUN python3 -m pip install \
      --upgrade pip \
      llvmcpy \
      pytest \
      pyyaml \
      --ignore-installed
RUN git clone https://github.com/viktormalik/rhel-kernel-get.git && \
    python3 -m pip install -r rhel-kernel-get/requirements.txt
ENTRYPOINT python3 -m pip install -e .; /bin/bash
