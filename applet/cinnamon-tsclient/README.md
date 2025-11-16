# tsclient Cinnamon Applet

This Cinnamon panel applet gives you a small launcher for the GTK4 tsclient
frontend.  It offers two quick actions:

* Launch the tsclient UI
* Reconnect to any profile listed in `~/.tsclient/mru.tsc`

## Installation

1. Create the Cinnamon applet directory if needed:
   `mkdir -p ~/.local/share/cinnamon/applets`
2. Copy this folder (`cinnamon-tsclient`) into
   `~/.local/share/cinnamon/applets/tsclient@cinnamon`.
3. Restart Cinnamon (`Alt+F2`, type `r`, and press Enter) or log out/in.
4. Right-click the panel ➜ *Add applets to the panel…* ➜ enable
   **Terminal Server Client**.

The applet reads the JSON MRU file written by tsclient 3.0+.  If your MRU list
is empty the menu will display a placeholder reminder.
