FROM fedora:26
MAINTAINER Viktor Malik <vmalik@redhat.com>
ENV LLVM_VERSION 5.0.0
ENV Z3_VERSION 4.5.0
RUN dnf update -y && \
    dnf install -y \
      bison \
      cmake \
      curl \
      flex \
      gcc-c++ \
      gcc \
      gdb \
      git \
      gmp-devel \
      ninja-build \
      python \
      vim \
      xz
# Download and build Z3
RUN curl -SL https://github.com/Z3Prover/z3/archive/z3-$Z3_VERSION.tar.gz | tar xz && \
    mkdir /z3-z3-$Z3_VERSION/build && \
    cd /z3-z3-$Z3_VERSION && \
    python contrib/cmake/bootstrap.py create && \
    cd /z3-z3-$Z3_VERSION/build && \
    cmake .. -GNinja \
             -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_INSTALL_PREFIX=/usr/local \
             -DBUILD_LIBZ3_SHARED=OFF \
             -DUSE_OPENMP=FALSE && \
    ninja && ninja install && \
    rm -r /z3-z3-$Z3_VERSION
# Download and unpack clang
RUN  curl -SL http://llvm.org/releases/$LLVM_VERSION/cfe-$LLVM_VERSION.src.tar.xz | tar xJ
# Download and unpack llvm from my repo 
RUN curl -SL https://github.com/viktormalik/llvm/archive/diffkemp.tar.gz | tar xz
RUN mv llvm-diffkemp llvm && \
    mv cfe-$LLVM_VERSION.src llvm/tools/clang && \
    cd llvm && \
    mkdir build && \
    cd build && \
    cmake .. -GNinja \
             -DCMAKE_BUILD_TYPE=Release \
             -DLLVM_ENABLE_ASSERTIONS=ON \
             -DCMAKE_INSTALL_PREFIX=/usr/local \
             -DLLVM_PARALLEL_LINK_JOBS=1 \
             -DLLVM_TARGETS_TO_BUILD=X86 \
			 -DLLVM_BUILD_LLVM_DYLIB=ON && \
    ninja -j4 && ninja install
# Install Python dependencies
RUN pip install \
	  llvmcpy \
	  enum34 \
	  pytest \
	  pyyaml
ENV PYTHONPATH /diffkemp:/usr/lib/python2.7:/usr/lib/python3.6

