Name:           tsclient
Version:        3.5.0
Release:        1%{?dist}
Summary:        GTK4 frontend for remote desktop clients
License:        GPL-3.0-or-later
URL:            https://github.com/woodpile72/tsclient
Source0:        %{url}/releases/download/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  intltool
BuildRequires:  gettext
BuildRequires:  pkgconfig(gtk4) >= 4.10
BuildRequires:  pkgconfig(json-glib-1.0)
BuildRequires:  pkgconfig(libsecret-1)
Requires:       rdesktop >= 1.9.0

%description
TSClient is a GTK4 frontend for rdesktop, FreeRDP, VNC viewers, Xnest,
and the Citrix ICA client. It reads Windows RDP files and saves JSON
profiles, with optional password storage in the desktop keyring.
Additional backends are optional and must be installed separately.

%prep
%setup -q

%build
%configure
%make_build

%check
# Display-dependent tests skip automatically on headless builders.
%make_build check

%install
%make_install
%find_lang %{name}

%files -f %{name}.lang
%license COPYING
%doc README.md AUTHORS NEWS ChangeLog
%{_bindir}/tsclient
%{_datadir}/applications/tsclient.desktop
%{_datadir}/tsclient/
%{_datadir}/icons/hicolor/*/apps/tsclient.png
%{_datadir}/icons/hicolor/scalable/apps/tsclient.svg
%{_datadir}/application-registry/tsclient.applications
%{_datadir}/mime-info/tsclient.*
%{_mandir}/man1/tsclient.1*

%changelog
* Mon Nov 16 2026 Erick Woods <erick@gnomepro.com> - 3.5.0-1
- modernization efforts
