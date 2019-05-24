# Use the development environment container and build DiffKemp inside it
FROM diffkemp-devel:latest
# Clone DiffKemp
RUN git clone https://github.com/viktormalik/diffkemp.git 
WORKDIR "/diffkemp"
# Build 
RUN mkdir build && \
    cd build && \
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release && \
    ninja -j4 && \
    cd ..
# Install
RUN pip install -r requirements.txt && \
    pip install -e .
# Remove packages not needed for runtime
RUN dnf remove -y \
   autoconf \
   automake \
   bison \
   cmake \
   flex \
   gdb \
   gettext \
   gettext-devel \
   gmp-devel \
   gperf \
   help2man \
   ninja-build \
   patch \
   rsync \
   texinfo \
   vim \
   wget \
   z3 \
   z3-devel
ENTRYPOINT []
