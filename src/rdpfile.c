/***************************************
*                                      *
*   RDP File Handlers                  *
*                                      *
***************************************/


#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <dirent.h> 

#include "rdpfile.h"
#include "support.h"
#include "tsc-presets.h"

static int rdp_file_load_legacy (rdp_file *rdp_in, const char *fqpath);
static gboolean rdp_file_load_json_data (rdp_file *rdp_in, const gchar *data, gsize length);
static void rdp_file_json_add_string (GString *buffer, gboolean *first, const gchar *name, const gchar *value);
static void rdp_file_json_add_int (GString *buffer, gboolean *first, const gchar *name, gint value);
static gboolean rdp_file_apply_json_value (rdp_file *rdp, const gchar *key, GScanner *scanner, GTokenType token);

static const gchar *const tsc_tls_versions[] = {
  "",
  "1.0",
  "1.1",
  "1.2",
  NULL
};

static gint
tsc_tls_version_index (const gchar *value)
{
  if (!value || !value[0])
    return 0;
  for (gint i = 1; tsc_tls_versions[i]; i++) {
    if (g_strcmp0 (value, tsc_tls_versions[i]) == 0)
      return i;
  }
  return 0;
}

static const gchar *
tsc_tls_version_value (guint index)
{
  if (index >= G_N_ELEMENTS (tsc_tls_versions) - 1)
    return "";
  return tsc_tls_versions[index] ? tsc_tls_versions[index] : "";
}

static gint
tsc_screen_preset_index (gint width, gint height)
{
  for (guint i = 0; i < TSC_SCREEN_PRESET_COUNT; i++) {
    if (tsc_screen_presets[i].width == width &&
        tsc_screen_presets[i].height == height)
      return (gint) i;
  }
  return -1;
}

static void
rdp_file_json_add_string (GString *buffer, gboolean *first, const gchar *name, const gchar *value)
{
  gchar *escaped = g_strescape (value ? value : "", NULL);
  g_string_append_printf (buffer, "%s  \"%s\": \"%s\"",
                          *first ? "" : ",\n",
                          name,
                          escaped);
  g_free (escaped);
  *first = FALSE;
}

static void
rdp_file_json_add_int (GString *buffer, gboolean *first, const gchar *name, gint value)
{
  g_string_append_printf (buffer, "%s  \"%s\": %d",
                          *first ? "" : ",\n",
                          name,
                          value);
  *first = FALSE;
}

