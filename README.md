
Terminal Server Client
tsclient == a GTK4 frontend for rdesktop and others.

http://www.erick.com/

Any questions or comments, email me at erick@erick.com 

Features: 
- GTK4 frontend for rdesktop/freerdp/vncviewer
- JSON-based `.tsc` profiles (legacy flat files are still read transparently)
- MRU JSON list stored in `~/.tsclient/mru.tsc` and consumed by the Cinnamon applet
- Quick protocol/profile picker embedded in the main UI
- Cinnamon panel applet under `applet/cinnamon-tsclient/` that launches recent profiles

Requirements:
- rdesktop >= 1.3.0 or a compatible RDP CLI (freerdp works too)
- vncviewer >= 4.0 for VNC sessions
- glib >= 2.76.0
- gtk4 >= 4.10

Building Debian/Ubuntu packages:
1. Install build deps: `sudo apt install build-essential devscripts debhelper \
   intltool pkg-config libgtk-4-dev libglib2.0-dev libgdk-pixbuf-2.0-dev \
   libpango1.0-dev libcairo2-dev`.
2. Run `dpkg-buildpackage -b -us -uc` from the repo root.
3. Install the resulting `../tsclient_*_amd64.deb`.

Building RPM packages:
1. Ensure `rpmbuild` and the spec deps are installed.
2. Run `./configure && make dist`.
3. Copy the tarball into `~/rpmbuild/SOURCES` and call
   `rpmbuild -ba tsclient.spec`.

Basic VNC support:
 makes a call to vncviewer which must be in your path. if a port is needed, 
 append :X to the hostname where X is the port where vnc is listening. 
 supported VNC options are -fullscreen, -geometry, -depth and -viewonly 
 (set by checking "no motion events"). For a vncpasswd file, put the full 
 path to the file in the "Protocol File" field.
