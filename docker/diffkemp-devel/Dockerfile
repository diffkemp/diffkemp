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
# Use custom patched 'diff' because the normal only prints 40 characters of
# the context function (when -F is used). We need at least 80 so that the entire
# names of the kernel functions are captured.
RUN git clone https://git.savannah.gnu.org/git/diffutils.git && \
    cd diffutils && \
    sed -i -e 's/40/80/g' src/context.c && \
    ./bootstrap && \
    ./configure && \
    make
RUN pip install \
      --upgrade pip \
      llvmcpy \
      enum34 \
      progressbar \
      pytest \
  pyyaml
ENV PYTHONPATH /diffkemp:/usr/lib/python2.7:/usr/lib/python3.6

