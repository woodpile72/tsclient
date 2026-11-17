#!/bin/sh
# Run from any directory. Requires cc, pkg-config, gio/libsecret development
# packages, dbus-run-session, gdbus, and gnome-keyring-daemon.
# No inherited session variables or keyring paths enter the private session.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
private=$(mktemp -d /tmp/tsc-private-keyring.XXXXXXXX)
trap 'rm -rf -- "$private"' EXIT HUP INT TERM
mkdir -m 700 "$private/home" "$private/config" "$private/data" \
  "$private/cache" "$private/runtime" "$private/control"
cc -std=c11 -Wall -Wextra -Werror -g -I"$root/src" \
  $(pkg-config --cflags gio-2.0 libsecret-1) \
  "$root/src/credentials.c" "$root/tests/test-credentials-keyring.c" \
  $(pkg-config --libs gio-2.0 libsecret-1) -o "$private/test"
# env -i explicitly removes DBUS_SESSION_BUS_ADDRESS, GNOME_KEYRING_CONTROL,
# SSH_AUTH_SOCK and every other inherited session variable before creating bus.
env -i PATH=/usr/bin:/bin LANG=C.UTF-8 \
  HOME="$private/home" XDG_CONFIG_HOME="$private/config" \
  XDG_DATA_HOME="$private/data" XDG_CACHE_HOME="$private/cache" \
  XDG_RUNTIME_DIR="$private/runtime" TSC_TEST_PRIVATE_KEYRING=1 \
  dbus-run-session -- sh -eu -c '
    private=$1
    printf "%s" "synthetic-keyring-unlock-only" |
      gnome-keyring-daemon --foreground --unlock --components=secrets \
        --control-directory="$private/control" >"$private/daemon.log" 2>&1 &
    daemon=$!
    trap '\''kill "$daemon" 2>/dev/null || :; wait "$daemon" 2>/dev/null || :'\'' EXIT HUP INT TERM
    attempt=0
    until gdbus call --session --dest org.freedesktop.DBus \
        --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.GetNameOwner \
        org.freedesktop.secrets >/dev/null 2>&1; do
      attempt=$((attempt + 1))
      if [ "$attempt" -ge 100 ] || ! kill -0 "$daemon" 2>/dev/null; then
        echo "Private keyring failed to start" >&2
        exit 1
      fi
      sleep 0.1
    done
    "$private/test"
  ' sh "$private"
