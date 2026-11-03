# Void Linux (glibc)
#FROM ghcr.io/void-linux/void-glibc:latest

#ARG TARGETPLATFORM
#ARG BUILDPLATFORM

#RUN echo "FINE"
#RUN echo "I am running on $BUILDPLATFORM, building for $TARGETPLATFORM"

# Update system and install dependencies
#RUN xbps-install -Syu -y
#RUN xbps-install -y bash
#RUN xbps-install -y cmake git ninja jq curl ccache wget sed util-linux coreutils shadow squashfs-tools
#RUN xbps-install -y qt6-base-devel thrift-devel openssl-devel boost-devel lmdb-devel acl-devel libunistring-devel libcurl-devel libzstd-devel patchelf
#RUN xbps-install -y pkg-config qt6-base qt6-tools go thrift base-devel
#RUN xbps-install -y tar xz which file
#RUN xbps-install -y yaml-cpp-devel leveldb-devel

#WORKDIR /app

#CMD ["bash", "-x", "/app/script/build_docker.sh"]

FROM opensuse/tumbleweed:latest

ARG TARGETPLATFORM
ARG BUILDPLATFORM

RUN echo "FINE"
RUN echo "I am running on $BUILDPLATFORM, building for $TARGETPLATFORM"

# Update system and install dependencies
RUN zypper refresh && zypper update -y

RUN zypper install -y \
 'patchelf' \
 'ccache' \
 'libboost_filesystem-devel' \
 'chrpath' \
 'cmake' \
 'go' \
 'pkgconfig' \
 'sed' \
 'libacl-devel' \
 'thrift' \
 'boost-devel' \
 'pkgconfig(thrift)' \
 'ninja' \
 'cmake(Qt6)' \
 'cmake(Qt6Core)' \
 'cmake(Qt6Gui)' \
 'cmake(Qt6Linguist)' \
 'cmake(Qt6Widgets)' \
 'cmake(Qt6Concurrent)' \
 'pkgconfig(xkbcommon)' \
 'cmake(yaml-cpp)' \
 'gcc-c++'

WORKDIR /app

CMD ["bash", "-x", "/app/script/build_docker.sh"]
