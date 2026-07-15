FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# -----------------------------------------------------------------------------
# Use Aliyun mirror (optional)
# -----------------------------------------------------------------------------
RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    sed -i 's/security.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list

# -----------------------------------------------------------------------------
# Install development tools
# -----------------------------------------------------------------------------
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ccache \
    build-essential \
    gcc \
    g++ \
    clang-14 \
    clangd-14 \
    clang-format-14 \
    clang-tools-14 \
    lldb-14 \
    gdb \
    cmake \
    ninja-build \
    make \
    git \
    pkg-config \
    wget \
    curl \
    unzip \
    vim \
    less \
    tree \
    jq \
    ca-certificates \
    software-properties-common \
    valgrind \
    strace \
    ltrace \
    iproute2 \
    procps \
    libssl-dev \
    libboost-all-dev \
    protobuf-compiler \
    libprotobuf-dev \
    libsnappy-dev \
    libbz2-dev \
    liblz4-dev \
    libzstd-dev \
    zlib1g-dev \
    libfmt-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    python3 \
    python3-pip && \
    rm -rf /var/lib/apt/lists/*

# -----------------------------------------------------------------------------
# clang symlink
# -----------------------------------------------------------------------------
RUN ln -sf /usr/bin/clang-14 /usr/bin/clang && \
    ln -sf /usr/bin/clang++-14 /usr/bin/clang++ && \
    ln -sf /usr/bin/clangd-14 /usr/bin/clangd && \
    ln -sf /usr/bin/clang-format-14 /usr/bin/clang-format

# -----------------------------------------------------------------------------
# Install latest CMake
# Ubuntu22 is only 3.22
# -----------------------------------------------------------------------------
ARG CMAKE_VERSION=3.27.9

RUN wget -q https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz && \
    tar -zxf cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz && \
    cd cmake-${CMAKE_VERSION}-linux-x86_64 && \
    cp -rf bin/* /usr/local/bin/ && \
    cp -rf share/* /usr/local/share/ && \
    cp -rf doc/* /usr/local/doc/ && \
    cp -rf man/* /usr/local/man/ && \
    cd .. && \
    rm -rf cmake-${CMAKE_VERSION}*

# -----------------------------------------------------------------------------
# Install Eclipse Paho MQTT C
# Ubuntu package is too old
# -----------------------------------------------------------------------------
RUN git clone --depth=1 https://github.com/eclipse/paho.mqtt.c.git /tmp/paho && \
    cmake -S /tmp/paho \
          -B /tmp/paho/build \
          -DPAHO_WITH_SSL=ON \
          -DPAHO_BUILD_STATIC=ON \
          -DPAHO_BUILD_SHARED=ON \
          -DPAHO_ENABLE_TESTING=OFF && \
    cmake --build /tmp/paho/build -j$(nproc) && \
    cmake --install /tmp/paho/build && \
    ldconfig && \
    rm -rf /tmp/paho

# -----------------------------------------------------------------------------
# Install AWS CRT C Libraries and AWS IoT Device SDK for C++
# -----------------------------------------------------------------------------
RUN git clone --recursive https://github.com/awslabs/aws-iot-device-sdk-cpp-v2.git && \
    cd aws-iot-device-sdk-cpp-v2 && \
    git checkout v1.18.0 && \
    git submodule update --init --recursive && \
    cmake -S . -B build \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DCMAKE_PREFIX_PATH=/usr/local \
      -DBUILD_SHARED_LIBS=ON && \
    cmake --build build --target install
    #cd .. && rm -rf aws-iot-device-sdk-cpp-v2

# Clean up build sources
WORKDIR /

# -----------------------------------------------------------------------------
# Environment
# -----------------------------------------------------------------------------
ENV CC=clang
ENV CXX=clang++

ENV CMAKE_GENERATOR=Ninja

ENV CMAKE_EXPORT_COMPILE_COMMANDS=ON

ENV CMAKE_BUILD_PARALLEL_LEVEL=8

ENV LD_LIBRARY_PATH=/usr/local/lib:${LD_LIBRARY_PATH}

WORKDIR /workspace

CMD ["/bin/bash"]