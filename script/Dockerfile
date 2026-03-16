# Base Alpine image
FROM alpine:latest

# Enable edge main & community repositories for Qt6
RUN  apk update

# Install development tools, compilers, and libraries
RUN apk add --no-cache \
    bash \
    build-base \
    cmake \
    clang clang-dev llvm-dev \
    git \
    ninja-build \
    bash \
    wget \
    github-cli \
    curl \
    pkgconfig \
    boost boost-dev \
    thrift thrift-dev \
    qt6-qtbase qt6-qtbase-dev \
    qt6-qttools qt6-qttools-dev \
    qt6-qt5compat \
    qt6-qtdeclarative qt6-qtdeclarative-dev \
    qt6-qtmultimedia qt6-qtmultimedia-dev

# Add Qt tools like lupdate, lrelease, and Qt Designer
RUN apk add --no-cache \
    qt6-qttools-dev qt6-qttools squashfs-tools

WORKDIR /app


CMD ["bash", "-c", "/app/script/build_docker.sh"]
