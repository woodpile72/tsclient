#define TSCLIENT_DEFAULT_ICON "tsclient"
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h> 

#include <gtk/gtk.h>
#include "freerdp.h"
#include <gdk/gdk.h>
#include <glib.h>
#include <glib/gprintf.h>

#include <glib/gi18n.h>

#include <sys/types.h> 
#include <sys/stat.h> 
#include <dirent.h> 
#include <fcntl.h> 

#include <arpa/inet.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <errno.h>
#include <signal.h>

#include "rdpfile.h"
#include "support.h"
#include "connect.h"

/* Stolen from jirka's vicious lib */
#define VE_IGNORE_EINTR(expr)                   \
        do {                                    \
                errno = 0;                      \
                expr;                           \
        } while G_UNLIKELY (errno == EINTR);


GtkWidget *lookup_widget (GtkWidget *widget, const gchar *widget_name)
{
  GtkRoot *root = gtk_widget_get_root (widget);
  GtkWidget *found = g_object_get_data (G_OBJECT (root ? GTK_WIDGET (root) : widget), widget_name);
  if (!found) g_warning ("Widget not found: %s", widget_name);
  return found;
}


static GList *pixmaps_directories = NULL;
static gboolean tsc_icon_paths_registered = FALSE;

static gboolean
tsc_pixmap_debug_enabled (void)
{
  static gint state = -1;
  if (state == -1)
    state = g_getenv ("TSCLIENT_PIXMAP_DEBUG") ? 1 : 0;
  return state == 1;
}

/* Use this function to set the directory containing installed pixmaps. */
void add_pixmap_directory (const gchar *directory) {
  pixmaps_directories = g_list_prepend (pixmaps_directories, g_strdup (directory));
  if (tsc_pixmap_debug_enabled ())
    g_message ("tsclient pixmap dir: %s", directory);
}


/* This is an internally used function to find pixmap files. */
gchar* find_pixmap_file (const gchar *filename) {
  GList *elem;
  gchar *pathname = NULL;
  static const gchar *fallback_dirs[] = {
    "icons",
    "./icons",
    "/usr/share/tsclient/icons",
    "/app/share/tsclient/icons",
    NULL
  };
  /* We step through each of the pixmaps directory to find it. */
  elem = pixmaps_directories;
  while (elem) {
    gchar *candidate = g_strdup_printf ("%s%s%s", (gchar*)elem->data,
                                        G_DIR_SEPARATOR_S, filename);
    if (g_file_test (candidate, G_FILE_TEST_EXISTS)) {
      pathname = candidate;
      break;
    }
    g_free (candidate);
    elem = elem->next;
  }

  if (!pathname) {
    for (const gchar **dir = fallback_dirs; *dir; dir++) {
      gchar *candidate = g_build_filename (*dir, filename, NULL);
      if (g_file_test (candidate, G_FILE_TEST_EXISTS)) {
        pathname = candidate;
        break;
      }
      g_free (candidate);
    }
  }
  if (tsc_pixmap_debug_enabled ()) {
    if (pathname)
      g_message ("tsclient pixmap found: %s -> %s", filename, pathname);
    else
      g_message ("tsclient pixmap missing: %s", filename);
  }
  return pathname;
}


/* This is an internally used function to create pixmaps. */
GtkWidget* create_pixmap (GtkWidget *widget, const gchar *filename) {
  gchar *pathname = NULL;
  GtkWidget *pixmap;

  if (!filename || !filename[0])
      return gtk_image_new ();

  pathname = find_pixmap_file (filename);
  if (!pathname) {
    g_warning ("Couldn't find pixmap file: %s", filename);
    return gtk_image_new ();
  }
  pixmap = gtk_image_new_from_file (pathname);
  g_free (pathname);
  return pixmap;
}


void
tsc_register_icon_theme_dirs (void)
{
  if (tsc_icon_paths_registered)
    return;

  GdkDisplay *display = gdk_display_get_default ();
  if (!display)
    return;

  GtkIconTheme *theme = gtk_icon_theme_get_for_display (display);
  if (!theme)
    return;

  for (GList *iter = pixmaps_directories; iter != NULL; iter = iter->next) {
    const gchar *dir = iter->data;
    if (!dir || !g_file_test (dir, G_FILE_TEST_IS_DIR))
      continue;
    gtk_icon_theme_add_search_path (theme, dir);
  }

  tsc_icon_paths_registered = TRUE;
}


