# Maintainer: qr243vbi

pkgname=(nekobox nekobox-core)
pkgver=5.9.31
pkgrel=1
pkgdesc="Cross-platform GUI proxy utility (Empowered by sing-box)"
arch=('x86_64' 'aarch64' 'riscv64')
url="https://github.com/qr243vbi/nekobox"
license=('GPL-3.0-or-later')
makedepends=('bash' 'gcc-libs' 'glibc' 'libx11' 'qt6-base' 'qt6-declarative' 'thrift' 'boost')
makedepends+=('cmake' 'gendesk' 'go' 'qt6-tools' 'vulkan-headers' 'cpio' 'upx' 'boost-libs')
source=("https://github.com/qr243vbi/nekobox/releases/download/${pkgver}/nekobox-unified-source-${pkgver}.tar.xz")
sha256sums=("3d4e3ca53799c2e940336480aa6dd2448c137bd16a6784f9a07fd7cf2559cb36")


prepare() {
    gendesk -f -n \
        --pkgname "${pkgname}" \
        --pkgdesc "${pkgdesc}" \
        --name "${pkgname^}" \
        --categories 'Network'
}


build() {
    export DEST=$PWD/build
    pushd "nekobox-unified-source-${pkgver}"
    export CGO_CPPFLAGS="${CPPFLAGS}"
    export CGO_CFLAGS="${CFLAGS}"
    export CGO_CXXFLAGS="${CXXFLAGS}"
    export CGO_LDFLAGS="${LDFLAGS}"
    export GOARCH=""
    export GOOS=linux
    export SKIP_UPDATER=y
    export GOFLAGS='-mod=vendor'
    export VERSION_SINGBOX="$(cat SingBox.Version)"
    ( bash -x script/build_go.sh ; )
    
    cmake -B "${DEST}" -S . \
        -D CMAKE_BUILD_TYPE=Release \
        -D CMAKE_INSTALL_PREFIX=/usr \
        -D SKIP_UPDATE_BUTTON=ON \
        -W no-dev
    cmake --build "${DEST}"
    popd
}


package_nekobox-core() {
    depends=('gcc-libs' 'glibc')
    install -Dm755 "$DEST"/nekobox_core -t "${pkgdir}/usr/lib/NekoBox"
    upx "${pkgdir}/usr/lib/NekoBox/nekobox_core"
}

package_nekobox() {
    depends=('bash' 'gcc-libs' 'glibc' 'libx11' 'qt6-base' 'qt6-declarative' 'nekobox-core' 'thrift' 'boost-libs')

    install -Dm755 "$DEST"/nekobox -t "${pkgdir}/usr/lib/NekoBox"
    upx "${pkgdir}/usr/lib/NekoBox/nekobox"

    echo '#!/bin/bash -x' > "nekobox.sh"
    echo 'exec /usr/lib/NekoBox/nekobox -appdata "$@"' >> "nekobox.sh"
    install -Dm755 "nekobox.sh" "${pkgdir}/usr/bin/nekobox"
    install -Dm644 "nekobox.desktop" -t "${pkgdir}/usr/share/applications"

    cd "nekobox-unified-source-${pkgver}"
    install -Dm644 srslist.json -t "${pkgdir}/usr/lib/NekoBox"
    install -Dm644 global.ini -t "${pkgdir}/usr/lib/NekoBox"
    cp *.js "${pkgdir}/usr/lib/NekoBox"
    cp -RfvT "res/public" "${pkgdir}/usr/lib/NekoBox/public"
    install -Dm644 res/public/icon.png "${pkgdir}/usr/share/pixmaps/nekobox.png"
}
