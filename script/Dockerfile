FROM opensuse/tumbleweed:latest
WORKDIR /app

ADD https://codefloe.com/qr243vbi/nekobox/raw/branch/main/nekobox.spec /app/nekobox.spec

#RUN zypper  addrepo https://download.opensuse.org/repositories/home:juzbun:NekoBox/openSUSE_Tumbleweed/ nekobox_tumbleweed
#RUN zypper  addrepo https://download.opensuse.org/repositories/home:juzbun:NekoBox/openSUSE_Factory_zSystems/ nekobox_s390x
#RUN zypper  addrepo https://download.opensuse.org/repositories/home:juzbun:NekoBox/openSUSE_Factory_ARM/ nekobox_arm
#RUN zypper  addrepo https://download.opensuse.org/repositories/home:juzbun:NekoBox/16.0/ nekobox_leap_16

RUN zypper -v -v -v -v --gpg-auto-import-keys ref

RUN zypper -n -v -v -v  install findutils rpm-build grep rpmdevtools

RUN ( rpmspec -q --buildrequires nekobox.spec | sed 's@\(.*\)@BuildRequires: \1@g;' ; echo "$(rpmdev-newspec buildreqs  -o - -t dummy)") > buildreqs.spec

RUN rpmbuild -bs buildreqs.spec "-D_topdir $PWD"

RUN zypper -n -v -v -v  install tar desktop-file-utils 
#linuxdeploy linuxdeploy-plugin-qt appimagetool
RUN zypper -n -v -v -v  source-install --no-recommends -d SRPMS/buildreqs-1.0-0.src.rpm
RUN zypper -n -v -v -v  install qt6-wayland squashfs wget ccache gh git


CMD ["bash", "-c", "/app/script/build_docker.sh"]