static gboolean
rdp_file_apply_json_value (rdp_file *rdp, const gchar *key, GScanner *scanner, GTokenType token)
{
#define SET_STR_FIELD(field) \
  if (g_strcmp0 (key, #field) == 0) { \
    if (token != G_TOKEN_STRING) \
      return FALSE; \
    rdp->field = g_strdup (scanner->value.v_string ? scanner->value.v_string : ""); \
    return TRUE; \
  }

#define SET_INT_FIELD(field) \
  if (g_strcmp0 (key, #field) == 0) { \
    if (token != G_TOKEN_INT) \
      return FALSE; \
    rdp->field = scanner->value.v_int; \
    return TRUE; \
  }

  SET_STR_FIELD (alternate_shell);
  SET_INT_FIELD (attach_to_console);
  SET_INT_FIELD (audiomode);
  SET_INT_FIELD (auto_connect);
  SET_INT_FIELD (bitmapcachepersistenable);
  SET_STR_FIELD (client_hostname);
  SET_INT_FIELD (compression);
  SET_STR_FIELD (description);
  SET_INT_FIELD (desktop_size_id);
  SET_INT_FIELD (desktopheight);
  SET_INT_FIELD (desktopwidth);
  SET_INT_FIELD (disable_full_window_drag);
  SET_INT_FIELD (disable_menu_anims);
  SET_INT_FIELD (disable_themes);
  SET_INT_FIELD (disable_wallpaper);
  SET_INT_FIELD (displayconnectionbar);
  SET_STR_FIELD (domain);
  SET_INT_FIELD (enable_alternate_shell);
  SET_INT_FIELD (enable_wm_keys);
  SET_STR_FIELD (full_address);
  SET_INT_FIELD (hide_wm_decorations);
  SET_STR_FIELD (keyboard_language);
  SET_INT_FIELD (keyboardhook);
  SET_INT_FIELD (no_motion_events);
  SET_STR_FIELD (password);
  SET_STR_FIELD (win_password);
  SET_STR_FIELD (progman_group);
  SET_INT_FIELD (protocol);
  SET_STR_FIELD (proto_file);
  SET_INT_FIELD (redirectcomports);
  SET_INT_FIELD (redirectdrives);
  SET_INT_FIELD (redirectprinters);
  SET_INT_FIELD (redirectsmartcards);
  SET_INT_FIELD (screen_mode_id);
  SET_INT_FIELD (session_bpp);
  SET_INT_FIELD (disable_encryption);
  SET_INT_FIELD (disable_client_encryption);
  SET_INT_FIELD (force_bitmap_updates);
  SET_INT_FIELD (use_backing_store);
  SET_INT_FIELD (disable_remote_ctrl);
  SET_INT_FIELD (sync_numlock);
  SET_STR_FIELD (tls_version);
  SET_STR_FIELD (local_codepage);
  SET_STR_FIELD (shell_working_directory);
  SET_STR_FIELD (username);
  SET_STR_FIELD (winposstr);

#undef SET_STR_FIELD
#undef SET_INT_FIELD

  return TRUE;
}

static gboolean
rdp_file_load_json_data (rdp_file *rdp, const gchar *data, gsize length)
{
  GScanner *scanner = g_scanner_new (NULL);
  gboolean success = FALSE;

  g_scanner_input_text (scanner, data, length);
  if (g_scanner_get_next_token (scanner) != '{') {
    g_scanner_destroy (scanner);
    return FALSE;
  }

  while (TRUE) {
    GTokenType token = g_scanner_get_next_token (scanner);
    if (token == G_TOKEN_EOF)
      break;
    if (token == '}') {
      success = TRUE;
      break;
    }
    if (token != G_TOKEN_STRING)
      break;
    gchar *key = g_strdup (scanner->value.v_string);
    if (g_scanner_get_next_token (scanner) != ':') {
      g_free (key);
      break;
    }
    token = g_scanner_get_next_token (scanner);
    if (!rdp_file_apply_json_value (rdp, key, scanner, token)) {
      g_free (key);
      break;
    }
    g_free (key);
    GTokenType next = g_scanner_peek_next_token (scanner);
    if (next == ',')
      g_scanner_get_next_token (scanner);
  }

  g_scanner_destroy (scanner);
  return success;
}


/***************************************
*                                      *
*   rdp_file_init                      *
*                                      *
***************************************/

int rdp_file_init (rdp_file *rdp_in)
{
  rdp_file *rdp = NULL;
  
  #ifdef TSCLIENT_DEBUG
  printf ("rdp_file_init\n");
  #endif

  /* swap the return array */
  rdp = rdp_in;

  rdp->alternate_shell = "";
  rdp->attach_to_console = 0;
  rdp->audiomode = 0;
  rdp->auto_connect = 0;
  rdp->bitmapcachepersistenable = 0;
  rdp->client_hostname = "";
  rdp->compression = 0;
  rdp->description = "";
  rdp->desktop_size_id = 0;
  rdp->desktopheight = 0;
  rdp->desktopwidth = 0;
  rdp->disable_client_encryption = 0;
  rdp->disable_full_window_drag = 0;
  rdp->disable_remote_ctrl = 0;
  rdp->disable_menu_anims = 0;
  rdp->disable_themes = 0;
  rdp->disable_wallpaper = 0;
  rdp->disable_encryption = 0;
  rdp->displayconnectionbar = 0;
  rdp->domain = "";
  rdp->enable_alternate_shell = 0;
  rdp->enable_wm_keys = 0;
  rdp->force_bitmap_updates = 0;
  rdp->full_address = "";
  rdp->hide_wm_decorations = 0;
  rdp->keyboard_language = "";
  rdp->keyboardhook = 0;
  rdp->local_codepage = "";
  rdp->no_motion_events = 0;
  rdp->password = "";
  rdp->win_password = "";
  rdp->progman_group = "";
  rdp->protocol = 0;
  rdp->proto_file = "";
  rdp->redirectcomports = 0;
  rdp->redirectdrives = 0;
  rdp->redirectprinters = 0;
  rdp->redirectsmartcards = 0;
  rdp->screen_mode_id = 0;
  rdp->session_bpp = 0;
  rdp->sync_numlock = 0;
  rdp->shell_working_directory = "";
  rdp->tls_version = "";
  rdp->use_backing_store = 0;
  rdp->username = "";
  rdp->winposstr = "";

  return 0;

}

/***************************************
*                                      *
*   rdp_file_load                      *
*                                      *
***************************************/

int rdp_file_load (rdp_file *rdp_in, const char *fqpath)
{
  gchar *contents = NULL;
  gsize length = 0;

  #ifdef TSCLIENT_DEBUG
  printf ("rdp_file_load\n");
  #endif

  if (g_file_get_contents (fqpath, &contents, &length, NULL)) {
    const gchar *ptr = contents;
    while (ptr && g_ascii_isspace (*ptr))
      ptr++;
    if (ptr && *ptr == '{') {
      gboolean ok = rdp_file_load_json_data (rdp_in, contents, length);
      g_free (contents);
      if (ok)
        return 0;
    } else {
      g_free (contents);
    }
  }

  return rdp_file_load_legacy (rdp_in, fqpath);
}

static int
rdp_file_load_legacy (rdp_file *rdp_in, const char *fqpath)
{
  FILE *fptr;
  int unicode;
  char buffer[MAX_BUFFER_SIZE];
  char tmpbuf[2];
  rdp_file *rdp = NULL;
  int i;
  int ch;

  rdp = rdp_in;

  if ((fptr = fopen(fqpath, "r")) == NULL) {
    return 1;
  }

  if (fgets(buffer, MAX_BUFFER_SIZE, fptr) == NULL) {
    fclose (fptr);
    return 0;
  }

  unicode = 0;
  if ((strlen(buffer) == 2) || (strlen(buffer) == 3)) {
    if ((buffer[0] == (char)255) && (buffer[1] == (char)254)) {
      unicode = 1;
    }
  }

  if (!unicode) {
    do {
      rdp_file_set_from_line (rdp, buffer);
    } while (fgets(buffer, MAX_BUFFER_SIZE, fptr) != NULL);
  }

  if (unicode) {
    rewind(fptr);
    ch = -1;
    buffer[0] = '\0';
    for (i=0; (i < 1000000 && feof(fptr) == 0); i++)
    {
      ch = fgetc(fptr);
      if (ch > 0 && (ch != 255 && ch != 254 && ch != 10)) {
        if (ch == 13) {
          rdp_file_set_from_line (rdp, buffer);
          buffer[0] = '\0';
        } else {
          g_unichar_to_utf8 (ch, tmpbuf);
          tmpbuf[1] = '\0';
          strcat(buffer, tmpbuf); 
          tmpbuf[0] = '\0';
          tmpbuf[1] = '\0';
        }
      }
    }
  }

  fclose (fptr);
  return 0;
}


/***************************************
*                                      *
*   rdp_file_save                      *
*                                      *
***************************************/

int rdp_file_save (rdp_file *rdp_in, const char *fqpath)
{
  rdp_file *rdp = rdp_in;
  GString *json = g_string_new ("{\n");
  gboolean first = TRUE;
  gboolean ok;

  rdp_file_json_add_string (json, &first, "alternate_shell", rdp->alternate_shell);
  rdp_file_json_add_string (json, &first, "client_hostname", rdp->client_hostname);
  rdp_file_json_add_string (json, &first, "description", rdp->description);
  rdp_file_json_add_string (json, &first, "domain", rdp->domain);
  rdp_file_json_add_string (json, &first, "full_address", rdp->full_address);
  rdp_file_json_add_string (json, &first, "keyboard_language", rdp->keyboard_language);
  rdp_file_json_add_string (json, &first, "password", rdp->password);
  rdp_file_json_add_string (json, &first, "win_password", rdp->win_password);
  rdp_file_json_add_string (json, &first, "progman_group", rdp->progman_group);
  rdp_file_json_add_string (json, &first, "proto_file", rdp->proto_file);
  rdp_file_json_add_string (json, &first, "shell_working_directory", rdp->shell_working_directory);
  rdp_file_json_add_string (json, &first, "username", rdp->username);
  rdp_file_json_add_string (json, &first, "winposstr", rdp->winposstr);
  rdp_file_json_add_int (json, &first, "attach_to_console", rdp->attach_to_console);
  rdp_file_json_add_int (json, &first, "audiomode", rdp->audiomode);
  rdp_file_json_add_int (json, &first, "auto_connect", rdp->auto_connect);
  rdp_file_json_add_int (json, &first, "bitmapcachepersistenable", rdp->bitmapcachepersistenable);
  rdp_file_json_add_int (json, &first, "compression", rdp->compression);
  rdp_file_json_add_int (json, &first, "desktop_size_id", rdp->desktop_size_id);
  rdp_file_json_add_int (json, &first, "desktopheight", rdp->desktopheight);
  rdp_file_json_add_int (json, &first, "desktopwidth", rdp->desktopwidth);
  rdp_file_json_add_int (json, &first, "disable_full_window_drag", rdp->disable_full_window_drag);
  rdp_file_json_add_int (json, &first, "disable_menu_anims", rdp->disable_menu_anims);
  rdp_file_json_add_int (json, &first, "disable_themes", rdp->disable_themes);
  rdp_file_json_add_int (json, &first, "disable_wallpaper", rdp->disable_wallpaper);
  rdp_file_json_add_int (json, &first, "displayconnectionbar", rdp->displayconnectionbar);
  rdp_file_json_add_int (json, &first, "enable_alternate_shell", rdp->enable_alternate_shell);
  rdp_file_json_add_int (json, &first, "enable_wm_keys", rdp->enable_wm_keys);
  rdp_file_json_add_int (json, &first, "hide_wm_decorations", rdp->hide_wm_decorations);
  rdp_file_json_add_int (json, &first, "keyboardhook", rdp->keyboardhook);
  rdp_file_json_add_int (json, &first, "no_motion_events", rdp->no_motion_events);
  rdp_file_json_add_int (json, &first, "protocol", rdp->protocol);
  rdp_file_json_add_int (json, &first, "redirectcomports", rdp->redirectcomports);
  rdp_file_json_add_int (json, &first, "redirectdrives", rdp->redirectdrives);
  rdp_file_json_add_int (json, &first, "redirectprinters", rdp->redirectprinters);
  rdp_file_json_add_int (json, &first, "redirectsmartcards", rdp->redirectsmartcards);
  rdp_file_json_add_int (json, &first, "screen_mode_id", rdp->screen_mode_id);
  rdp_file_json_add_int (json, &first, "session_bpp", rdp->session_bpp);
  rdp_file_json_add_int (json, &first, "disable_encryption", rdp->disable_encryption);
  rdp_file_json_add_int (json, &first, "disable_client_encryption", rdp->disable_client_encryption);
  rdp_file_json_add_int (json, &first, "force_bitmap_updates", rdp->force_bitmap_updates);
  rdp_file_json_add_int (json, &first, "use_backing_store", rdp->use_backing_store);
  rdp_file_json_add_int (json, &first, "disable_remote_ctrl", rdp->disable_remote_ctrl);
  rdp_file_json_add_int (json, &first, "sync_numlock", rdp->sync_numlock);
  rdp_file_json_add_string (json, &first, "tls_version", rdp->tls_version);
  rdp_file_json_add_string (json, &first, "local_codepage", rdp->local_codepage);

  g_string_append (json, "\n}\n");
  ok = g_file_set_contents (fqpath, json->str, json->len, NULL);
  g_string_free (json, TRUE);

  return ok ? 0 : 1;
}


/***************************************
*                                      *
*   rdp_file_set_screen                *
*                                      *
***************************************/

int rdp_file_set_screen (rdp_file *rdp_in, GtkWidget *main_window)
{
  rdp_file *rdp = NULL;
  GtkWidget *widget;
  guint pos = 0;
  gint dsize = -1;
  GtkWidget *aln_size;
  GtkWidget *opt_custom;
  GtkWidget *opt_full;
  gboolean use_custom = FALSE;
  gboolean fullscreen = FALSE;
  GtkWidget *aln_color;
  GtkWidget *opt_color_specific;
  gboolean use_color_specific = FALSE;
  
  #ifdef TSCLIENT_DEBUG
  printf ("rdp_file_set_screen\n");
  #endif

  /* swap the return array */
  rdp = rdp_in;

  widget = lookup_widget (main_window, "txtComputer");
  gtk_editable_delete_text ((GtkEditable*) widget, 0, -1);
  gtk_editable_insert_text((GtkEditable*) widget, rdp->full_address, strlen(rdp->full_address), &pos);

  widget = lookup_widget (main_window, "optProtocol");
  switch (rdp->protocol) {
  case 1:
    tsc_dropdown_set_selected (widget, 2);
    break;
  case 2:
    tsc_dropdown_set_selected (widget, 3);
    break;
  case 3:
    tsc_dropdown_set_selected (widget, 4);
    break;
  case 4:
    tsc_dropdown_set_selected (widget, 1);
    break;
  default:
    tsc_dropdown_set_selected (widget, 0);
  }

  widget = lookup_widget (main_window, "txtUsername");
  gtk_editable_delete_text ((GtkEditable*) widget, 0, -1);
  gtk_editable_insert_text((GtkEditable*) widget, (gchar *)rdp->username, strlen(rdp->username), &pos);

  widget = lookup_widget (main_window, "txtPassword");
  gtk_editable_delete_text ((GtkEditable*) widget, 0, -1);
  gtk_editable_insert_text((GtkEditable*) widget, (gchar *)rdp->password, strlen(rdp->password), &pos);

  /* win password (oh, ugly)*/
  g_object_set_data (G_OBJECT (main_window), "win_password", rdp->win_password);

  widget = lookup_widget (main_window, "txtDomain");
  gtk_editable_delete_text ((GtkEditable*) widget, 0, -1);
  gtk_editable_insert_text((GtkEditable*) widget, (gchar *)rdp->domain, strlen(rdp->domain), &pos);

  widget = lookup_widget (main_window, "txtClientHostname");
  gtk_editable_delete_text ((GtkEditable*) widget, 0, -1);
  gtk_editable_insert_text((GtkEditable*) widget, (gchar *)rdp->client_hostname, strlen(rdp->client_hostname), &pos);

  widget = lookup_widget (main_window, "txtProtoFile");
  gtk_editable_delete_text ((GtkEditable*) widget, 0, -1);
  gtk_editable_insert_text((GtkEditable*) widget, (gchar *)rdp->proto_file, strlen(rdp->proto_file), &pos);

  widget = lookup_widget (main_window, "optTlsVersion");
  if (widget)
    tsc_dropdown_set_selected (widget, tsc_tls_version_index (rdp->tls_version));

  widget = lookup_widget (main_window, "chkDisableEncryption");
  if (widget)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), rdp->disable_encryption == 1);

  widget = lookup_widget (main_window, "chkDisableClientEncryption");
  if (widget)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), rdp->disable_client_encryption == 1);

  dsize = tsc_screen_preset_index (rdp->desktopwidth, rdp->desktopheight);

  if (rdp->screen_mode_id == 2) {
    widget = lookup_widget (main_window, "optSize3");
    tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
  } else {
    if (dsize >= 0) {
      widget = lookup_widget (main_window, "optSize2");
      tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
      widget = lookup_widget (main_window, "optSize");
      tsc_dropdown_set_selected (widget, (guint) dsize);
    } else {
      widget = lookup_widget (main_window, "optSize1");
      tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
      widget = lookup_widget (main_window, "optSize");
      tsc_dropdown_set_selected (widget, 0);
    }
  }

  aln_size = lookup_widget (main_window, "alnSize");
  opt_custom = lookup_widget (main_window, "optSize2");
  opt_full = lookup_widget (main_window, "optSize3");
  if (opt_custom)
    use_custom = tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (opt_custom));
  if (opt_full)
    fullscreen = tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (opt_full));

  if (aln_size) {
    gtk_widget_set_sensitive (aln_size, use_custom && !fullscreen);
  }

  switch (rdp->session_bpp) {
  case 8:
    widget = lookup_widget (main_window, "optColor2");
    tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
    widget = lookup_widget (main_window, "optColor");
    tsc_dropdown_set_selected (widget, 0);
    break;
  case 15:
    widget = lookup_widget (main_window, "optColor2");
    tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
    widget = lookup_widget (main_window, "optColor");
    tsc_dropdown_set_selected (widget, 1);
    break;
  case 16:
    widget = lookup_widget (main_window, "optColor2");
    tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
    widget = lookup_widget (main_window, "optColor");
    tsc_dropdown_set_selected (widget, 2);
    break;
  case 24:
    widget = lookup_widget (main_window, "optColor2");
    tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
    widget = lookup_widget (main_window, "optColor");
    tsc_dropdown_set_selected (widget, 3);
    break;
  case 32:
    widget = lookup_widget (main_window, "optColor2");
    tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
    widget = lookup_widget (main_window, "optColor");
    tsc_dropdown_set_selected (widget, 4);
    break;
  default:
    widget = lookup_widget (main_window, "optColor1");
    tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
    widget = lookup_widget (main_window, "optColor");
    tsc_dropdown_set_selected (widget, 2);
    break;
  }

  aln_color = lookup_widget (main_window, "alnColor");
  opt_color_specific = lookup_widget (main_window, "optColor2");
  if (opt_color_specific)
    use_color_specific = tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (opt_color_specific));
  if (aln_color)
    gtk_widget_set_sensitive (aln_color, use_color_specific);

  widget = lookup_widget (main_window, "chkForceBitmap");
  if (widget)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), rdp->force_bitmap_updates == 1);

  widget = lookup_widget (main_window, "chkBackingStore");
  if (widget)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), rdp->use_backing_store == 1);

  widget = lookup_widget (main_window, "txtKeyboardLang");
  gtk_editable_delete_text ((GtkEditable*) widget, 0, -1);
  gtk_editable_insert_text((GtkEditable*) widget, (gchar *)rdp->keyboard_language, strlen(rdp->keyboard_language), &pos);

  widget = lookup_widget (main_window, "chkDisableCtrl");
  if (widget)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), rdp->disable_remote_ctrl == 1);

  widget = lookup_widget (main_window, "chkSyncNumlock");
  if (widget)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), rdp->sync_numlock == 1);

  widget = lookup_widget (main_window, "txtCodepage");
  if (widget) {
    gtk_editable_delete_text ((GtkEditable*) widget, 0, -1);
    if (rdp->local_codepage && *rdp->local_codepage)
      gtk_editable_insert_text ((GtkEditable*) widget, (gchar *) rdp->local_codepage,
                                strlen (rdp->local_codepage), &pos);
  }


  widget = lookup_widget (main_window, "chkStartProgram");
  if (rdp->enable_alternate_shell == 1)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  else
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

  widget = lookup_widget (main_window, "txtProgramPath");
  gtk_editable_delete_text ((GtkEditable*) widget, 0, -1);
  if (strlen(rdp->alternate_shell) > 0) {
    gtk_editable_insert_text((GtkEditable*) widget, (gchar *)rdp->alternate_shell, strlen(rdp->alternate_shell), &pos);
    gtk_editable_set_editable ((GtkEditable*) widget, TRUE);
    widget = lookup_widget (main_window, "chkStartProgram");
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  }

  widget = lookup_widget (main_window, "txtStartFolder");
  gtk_editable_delete_text ((GtkEditable*) widget, 0, -1);
  if (strlen(rdp->shell_working_directory) > 0) {
    gtk_editable_insert_text((GtkEditable*) widget, (gchar *)rdp->shell_working_directory, strlen(rdp->shell_working_directory), &pos);
    gtk_editable_set_editable ((GtkEditable*) widget, TRUE);
    widget = lookup_widget (main_window, "chkStartProgram");
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  }
  
  // the checkboxes on the performance frame
  widget = lookup_widget (main_window, "chkBitmapCache");
  if (rdp->bitmapcachepersistenable == 1)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  else
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

  widget = lookup_widget (main_window, "chkDesktopBackground");
  if (rdp->disable_wallpaper == 1)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  else
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

  widget = lookup_widget (main_window, "chkWindowContent");
  if (rdp->disable_full_window_drag == 1)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  else
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

  widget = lookup_widget (main_window, "chkAnimation");
  if (rdp->disable_menu_anims == 1)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  else
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

  widget = lookup_widget (main_window, "chkThemes");
  if (rdp->disable_themes == 1)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  else
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);


  // the checkboxes on the performance frame (extras)
  widget = lookup_widget (main_window, "chkNoMotionEvents");
  if (rdp->no_motion_events == 1)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  else
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

  widget = lookup_widget (main_window, "chkEnableWMKeys");
  if (rdp->enable_wm_keys == 1)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  else
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

  widget = lookup_widget (main_window, "chkHideWMDecorations");
  if (rdp->hide_wm_decorations == 1)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  else
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

  widget = lookup_widget (main_window, "chkAttachToConsole");
  if (rdp->attach_to_console == 1)
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  else
    tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);

  // the stuff on the resources frame
  switch (rdp->audiomode) {
  case 1:
    widget = lookup_widget (main_window, "optSound2");
    tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
    break;
  case 2:
    widget = lookup_widget (main_window, "optSound3");
    tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
    break;
  default:
    widget = lookup_widget (main_window, "optSound1");
    tsc_toggle_button_set_active ((GtkToggleButton*) widget, TRUE);
    break;
  }

  // the stuff on the resources frame
  widget = lookup_widget (main_window, "optKeyboard");
  tsc_dropdown_set_selected (widget, rdp->keyboardhook);

