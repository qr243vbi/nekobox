# openSUSE Tumbleweed
FROM opensuse/tumbleweed:latest

ARG TARGETPLATFORM
ARG BUILDPLATFORM

RUN echo "FINE"
RUN echo "I am running on $BUILDPLATFORM, building for $TARGETPLATFORM"

# Update repositories and install everything
RUN zypper --non-interactive install \
        bash \
        patterns-devel-base-devel_basis \
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
        fuse3 \
        squashfs \
        qt6-tools-devel \
        qt6-base-devel \
        qt6-declarative-devel \
        qt6-wayland-devel \
        xcb-util-wm-devel \
        file \
        Mesa-dri \
        glu-devel \
        lmdb-devel \
        libunistring-devel \
        libzstd-devel \
        go \
        openssl-devel \
        libacl-devel \
        libcurl-devel
WORKDIR /app

CMD ["bash", "-x", "/app/script/build_docker.sh"]
