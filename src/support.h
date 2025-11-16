
#ifndef SUPPORT_H
#define SUPPORT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include "gtkcompat.h"

#ifdef TSCLIENT_DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

#define TSC_WM_COMPLEX 0
#define TSC_WM_COMPACT 1
#define MAX_ARGVS 16
#define MAX_ARGV_LEN 255

#define HOOKUP_OBJECT(component,widget,name) \
  g_object_set_data (G_OBJECT (component), name, widget)

/*
 * This function returns a widget in a component created by Glade.
 * Call it with the toplevel widget in the component (i.e. a window/dialog),
 * or alternatively any widget in the component, and the name of the widget
 * you want returned.
 */
GtkWidget* lookup_widget (GtkWidget *widget, const gchar *widget_name);

gchar* find_pixmap_file (const gchar *filename);

/* Use this function to set the directory containing installed pixmaps. */
void add_pixmap_directory (const gchar *directory);
void tsc_register_icon_theme_dirs (void);

/* This is used to create the pixmaps used in the interface. */
GtkWidget* create_pixmap (GtkWidget *widget, const gchar *filename);

/* This is used to create the pixbufs used in the interface. */
GdkPixbuf* create_pixbuf (const gchar *filename);

GtkWidget *tsc_dropdown_new (const gchar *const *items);
guint tsc_dropdown_get_selected (GtkWidget *dropdown);
void tsc_dropdown_set_selected (GtkWidget *dropdown, guint idx);
GtkStringList *tsc_dropdown_get_model (GtkWidget *dropdown);
gchar *tsc_dropdown_get_selected_text (GtkWidget *dropdown);
const gchar *tsc_dropdown_get_string (GtkWidget *dropdown, guint idx);
void tsc_dropdown_append (GtkWidget *dropdown, const gchar *label);
void tsc_dropdown_clear (GtkWidget *dropdown);

int tsc_check_files ();

gchar *tsc_home_path ();

int tsc_launch_remote (rdp_file *rdp_in, int launch_async, gchar** error);

void tsc_connect_error (rdp_file * rdp, const gchar* error);
void tsc_error_message (gchar *message);

void tsc_about_dialog ();

void tsc_quick_pick_activate (GtkDropDown *widget, GParamSpec *pspec, gpointer user_data);

void tsc_set_protocol_widgets (GtkWidget *main_win, gint protocol);

#endif /* SUPPORT_H */
