#
# spec file for package nekobox
#
# Copyright (c) 2026 SUSE LLC and contributors
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via https://bugs.opensuse.org/
#


Name:           nekobox
Version: 5.9.31
Release:        0%{?autorelease}
Summary:        Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
License:        GPL-3.0-only
URL:            https://github.com/qr243vbi/nekobox
Source0: https://github.com/qr243vbi/nekobox/releases/download/%{version}/nekobox-unified-source-%{version}.tar.xz
Source1:        nekobox.desktop
Source2:        start.sh
BuildRequires:  chrpath
BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  golang >= 1.24
BuildRequires:  make
BuildRequires:  pkgconfig
BuildRequires:  sed
BuildRequires:  thrift
BuildRequires:  (libboost-devel or boost-devel)
BuildRequires:  (libthrift-devel or thrift-devel)
BuildRequires:  (ninja or ninja-build)
BuildRequires:  cmake(Qt6)
BuildRequires:  cmake(Qt6Core)
BuildRequires:  cmake(Qt6Gui)
BuildRequires:  cmake(Qt6Linguist)
BuildRequires:  cmake(Qt6Network)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Svg)
BuildRequires:  cmake(Qt6Widgets)
Requires:       nekobox-core
%define core nekobox_core

%package -n nekobox-core
Summary:        %{summary}

%description
%{summary}.

%description -n nekobox-core
%{summary}.

%prep
%autosetup -p1 -n nekobox-unified-source-%{version}

%build

(
DEST=$PWD/build
SKIP_UPDATER=y
GOFLAGS='-mod=vendor %{?gobuildflags}'
VERSION_SINGBOX="$(cat SingBox.Version)"
. script/build_go.sh
)

%if %{undefined optflags}
%define optflags -O2 -g -m64 -fmessage-length=0 -D_FORTIFY_SOURCE=2 -fstack-protector -funwind-tables -fasynchronous-unwind-tables
%endif

(
export INPUT_VERSION="%{version}"
%cmake -DSKIP_UPDATE_BUTTON=ON
%cmake_build
)

%install
(
%cmake_install
)
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_datadir}/applications
mkdir -p %{buildroot}%{_datadir}/icons

install -Dm644 %{SOURCE1}   %{buildroot}%{_datadir}/applications/%{name}.desktop
install -Dm755 %{SOURCE2}   %{buildroot}%{_bindir}/%{name}

cp      build/%{core}       %{buildroot}%{_libexecdir}/%{name}/%{core}
cp      global.ini          %{buildroot}%{_libexecdir}/%{name}/public/global.ini
cp      srslist.json        %{buildroot}%{_libexecdir}/%{name}/public/srslist.json
cp      res/%{name}.ico     %{buildroot}%{_datadir}/icons/%{name}.ico

chrpath -d                  %{buildroot}%{_libexecdir}/%{name}/%{name}

sed -i '
  s~@NAME@~%{name}~g; 
  s~@START@~%{_bindir}/%{name}~g;
  s~@ICON@~%{_datadir}/icons/%{name}.ico~g;
  ' %{buildroot}%{_datadir}/applications/%{name}.desktop

sed -i '
  s~@SH@~/bin/sh~g;
  s~@MAIN@~%{_libexecdir}/%{name}/%{name}~g;
  ' %{buildroot}%{_bindir}/%{name}

%files
%attr(0755, -, -) %{_bindir}/%{name}
%attr(0755, -, -) %{_libexecdir}/%{name}/%{name}
%dir %{_libexecdir}/%{name}/public
%attr(0644, -, -) %{_libexecdir}/%{name}/public/*.*
%attr(0644, -, -) %{_datadir}/icons/%{name}.ico
%attr(0644, -, -) %{_datadir}/applications/%{name}.desktop
%license LICENSE

%files -n nekobox-core
%dir %{_libexecdir}/%{name}
%caps(cap_net_admin=pe) %attr(0755, -, -) %{_libexecdir}/%{name}/%{core}

%changelog
