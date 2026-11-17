/* Profile/form bindings and saved-profile discovery. */
#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>

#include "rdpfile.h"
#include "support.h"
#include "tsc-presets.h"
#include "connection.h"

/* One mapping for both directions keeps field names and ownership consistent. */
typedef struct {
  const char *widget;
  gsize offset;
} TextBinding;

typedef struct {
  const char *widget;
  gsize offset;
  gboolean inverted;
} CheckBinding;

#define TEXT(widget, field) {widget, G_STRUCT_OFFSET (rdp_file, field)}
static const TextBinding text_bindings[] = {
  TEXT ("txtComputer", full_address),
  TEXT ("txtUsername", username),
  TEXT ("txtPassword", password),
  TEXT ("txtDomain", domain),
  TEXT ("txtClientHostname", client_hostname),
  TEXT ("txtProtoFile", proto_file),
  TEXT ("txtKeyboardLang", keyboard_language),
  TEXT ("txtCodepage", local_codepage),
  TEXT ("txtProgramPath", alternate_shell),
  TEXT ("txtStartFolder", shell_working_directory)
};
#undef TEXT

#define CHECK(widget, field, inverted) {widget, G_STRUCT_OFFSET (rdp_file, field), inverted}
static const CheckBinding check_bindings[] = {
  CHECK ("chkDisableEncryption", disable_encryption, FALSE),
  CHECK ("chkDisableClientEncryption", disable_client_encryption, FALSE),
  CHECK ("chkForceBitmap", force_bitmap_updates, FALSE),
  CHECK ("chkBackingStore", use_backing_store, FALSE),
  CHECK ("chkDisableCtrl", disable_remote_ctrl, FALSE),
  CHECK ("chkSyncNumlock", sync_numlock, FALSE),
  CHECK ("chkStartProgram", enable_alternate_shell, FALSE),
  CHECK ("chkBitmapCache", bitmapcachepersistenable, FALSE),
  CHECK ("chkDesktopBackground", disable_wallpaper, TRUE),
  CHECK ("chkWindowContent", disable_full_window_drag, TRUE),
  CHECK ("chkAnimation", disable_menu_anims, TRUE),
  CHECK ("chkThemes", disable_themes, TRUE),
  CHECK ("chkNoMotionEvents", no_motion_events, FALSE),
  CHECK ("chkEnableWMKeys", enable_wm_keys, FALSE),
  CHECK ("chkHideWMDecorations", hide_wm_decorations, FALSE),
  CHECK ("chkAttachToConsole", attach_to_console, FALSE),
  CHECK ("chkRememberPassword", remember_password, FALSE)
};
#undef CHECK

static const int protocols[] = { 0, 4, 1, 2, 3, 5 };
static const int color_depths[] = { 8, 15, 16, 24, 32 };
static const char *const tls_versions[] = { "", "1.0", "1.1", "1.2" };
static const char *const sound_buttons[] = { "optSound1", "optSound2", "optSound3" };

static gint value_index (const int *values, guint count, int value)
{
  for (guint i = 0; i < count; i++)
    if (values[i] == value) return i;
  return -1;
}

static gboolean check_active (GtkWidget *window, const char *name)
{
  GtkWidget *widget = lookup_widget (window, name);
  return widget && gtk_check_button_get_active (GTK_CHECK_BUTTON (widget));
}

static void set_check (GtkWidget *window, const char *name, gboolean active)
{
  GtkWidget *widget = lookup_widget (window, name);
  if (widget) gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), active);
}

static void replace_text (char **field, const char *text)
{
  char *owned = g_strdup (text ? text : "");
  g_free (*field);
  *field = owned;
}

