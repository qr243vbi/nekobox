# Base Debian image
FROM debian:oldstable

# Update package index and install required packages
ARG TARGETPLATFORM
ARG BUILDPLATFORM

RUN echo "FINE"
RUN echo "I am running on $BUILDPLATFORM, building for $TARGETPLATFORM"

RUN apt-get update 

RUN apt-get install -y --no-install-recommends \
    bash \
    build-essential \
    cmake

RUN apt-get install -y --no-install-recommends \
    git \
    ninja-build \
    jq \
    curl \
    wget \
    pkg-config

RUN apt-get install -y --no-install-recommends \
    thrift-compiler \
    libboost-all-dev \
    libthrift-dev \
    thrift-compiler

RUN apt-get install -y --no-install-recommends \
    patchelf \
    fuse \
    squashfs-tools

RUN apt-get install -y --no-install-recommends \
    qt6-tools-dev-tools \
    qt6-tools-dev \
    qt6-base-dev \
    qt6-base-dev-tools \
    linguist-qt6 \
    qt6-declarative-dev

RUN apt-get install -y --no-install-recommends \
    libacl1-dev \
    ca-certificates \
    qt6-wayland \
    libxcb-xinerama0

RUN apt-get install -y file
RUN apt-get install -y mesa-utils libegl1-mesa-dev libgl1-mesa-dev libglu1-mesa-dev
RUN apt-get install -y --no-install-recommends qmake6 chrpath
RUN apt-get install -y --no-install-recommends liblmdb-dev

WORKDIR "/app"
CMD ["bash", "-x", "/app/script/build_docker.sh"]
