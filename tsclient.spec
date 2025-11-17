%define	name		tsclient
%define	version		3.4
%define	release		1
%define	_desktop        %{_datadir}/applications
%define	_serverdir      %{_prefix}/lib/bonobo/servers
%define	_buildroot      %{_tmppath}/%{name}-%{version}

Summary:  Terminal Server Client GTK4 frontend for remote desktop tools.
Name:     %{name}
Version:  %{version}
Release:  %{release}
License:  GPLv3+
Group:    User Interface/Desktops
URL:      http://%{name}.sourceforge.net/ 
Vendor:		Erick Woods <erick@gnomepro.com>
Source:		%{name}-%{version}.tar.gz
BuildRoot:	/var/tmp/%{name}-%{version}
Requires:	glib2 >= 2.76.0, gtk4 >= 4.10, rdesktop >= 1.3.0, vnc >= 4.0
BuildRequires:	glib2-devel >= 2.76.0, gtk4-devel >= 4.10


%description
Terminal Server Client is a GTK4 frontend for rdesktop, VNC, and other remote
desktop tools.  This package also ships a Cinnamon panel applet (installed under
%{_datadir}/tsclient/cinnamon-applet) that can be copied into
~/.local/share/cinnamon/applets for quick profile launches.


%prep
%setup


%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%_prefix
make


%install
[ -n "%{_buildroot}" -a "%{_buildroot}" != / ] && rm -rf %{_buildroot}
make prefix=%{_buildroot}%{_prefix} install
mkdir -p %{_buildroot}%{_desktop}
mkdir -p %{_buildroot}%{_datadir}/tsclient/icons
cp -a icons/* %{_buildroot}%{_datadir}/tsclient/icons/
mkdir -p %{_buildroot}%{_datadir}/tsclient
cp -a applet/cinnamon-tsclient %{_buildroot}%{_datadir}/tsclient/cinnamon-applet


%clean
[ -n "%{_buildroot}" -a "%{_buildroot}" != / ] && rm -rf %{_buildroot}


%files
%defattr(-,root,root)
%doc README ChangeLog AUTHORS NEWS
%{_prefix}/bin/tsclient
%{_desktop}/tsclient.desktop
%{_datadir}/tsclient/icons
%{_datadir}/icons/hicolor/*/apps/tsclient.png
%{_datadir}/icons/hicolor/scalable/apps/tsclient.svg
%{_datadir}/locale/*/LC_MESSAGES/tsclient.mo
%{_datadir}/application-registry/tsclient.*
%{_datadir}/mime-info/tsclient.*
%{_datadir}/man/man1/tsclient.1*
%{_datadir}/tsclient/cinnamon-applet


%changelog
* Mon Nov 17 2025 - Erick Woods <erick@gnomepro.com>
- 3.4 release: default GTK to cairo renderer, refreshed GTK4 packaging, and
  Flatpak/intltool fixes.

* Mon Sep 30 2002 - Erick Woods <erick@gnomepro.com>
- This file was created
