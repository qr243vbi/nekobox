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


RUN zypper install -y 'patchelf'
RUN zypper install -y 'ccache'
RUN zypper install -y 'libboost_filesystem-devel'
RUN zypper install -y 'cmake'
RUN zypper install -y 'go'
RUN zypper install -y 'pkgconfig'
RUN zypper install -y 'sed'
RUN zypper install -y 'thrift'
RUN zypper install -y 'boost-devel'
RUN zypper install -y 'pkgconfig(thrift)'
RUN zypper install -y 'ninja'
RUN zypper install -y 'pkgconfig(libacl)'
RUN zypper install -y 'cmake(leveldb)'
RUN zypper install -y 'pkgconfig(snappy)'
RUN zypper install -y 'gcc-c++'
RUN zypper install -y 'cmake(Qt6)'
RUN zypper install -y 'cmake(Qt6Core)'
RUN zypper install -y 'cmake(Qt6Gui)'
RUN zypper install -y 'cmake(Qt6Linguist)'
RUN zypper install -y 'cmake(Qt6Widgets)'
RUN zypper install -y 'cmake(Qt6Concurrent)'
RUN zypper install -y 'pkgconfig(xkbcommon)'
RUN zypper install -y 'cmake(yaml-cpp)'
RUN zypper install -y 'pkgconfig(libcurl)'
RUN zypper install -y --force-resolution 'libopenssl-devel'


WORKDIR /app

CMD ["bash", "-x", "/app/script/build_docker.sh"]