/*  widget = lookup_widget (main_window, "optKeyboard");
  switch (rdp->keyboardhook) {
  case 1:
    tsc_dropdown_set_selected (widget, 1);
    break;
  case 2:
    tsc_dropdown_set_selected (widget, 2);
    break;
  default:
    tsc_dropdown_set_selected (widget, 0);
    break;
  } */
      
  tsc_set_protocol_widgets (main_window, rdp->protocol);
  
  // end if and drop out
  return 0;

}


/***************************************
*                                      *
*   rdp_file_get_screen                *
*                                      *
***************************************/

int rdp_file_get_screen (rdp_file *rdp_in, GtkWidget *main_window)
{
  rdp_file *rdp = NULL;
  GtkWidget *widget;
  char *value = "";
  
  #ifdef TSCLIENT_DEBUG
  printf ("rdp_file_get_screen\n");
  #endif

  /* swap the return array */
  rdp = rdp_in;
  
  widget = lookup_widget (main_window, "txtComputer");
  value = gtk_editable_get_chars ((GtkEditable*) widget, 0, -1);
  if (value) rdp->full_address = value;

  widget = lookup_widget (main_window, "optProtocol");
  switch (tsc_dropdown_get_selected (widget)) {
  case 1:
    rdp->protocol = 4;
    break;
  case 2:
    rdp->protocol = 1;
    break;
  case 3:
    rdp->protocol = 2;
    break;
  case 4:
    rdp->protocol = 3;
    break;
  default:
    rdp->protocol = 0;
  }
  
  widget = lookup_widget (main_window, "txtUsername");
  value = gtk_editable_get_chars ((GtkEditable*) widget, 0, -1);
  if (value) rdp->username = value;

  widget = lookup_widget (main_window, "txtPassword");
  value = gtk_editable_get_chars ((GtkEditable*) widget, 0, -1);
  if (value) rdp->password = value;

  /* win password (oh, ugly)*/
  rdp->win_password = g_object_get_data (G_OBJECT (main_window), "win_password");
  
  widget = lookup_widget (main_window, "txtDomain");
  value = gtk_editable_get_chars ((GtkEditable*) widget, 0, -1);
  if (value) rdp->domain = value;
  
  widget = lookup_widget (main_window, "txtClientHostname");
  value = gtk_editable_get_chars ((GtkEditable*) widget, 0, -1);
  if (value) rdp->client_hostname = value;
    
  widget = lookup_widget (main_window, "txtProtoFile");
  value = gtk_editable_get_chars ((GtkEditable*) widget, 0, -1);
  if (value) rdp->proto_file = value;
  
  widget = lookup_widget (main_window, "optTlsVersion");
  if (widget) {
    guint tls_idx = tsc_dropdown_get_selected (widget);
    rdp->tls_version = g_strdup (tsc_tls_version_value (tls_idx));
  }
  
  widget = lookup_widget (main_window, "chkDisableEncryption");
  rdp->disable_encryption = widget && tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) ? 1 : 0;
  
  widget = lookup_widget (main_window, "chkDisableClientEncryption");
  rdp->disable_client_encryption = widget && tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) ? 1 : 0;
  
  widget = lookup_widget (main_window, "txtKeyboardLang");
  value = gtk_editable_get_chars ((GtkEditable*) widget, 0, -1);
  if (value) rdp->keyboard_language = value;
  
  widget = lookup_widget (main_window, "chkDisableCtrl");
  rdp->disable_remote_ctrl = widget && tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) ? 1 : 0;
  
  widget = lookup_widget (main_window, "chkSyncNumlock");
  rdp->sync_numlock = widget && tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) ? 1 : 0;
  
  widget = lookup_widget (main_window, "txtCodepage");
  value = gtk_editable_get_chars ((GtkEditable*) widget, 0, -1);
  if (value) rdp->local_codepage = value;

  rdp->desktop_size_id = 1;
  rdp->screen_mode_id = 1;
  widget = lookup_widget (main_window, "optSize1");
  if (tsc_toggle_button_get_active ((GtkToggleButton *) widget)) {
    rdp->desktopwidth = 0;
    rdp->desktopheight = 0;
  }

  widget = lookup_widget (main_window, "optSize3");
  if (tsc_toggle_button_get_active ((GtkToggleButton *) widget)) {
    rdp->screen_mode_id = 2;
    rdp->desktopwidth = 0;
    rdp->desktopheight = 0;
  }

  widget = lookup_widget (main_window, "optSize2");
  if (tsc_toggle_button_get_active ((GtkToggleButton *) widget)) {
    widget = lookup_widget (main_window, "optSize");
    gint size_index = (gint) tsc_dropdown_get_selected (widget);
    if (size_index >= 0 && size_index < (gint) TSC_SCREEN_PRESET_COUNT) {
      rdp->desktopwidth = tsc_screen_presets[size_index].width;
      rdp->desktopheight = tsc_screen_presets[size_index].height;
    } else {
      rdp->desktopwidth = 0;
      rdp->desktopheight = 0;
    }
  }
  
  widget = lookup_widget (main_window, "optColor1");
  if (tsc_toggle_button_get_active ((GtkToggleButton *) widget)) {
    rdp->session_bpp = 0;
  } else {
    widget = lookup_widget (main_window, "optColor");
    gint color_index = (gint) tsc_dropdown_get_selected (widget);
    if (color_index > -1) {
      switch (color_index) {
      case 0:
        rdp->session_bpp = 8;
        break;
      case 1:
        rdp->session_bpp = 15;
        break;
      case 2:
        rdp->session_bpp = 16;
        break;
      case 3:
        rdp->session_bpp = 24;
        break;
      case 4:
        rdp->session_bpp = 32;
        break;
      default:
        rdp->session_bpp = 0;
        break;
      }
    }
  }
  
  widget = lookup_widget (main_window, "chkForceBitmap");
  rdp->force_bitmap_updates = widget && tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) ? 1 : 0;
  
  widget = lookup_widget (main_window, "chkBackingStore");
  rdp->use_backing_store = widget && tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) ? 1 : 0;
  
  widget = lookup_widget (main_window, "optKeyboard");
  rdp->keyboardhook = tsc_dropdown_get_selected (widget);

  widget = lookup_widget (main_window, "optSound1");
  if (tsc_toggle_button_get_active ((GtkToggleButton *) widget)) {
    rdp->audiomode = 0;
  }
  widget = lookup_widget (main_window, "optSound2");
  if (tsc_toggle_button_get_active ((GtkToggleButton *) widget)) {
    rdp->audiomode = 1;
  }
  widget = lookup_widget (main_window, "optSound3");
  if (tsc_toggle_button_get_active ((GtkToggleButton *) widget)) {
    rdp->audiomode = 2;
  }

  widget = lookup_widget (main_window, "chkStartProgram");
  if (tsc_toggle_button_get_active ((GtkToggleButton*)widget))
    rdp->enable_alternate_shell = 1;
  else
    rdp->enable_alternate_shell = 0;

  widget = lookup_widget (main_window, "txtProgramPath");
  value = gtk_editable_get_chars ((GtkEditable*) widget, 0, -1);
  if (value) rdp->alternate_shell =  value;
  
  widget = lookup_widget (main_window, "txtStartFolder");
  value = gtk_editable_get_chars ((GtkEditable*) widget, 0, -1);
  if (value) rdp->shell_working_directory = value;
  

  widget = lookup_widget (main_window, "chkBitmapCache");
  if (tsc_toggle_button_get_active ((GtkToggleButton*)widget))
    rdp->bitmapcachepersistenable = 1;
  else
    rdp->bitmapcachepersistenable = 0;


  widget = lookup_widget (main_window, "chkDesktopBackground");
  if (!tsc_toggle_button_get_active ((GtkToggleButton*)widget))
    rdp->disable_wallpaper = 1;
  else
    rdp->disable_wallpaper = 0;

  widget = lookup_widget (main_window, "chkWindowContent");
  if (!tsc_toggle_button_get_active ((GtkToggleButton*)widget))
    rdp->disable_full_window_drag = 1;
  else
    rdp->disable_full_window_drag = 0;

  widget = lookup_widget (main_window, "chkAnimation");
  if (!tsc_toggle_button_get_active ((GtkToggleButton*)widget))
    rdp->disable_menu_anims = 1;
  else
    rdp->disable_menu_anims = 0;

  widget = lookup_widget (main_window, "chkThemes");
  if (!tsc_toggle_button_get_active ((GtkToggleButton*)widget))
    rdp->disable_themes = 1;
  else
    rdp->disable_themes = 0;

  
  // Extra
  widget = lookup_widget (main_window, "chkNoMotionEvents");
  if (tsc_toggle_button_get_active ((GtkToggleButton*)widget))
    rdp->no_motion_events = 1;
  else
    rdp->no_motion_events = 0;

  widget = lookup_widget (main_window, "chkEnableWMKeys");
  if (tsc_toggle_button_get_active ((GtkToggleButton*)widget))
    rdp->enable_wm_keys = 1;
  else
    rdp->enable_wm_keys = 0;

  widget = lookup_widget (main_window, "chkHideWMDecorations");
  if (tsc_toggle_button_get_active ((GtkToggleButton*)widget))
    rdp->hide_wm_decorations = 1;
  else
    rdp->hide_wm_decorations = 0;

  widget = lookup_widget (main_window, "chkAttachToConsole");
  if (tsc_toggle_button_get_active ((GtkToggleButton*)widget))
    rdp->attach_to_console = 1;
  else
    rdp->attach_to_console = 0;

  return 0;

}