int rdp_file_set_screen (rdp_file *rdp, GtkWidget *window)
{
  g_object_set_data (G_OBJECT (window), "tsc-setting-profile", GINT_TO_POINTER (1));
  for (guint i = 0; i < G_N_ELEMENTS (text_bindings); i++) {
    const TextBinding *binding = &text_bindings[i];
    GtkWidget *widget = lookup_widget (window, binding->widget);
    const char *value = *(char **) ((guint8 *) rdp + binding->offset);
    if (widget) gtk_editable_set_text (GTK_EDITABLE (widget), value ? value : "");
  }
  for (guint i = 0; i < G_N_ELEMENTS (check_bindings); i++) {
    const CheckBinding *binding = &check_bindings[i];
    int value = *(int *) ((guint8 *) rdp + binding->offset);
    set_check (window, binding->widget, value == (binding->inverted ? 0 : 1));
  }
  g_object_set_data_full (G_OBJECT (window), "win_password",
                          g_strdup (rdp->win_password), g_free);

  gint protocol_index = value_index (protocols, G_N_ELEMENTS (protocols), rdp->protocol);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (lookup_widget (window, "optProtocol")),
                              protocol_index < 0 ? 0 : (guint) protocol_index);
  guint tls_index = 0;
  for (guint i = 1; i < G_N_ELEMENTS (tls_versions); i++)
    if (g_strcmp0 (rdp->tls_version, tls_versions[i]) == 0) tls_index = i;
  GtkWidget *widget = lookup_widget (window, "optTlsVersion");
  if (widget) gtk_drop_down_set_selected (GTK_DROP_DOWN (widget), tls_index);

  gint size_index = -1;
  for (guint i = 0; i < TSC_SCREEN_PRESET_COUNT; i++)
    if (tsc_screen_presets[i].width == rdp->desktopwidth &&
        tsc_screen_presets[i].height == rdp->desktopheight) {
      size_index = i;
      break;
    }
  gboolean fullscreen = rdp->screen_mode_id == 2;
  set_check (window, fullscreen ? "optSize3" : size_index >= 0 ? "optSize2" : "optSize1", TRUE);
  if (!fullscreen)
    gtk_drop_down_set_selected (GTK_DROP_DOWN (lookup_widget (window, "optSize")),
                                size_index < 0 ? 0 : (guint) size_index);
  widget = lookup_widget (window, "alnSize");
  if (widget) gtk_widget_set_sensitive (widget, check_active (window, "optSize2") && !fullscreen);

  gint color_index = value_index (color_depths, G_N_ELEMENTS (color_depths), rdp->session_bpp);
  set_check (window, color_index < 0 ? "optColor1" : "optColor2", TRUE);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (lookup_widget (window, "optColor")),
                              color_index < 0 ? 2 : (guint) color_index);
  widget = lookup_widget (window, "alnColor");
  if (widget) gtk_widget_set_sensitive (widget, check_active (window, "optColor2"));

  /* Legacy profiles enable the program whenever either program field is set. */
  const char *program_values[] = { rdp->alternate_shell, rdp->shell_working_directory };
  const char *program_widgets[] = { "txtProgramPath", "txtStartFolder" };
  for (guint i = 0; i < G_N_ELEMENTS (program_values); i++) {
    if (program_values[i] && *program_values[i]) {
      gtk_editable_set_editable (GTK_EDITABLE (lookup_widget (window, program_widgets[i])), TRUE);
      set_check (window, "chkStartProgram", TRUE);
    }
  }
  guint sound_index = rdp->audiomode >= 0 && rdp->audiomode < (int) G_N_ELEMENTS (sound_buttons)
                        ? (guint) rdp->audiomode : 0;
  set_check (window, sound_buttons[sound_index], TRUE);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (lookup_widget (window, "optKeyboard")), rdp->keyboardhook);

  /* Apply sensitivity last, after radio/check callbacks have run. */
  tsc_set_protocol_widgets (window, rdp->protocol);
  tsc_credentials_load_for_window (window, rdp);
  g_object_set_data (G_OBJECT (window), "tsc-setting-profile", NULL);
  return 0;
}

int rdp_file_get_screen (rdp_file *rdp, GtkWidget *window)
{
  for (guint i = 0; i < G_N_ELEMENTS (text_bindings); i++) {
    const TextBinding *binding = &text_bindings[i];
    GtkWidget *widget = lookup_widget (window, binding->widget);
    if (widget)
      replace_text ((char **) ((guint8 *) rdp + binding->offset),
                    gtk_editable_get_text (GTK_EDITABLE (widget)));
  }
  for (guint i = 0; i < G_N_ELEMENTS (check_bindings); i++) {
    const CheckBinding *binding = &check_bindings[i];
    gboolean active = check_active (window, binding->widget);
    *(int *) ((guint8 *) rdp + binding->offset) = binding->inverted ? !active : active;
  }
  replace_text (&rdp->win_password, g_object_get_data (G_OBJECT (window), "win_password"));

  guint index = gtk_drop_down_get_selected (GTK_DROP_DOWN (lookup_widget (window, "optProtocol")));
  rdp->protocol = index < G_N_ELEMENTS (protocols) ? protocols[index] : 0;
  GtkWidget *widget = lookup_widget (window, "optTlsVersion");
  if (widget) {
    index = gtk_drop_down_get_selected (GTK_DROP_DOWN (widget));
    replace_text (&rdp->tls_version, index < G_N_ELEMENTS (tls_versions) ? tls_versions[index] : "");
  }

  rdp->desktop_size_id = 1;
  rdp->screen_mode_id = check_active (window, "optSize3") ? 2 : 1;
  if (check_active (window, "optSize1") || rdp->screen_mode_id == 2)
    rdp->desktopwidth = rdp->desktopheight = 0;
  if (check_active (window, "optSize2")) {
    index = gtk_drop_down_get_selected (GTK_DROP_DOWN (lookup_widget (window, "optSize")));
    rdp->desktopwidth = index < TSC_SCREEN_PRESET_COUNT ? tsc_screen_presets[index].width : 0;
    rdp->desktopheight = index < TSC_SCREEN_PRESET_COUNT ? tsc_screen_presets[index].height : 0;
  }

  if (check_active (window, "optColor1")) rdp->session_bpp = 0;
  else {
    index = gtk_drop_down_get_selected (GTK_DROP_DOWN (lookup_widget (window, "optColor")));
    if (index != GTK_INVALID_LIST_POSITION)
      rdp->session_bpp = index < G_N_ELEMENTS (color_depths) ? color_depths[index] : 0;
  }
  rdp->keyboardhook = gtk_drop_down_get_selected (GTK_DROP_DOWN (lookup_widget (window, "optKeyboard")));
  for (guint i = 0; i < G_N_ELEMENTS (sound_buttons); i++)
    if (check_active (window, sound_buttons[i])) rdp->audiomode = i;
  return 0;
}

