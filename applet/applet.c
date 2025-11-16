/***************************************
*                                      *
*   tsclient applet code               *
*                                      *
***************************************/


#include <config.h>

#ifdef HAVE_GNOME

#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomeui/gnome-window-icon.h>
#include <panel-applet.h>

#include <libintl.h>
#include <locale.h>

#include "applet.h"
#include "../src/mrulist.h"
#include "../src/rdpfile.h"
#include "../src/support.h"

AppletData *g_data;


/***************************************
*                                      *
*   main() code                        *
*                                      *
***************************************/
PANEL_APPLET_BONOBO_FACTORY (APPLET_FACTORY_IID,
                            PANEL_TYPE_APPLET,
                            _("Terminal Server Client Applet"),
                            "0", 
                            tsclient_applet_factory, 
                            NULL)


static gboolean tsclient_applet_factory (PanelApplet *applet, 
                                         const gchar *iid, 
                                         gpointer data)
{
  if (strcmp (iid, APPLET_IID) != 0)
    return FALSE;
  tsclient_applet_create (applet);
  return TRUE;
}


/***************************************
*                                      *
*   interface code                     *
*                                      *
***************************************/
void tsclient_applet_create (PanelApplet *applet)
{
  BonoboUIComponent *component;
  int i = 0;

  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (PACKAGE, "UTF-8");
  textdomain (PACKAGE);

  // make sure our dirs & files exist
  tsc_check_files ();

  g_data = g_new0 (AppletData, 1);

  if (run_tsclient_cmd == NULL)
    run_tsclient_cmd = g_find_program_in_path ("tsclient");

  // Setup the applet frame & icon
  add_pixmap_directory (PACKAGE_DATA_DIR "/tsclient/icons/");
  g_data->imgsrc = create_pixbuf ("tsclient_64x64.png");
	//gnome_window_icon_set_default_from_file (PACKAGE_DATA_DIR "/" PACKAGE "/tsclient/icons/tsclient_256x256.png");
	gnome_window_icon_set_default_from_file (PACKAGE_DATA_DIR "/tsclient/icons/tsclient_256x256.png");

  g_data->frame = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (g_data->frame), 0);
  gtk_frame_set_shadow_type (GTK_FRAME (g_data->frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add (GTK_CONTAINER (applet), g_data->frame);
  gtk_widget_show (g_data->frame);

  g_data->image = create_pixmap (g_data->image, "tsclient_64x64.png");
  gtk_container_add (GTK_CONTAINER (g_data->frame), g_data->image);
  gtk_widget_show (g_data->image);

  g_data->applet = applet;

  g_data->tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (g_data->tooltips,
          GTK_WIDGET(g_data->applet), 
          _("Terminal Server Client Applet"), NULL);
  
  
  // applet callbacks
  g_signal_connect (GTK_WIDGET (g_data->applet),
        "button-release-event",
        G_CALLBACK (applet_popup_show), g_data);

  g_signal_connect (GTK_WIDGET(g_data->applet),
        "key-press-event",
        G_CALLBACK (applet_key_press_event), g_data);
  
  g_signal_connect (GTK_WIDGET(g_data->applet),
        "destroy", 
        G_CALLBACK (applet_destroy), g_data);

  g_signal_connect (GTK_WIDGET(g_data->applet),
        "change_size",
        G_CALLBACK (applet_change_size), g_data);

	g_signal_connect (GTK_WIDGET(g_data->applet),
			  "change_orient",
			  G_CALLBACK (applet_change_orient), g_data);

  g_signal_connect (GTK_WIDGET(g_data->applet),
        "change_background",
        G_CALLBACK(applet_change_background), g_data);

  panel_applet_setup_menu (g_data->applet,
        applet_menu_xml,
        tsclient_applet_menu_verbs,
        g_data);

  component = panel_applet_get_popup_component (g_data->applet);

  if (run_tsclient_cmd == NULL)
    bonobo_ui_component_rm (component, "/popups/popup/RunTSClient", NULL);

	applet_change_orient (GTK_WIDGET (g_data->applet),
				 panel_applet_get_orient (g_data->applet), g_data);
  
  applet_change_size (GTK_WIDGET (g_data->applet),
      panel_applet_get_size (g_data->applet), g_data);

  gtk_widget_show_all (GTK_WIDGET (g_data->applet));
}

void
create_menu (gpointer key, gpointer value, gpointer menu)
{
  GtkMenu* submenu = NULL;
  GtkMenuItem *subitem =NULL;
  GtkMenuItem* itemfound = NULL;
  
  GList* list = NULL;
  gchar* label_text = NULL;
  gchar *name = g_strdup (key);
  gchar *value_c = g_strdup (value);
  
  GtkMenuItem *mi = (GtkMenuItem *) gtk_menu_item_new_with_label (name);
	
  list = gtk_container_get_children (GTK_CONTAINER(menu));
  itemfound = NULL;  
  for ( ; (list != NULL) && (itemfound == NULL) ; list = g_list_next (list)) {
    if (GTK_IS_MENU_ITEM(list->data)) {
      if (GTK_IS_LABEL(GTK_BIN(list->data)->child)) {
	      label_text = gtk_label_get_text (GTK_LABEL(GTK_BIN(list->data)->child));
	      if (g_ascii_strcasecmp (label_text, value_c) == 0)
	        itemfound = GTK_MENU_ITEM(list->data);
      }
    }
  }

  if (g_ascii_strcasecmp (value_c, "-1") != 0){
	  
  	g_signal_connect (GTK_WIDGET (mi), "activate",GTK_SIGNAL_FUNC (applet_menu_item),
	  	g_build_path ("/", value_c, name, NULL));
	  
    if (itemfound != NULL) {
	    submenu = gtk_menu_item_get_submenu (itemfound);
	    subitem = itemfound;
    } else {
	    submenu = GTK_MENU (gtk_menu_new ());
	    subitem = (GtkMenuItem *) gtk_menu_item_new_with_label (value_c);
	    gtk_menu_shell_append ((GtkMenuShell *) menu, GTK_WIDGET (subitem));
	    gtk_widget_show (GTK_WIDGET (subitem));
    }
		
    gtk_menu_item_set_submenu (subitem, GTK_WIDGET (submenu));
    gtk_menu_shell_append ((GtkMenuShell *) submenu, GTK_WIDGET (mi));
    
    gtk_widget_show (GTK_WIDGET (mi));  
  } else {
  	g_signal_connect (GTK_WIDGET (mi), "activate",GTK_SIGNAL_FUNC (applet_menu_item), name);
    if (itemfound == NULL) {
      gtk_menu_shell_append ((GtkMenuShell *) menu, GTK_WIDGET (mi));
      gtk_widget_show (GTK_WIDGET (mi));
    }
  }
}


gboolean applet_popup_show (GtkWidget  *widget,
                            GdkEvent   *event,
                            AppletData *data)
{
  GtkMenu   *menu = NULL;
  GtkMenuItem *mi = NULL;
  GHashTable* hash=NULL;
  
  g_return_val_if_fail (widget != NULL, FALSE) ;
  g_return_val_if_fail (event != NULL, FALSE) ;
  g_return_val_if_fail (data != NULL, FALSE) ;

  switch (event->type) {
  case GDK_BUTTON_RELEASE :
    if (event->button.button == 1) { 
      /*user pressed the left mouse button*/

	  hash = rdp_files_to_hash ();
	  menu = GTK_MENU (gtk_menu_new ());

	  g_hash_table_foreach (hash, create_menu, menu);

	  mi = (GtkMenuItem *) gtk_separator_menu_item_new ();
	  gtk_menu_shell_append ((GtkMenuShell *) menu, GTK_WIDGET (mi));
	  gtk_widget_show (GTK_WIDGET (mi));

	  mi = (GtkMenuItem *) gtk_menu_item_new_with_label (_("New Connection..."));
	  g_signal_connect (GTK_WIDGET(mi), "activate", GTK_SIGNAL_FUNC(applet_launch_tsclient), NULL);
	  gtk_menu_shell_append ((GtkMenuShell *) menu, GTK_WIDGET (mi));
	  gtk_widget_show (GTK_WIDGET (mi));

      gtk_menu_popup (menu, NULL, NULL, NULL, data,
			  event->button.button,
			  gtk_get_current_event_time ());
	  
	  data->popup = menu;
	  g_hash_table_destroy (hash);
	  
      return TRUE;
    }
    break ;
  default :
    break ;
  }
  return FALSE;
}


/***************************************
*                                      *
*   applet_popup_hide                *
*                                      *
***************************************/
void applet_popup_hide (AppletData *data, gboolean revert)
{
  gint vol;

  if (data->popup) {
    gdk_pointer_ungrab (GDK_CURRENT_TIME);
    gdk_keyboard_ungrab (GDK_CURRENT_TIME);
  }
}


/***************************************
*                                      *
*   applet_menu_item                   *
*                                      *
***************************************/
void applet_menu_item (GtkMenuItem *menuitem, gpointer user_data)
{
  gchar *file_name;
  rdp_file *rdp = NULL;
  GtkWidget *dialog;
  // build path to file
  file_name = g_build_path ((const gchar *)"/", g_get_home_dir (), ".tsclient", g_strdup (user_data), NULL);

  // check for file in ~/
  if (g_file_test (file_name, G_FILE_TEST_EXISTS)) {
    // load if exists
    rdp = g_new (rdp_file, 1);
    rdp_file_init (rdp);
    rdp_file_load (rdp, file_name);
    gchar** error = g_malloc(sizeof(gchar*));
    tsc_launch_remote (rdp, 1, error);
    if(*error) {
	tsc_error_message(*error);
	g_free(*error);
    }
    g_free (error);
    g_free (rdp);
  }
  g_free (file_name);
  return;
}


gboolean applet_key_press_event (GtkWidget *widget, GdkEventKey *event, AppletData *data)
{
  switch (event->keyval) {
  case GDK_Escape:
    /* Revert. */
    applet_popup_hide (data, TRUE);
    return TRUE;
  case GDK_KP_Enter:
  case GDK_ISO_Enter:
  case GDK_3270_Enter:
  case GDK_Return:
  case GDK_space:
  case GDK_KP_Space:
    /* Apply. */
    if (data->popup != NULL)
      applet_popup_hide (data, FALSE);
    else
      applet_popup_show (widget, (GdkEvent *)event, data);
    return TRUE;
  default:
    break;
  }
  return FALSE;
}


void applet_destroy (GtkWidget *widget, AppletData *data)
{
  if (data->timeout != 0) {
    gtk_timeout_remove (data->timeout);
    data->timeout = 0;
  }
  g_free (data);
}


void applet_change_background (PanelApplet *applet,
          PanelAppletBackgroundType type,
          GdkColor *color,
          const gchar *pixmap, AppletData *data)
{
  GtkRcStyle *rc_style;
  if (type == PANEL_NO_BACKGROUND) {
    rc_style = gtk_rc_style_new ();
    gtk_widget_modify_style (GTK_WIDGET (data->applet), rc_style);
    gtk_rc_style_unref (rc_style);
  } else if (type == PANEL_COLOR_BACKGROUND) {
    gtk_widget_modify_bg (GTK_WIDGET (data->applet), GTK_STATE_NORMAL, color);
  }
}


void applet_change_size (GtkWidget *w, gint size, AppletData *data)
{
	GdkPixbuf *pixbuf, *scaled = NULL;

  pixbuf = gdk_pixbuf_copy (data->imgsrc);

  applet_popup_hide (data, FALSE);

	scaled = gdk_pixbuf_scale_simple (pixbuf, size - 3, size - 3, GDK_INTERP_HYPER);  // GDK_INTERP_BILINEAR
	gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), scaled);
	g_object_unref (scaled);
	gtk_widget_set_size_request (GTK_WIDGET (data->frame), 
					     MAX (11, size), MAX (11, size));
	g_object_unref (pixbuf);

}


