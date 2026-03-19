<<<<<<< HEAD
FROM opensuse/tumbleweed:latest
WORKDIR /app

ADD https://codefloe.com/qr243vbi/nekobox/raw/branch/main/nekobox.spec /app/nekobox.spec

RUN zypper -n in findutils rpm-build grep rpmdevtools

RUN ( rpmspec -q --buildrequires nekobox.spec | sed 's@\(.*\)@BuildRequires: \1@g;' ; echo "$(rpmdev-newspec buildreqs  -o - -t dummy)") > buildreqs.spec

RUN rpmbuild -bs buildreqs.spec "-D_topdir $PWD"

RUN zypper  addrepo https://download.opensuse.org/repositories/home:juzbun:NekoBox/openSUSE_Tumbleweed/ nekobox_tumbleweed
RUN zypper  addrepo https://download.opensuse.org/repositories/home:juzbun:NekoBox/openSUSE_Factory_zSystems/ nekobox_s390x
RUN zypper  addrepo https://download.opensuse.org/repositories/home:juzbun:NekoBox/openSUSE_Factory_ARM/ nekobox_arm
RUN zypper  addrepo https://download.opensuse.org/repositories/home:juzbun:NekoBox/16.0/ nekobox_leap_16
RUN zypper --gpg-auto-import-keys ref
RUN zypper -n in tar desktop-file-utils linuxdeploy linuxdeploy-plugin-qt appimagetool

RUN zypper -n source-install --no-recommends -d SRPMS/buildreqs-1.0-0.src.rpm
RUN zypper -n install qt6-wayland squashfs

CMD ["bash", "-c", "/app/script/build_docker.sh"] 
=======
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

RUN apt-get install -y --no-install-recommends ca-certificates
RUN apt-get install -y --no-install-recommends qt6-wayland libxcb-xinerama0
RUN apt-get install -y file
RUN apt-get install -y mesa-utils libegl1-mesa-dev libgl1-mesa-dev libglu1-mesa-dev
RUN apt-get install -y --no-install-recommends qmake6

CMD ["bash", "-c", "/app/script/build_docker.sh"]
>>>>>>> other-repo/main