int rdp_load_profile_launcher (GtkWidget *main_window)
{
  GtkWidget *opt;
  GSList    *lptr = NULL;
  gint      cnt, ret;
  
  #ifdef TSCLIENT_DEBUG
  printf ("rdp_load_profile_launcher\n");
  #endif

  opt = lookup_widget (main_window, "optProfileLauncher");
  ret = rdp_files_to_list (&lptr);
  cnt = 0;
  tsc_dropdown_clear (opt);
  tsc_dropdown_append (opt, _("Quick Connect"));

  for (GSList *item = lptr; item; item = item->next) {
    tsc_dropdown_append (opt, item->data);
    cnt++;
  }
  g_slist_free_full (lptr, g_free);
  gtk_widget_set_sensitive (opt, cnt > 0);

  /* complete successfully */
  return 0;
}

/***************************************
*                                      *
*   read_dir_list                      *
*                                      *
***************************************/ 
void read_dir_list (gchar *dir_name, GSList **list, const gchar *value) {
  GError *error = NULL;
  GDir *dir;
  const gchar *name;
  gchar *tmp_name;

  dir = g_dir_open (dir_name, 0, &error);
  if (!dir) { g_clear_error (&error); return; }
  while ((name = g_dir_read_name (dir)) != NULL) {
    tmp_name = g_build_path ("/", dir_name, name, NULL);
    if (g_file_test (tmp_name, G_FILE_TEST_IS_DIR) && !g_file_test (tmp_name, G_FILE_TEST_IS_SYMLINK)) {
      gchar *relative = strcmp (value, "-1") == 0 ? g_strdup (name) : g_build_filename (value, name, NULL);
      read_dir_list (tmp_name, list, relative);
      g_free (relative);
    } else {
      if ((g_str_has_suffix (name, ".rdp") || g_str_has_suffix (name, ".tsc")) &&
          strcmp (name, "last.tsc") && strcmp (name, "default.tsc") && strcmp (name, "mru.tsc")) {
        gchar *tmp;
        if (g_ascii_strcasecmp (value, "-1") == 0) {
          tmp = g_strdup (name);
        } else {
          tmp = g_build_path ("/", value, name, NULL);
        }
        *list = g_slist_append (*list, tmp);
      }
    }
    g_free (tmp_name);
  }
  g_dir_close (dir);
}

/***************************************
*                                      *
*   rdp_files_to_list                  *
*                                      *
***************************************/

int rdp_files_to_list (GSList** list)
{
  gchar *path_name;
  gint i;
	
  #ifdef TSCLIENT_DEBUG
  printf ("rdp_files_to_list\n");
  #endif

  // create .tsclient dir in ~/
  path_name = tsc_home_path ();
  
  read_dir_list (path_name, list, "-1");
  
  #ifdef TSCLIENT_DEBUG
  printf ("rdp_files_to_list count: %d\n", g_slist_length (*list));
  #endif

  g_free (path_name);
  // complete successfully
  return 0;
}


/***************************************
*                                      *
*   read_dir                           *
*                                      *
***************************************/ 
void read_dir (gchar *dir_name, GHashTable *hash, const gchar *value) {
  GError *error = NULL;
  GDir *dir;
  const gchar *name;
  gchar *tmp_name;

  dir = g_dir_open (dir_name, 0, &error);
  if (!dir) { g_clear_error (&error); return; }
  while ((name = g_dir_read_name (dir)) != NULL) {
    tmp_name = g_build_path ("/", dir_name, name, NULL);
    if (g_file_test (tmp_name, G_FILE_TEST_IS_DIR) && !g_file_test (tmp_name, G_FILE_TEST_IS_SYMLINK)) {
      read_dir (tmp_name, hash, name);
    } else {
      if ((g_str_has_suffix (name, ".rdp") || g_str_has_suffix (name, ".tsc")) &&
          strcmp (name, "last.tsc") && strcmp (name, "default.tsc") && strcmp (name, "mru.tsc")) {
        g_hash_table_insert (hash, g_strdup (name), g_strdup (value));
	  }
    }
	g_free (tmp_name);
  }
  g_dir_close (dir);
}

/***************************************
*                                      *
*   rdp_files_to_hash                  *
*                                      *
***************************************/

GHashTable* rdp_files_to_hash(void){
  gchar *path_name;
  GHashTable *hash;
	
  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	
  #ifdef TSCLIENT_DEBUG
  printf ("rdp_files_to_list\n");
  #endif

  path_name = g_build_path ("/", g_get_home_dir (), ".tsclient", NULL);
  read_dir (path_name, hash, "-1");
  g_free (path_name);
  return hash;
};