/* This is an internally used function to create pixmaps. */
GdkPixbuf* create_pixbuf (const gchar *filename) {
  gchar *pathname = NULL;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  if (!filename || !filename[0])
      return NULL;

  pathname = find_pixmap_file (filename);

  if (!pathname)
    {
      g_warning ("Couldn't find pixmap file: %s", filename);
      return NULL;
    }

  pixbuf = gdk_pixbuf_new_from_file (pathname, &error);
  if (!pixbuf)
    {
      fprintf (stderr, "Failed to load pixbuf file: %s: %s\n",
               pathname, error->message);
      g_error_free (error);
    }
  g_free (pathname);
  return pixbuf;
}


GtkWidget *
tsc_dropdown_new (const gchar *const *items) {
  GtkStringList *model = gtk_string_list_new (NULL);
  guint i;

  if (items) {
    for (i = 0; items[i]; i++) {
      gtk_string_list_append (model, items[i]);
    }
  }

  GtkWidget *dropdown = gtk_drop_down_new (G_LIST_MODEL (model), NULL);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), 0);
  return dropdown;
}

GtkStringList *
tsc_dropdown_get_model (GtkWidget *dropdown) {
  GtkStringList *model = GTK_STRING_LIST (gtk_drop_down_get_model (GTK_DROP_DOWN (dropdown)));
  return model;
}

guint
tsc_dropdown_get_selected (GtkWidget *dropdown) {
  return gtk_drop_down_get_selected (GTK_DROP_DOWN (dropdown));
}

void
tsc_dropdown_set_selected (GtkWidget *dropdown, guint idx) {
  gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), idx);
}

gchar *
tsc_dropdown_get_selected_text (GtkWidget *dropdown) {
  GObject *item = gtk_drop_down_get_selected_item (GTK_DROP_DOWN (dropdown));
  if (!item)
    return NULL;
  return g_strdup (gtk_string_object_get_string (GTK_STRING_OBJECT (item)));
}

const gchar *
tsc_dropdown_get_string (GtkWidget *dropdown, guint idx) {
  GtkStringList *model = tsc_dropdown_get_model (dropdown);
  if (!model)
    return NULL;
  if (idx >= g_list_model_get_n_items (G_LIST_MODEL (model)))
    return NULL;
  return gtk_string_list_get_string (model, idx);
}

void
tsc_dropdown_append (GtkWidget *dropdown, const gchar *label) {
  GtkStringList *model = tsc_dropdown_get_model (dropdown);
  if (!model)
    return;
  gtk_string_list_append (model, label);
}

void
tsc_dropdown_clear (GtkWidget *dropdown) {
  GtkStringList *model = tsc_dropdown_get_model (dropdown);
  if (!model)
    return;
  guint items = g_list_model_get_n_items (G_LIST_MODEL (model));
  while (items > 0) {
    gtk_string_list_remove (model, items - 1);
    items--;
  }
}


/***************************************
*                                      *
*   tsc_check_files                    *
*                                      *
***************************************/
 
