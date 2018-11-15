# Use the development environment container and build DiffKemp inside it
FROM diffkemp-devel:latest
RUN git clone --recurse-submodules https://github.com/viktormalik/diffkemp.git && \
    cd diffkemp && \
    mkdir build && \
    cd build && \
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release && \
    ninja -j4

