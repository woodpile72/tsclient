# Terminal Server Client

TSClient 3.5.0 is a C/GTK4 frontend for rdesktop, FreeRDP, VNC viewers,
Xnest, and the Citrix ICA client.

[Source, releases, and issues](https://github.com/woodpile72/tsclient)

## Build and test

Requires GTK4 >= 4.10, GLib, json-glib, libsecret, a C compiler, Autotools,
intltool, gettext, and pkg-config. Install your chosen remote desktop backend
separately. Optional password storage requires a desktop Secret Service.

On Debian/Ubuntu:

```sh
sudo apt install build-essential autoconf automake debhelper intltool gettext \
  pkg-config libgtk-4-dev libjson-glib-dev libsecret-1-dev xvfb
autoreconf -fi
./configure --prefix=/usr
make
GTK_A11Y=none GTK_IM_MODULE=simple GSK_RENDERER=cairo xvfb-run -a make check
```

TSClient defaults to GTK's cairo renderer when `GSK_RENDERER` is unset.
It does not require users to join the render group.

## Packages

Build a DEB with `dpkg-buildpackage -b -us -uc`, then install the resulting
`../tsclient_3.5.0-1_amd64.deb` using `sudo apt install /absolute/path/to/package.deb`.
The architecture suffix depends on the build host.

For RPM, on an RPM-based distribution with the spec's build dependencies
installed, run `make dist`, copy `tsclient-3.5.0.tar.gz` to
`~/rpmbuild/SOURCES/`, then run `rpmbuild -ba tsclient.spec`.

The Cinnamon applet in `applet/cinnamon-tsclient/` is optional source material;
these packages do not install it. See its own README for manual installation.

## Flatpak

`com.tsclient.TSClient.yml` builds the local source tarball; the release manifest
`com.tsclient.app.yml` downloads the GitHub release tarball and verifies its SHA256.
Both use GNOME 50 and bundle rdesktop, including its upstream RSA-length fix.
Disable the builder cache for local tarball builds so rebuilding the same
version does not reuse an older archive.

```sh
make dist
flatpak install flathub org.flatpak.Builder org.gnome.Sdk//50 org.gnome.Platform//50
flatpak run org.flatpak.Builder --user --force-clean --disable-cache \
  --disable-rofiles-fuse --install builddir com.tsclient.TSClient.yml
flatpak run com.tsclient.app
```

The sandbox allows network, display, audio, and Secret Service access for the
optional keyring feature. Its exported desktop file and icons use the app ID.
A host-installed FreeRDP or VNC client is not available inside the sandbox.
Publishing a GitHub release does not submit or publish an app to Flathub.

## Connections

Select RDPv4/RDPv5 for rdesktop or RDP (FreeRDP) for `xfreerdp3`/`xfreerdp`.
Arguments are passed directly without a shell. FreeRDP retains its authentication,
TLS, and certificate-validation defaults; rdesktop-only controls are disabled.

For VNC, use the display/port syntax supported by your viewer. If needed, select
its password file using Protocol File. Other backend settings depend on the
installed client. See `man tsclient` or `tsclient --help` for command-line usage.

Profiles and passwords:
- Open Windows `.rdp` files (UTF-8 or UTF-16) and TSClient `.tsc` files. Save As
  writes `.tsc` JSON; it does not overwrite Windows `.rdp` files.
- Profiles and the recent-server list are saved atomically with owner-only
  permissions. New saves omit passwords, including automatic `last.tsc` saves.
- “Remember password in keyring” is off by default. When enabled, Connect,
  Save As, or Exit stores the password in the desktop keyring for that exact
  protocol, server, username, and domain. Uncheck it and save/connect/exit to
  remove the saved credential for the current connection.
- Changing the connection identity clears the password and resets
  Remember password. It does not delete credentials for other connections.
- If the keyring is unavailable or locked, TSClient reports an error; it never
  falls back to writing a password into a profile. A sandbox must have access
  to a Secret Service to use this optional feature.
- Older profiles still load, including the old C-style escaped format. Existing
  plaintext passwords disappear from a profile when you save over that file;
  other profile files and backups are not rewritten automatically.

Sessions and tests:
- Connections run without blocking the GTK main loop. The Connect button becomes
  Cancel while a session is running; cancellation or exiting terminates the
  direct client process. Client-created child processes are not managed.
- Build with `autoreconf -fi && ./configure --prefix=/usr && make`.
- Run `GTK_A11Y=none GTK_IM_MODULE=simple GSK_RENDERER=cairo xvfb-run -a make check`.
  Tests use temporary profiles and mock clients/keyrings, not live servers.

Credential integration tests:
- `test-connection-credentials` is included in `make check`. It exercises the
  real connection flow with mocked keyring, profile writes, sessions, and
  application hold/release calls. All regression cases run by default, including
  cancellation, late callbacks, legacy-password migration, explicit empty-secret
  storage, and clearing manually entered passwords when identity changes.
- To build and run just this test without regenerating build files, run from
  the repository root:

  ```sh
  cc -std=c11 -Wall -Wextra -Werror -g -Isrc \
    $(pkg-config --cflags gtk4) tests/test-connection-credentials.c \
    $(pkg-config --libs gtk4) -o /tmp/test-connection-credentials
  GTK_A11Y=none GTK_IM_MODULE=simple GSK_RENDERER=cairo \
    xvfb-run -a /tmp/test-connection-credentials
  ```

- For an optional real libsecret lifecycle test, run
  `./tests/run-credentials-keyring.sh`. It requires the development dependencies
  above, `dbus-run-session`, `gdbus`, and `gnome-keyring-daemon`. The runner uses
  synthetic secrets, a private session bus and keyring daemon, and temporary
  HOME/XDG/runtime directories. It strips inherited session/keyring/socket
  variables and removes the temporary state afterward. Never run the underlying
  `test-credentials-keyring.c` executable directly against a desktop session.
  This isolated lifecycle test is separate from `make check`.
