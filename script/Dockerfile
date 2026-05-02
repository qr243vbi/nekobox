# openSUSE Tumbleweed
FROM opensuse/tumbleweed:latest

ARG TARGETPLATFORM
ARG BUILDPLATFORM

RUN echo "FINE"
RUN echo "I am running on $BUILDPLATFORM, building for $TARGETPLATFORM"

# Update repositories and install everything
RUN zypper --non-interactive install --no-allow-arch-change \
        bash \
        patterns-devel-base-devel_basis \
        cmake \
        git \
        ninja \
        jq \
        curl \
        ccache \
        wget \
        gcc-c++ \
        pkg-config \
        thrift \
        boost-devel \
        libthrift-devel \
        patchelf \
        fuse3 \
        squashfs \
        qt6-tools-devel \
        qt6-base-devel \
        qt6-tools-linguist \
        qt6-linguist-devel \
        qt6-declarative-devel \
        qt6-wayland-devel \
        xcb-util-wm-devel \
        libboost_filesystem-devel \
        file \
        glu-devel \
        lmdb-devel \
        libunistring-devel \
        libzstd-devel \
        go \
        libopenssl-devel \
        libacl-devel \
        libcurl-devel
WORKDIR /app

CMD ["bash", "-x", "/app/script/build_docker.sh"]
