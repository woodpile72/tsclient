/***************************************
*                                      *
*   tsclient applet code               *
*                                      *
***************************************/

#define GNOMELOCALEDIR "/usr/share/locale"

#define IS_PANEL_HORIZONTAL(x) (x == PANEL_APPLET_ORIENT_UP || x == PANEL_APPLET_ORIENT_DOWN)

#define APPLET_IID         "OAFIID:GNOME_TSClientApplet"
#define APPLET_FACTORY_IID "OAFIID:GNOME_TSClientApplet_Factory"
#define APPLET_UI          "GNOME_TSClientApplet.xml"

static gchar *run_tsclient_cmd = NULL;

static const char applet_menu_xml [] =
        "<popup name=\"button3\">\n"
        "  <menuitem name=\"RunTSClient\" verb=\"RunTSClient\" _label=\"_Run Terminal Server Client ...\"/>\n"
        "  <menuitem name=\"About\" verb=\"About\" _label=\"_About ...\" pixtype=\"stock\" pixname=\"gnome-stock-about\"/>\n"
        "</popup>\n";


typedef struct {
  PanelApplet *applet;
  PanelAppletOrient orientation;
  gint timeout;

  // The menu and scale
  GtkMenu     *popup;
  GtkWidget   *frame;
  //GtkWidget   *event_box;
  GtkWidget   *image;
  GdkPixbuf	  *imgsrc;
  GtkTooltips *tooltips;
} AppletData;


static gboolean tsclient_applet_factory (PanelApplet *applet, const gchar *iid, gpointer data);

void tsclient_applet_create (PanelApplet *applet);

gboolean applet_popup_show (GtkWidget *widget, GdkEvent *event, AppletData *data);

void applet_popup_hide (AppletData *data, gboolean revert);

void applet_menu_item (GtkMenuItem *menuitem, gpointer user_data);

gboolean applet_key_press_event (GtkWidget *widget, GdkEventKey *event, AppletData *data);

void applet_destroy (GtkWidget *widget, AppletData *data);

void applet_change_background (PanelApplet *applet,
          PanelAppletBackgroundType type,
          GdkColor *color,
          const gchar *pixmap, AppletData *data);

void applet_change_size (GtkWidget *w, gint size, AppletData *data);

void applet_change_orient (GtkWidget *w, PanelAppletOrient o, AppletData *data);

void applet_launch_tsclient (BonoboUIComponent *uic, AppletData *data, const gchar *verbname);

void applet_about (BonoboUIComponent *uic, AppletData *data, const gchar *verbname);

const BonoboUIVerb tsclient_applet_menu_verbs[] = {
    BONOBO_UI_UNSAFE_VERB("RunTSClient", applet_launch_tsclient),
    BONOBO_UI_UNSAFE_VERB("About", applet_about),
    BONOBO_UI_VERB_END
};

void add_atk_namedesc (GtkWidget *widget, const gchar *name, const gchar *desc);

