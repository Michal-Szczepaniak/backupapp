Name:       backupapp

Summary:    Backupapp
Version:    0.1
Release:    1
License:    LICENSE
URL:        http://example.org/
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   curl
Requires:   gnu-tar
Requires:   gnu-gzip
Requires:   util-linux
Requires:   gnu-coreutils
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  desktop-file-utils

%description
An app for backup, a backup app.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5 
%make_build

%install
%qmake5_install

desktop-file-install --delete-original --dir %{buildroot}%{_datadir}/applications %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(4755,root,root,4755)
%{_bindir}/%{name}
%defattr(-,root,root,-)
%dir %{_sysconfdir}/%{name}
%{_sysconfdir}/%{name}/profile.conf.example
%{_sysconfdir}/%{name}/full-webdav.conf
%{_sysconfdir}/%{name}/home-rsync.conf
%{_sysconfdir}/%{name}/home-webdav.conf
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%attr(0755,root,root) /usr/lib/oneshot.d/backupapp-remove-old-rootfs.sh