/***************************************
*                                      *
*   rdp_file_set_from_line             *
*                                      *
***************************************/

int rdp_file_set_from_line (rdp_file *rdp_in, const char *str_in)
{
  char *str;
  char *token;
  char key[MAX_KEY_SIZE];
  char value[MAX_VALUE_SIZE];
  rdp_file *rdp = NULL;

  key[0] = '\0';
  value[0] = '\0';
  str = g_strdup (str_in);

  /* grab the key token */
  if ((token = strtok(str, ":")))
  {
    strncpy(key, token, MAX_KEY_SIZE - 1);
    key[MAX_KEY_SIZE - 1] = '\0';

    /* discard the type token */
    token = strtok(NULL, ":");

    /* grab the value token */
    if((token = strtok(NULL, "\n"))) {
      if(token[strlen(token) - 1] == '\r') token[strlen(token) - 1] = '\0';
      strncpy(value, token, MAX_VALUE_SIZE - 1);
      value[MAX_VALUE_SIZE - 1] = '\0';
    }

    rdp = rdp_in;

    if (strcmp(key, "alternate shell") == 0) {
      rdp->alternate_shell = g_strdup(value);
    }
    if (strcmp(key, "attach to console") == 0) {
      rdp->attach_to_console = atoi(value);
    }
    if (strcmp(key, "audiomode") == 0) {
      rdp->audiomode = atoi(value);
    }
    if (strcmp(key, "auto connect") == 0) {
      rdp->auto_connect = atoi(value);
    }
    if (strcmp(key, "bitmapcachepersistenable") == 0) {
      rdp->bitmapcachepersistenable = atoi(value);
    }
    if (strcmp(key, "client hostname") == 0) {
      rdp->client_hostname = g_strdup(value);
    }
    if (strcmp(key, "compression") == 0) {
      rdp->compression = atoi(value);
    }
    if (strcmp(key, "description") == 0) {
      rdp->description = g_strdup(value);
    }
    if (strcmp(key, "desktop size id") == 0) {
      rdp->desktop_size_id = atoi(value);
    }
    if (strcmp(key, "desktopheight") == 0) {
      rdp->desktopheight = atoi(value);
    }
    if (strcmp(key, "desktopwidth") == 0) {
      rdp->desktopwidth = atoi(value);
    }
    if (strcmp(key, "disable full window drag") == 0) {
      rdp->disable_full_window_drag = atoi(value);
    }
    if (strcmp(key, "disable menu anims") == 0) {
      rdp->disable_menu_anims = atoi(value);
    }
    if (strcmp(key, "disable themes") == 0) {
      rdp->disable_themes = atoi(value);
    }
    if (strcmp(key, "disable wallpaper") == 0) {
      rdp->disable_wallpaper = atoi(value);
    }
    if (strcmp(key, "displayconnectionbar") == 0) {
      rdp->displayconnectionbar = atoi(value);
    }
    if (strcmp(key, "domain") == 0) {
      rdp->domain = g_strdup(value);
    }
    if (strcmp(key, "enable alternate shell") == 0) {
      rdp->enable_alternate_shell = atoi(value);
    }
    if (strcmp(key, "enable wm keys") == 0) {
      rdp->enable_wm_keys = atoi(value);
    }
    if (strcmp(key, "full address") == 0) {
      rdp->full_address = g_strdup(value);
    }
    if (strcmp(key, "hide wm decorations") == 0) {
      rdp->hide_wm_decorations = atoi(value);
    }
    if (strcmp(key, "keyboard language") == 0) {
      rdp->keyboard_language = g_strdup(value);
    }
    if (strcmp(key, "keyboardhook") == 0) {
      rdp->keyboardhook = atoi(value);
    }
    if (strcmp(key, "no motion events") == 0) {
      rdp->no_motion_events = atoi(value);
    }
    if (strcmp(key, "password") == 0) {
      rdp->password = g_strdup(value);
    }
    if (strcmp(key, "password 51") == 0) {
      rdp->win_password = g_strdup(value);
    }
    if (strcmp(key, "progman group") == 0) {
      rdp->progman_group = g_strdup(value);
    }
    if (strcmp(key, "protocol") == 0) {
      rdp->protocol = atoi(value);
    }
    if (strcmp(key, "protocol file") == 0) {
      rdp->proto_file = g_strdup(value);
    }
    if (strcmp(key, "redirectcomports") == 0) {
      rdp->redirectcomports = atoi(value);
    }
    if (strcmp(key, "redirectdrives") == 0) {
      rdp->redirectdrives = atoi(value);
    }
    if (strcmp(key, "redirectprinters") == 0) {
      rdp->redirectprinters = atoi(value);
    }
    if (strcmp(key, "redirectsmartcards") == 0) {
      rdp->redirectsmartcards = atoi(value);
    }
    if (strcmp(key, "screen mode id") == 0) {
      rdp->screen_mode_id = atoi(value);
    }
    if (strcmp(key, "session bpp") == 0) {
      rdp->session_bpp = atoi(value);
    }
    if (strcmp(key, "shell working directory") == 0) {
      rdp->shell_working_directory = g_strdup(value);
    }
    if (strcmp(key, "username") == 0) {
      rdp->username = g_strdup(value);
    }
    if (strcmp(key, "winposstr") == 0) {
      rdp->winposstr = g_strdup(value);
    }

    //smode_id = atoi(value);
  }  // end if and drop out

  // clean up
  g_free (str);

  return 0;
}


