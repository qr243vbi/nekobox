# Void Linux (glibc)
FROM ghcr.io/void-linux/void-glibc:latest

ARG TARGETPLATFORM
ARG BUILDPLATFORM

RUN echo "FINE"
RUN echo "I am running on $BUILDPLATFORM, building for $TARGETPLATFORM"

# Update repositories and install everything
RUN xbps-install -Syu xbps && \
    xbps-install -Sy \
        bash \
        base-devel \
        cmake \
        git \
        ninja \
        jq \
        curl \
        wget \
        pkg-config \
        thrift \
        boost-devel \
        thrift-devel \
        patchelf \
        fuse \
        squashfs-tools \
        qt6-tools-devel \
        qt6-base-devel \
        qt6-declarative-devel \
        qt6-wayland-devel \
        xcb-util-wm \
        file \
        mesa-dri \
        glu-devel \
        lmdb-devel \
        libunistring-devel \
        libzstd-devel \
        cmake ninja pkg-config qt6-base qt6-tools go thrift \
        qt6-base-devel qt6-declarative-devel thrift-devel \
        openssl-devel boost-devel lmdb-devel acl-devel \
        libunistring-devel libcurl-devel libzstd-devel patchelf go

WORKDIR /app

CMD ["bash", "-x", "/app/script/build_docker.sh"]
