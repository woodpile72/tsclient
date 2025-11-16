const Applet = imports.ui.applet;
const PopupMenu = imports.ui.popupMenu;
const Util = imports.misc.util;
const GLib = imports.gi.GLib;
const ByteArray = imports.byteArray;
const Gio = imports.gi.Gio;

function _(str) {
    return str;
}

class TsclientApplet extends Applet.IconApplet {
    constructor(metadata, orientation, panelHeight, instanceId) {
        super(orientation, panelHeight, instanceId);
        this.set_applet_icon_name('tsclient');
        this.set_applet_tooltip('Terminal Server Client');

        this.menuManager = new PopupMenu.PopupMenuManager(this);
        this.menu = new Applet.AppletPopupMenu(this, orientation);
        this.menuManager.addMenu(this.menu);

        this._buildMenu();
    }

    on_applet_clicked(event) {
        this.menu.toggle();
    }

    _buildMenu() {
        this.menu.removeAll();

        const openItem = new PopupMenu.PopupMenuItem(_('Open tsclient…'));
        openItem.connect('activate', () => this._launchTsclient());
        this.menu.addMenuItem(openItem);

        const refreshItem = new PopupMenu.PopupMenuItem(_('Refresh Profiles'));
        refreshItem.connect('activate', () => this._rebuildMruSection());
        this.menu.addMenuItem(refreshItem);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        this._mruSection = new PopupMenu.PopupMenuSection();
        this.menu.addMenuItem(this._mruSection);
        this._rebuildMruSection();
    }

    _launchTsclient(profilePath = null) {
        if (profilePath) {
            const command = `tsclient -x ${GLib.shell_quote(profilePath)}`;
            Util.spawnCommandLine(command);
        } else {
            Util.spawnCommandLine('tsclient');
        }
    }

    _rebuildMruSection() {
        this._mruSection.removeAll();
        const entries = this._loadMruEntries();
        if (entries.length === 0) {
            const empty = new PopupMenu.PopupMenuItem(_('No saved profiles'), { reactive: false });
            this._mruSection.addMenuItem(empty);
            return;
        }

        entries.forEach(path => {
            const item = new PopupMenu.PopupMenuItem(path);
            item.connect('activate', () => this._launchTsclient(path));
            this._mruSection.addMenuItem(item);
        });
    }

    _loadMruEntries() {
        const dir = GLib.build_filenamev([GLib.get_home_dir(), '.tsclient']);
        const filename = GLib.build_filenamev([dir, 'mru.tsc']);
        try {
            const file = Gio.File.new_for_path(filename);
            const [ok, contents] = file.load_contents(null);
            if (!ok)
                return [];
            const text = ByteArray.toString(contents);
            const data = JSON.parse(text);
            if (Array.isArray(data))
                return data;
        } catch (e) {
            global.log(`tsclient applet: ${e}`);
        }
        return [];
    }
}

function main(metadata, orientation, panelHeight, instanceId) {
    return new TsclientApplet(metadata, orientation, panelHeight, instanceId);
}