/***************************************
*                                      *
*   rdp_load_profile_launcher          *
*                                      *
***************************************/
 
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

  while (lptr) {
    tsc_dropdown_append (opt, lptr->data);

    g_free (lptr->data);
    lptr = lptr->next;
    cnt++;
  }

  g_slist_free (lptr);
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
  GError *error;
  GDir *dir;
  const gchar *name;
  gchar *tmp_name;

  dir = g_dir_open (dir_name, 0, &error);
  while ((name = g_dir_read_name (dir)) != NULL) {
    tmp_name = g_build_path ("/", dir_name, name, NULL);
    if (g_file_test (tmp_name, G_FILE_TEST_IS_DIR)) {
      read_dir_list (tmp_name, list, name);
    } else {
      if (g_str_has_suffix (name, ".rdp")){
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
  GError *error;
  GDir *dir;
  const gchar *name;
  gchar *tmp_name;

  dir = g_dir_open (dir_name, 0, &error);
  while ((name = g_dir_read_name (dir)) != NULL) {
    tmp_name = g_build_path ("/", dir_name, name, NULL);
    if (g_file_test (tmp_name, G_FILE_TEST_IS_DIR)) {
      read_dir (tmp_name, hash, name);
    } else {
      if (g_str_has_suffix (name, ".rdp")){
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
	
  hash = g_hash_table_new (g_str_hash, g_str_equal);
	
  #ifdef TSCLIENT_DEBUG
  printf ("rdp_files_to_list\n");
  #endif

  path_name = g_build_path ("/", g_get_home_dir (), ".tsclient", NULL);
  read_dir (path_name, hash, "-1");
  g_free (path_name);
  return hash;
};
