# The version comes from packaging/version.sh: rpmbuild --define "pkgver $(packaging/version.sh)"
Name:           shineshift
Version:        %{pkgver}
Release:        1%{?dist}
Summary:        Monitor brightness from the tray, over DDC/CI
License:        GPL-3.0-or-later AND MIT
URL:            https://github.com/ItzELECTR0/ShineShift
Source:         %{name}.tar.gz
ExclusiveArch:  x86_64 aarch64

# Zig 0.17 is newer than Fedora's, so it is not listed and has to be on PATH.
BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  cmake(LayerShellQt)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Quick)
BuildRequires:  cmake(Qt6QuickControls2)
BuildRequires:  pkgconfig(ddcutil) >= 2.2.1

Requires:       ddcutil
Requires:       layer-shell-qt
Requires:       qt6qml(QtQuick)
Requires:       qt6qml(QtQuick.Controls.Basic)
Requires:       qt6qml(QtQuick.Effects)
Requires:       qt6qml(QtQuick.Layouts)
Requires:       qt6qml(QtQuick.Shapes)
Recommends:     xdg-desktop-portal
Recommends:     xset

%description
ShineShift sets brightness, contrast and other DDC/CI values per monitor from
a tray flyout or the command line, and SDR brightness for HDR outputs on
Hyprland.

%prep
%autosetup -n %{name}
cp ui/icons/LICENSE fluentui-system-icons-LICENSE

%build
%cmake -G Ninja
%cmake_build

%install
%cmake_install
install -Dm644 data/themes/Midnight.json %{buildroot}%{_docdir}/%{name}/themes/Midnight.json

%check
ZIG_GLOBAL_CACHE_DIR=$PWD/zig-cache zig build test --cache-dir $PWD/zig-test-cache

%files
%license LICENSE fluentui-system-icons-LICENSE
%{_bindir}/shineshift
%{_datadir}/applications/net.electris.ShineShift.desktop
%{_docdir}/%{name}/