void applet_change_orient (GtkWidget *w, PanelAppletOrient o, AppletData *data)
{
	applet_popup_hide (data, FALSE);
	data->orientation = o;
}


void applet_launch_tsclient (BonoboUIComponent *uic, AppletData *data, const gchar *verbname)
{
  char *c_argv[MAX_ARGVS];
  int c_argc = 0;
  GError *error = NULL;

  if (!run_tsclient_cmd) return;

  c_argv[c_argc++] = strdup (run_tsclient_cmd);
  c_argv[c_argc++] = NULL;

  // complete events in gtk queue
  while (gtk_events_pending ())
    gtk_main_iteration ();

  if (!g_spawn_async (NULL, (gchar**)c_argv, NULL, G_SPAWN_SEARCH_PATH,
        NULL, NULL, NULL, &error))
    printf ("failed: spawn_async\n");

  if (error) {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (NULL,
          GTK_DIALOG_DESTROY_WITH_PARENT,
          GTK_MESSAGE_ERROR,
          GTK_BUTTONS_CLOSE,
          "There was an error executing '%s' :\n %s",
          run_tsclient_cmd, error->message);

    g_signal_connect (dialog, "response",
    G_CALLBACK (gtk_widget_destroy), NULL);

    gtk_window_set_resizable (GTK_WINDOW(dialog), FALSE);
    gtk_widget_show (dialog);
    g_error_free (error);
  }
}


void applet_about (BonoboUIComponent *uic, AppletData *data, const gchar *verbname)
{
  tsc_about_dialog ();
}


/* Accessible name and description */
void add_atk_namedesc (GtkWidget *widget, const gchar *name, const gchar *desc)
{
  AtkObject *atk_obj;
  atk_obj = gtk_widget_get_accessible (widget);
  atk_object_set_name (atk_obj, name);
  atk_object_set_description (atk_obj, desc);
}
#else
int main(){return 0;}
#endif
