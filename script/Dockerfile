# Base Debian image
FROM debian:oldstable

# Update package index and install required packages
RUN apt-get update

RUN apt-get install -y --no-install-recommends \
    bash \
    build-essential \
    cmake

RUN apt-get install -y --no-install-recommends \
    git \
    ninja-build \
    wget \
    curl \
    pkg-config

RUN apt-get install -y --no-install-recommends \
    thrift-compiler \
    libboost-all-dev \
    libthrift-dev

RUN apt-get install -y --no-install-recommends \
    patchelf \
    fuse \
    squashfs-tools

RUN apt-get install -y --no-install-recommends patchelf qt6-tools-dev-tools qt6-tools-dev qt6-base-dev qt6-base-dev-tools cmake linguist-qt6 thrift-compiler libthrift-dev qt6-declarative-dev libboost-all-dev

RUN apt-get install -y --no-install-recommends libacl1-dev

RUN apt-get install -y --no-install-recommends ca-certificates
RUN apt-get install -y --no-install-recommends qt6-wayland libxcb-xinerama0
RUN apt-get install -y file
RUN apt-get install -y mesa-utils libegl1-mesa-dev libgl1-mesa-dev libglu1-mesa-dev
RUN apt-get install -y --no-install-recommends qmake6

CMD ["bash", "-c", "/app/script/build_docker.sh"]
