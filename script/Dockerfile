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