int tsc_check_files () {
  gchar *home = tsc_home_path ();
  gchar *file_name = g_build_path ("/", home, "mru.tsc", NULL);
  int ret = 0;

  #ifdef TSCLIENT_DEBUG
  printf ("tsc_check_files\n");
  #endif

  // create .tsclient dir in ~/
  mkdir (home, 0700);

  // create mru.tsc dir in ~/.tsclient/
  if (!g_file_test (file_name, G_FILE_TEST_EXISTS)) {
    ret = open (file_name, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (ret > 0) close (ret);
  }

  // create last.tsc dir in ~/.tsclient/
  if (!g_file_test (file_name, G_FILE_TEST_EXISTS)) {
    ret = open (file_name, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (ret > 0) close (ret);
  }

  /* clean up */
  g_free (file_name);
  g_free (home);
  /* complete successfully */
  return 0;
}


/***************************************
*                                      *
*   tsc_home_path                      *
*                                      *
***************************************/
 
gchar *tsc_home_path () {

  #ifdef TSCLIENT_DEBUG
  printf ("tsc_home_path\n");
  #endif

  return g_build_path ("/", g_get_home_dir(), ".tsclient", NULL);

}


void tsc_error_message (gchar *message)
{
  GtkAlertDialog *alert;

  #ifdef TSCLIENT_DEBUG
  printf ("tsc_error_message\n");
  #endif

  alert = gtk_alert_dialog_new (_("An error has occurred."));
  gtk_alert_dialog_set_detail (alert, message ? message : "");
  gtk_alert_dialog_show (alert,
                         gConnect ? GTK_WINDOW (gConnect) : NULL);
  g_object_unref (alert);
}


void tsc_about_dialog ()
{
  const gchar *name      = _("Terminal Server Client");
  const gchar *copyright = "(C) 2002-2025 Erick Woods";
  const gchar *website = "http://www.erick.com/";

  const gchar *authors[] = {
			   "Erick Woods <erick@gnomepro.com>",
			   "Kyle Davis <kyle@gnomeuser.org>",
			   "Jonh Wendell <wendell@bani.com.br>",
			   NULL };

  const gchar *artists[] = {
			   "Erick Woods <erick@gnomepro.com>",
			   "Jakub Steiner",
			   NULL };

  const gchar *license = _(\
    "Terminal Server Client is licensed under the \n" \
    "GNU General Public License (GPL)\n<http://www.gnu.org/licenses/gpl.html>");

  const gchar *comments = \
    _("Terminal Server Client is a frontend for \nrdesktop, FreeRDP, vncviewer, wfica and xnest.");
  
  gchar *translators;
  /* Translators comment: put your own name here to appear in the about dialog. */
  translators = _("translator-credits");

  if (!strcmp (translators, "translator-credits"))
    translators = NULL;

  gtk_show_about_dialog (NULL,
			 "name",               name,
			 "comments",           comments,
			 "version",            VERSION,
			 "license",            license,
			 "authors",            authors,
			 "translator-credits", translators,
			 "logo-icon-name",     "tsclient",
			 "copyright",		copyright,
			 "website",		website,
			 "artists",		artists,
			 NULL);
}


void tsc_quick_pick_activate (GtkDropDown *widget, GParamSpec *pspec, gpointer user_data)
{
  gchar *file_name, *rdp_name;
  gchar *home = tsc_home_path ();
  rdp_file *rdp = NULL;
  (void) pspec;
  (void) user_data;

  #ifdef TSCLIENT_DEBUG
  printf ("tsc_quick_pick_activate\n");
  #endif

  /* Do nothing if it's the first element (just a instruction)*/
  if (gtk_drop_down_get_selected (widget) == 0 ||
      gtk_drop_down_get_selected (widget) == GTK_INVALID_LIST_POSITION) {
    g_free (home);
    return;
  }

  rdp_name = tsc_dropdown_get_selected_text (GTK_WIDGET (widget));

  // build path to file
  file_name = g_build_path ("/", home, (const gchar *)rdp_name, NULL);
  // check for file in ~/
  if (g_file_test (file_name, G_FILE_TEST_EXISTS)) {
    rdp = g_new0 (rdp_file, 1);
    rdp_file_init (rdp);
    if (rdp_file_load (rdp, file_name) == 0) {
      if (gConnect)
        rdp_file_set_screen (rdp, gConnect);
    } else {
      g_warning ("Failed to load profile %s", file_name);
    }
    rdp_file_free (rdp);
  } else {
    g_warning ("Quick connect profile missing: %s", file_name);
  }
  g_free (file_name);
  g_free (home);
  g_free (rdp_name);

  gtk_drop_down_set_selected (widget, 0);

  return;
}

void
tsc_set_protocol_widgets (GtkWidget *main_win, gint protocol)
{
  #ifdef TSCLIENT_DEBUG
  printf ("tsc_set_protocol_widgets: %d\n", protocol);
  #endif

  gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "btnProtoFile"), TRUE);
  gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProtoFile"), TRUE);
  
  gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "vbxSound"), TRUE);
  
  gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "chkStartProgram"), TRUE);
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (g_object_get_data (G_OBJECT (main_win), "chkStartProgram")))) {
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProgramPath"), TRUE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtStartFolder"), TRUE);
  } else {
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProgramPath"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtStartFolder"), FALSE);
  }
  gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "chkHideWMDecorations"), FALSE);

  /* These controls use rdesktop-specific semantics or flags. */
  const gchar *legacy_controls[] = {
    "chkDisableEncryption", "chkDisableClientEncryption", "optTlsVersion",
    "chkForceBitmap", "chkBackingStore", "chkDisableCtrl", "chkSyncNumlock",
    "txtKeyboardLang", "txtCodepage", "chkBitmapCache", "chkNoMotionEvents", NULL
  };
  for (guint i = 0; legacy_controls[i]; i++) {
    GtkWidget *control = g_object_get_data (G_OBJECT (main_win), legacy_controls[i]);
    if (control) gtk_widget_set_sensitive (control, protocol != 5);
  }

  switch (protocol) {
  case 0:  // rdp v4
  case 5:  // FreeRDP
  case 4:  // rdp v5
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtPassword"), TRUE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "btnProtoFile"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProtoFile"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "chkHideWMDecorations"), TRUE);
    break;
  case 1:  // vnc
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtPassword"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "vbxSound"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "chkStartProgram"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProgramPath"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtStartFolder"), FALSE);
    break;
  case 2:  // xdmcp
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtPassword"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "btnProtoFile"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProtoFile"), FALSE);

    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "vbxSound"), FALSE);

    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "chkStartProgram"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProgramPath"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtStartFolder"), FALSE);
    break;
  case 3:  // ica
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtPassword"), TRUE);
    break;
  default:
    break;
  }
  
}
