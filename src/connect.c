/***************************************
*                                      *
*   frmConnect Create & Events         *
*                                      *
***************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <math.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <gio/gio.h>

#include <glib/gi18n.h>

#include "rdpfile.h"
#include "support.h"
#include "connect.h"
#include "mrulist.h"
#include "main.h"
#include "tsc-presets.h"

static void on_recent_server_selected (GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data);
static GtkFileDialog *tsc_create_rdp_dialog (const gchar *title, gboolean include_all_filter);
static GFile *tsc_file_dialog_run_open (GtkWindow *parent, GtkFileDialog *dialog);
static GFile *tsc_file_dialog_run_save (GtkWindow *parent, GtkFileDialog *dialog);
static GtkWidget *tsc_create_banner (void);
static GdkTexture *tsc_load_icon_texture (const gchar *const *candidates, gint size);
static void tsc_ensure_form_css (void);
static void tsc_align_panel_icon (GtkWidget *widget);
static void tsc_update_size_controls (void);
static void tsc_update_color_controls (void);

GtkWidget *gConnect = NULL;

typedef GFile *(*TscFileDialogFinish) (GtkFileDialog *dialog, GAsyncResult *res, GError **error);
typedef void (*TscFileDialogStart) (GtkFileDialog *dialog, GtkWindow *parent, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);

typedef struct {
  GMainLoop *loop;
  GFile *file;
  TscFileDialogFinish finish;
} TscFileDialogState;

static void
tsc_file_dialog_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  TscFileDialogState *state = user_data;
  GError *error = NULL;
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);

  state->file = state->finish (dialog, res, &error);
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("File dialog error: %s", error->message);
    g_error_free (error);
  }

  if (state->loop && g_main_loop_is_running (state->loop))
    g_main_loop_quit (state->loop);
}

static GFile *
tsc_file_dialog_run (GtkFileDialog *dialog, GtkWindow *parent,
                     TscFileDialogStart start_cb, TscFileDialogFinish finish_cb)
{
  TscFileDialogState state = {0};
  state.finish = finish_cb;
  state.loop = g_main_loop_new (NULL, FALSE);
  gtk_file_dialog_set_modal (dialog, TRUE);
  start_cb (dialog, parent, NULL, tsc_file_dialog_cb, &state);
  g_main_loop_run (state.loop);
  g_main_loop_unref (state.loop);
  return state.file;
}

static GFile *
tsc_file_dialog_run_open (GtkWindow *parent, GtkFileDialog *dialog)
{
  return tsc_file_dialog_run (dialog, parent, gtk_file_dialog_open, gtk_file_dialog_open_finish);
}

static GFile *
tsc_file_dialog_run_save (GtkWindow *parent, GtkFileDialog *dialog)
{
  return tsc_file_dialog_run (dialog, parent, gtk_file_dialog_save, gtk_file_dialog_save_finish);
}

static GdkTexture *
tsc_load_icon_texture (const gchar *const *candidates, gint size)
{
  for (gint i = 0; candidates[i]; i++) {
    gchar *path = find_pixmap_file (candidates[i]);
    if (!path)
      continue;
    GError *error = NULL;
    GdkPixbuf *pix = gdk_pixbuf_new_from_file_at_scale (path, size, size, TRUE, &error);
    g_free (path);
    if (!pix) {
      g_warning ("Failed to load icon %s: %s", candidates[i], error ? error->message : "unknown error");
      if (error)
        g_error_free (error);
      continue;
    }
    GdkTexture *texture = gdk_texture_new_for_pixbuf (pix);
    g_object_unref (pix);
    return texture;
  }
  return NULL;
}

static void
tsc_ensure_form_css (void)
{
  static GtkCssProvider *form_provider = NULL;
  if (form_provider)
    return;

  form_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (form_provider,
    ".tsc-root entry, .tsc-root combobox, .tsc-root spinbutton {"
    "  min-height: 42px;"
    "  padding: 6px 8px;"
    "  font-size: inherit;"
    "}"
    ".tsc-root .tsc-action {"
    "  min-height: 42px;"
    "  padding: 6px 12px;"
    "  font-size: inherit;"
    "}"
    ".tsc-root .tsc-action label {"
    "  font-size: inherit;"
    "}"
    ".tsc-root {"
    "  font-size: inherit;"
    "}", -1);

  GdkDisplay *display = gdk_display_get_default ();
  if (display) {
    gtk_style_context_add_provider_for_display (
      display,
      GTK_STYLE_PROVIDER (form_provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
}

static void
tsc_align_panel_icon (GtkWidget *widget)
{
  if (!widget)
    return;
  gtk_widget_set_size_request (widget, 64, 64);
  gtk_widget_set_valign (widget, GTK_ALIGN_START);
  gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
}

static void
tsc_update_size_controls (void)
{
  if (!gConnect)
    return;

  GtkWidget *aln_size = lookup_widget (gConnect, "alnSize");
  GtkWidget *opt_custom = lookup_widget (gConnect, "optSize2");
  GtkWidget *opt_full = lookup_widget (gConnect, "optSize3");
  gboolean use_custom = FALSE;
  gboolean fullscreen = FALSE;

  if (opt_custom)
    use_custom = tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (opt_custom));
  if (opt_full)
    fullscreen = tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (opt_full));

  if (aln_size)
    gtk_widget_set_sensitive (aln_size, use_custom && !fullscreen);
}

static void
tsc_update_color_controls (void)
{
  if (!gConnect)
    return;

  GtkWidget *aln_color = lookup_widget (gConnect, "alnColor");
  GtkWidget *opt_color_specific = lookup_widget (gConnect, "optColor2");
  gboolean use_specific = FALSE;

  if (opt_color_specific)
    use_specific = tsc_toggle_button_get_active (GTK_TOGGLE_BUTTON (opt_color_specific));

  if (aln_color)
    gtk_widget_set_sensitive (aln_color, use_specific);
}

static GtkWidget *
tsc_create_banner (void)
{
static const gchar *icon_candidates[] = {
    "tsclient_256x256.png",
    "tsclient_192x192.png",
    "tsclient_128x128.png",
    "tsclient_96x96.png",
    "tsclient_64x64.png",
    "tsclient_48x48.png",
    "tsclient_32x32.png",
    "tsclient_24x24.png",
    "tsclient_16x16.png",
    "tsclient.png",
    NULL
  };

  GtkWidget *overlay = gtk_overlay_new ();
  gtk_widget_set_name (overlay, "tsc-banner");
  gtk_widget_set_hexpand (overlay, TRUE);
  gtk_widget_set_halign (overlay, GTK_ALIGN_FILL);
  gtk_widget_set_vexpand (overlay, FALSE);
  gtk_widget_set_valign (overlay, GTK_ALIGN_START);
  gtk_widget_set_size_request (overlay, -1, 88);

  GtkCssProvider *css = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css,
                                   "#tsc-banner {"
                                   "  background: #5A6EBE;"
                                   "  min-height: 68px;"
                                   "  padding: 20px;"
                                   "}"
                                   ".tsc-banner-text {"
                                   "  color: #FFFFFF;"
                                   "  font-weight: 700;"
                                   "}",
                                   -1);
  gtk_style_context_add_provider (gtk_widget_get_style_context (overlay),
                                  GTK_STYLE_PROVIDER (css),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css);

  GtkWidget *content = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
  gtk_widget_set_hexpand (content, TRUE);
  gtk_widget_set_halign (content, GTK_ALIGN_START);
  gtk_widget_set_vexpand (content, FALSE);
  gtk_widget_set_valign (content, GTK_ALIGN_CENTER);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), content);

  GdkTexture *left_texture = tsc_load_icon_texture (icon_candidates, 96);
  if (left_texture) {
    GtkWidget *left_picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (left_texture));
    gtk_widget_set_size_request (left_picture, 96, 96);
    gtk_picture_set_content_fit (GTK_PICTURE (left_picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_box_append (GTK_BOX (content), left_picture);
    g_object_unref (left_texture);
  }

  GtkWidget *text_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_hexpand (text_box, TRUE);
  gtk_widget_set_halign (text_box, GTK_ALIGN_START);
  gtk_widget_set_valign (text_box, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (content), text_box);

  GtkWidget *line1 = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (line1),
                        "<span size='15600' weight='bold' foreground='#FFFFFF'>Terminal Server</span>");
  gtk_label_set_xalign (GTK_LABEL (line1), 0.0);
  gtk_widget_add_css_class (line1, "tsc-banner-text");
  gtk_box_append (GTK_BOX (text_box), line1);

  GtkWidget *line2 = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (line2),
                        "<span size='13200' weight='bold' foreground='#FFFFFF'>Client</span>");
  gtk_label_set_xalign (GTK_LABEL (line2), 0.0);
  gtk_widget_add_css_class (line2, "tsc-banner-text");
  gtk_box_append (GTK_BOX (text_box), line2);

  GdkTexture *right_texture = tsc_load_icon_texture (icon_candidates, 320);
  if (right_texture) {
    GtkWidget *watermark = gtk_picture_new_for_paintable (GDK_PAINTABLE (right_texture));
    gtk_picture_set_content_fit (GTK_PICTURE (watermark), GTK_CONTENT_FIT_COVER);
    gtk_widget_set_size_request (watermark, 280, 140);
    gtk_widget_set_opacity (watermark, 0.2);
    gtk_widget_set_hexpand (watermark, FALSE);
    gtk_widget_set_vexpand (watermark, FALSE);
    gtk_widget_set_halign (watermark, GTK_ALIGN_END);
    gtk_widget_set_valign (watermark, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_end (watermark, 12);
    gtk_overlay_add_overlay (GTK_OVERLAY (overlay), watermark);
    g_object_unref (right_texture);
  }

  return overlay;
}

static void
tsc_file_dialog_apply_rdp_filters (GtkFileDialog *dialog, gboolean include_all_filter)
{
  GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  GtkFileFilter *rdp_filter = gtk_file_filter_new ();

  gtk_file_filter_set_name (rdp_filter, _("RDP Files"));
  gtk_file_filter_add_pattern (rdp_filter, "*.rdp");
  g_list_store_append (filters, rdp_filter);

  if (include_all_filter) {
    GtkFileFilter *all_filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (all_filter, _("All Files"));
    gtk_file_filter_add_pattern (all_filter, "*");
    g_list_store_append (filters, all_filter);
    g_object_unref (all_filter);
  }

  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  gtk_file_dialog_set_default_filter (dialog, rdp_filter);

  g_object_unref (filters);
  g_object_unref (rdp_filter);
}

static GtkFileDialog *
tsc_create_rdp_dialog (const gchar *title, gboolean include_all_filter)
{
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, title);
  tsc_file_dialog_apply_rdp_filters (dialog, include_all_filter);
  return dialog;
}


/***************************************
*                                      *
*   frmConnect Create                  *
*                                      *
***************************************/

int 
create_frmConnect (void)
{
  gint i = 0;
  gsize preset_count = 0;
  const gchar **size_items = NULL;
  const gchar *color_items[] = {
    _("256 Colors (8 bit)"),
    _("High Color (15 bit)"),
    _("High Color (16 bit)"),
    _("True Color (24 bit)"),
    _("True Color (32 bit)"),
	  NULL
  };
  const gchar *tls_version_labels[] = {
    _("Negotiate TLS (default)"),
    _("TLS 1.0"),
    _("TLS 1.1"),
    _("TLS 1.2"),
    NULL
  };

  GtkWidget *frmConnect;
  
  GtkWidget *vbxComplete;
  GtkWidget *nbkComplete;
  GtkWidget *vbxCompact;

  GtkWidget *vbxMain;
  GtkWidget *vbxBanner;
  GtkWidget *imgBanner;
  const gchar *lang = gtk_get_default_language () ?
    pango_language_to_string (gtk_get_default_language ()) : "en";
  gchar *banner_lang;


  // Profile Launcher Widgets
  GtkWidget *hbxProfileLauncher;
  GtkWidget *optProfileLauncher;
  // Profile File Ops
  GtkWidget *hbxFileOps;
  GtkWidget *btnSaveAs;
  GtkWidget *btnOpen;

  // General Tab Widgets
  GtkWidget *lblGeneralTab1;
  GtkWidget *vbxGeneralTab1;

  // Logon Frame Widgets
  GtkWidget *frameLogon;
  GtkWidget *tblLogon0;
  GtkWidget *tblLogon1;
  GtkWidget *lblLogonFrame;
  GtkWidget *lblLogonNote;
  GtkWidget *imgGeneralLogon;
  GtkWidget *lblComputer;
  GtkWidget *cboComputer;
  GtkWidget *txtComputer;
  GtkWidget *lblProtocol;
  GtkWidget *optProtocol;
  GtkWidget *lblUsername;
  GtkWidget *txtUsername;
  GtkWidget *lblPassword;
  GtkWidget *txtPassword;
  GtkWidget *lblDomain;
  GtkWidget *txtDomain;
  GtkWidget *lblClientHostname;
  GtkWidget *txtClientHostname;
  GtkWidget *lblProtoFile;
  GtkWidget *hbxProtoFile;
  GtkWidget *txtProtoFile;
  GtkWidget *btnProtoFile;

  // Display Tab Widgets
  GtkWidget *lblDisplayTab1;
  GtkWidget *vbxDisplayTab1;
  // Security Frame Widgets
  GtkWidget *frameSecurity;
  GtkWidget *lblSecurityFrame;
  GtkWidget *tblSecurity;
  GtkWidget *optTlsVersion;
  GtkWidget *chkDisableEncryption;
  GtkWidget *chkDisableClientEncryption;

  // Size Frame Widgets
  GtkWidget *frameSize;
  GtkWidget *lblSizeFrame;
  GtkWidget *vbxSize;
  GtkWidget *imgSize;
  GtkWidget *hbxSize;
  GtkWidget *optSize1;
  GtkWidget *optSize2;
  GtkWidget *optSize3;
  GtkWidget *alnSize;
  GtkWidget *optSize;

  // Color Frame Widgets
  GtkWidget *frameColor;
  GtkWidget *lblColorFrame;
  GtkWidget *hbxColor;
  GtkWidget *imgColor;
  GtkWidget *vbxColor;
  GtkWidget *optColor1;
  GtkWidget *optColor2;
  GtkWidget *alnColor;
  GtkWidget *optColor;
  GtkWidget *chkForceBitmap;
  GtkWidget *chkBackingStore;

  // Local Resources Tab Widgets
  GtkWidget *lblLocalTab1;
  GtkWidget *vbxLocalTab1;

  // Sound Frame Widgets
  GtkWidget *frameSound;
  GtkWidget *lblSoundFrame;
  GtkWidget *tblSound;
  GtkWidget *imgSound;
  GtkWidget *vbxSound;
  GtkWidget *optSound1;
  GtkWidget *optSound2;
  GtkWidget *optSound3;

  // Keyboard Frame Widgets
  GtkWidget *frameKeyboard;
  GtkWidget *lblKeyboardFrame;
  GtkWidget *tblKeyboard;
  GtkWidget *lblKeyboard;
  GtkWidget *imgKeyboard;
  GtkWidget *optKeyboard;
  GtkWidget *lblKeyboardLang;
  GtkWidget *txtKeyboardLang;
  GtkWidget *chkDisableCtrl;
  GtkWidget *chkSyncNumlock;
  GtkWidget *lblCodepage;
  GtkWidget *txtCodepage;

  // Program Tab Widgets
  GtkWidget *lblProgramsTab1;

  // Program Frame Widgets
  GtkWidget *frameProgram;
  GtkWidget *lblProgramFrame;
  GtkWidget *tblProgram;
  GtkWidget *chkStartProgram;
  GtkWidget *imgProgram;
  GtkWidget *lblProgramPath;
  GtkWidget *txtProgramPath;
  GtkWidget *lblStartFolder;
  GtkWidget *txtStartFolder;

  // Performance Tab Widgets
  GtkWidget *lblPerformanceTab1;

  // Performance Frame Widgets
  GtkWidget *framePerform;
  GtkWidget *lblPerformFrame;
  GtkWidget *tblPerform;
  GtkWidget *imgPerform;
  GtkWidget *lblPerformanceOptions;
  GtkWidget *vbxExpChecks;
  GtkWidget *chkDesktopBackground;
  GtkWidget *chkWindowContent;
  GtkWidget *chkAnimation;
  GtkWidget *chkThemes;
  GtkWidget *chkBitmapCache;
  GtkWidget *chkNoMotionEvents;
  GtkWidget *chkEnableWMKeys;
  GtkWidget *chkHideWMDecorations;
  GtkWidget *chkAttachToConsole;

  GSList *optSize1_group = NULL;
  GSList *optColor1_group = NULL;
  GSList *optSound1_group = NULL;

  // Complete Button Box Widgets
  GtkWidget *hbbAppOps;
  GtkWidget *btnConnect;
  GtkWidget *alnConnect;
  GtkWidget *hbxConnect;
  GtkWidget *imgConnect;
  GtkWidget *lblConnect;
  GtkWidget *btnQuit;
  GtkWidget *btnHelp;

  #ifdef TSCLIENT_DEBUG
  printf ("create_frmConnect\n");
  #endif

  tsc_ensure_form_css ();

  /*
    This is the main form
  */
  frmConnect = gtk_application_window_new (tsc_app);
  gtk_window_set_title (GTK_WINDOW (frmConnect), _("Terminal Server Client"));
  gtk_window_set_default_size (GTK_WINDOW (frmConnect), 676, 560);
  gtk_window_set_resizable (GTK_WINDOW (frmConnect), TRUE);
  gtk_widget_add_css_class (frmConnect, "tsc-root");
  gtk_window_set_icon_name (GTK_WINDOW (frmConnect), "tsclient");
  
  /*
    This is the main container
  */
  vbxMain = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frmConnect), vbxMain);

  /*
    This is the banner at the top
  */
  vbxBanner = tsc_create_banner ();
  gtk_box_pack_start (GTK_BOX (vbxMain), vbxBanner, FALSE, TRUE, 0);

  /*
    This is the complete container
  */
  vbxComplete = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbxMain), vbxComplete, TRUE, TRUE, 0);


  /*
    This is the label for the Connection Profile
  lblConnectionProfile = gtk_label_new (_("Connection Profile"));
  gtk_widget_set_name (lblConnectionProfile, "lblConnectionProfile");
  gtk_label_set_markup (GTK_LABEL (lblConnectionProfile), g_strconcat ("<span weight=\"bold\">", _("Connection Profile"), "</span>", NULL));
  gtk_label_set_justify (GTK_LABEL (lblConnectionProfile), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lblConnectionProfile), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbxComplete), lblConnectionProfile, TRUE, FALSE, 0);
  gtk_misc_set_padding (GTK_MISC (lblConnectionProfile), 6, 3);
  */


  /*
    This is the notebook for the Complete mode
  */
  nbkComplete = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (vbxComplete), nbkComplete, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (nbkComplete), 6);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (nbkComplete), FALSE);

  vbxGeneralTab1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (nbkComplete), vbxGeneralTab1);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (nbkComplete), vbxGeneralTab1,
                                      TRUE, TRUE, GTK_PACK_START);
                                      

  frameLogon = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbxGeneralTab1), frameLogon, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frameLogon), 3);
  gtk_frame_set_shadow_type (GTK_FRAME (frameLogon), GTK_SHADOW_NONE);

  lblLogonFrame = gtk_label_new (_("Logon Settings"));
  gtk_label_set_markup (GTK_LABEL (lblLogonFrame), g_strconcat ("<span weight=\"bold\">", _("Logon Settings"), "</span>", NULL));
  gtk_frame_set_label_widget (GTK_FRAME (frameLogon), lblLogonFrame);
  gtk_label_set_justify (GTK_LABEL (lblLogonFrame), GTK_JUSTIFY_LEFT);

  tblLogon0 = gtk_table_new (2, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (frameLogon), tblLogon0);

  lblLogonNote = gtk_label_new (_("Type the name of the computer or choose a computer from the drop-down list."));
  gtk_table_attach (GTK_TABLE (tblLogon0), lblLogonNote, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_label_set_justify (GTK_LABEL (lblLogonNote), GTK_JUSTIFY_LEFT);
  gtk_label_set_wrap (GTK_LABEL (lblLogonNote), TRUE);
  gtk_misc_set_alignment (GTK_MISC (lblLogonNote), 0, 0.5);

  imgGeneralLogon = create_pixmap (frmConnect, "icon_laptop.png");
  gtk_table_attach (GTK_TABLE (tblLogon0), imgGeneralLogon, 0, 1, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_padding (GTK_MISC (imgGeneralLogon), 3, 3);
  tsc_align_panel_icon (imgGeneralLogon);

  tblLogon1 = gtk_table_new (6, 2, FALSE);
  gtk_table_attach (GTK_TABLE (tblLogon0), tblLogon1, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_table_set_row_spacings (GTK_TABLE (tblLogon1), 4);
  gtk_table_set_col_spacings (GTK_TABLE (tblLogon1), 4);

  lblComputer = gtk_label_new_with_mnemonic (_("Compu_ter:"));
  gtk_table_attach (GTK_TABLE (tblLogon1), lblComputer, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 6, 6);
  gtk_label_set_justify (GTK_LABEL (lblComputer), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lblComputer), 0, 0.5);

  lblProtocol = gtk_label_new_with_mnemonic (_("Pro_tocol:"));
  gtk_table_attach (GTK_TABLE (tblLogon1), lblProtocol, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 6, 6);
  gtk_label_set_justify (GTK_LABEL (lblProtocol), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lblProtocol), 0, 0.5);

  lblUsername = gtk_label_new_with_mnemonic (_("_User Name:"));
  gtk_table_attach (GTK_TABLE (tblLogon1), lblUsername, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 6, 6);
  gtk_label_set_justify (GTK_LABEL (lblUsername), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lblUsername), 0, 0.5);

  lblPassword = gtk_label_new_with_mnemonic (_("Pass_word:"));
  gtk_table_attach (GTK_TABLE (tblLogon1), lblPassword, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 6, 6);
  gtk_label_set_justify (GTK_LABEL (lblPassword), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lblPassword), 0, 0.5);

  lblDomain = gtk_label_new_with_mnemonic (_("Do_main:"));
  gtk_table_attach (GTK_TABLE (tblLogon1), lblDomain, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 6, 6);
  gtk_label_set_justify (GTK_LABEL (lblDomain), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lblDomain), 0, 0.5);

  lblClientHostname = gtk_label_new_with_mnemonic (_("C_lient Hostname:"));
  gtk_table_attach (GTK_TABLE (tblLogon1), lblClientHostname, 0, 1, 5, 6,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 6, 6);
  gtk_label_set_justify (GTK_LABEL (lblClientHostname), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lblClientHostname), 0, 0.5);

  lblProtoFile = gtk_label_new_with_mnemonic (_("Prot_ocol File:"));
  gtk_table_attach (GTK_TABLE (tblLogon1), lblProtoFile, 0, 1, 6, 7,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 6, 6);
  gtk_label_set_justify (GTK_LABEL (lblProtoFile), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lblProtoFile), 0, 0.5);

  GtkWidget *hbxComputer = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (tblLogon1), hbxComputer, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  txtComputer = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbxComputer), txtComputer, TRUE, TRUE, 0);
  gtk_entry_set_activates_default (GTK_ENTRY (txtComputer), TRUE);
  gtk_widget_set_tooltip_text (txtComputer, _("Enter the name or address of the remote system."));

  cboComputer = tsc_dropdown_new ((const gchar * const []) { _("Recent"), NULL });
  gtk_box_pack_start (GTK_BOX (hbxComputer), cboComputer, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (cboComputer, FALSE);
  g_signal_connect (cboComputer, "notify::selected",
                    G_CALLBACK (on_recent_server_selected), txtComputer);

  const gchar *protocol_items[] = {
    _("RDPv4 (legacy)"),
    _("RDPv5 (recommended)"),
    _("VNC"),
    _("XDMCP"),
    _("ICA"),
    NULL
  };
  optProtocol = tsc_dropdown_new (protocol_items);
  gtk_table_attach (GTK_TABLE (tblLogon1), optProtocol, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (optProtocol, _("Select the protocol to use for this connection."));
  tsc_dropdown_set_selected (optProtocol, 1);
  g_signal_connect (optProtocol, "notify::selected",
                    G_CALLBACK (on_protocol_changed), frmConnect);


  txtUsername = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (txtUsername), TRUE);
  gtk_table_attach (GTK_TABLE (tblLogon1), txtUsername, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (txtUsername, _("Enter the username for the remote system.\nFor VNC, enter the path to your saved vnc password file."));

  txtPassword = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (txtPassword), TRUE);
  gtk_table_attach (GTK_TABLE (tblLogon1), txtPassword, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_entry_set_visibility (GTK_ENTRY (txtPassword), FALSE);
  gtk_widget_set_tooltip_text (txtPassword, _("Enter the password for the remote system."));

  txtDomain = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (txtDomain), TRUE);
  gtk_table_attach (GTK_TABLE (tblLogon1), txtDomain, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (txtDomain, _("Enter the domain for the remote system."));

  txtClientHostname = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (txtClientHostname), TRUE);
  gtk_table_attach (GTK_TABLE (tblLogon1), txtClientHostname, 1, 2, 5, 6,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (txtClientHostname, _("Enter the local hostname for this system."));

  hbxProtoFile = gtk_hbox_new (FALSE, 2);
  gtk_table_attach (GTK_TABLE (tblLogon1), hbxProtoFile, 1, 2, 6, 7,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  txtProtoFile = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (txtProtoFile), TRUE);
  gtk_box_pack_start (GTK_BOX (hbxProtoFile), txtProtoFile, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (txtProtoFile, _("Some protocols require a file containing settings. If required, enter the path to the file here."));

  btnProtoFile = gtk_button_new_with_label ("...");
  gtk_widget_add_css_class (btnProtoFile, "tsc-action");
  gtk_container_add (GTK_CONTAINER (hbxProtoFile), btnProtoFile);

  frameSecurity = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbxGeneralTab1), frameSecurity, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frameSecurity), 3);
  gtk_frame_set_shadow_type (GTK_FRAME (frameSecurity), GTK_SHADOW_NONE);

  lblSecurityFrame = gtk_label_new (_("Security"));
  gtk_label_set_markup (GTK_LABEL (lblSecurityFrame),
                        g_strconcat ("<span weight=\"bold\">", _("Security"), "</span>", NULL));
  gtk_frame_set_label_widget (GTK_FRAME (frameSecurity), lblSecurityFrame);
  gtk_label_set_justify (GTK_LABEL (lblSecurityFrame), GTK_JUSTIFY_LEFT);

  tblSecurity = gtk_table_new (3, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (frameSecurity), tblSecurity);
  gtk_table_set_col_spacings (GTK_TABLE (tblSecurity), 6);

  optTlsVersion = tsc_dropdown_new (tls_version_labels);
  gtk_table_attach (GTK_TABLE (tblSecurity), optTlsVersion, 0, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (optTlsVersion, _("Choose which TLS version to advertise to the server."));
  tsc_dropdown_set_selected (optTlsVersion, 0);

  chkDisableEncryption = gtk_check_button_new_with_mnemonic (_("Disable all encryption (-e)"));
  gtk_table_attach (GTK_TABLE (tblSecurity), chkDisableEncryption, 0, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  chkDisableClientEncryption = gtk_check_button_new_with_mnemonic (_("Disable client-to-server encryption (-E)"));
  gtk_table_attach (GTK_TABLE (tblSecurity), chkDisableClientEncryption, 0, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  vbxDisplayTab1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (nbkComplete), vbxDisplayTab1);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (nbkComplete), vbxDisplayTab1,
                                      TRUE, TRUE, GTK_PACK_START);

  frameSize = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbxDisplayTab1), frameSize, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frameSize), 3);
  gtk_frame_set_shadow_type (GTK_FRAME (frameSize), GTK_SHADOW_NONE);

  lblSizeFrame = gtk_label_new (_("Remote Desktop Size"));
  gtk_label_set_markup (GTK_LABEL (lblSizeFrame), g_strconcat ("<span weight=\"bold\">", _("Remote Desktop Size"), "</span>", NULL));
  gtk_frame_set_label_widget (GTK_FRAME (frameSize), lblSizeFrame);
  gtk_label_set_justify (GTK_LABEL (lblSizeFrame), GTK_JUSTIFY_LEFT);

  hbxSize = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frameSize), hbxSize);

  imgSize = create_pixmap (frmConnect, "icon_size.png");
  gtk_box_pack_start (GTK_BOX (hbxSize), imgSize, FALSE, TRUE, 0);
  gtk_misc_set_alignment (GTK_MISC (imgSize), 0, 0);
  gtk_misc_set_padding (GTK_MISC (imgSize), 3, 3);
  tsc_align_panel_icon (imgSize);

  vbxSize = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbxSize), vbxSize, TRUE, TRUE, 0);

  optSize1 = gtk_radio_button_new_with_mnemonic (NULL, _("Use default screen size"));
  gtk_box_pack_start (GTK_BOX (vbxSize), optSize1, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (optSize1), 3);
  gtk_widget_set_tooltip_text (optSize1, _("Use the default screen size."));
  tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (optSize1), TRUE);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (optSize1), optSize1_group);
  optSize1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (optSize1));

  optSize2 = gtk_radio_button_new_with_mnemonic (NULL, _("Use specified screen size"));
  gtk_box_pack_start (GTK_BOX (vbxSize), optSize2, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (optSize2), 3);
  gtk_widget_set_tooltip_text (optSize2, _("Use the list to choose the screen size to use."));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (optSize2), optSize1_group);
  optSize1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (optSize2));

  alnSize = gtk_alignment_new (0.5, 0.5, 0.88, 1);
  gtk_box_pack_start (GTK_BOX (vbxSize), alnSize, FALSE, TRUE, 0);

  preset_count = TSC_SCREEN_PRESET_COUNT;
  size_items = g_new0 (const gchar *, preset_count + 1);
  for (gsize idx = 0; idx < preset_count; idx++) {
    size_items[idx] = _(tsc_screen_presets[idx].label);
  }
  optSize = tsc_dropdown_new (size_items);
  g_free ((gpointer) size_items);
  gtk_container_add (GTK_CONTAINER (alnSize), optSize);

  optSize3 = gtk_radio_button_new_with_mnemonic (NULL, _("Operate in full screen mode"));
  gtk_box_pack_start (GTK_BOX (vbxSize), optSize3, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (optSize3), 3);
  gtk_widget_set_tooltip_text (optSize3, _("Work in full screen mode."));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (optSize3), optSize1_group);



  frameColor = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbxDisplayTab1), frameColor, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frameColor), 3);
  gtk_frame_set_shadow_type (GTK_FRAME (frameColor), GTK_SHADOW_NONE);

  lblColorFrame = gtk_label_new (_("Colors"));
  gtk_label_set_markup (GTK_LABEL (lblColorFrame), g_strconcat ("<span weight=\"bold\">", _("Colors"), "</span>", NULL));
  gtk_frame_set_label_widget (GTK_FRAME (frameColor), lblColorFrame);
  gtk_label_set_justify (GTK_LABEL (lblColorFrame), GTK_JUSTIFY_LEFT);

  hbxColor = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frameColor), hbxColor);

  imgColor = create_pixmap (frmConnect, "icon_colors.png");
  gtk_box_pack_start (GTK_BOX (hbxColor), imgColor, FALSE, TRUE, 0);
  gtk_misc_set_alignment (GTK_MISC (imgColor), 0, 0);
  gtk_misc_set_padding (GTK_MISC (imgColor), 3, 3);
  tsc_align_panel_icon (imgColor);

  vbxColor = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbxColor), vbxColor, TRUE, TRUE, 0);

  optColor1 = gtk_radio_button_new_with_mnemonic (NULL, _("Use default color depth"));
  gtk_box_pack_start (GTK_BOX (vbxColor), optColor1, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (optColor1), 3);
  gtk_widget_set_tooltip_text (optColor1, _("Use the default color depth."));
  tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (optColor1), TRUE);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (optColor1), optColor1_group);
  optColor1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (optColor1));

  optColor2 = gtk_radio_button_new_with_mnemonic (NULL, _("Use specified color depth"));
  gtk_box_pack_start (GTK_BOX (vbxColor), optColor2, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (optColor2), 3);
  gtk_widget_set_tooltip_text (optColor2, _("Use the list to choose the color depth to use."));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (optColor2), optColor1_group);

  alnColor = gtk_alignment_new (0.5, 0.5, 0.88, 1);
  gtk_box_pack_start (GTK_BOX (vbxColor), alnColor, FALSE, TRUE, 0);

  optColor = tsc_dropdown_new (color_items);
  gtk_container_add (GTK_CONTAINER (alnColor), optColor);
  gtk_widget_set_tooltip_text (optColor, _("Pick a color depth to request from the server."));

  chkForceBitmap = gtk_check_button_new_with_mnemonic (_("Force bitmap updates (-b)"));
  gtk_box_pack_start (GTK_BOX (vbxColor), chkForceBitmap, FALSE, FALSE, 0);

  chkBackingStore = gtk_check_button_new_with_mnemonic (_("Use X server backing store (-B)"));
  gtk_box_pack_start (GTK_BOX (vbxColor), chkBackingStore, FALSE, FALSE, 0);

  vbxLocalTab1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (nbkComplete), vbxLocalTab1);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (nbkComplete), vbxLocalTab1,
                                      TRUE, TRUE, GTK_PACK_START);

  frameSound = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbxLocalTab1), frameSound, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frameSound), 3);
  gtk_frame_set_shadow_type (GTK_FRAME (frameSound), GTK_SHADOW_NONE);

  lblSoundFrame = gtk_label_new_with_mnemonic (_("Remote Computer _Sound"));
  gtk_label_set_markup (GTK_LABEL (lblSoundFrame), g_strconcat ("<span weight=\"bold\">", _("Remote Computer Sound"), "</span>", NULL));
  gtk_frame_set_label_widget (GTK_FRAME (frameSound), lblSoundFrame);
  gtk_label_set_justify (GTK_LABEL (lblSoundFrame), GTK_JUSTIFY_LEFT);

  tblSound = gtk_table_new (1, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (frameSound), tblSound);
  gtk_table_set_col_spacings (GTK_TABLE (tblSound), 6);

  imgSound = create_pixmap (frmConnect, "icon_sound.png");
  gtk_misc_set_padding (GTK_MISC (imgSound), 3, 3);
  gtk_table_attach (GTK_TABLE (tblSound), imgSound, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) 0, 0, 0);
  tsc_align_panel_icon (imgSound);

  vbxSound = gtk_vbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE (tblSound), vbxSound, 1, 2, 0, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  optSound1 = gtk_radio_button_new_with_mnemonic (NULL, _("On the local computer"));
  gtk_box_pack_start (GTK_BOX (vbxSound), optSound1, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (optSound1), 3);
  gtk_widget_set_tooltip_text (optSound1, _("On the local computer"));
  tsc_toggle_button_set_active (GTK_TOGGLE_BUTTON (optSound1), TRUE);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (optSound1), optSound1_group);
  optSound1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (optSound1));

  optSound2 = gtk_radio_button_new_with_mnemonic (NULL, _("On the remote computer"));
  gtk_box_pack_start (GTK_BOX (vbxSound), optSound2, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (optSound2), 3);
  gtk_widget_set_tooltip_text (optSound2, _("On the remote computer"));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (optSound2), optSound1_group);

  optSound3 = gtk_radio_button_new_with_mnemonic (NULL, _("Do not play"));
  gtk_box_pack_start (GTK_BOX (vbxSound), optSound3, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (optSound3), 3);
  gtk_widget_set_tooltip_text (optSound3, _("Do not play"));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (optSound3), optSound1_group);


  frameKeyboard = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbxLocalTab1), frameKeyboard, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frameKeyboard), 3);
  gtk_frame_set_shadow_type (GTK_FRAME (frameKeyboard), GTK_SHADOW_NONE);

  lblKeyboardFrame = gtk_label_new_with_mnemonic (_("_Keyboard"));
  gtk_label_set_markup (GTK_LABEL (lblKeyboardFrame), g_strconcat ("<span weight=\"bold\">", _("Keyboard"), "</span>", NULL));
  gtk_frame_set_label_widget (GTK_FRAME (frameKeyboard), lblKeyboardFrame);
  gtk_label_set_justify (GTK_LABEL (lblKeyboardFrame), GTK_JUSTIFY_LEFT);

  tblKeyboard = gtk_table_new (2, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (frameKeyboard), tblKeyboard);
  gtk_table_set_col_spacings (GTK_TABLE (tblKeyboard), 6);

  const gchar *keyboard_items[] = {
    _("On the local computer"),
    _("On the remote computer"),
    _("In full screen mode only"),
    NULL
  };
  optKeyboard = tsc_dropdown_new (keyboard_items);
  gtk_table_attach (GTK_TABLE (tblKeyboard), optKeyboard, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  lblKeyboardLang = gtk_label_new (_("Use the following keyboard language\n(2 char keycode)"));
  gtk_table_attach (GTK_TABLE (tblKeyboard), lblKeyboardLang, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (lblKeyboardLang), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lblKeyboardLang), 0, 0.5);

  txtKeyboardLang = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (tblKeyboard), txtKeyboardLang, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  chkDisableCtrl = gtk_check_button_new_with_mnemonic (_("Disable remote Ctrl key combinations (-t)"));
  gtk_table_attach (GTK_TABLE (tblKeyboard), chkDisableCtrl, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  chkSyncNumlock = gtk_check_button_new_with_mnemonic (_("Synchronize Num Lock state (-N)"));
  gtk_table_attach (GTK_TABLE (tblKeyboard), chkSyncNumlock, 1, 2, 5, 6,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  lblCodepage = gtk_label_new (_("Local codepage (for -L)"));
  gtk_table_attach (GTK_TABLE (tblKeyboard), lblCodepage, 1, 2, 6, 7,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (lblCodepage), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lblCodepage), 0, 0.5);

  txtCodepage = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (tblKeyboard), txtCodepage, 1, 2, 7, 8,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  lblKeyboard = gtk_label_new (_("Apply Windows key combinations\n(for example ALT+TAB) (unsupported)"));
  gtk_table_attach (GTK_TABLE (tblKeyboard), lblKeyboard, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (lblKeyboard), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lblKeyboard), 0, 0.5);

  imgKeyboard = create_pixmap (frmConnect, "icon_keyboard.png");
  gtk_table_attach (GTK_TABLE (tblKeyboard), imgKeyboard, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) 0, 0, 0);
  gtk_misc_set_padding (GTK_MISC (imgKeyboard), 3, 3);
  tsc_align_panel_icon (imgKeyboard);

  frameProgram = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (nbkComplete), frameProgram);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (nbkComplete), frameProgram,
                                      TRUE, TRUE, GTK_PACK_START);
  gtk_container_set_border_width (GTK_CONTAINER (frameProgram), 3);
  gtk_frame_set_shadow_type (GTK_FRAME (frameProgram), GTK_SHADOW_NONE);

  lblProgramFrame = gtk_label_new (_("Start a Program"));
  gtk_label_set_markup (GTK_LABEL (lblProgramFrame), g_strconcat ("<span weight=\"bold\">", _("Start a Program"), "</span>", NULL));
  gtk_frame_set_label_widget (GTK_FRAME (frameProgram), lblProgramFrame);
  gtk_label_set_justify (GTK_LABEL (lblProgramFrame), GTK_JUSTIFY_LEFT);

  tblProgram = gtk_table_new (2, 5, FALSE);
  gtk_container_add (GTK_CONTAINER (frameProgram), tblProgram);

  imgProgram = create_pixmap (frmConnect, "icon_program.png");
  gtk_table_attach (GTK_TABLE (tblProgram), imgProgram, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_misc_set_padding (GTK_MISC (imgProgram), 3, 3);
  tsc_align_panel_icon (imgProgram);

  chkStartProgram = gtk_check_button_new_with_mnemonic (_("Start the following program on connection"));
  gtk_table_attach (GTK_TABLE (tblProgram), chkStartProgram, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (chkStartProgram), 3);

  lblProgramPath = gtk_label_new (_("Program path and filename"));
  gtk_table_attach (GTK_TABLE (tblProgram), lblProgramPath, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_label_set_justify (GTK_LABEL (lblProgramPath), GTK_JUSTIFY_LEFT);

  txtProgramPath = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (tblProgram), txtProgramPath, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  lblStartFolder = gtk_label_new (_("Start in the following folder"));
  gtk_table_attach (GTK_TABLE (tblProgram), lblStartFolder, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_label_set_justify (GTK_LABEL (lblStartFolder), GTK_JUSTIFY_LEFT);

  txtStartFolder = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (tblProgram), txtStartFolder, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);


  framePerform = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (nbkComplete), framePerform);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (nbkComplete), framePerform,
                                      TRUE, TRUE, GTK_PACK_START);
  gtk_container_set_border_width (GTK_CONTAINER (framePerform), 3);
  gtk_frame_set_shadow_type (GTK_FRAME (framePerform), GTK_SHADOW_NONE);

  lblPerformFrame = gtk_label_new (_("Optimize Performance"));
  gtk_label_set_markup (GTK_LABEL (lblPerformFrame), g_strconcat ("<span weight=\"bold\">", _("Optimize Performance"), "</span>", NULL));
  gtk_frame_set_label_widget (GTK_FRAME (framePerform), lblPerformFrame);
  gtk_label_set_justify (GTK_LABEL (lblPerformFrame), GTK_JUSTIFY_LEFT);

  tblPerform = gtk_table_new (2, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (framePerform), tblPerform);

  imgPerform = create_pixmap (frmConnect, "icon_perform.png");
  gtk_table_attach (GTK_TABLE (tblPerform), imgPerform, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_misc_set_padding (GTK_MISC (imgPerform), 3, 3);
  tsc_align_panel_icon (imgPerform);

  vbxExpChecks = gtk_vbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE (tblPerform), vbxExpChecks, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

  chkDesktopBackground = gtk_check_button_new_with_mnemonic (_("Desktop background (u)"));
  gtk_box_pack_start (GTK_BOX (vbxExpChecks), chkDesktopBackground, FALSE, FALSE, 0);

  chkWindowContent = gtk_check_button_new_with_mnemonic (_("Show content of window while dragging (u)"));
  gtk_box_pack_start (GTK_BOX (vbxExpChecks), chkWindowContent, FALSE, FALSE, 0);

  chkAnimation = gtk_check_button_new_with_mnemonic (_("Menu and window animation (u)"));
  gtk_box_pack_start (GTK_BOX (vbxExpChecks), chkAnimation, FALSE, FALSE, 0);

  chkThemes = gtk_check_button_new_with_mnemonic (_("Themes (u)"));
  gtk_box_pack_start (GTK_BOX (vbxExpChecks), chkThemes, FALSE, FALSE, 0);

  chkBitmapCache = gtk_check_button_new_with_mnemonic (_("Enable bitmap caching"));
  gtk_box_pack_start (GTK_BOX (vbxExpChecks), chkBitmapCache, FALSE, FALSE, 0);

  chkNoMotionEvents = gtk_check_button_new_with_mnemonic (_("Do not send motion events"));
  gtk_box_pack_start (GTK_BOX (vbxExpChecks), chkNoMotionEvents, FALSE, FALSE, 0);

  chkEnableWMKeys = gtk_check_button_new_with_mnemonic (_("Enable window manager's key bindings"));
  gtk_box_pack_start (GTK_BOX (vbxExpChecks), chkEnableWMKeys, FALSE, FALSE, 0);

  chkHideWMDecorations = gtk_check_button_new_with_mnemonic (_("Hide window manager's decorations"));
  gtk_box_pack_start (GTK_BOX (vbxExpChecks), chkHideWMDecorations, FALSE, FALSE, 0);

  chkAttachToConsole = gtk_check_button_new_with_mnemonic (_("Attach to console"));
  gtk_box_pack_start (GTK_BOX (vbxExpChecks), chkAttachToConsole, FALSE, FALSE, 0);


  lblPerformanceOptions = gtk_label_new (_("Options available for optimizing performance"));
  gtk_table_attach (GTK_TABLE (tblPerform), lblPerformanceOptions, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_label_set_justify (GTK_LABEL (lblPerformanceOptions), GTK_JUSTIFY_LEFT);
  gtk_label_set_wrap (GTK_LABEL (lblPerformanceOptions), TRUE);
  gtk_misc_set_alignment (GTK_MISC (lblPerformanceOptions), 0, 0.5);

  /*
    These are the labels for the notebook control
  */

  lblGeneralTab1 = gtk_label_new_with_mnemonic (_("_General"));
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (nbkComplete), gtk_notebook_get_nth_page (GTK_NOTEBOOK (nbkComplete), 0), lblGeneralTab1);
  gtk_label_set_justify (GTK_LABEL (lblGeneralTab1), GTK_JUSTIFY_LEFT);

  lblDisplayTab1 = gtk_label_new_with_mnemonic (_("_Display"));
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (nbkComplete), gtk_notebook_get_nth_page (GTK_NOTEBOOK (nbkComplete), 1), lblDisplayTab1);
  gtk_label_set_justify (GTK_LABEL (lblDisplayTab1), GTK_JUSTIFY_LEFT);

  lblLocalTab1 = gtk_label_new_with_mnemonic (_("Local _Resources"));
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (nbkComplete), gtk_notebook_get_nth_page (GTK_NOTEBOOK (nbkComplete), 2), lblLocalTab1);
  gtk_label_set_justify (GTK_LABEL (lblLocalTab1), GTK_JUSTIFY_LEFT);

  lblProgramsTab1 = gtk_label_new_with_mnemonic (_("_Programs"));
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (nbkComplete), gtk_notebook_get_nth_page (GTK_NOTEBOOK (nbkComplete), 3), lblProgramsTab1);
  gtk_label_set_justify (GTK_LABEL (lblProgramsTab1), GTK_JUSTIFY_LEFT);

  lblPerformanceTab1 = gtk_label_new_with_mnemonic (_("Perf_ormance"));
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (nbkComplete), gtk_notebook_get_nth_page (GTK_NOTEBOOK (nbkComplete), 4), lblPerformanceTab1);
  gtk_label_set_justify (GTK_LABEL (lblPerformanceTab1), GTK_JUSTIFY_LEFT);

  gtk_label_set_mnemonic_widget (GTK_LABEL (lblComputer), txtComputer);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblProtocol), optProtocol);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblUsername), txtUsername);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblPassword), txtPassword);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblDomain), txtDomain);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblClientHostname), txtClientHostname);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblProtoFile), txtProtoFile);


  /*
    This is the Profile Launcher
  */
  hbxProfileLauncher = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbxComplete), hbxProfileLauncher, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbxProfileLauncher), 6);
  gtk_box_set_spacing (GTK_BOX (hbxProfileLauncher), 6);

  optProfileLauncher = tsc_dropdown_new ((const gchar * const []) { _("Quick Connect"), NULL });
  gtk_widget_set_tooltip_text (optProfileLauncher, _("Select a saved profile to launch it immediately."));
  gtk_box_pack_start (GTK_BOX (hbxProfileLauncher), optProfileLauncher, TRUE, TRUE, 0);
  g_signal_connect (optProfileLauncher, "notify::selected",
                    G_CALLBACK (tsc_quick_pick_activate), NULL);

  hbxFileOps = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbxProfileLauncher), hbxFileOps, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbxFileOps), 0);
  gtk_box_set_spacing (GTK_BOX (hbxFileOps), 6);

  btnOpen = gtk_button_new_with_mnemonic (_("_Open…"));
  gtk_widget_add_css_class (btnOpen, "tsc-action");
  gtk_widget_set_tooltip_text (btnOpen, _("Open an existing profile file."));
  gtk_box_pack_start (GTK_BOX (hbxFileOps), btnOpen, TRUE, TRUE, 0);

  btnSaveAs = gtk_button_new_with_mnemonic (_("Save _As…"));
  gtk_widget_add_css_class (btnSaveAs, "tsc-action");
  gtk_widget_set_tooltip_text (btnSaveAs, _("Save the current settings to a profile file."));
  gtk_box_pack_start (GTK_BOX (hbxFileOps), btnSaveAs, TRUE, TRUE, 0);


  /*
    This is the button box for the Complete mode
  */

  hbbAppOps = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbxComplete), hbbAppOps, FALSE, TRUE, 0);
  gtk_widget_set_hexpand (hbbAppOps, TRUE);
  gtk_widget_set_halign (hbbAppOps, GTK_ALIGN_END);
  gtk_container_set_border_width (GTK_CONTAINER (hbbAppOps), 6);

  btnHelp = gtk_button_new_with_mnemonic (_("_About"));
  gtk_widget_add_css_class (btnHelp, "tsc-action");
  gtk_box_pack_start (GTK_BOX (hbbAppOps), btnHelp, FALSE, FALSE, 0);

  btnQuit = gtk_button_new_with_mnemonic (_("_Exit"));
  gtk_widget_add_css_class (btnQuit, "tsc-action");
  gtk_box_pack_start (GTK_BOX (hbbAppOps), btnQuit, FALSE, FALSE, 0);

  btnConnect = gtk_button_new ();
  gtk_widget_add_css_class (btnConnect, "tsc-action");
  gtk_box_pack_start (GTK_BOX (hbbAppOps), btnConnect, FALSE, FALSE, 0);
  gtk_window_set_default_widget (GTK_WINDOW (frmConnect), btnConnect);


  alnConnect = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_container_add (GTK_CONTAINER (btnConnect), alnConnect);

  hbxConnect = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (alnConnect), hbxConnect);

  lblConnect = gtk_label_new_with_mnemonic (_("Co_nnect"));
  gtk_box_pack_start (GTK_BOX (hbxConnect), lblConnect, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (lblConnect), GTK_JUSTIFY_LEFT);


  g_signal_connect (G_OBJECT (btnConnect), "clicked",
                    G_CALLBACK (on_btnConnect_clicked),
                    NULL);
  g_signal_connect (G_OBJECT (btnQuit), "clicked",
                    G_CALLBACK (on_btnQuit_clicked),
                    NULL);
  g_signal_connect (G_OBJECT (btnHelp), "clicked",
                    G_CALLBACK (on_btnHelp_clicked),
                    NULL);

  g_signal_connect (G_OBJECT (optSize1), "toggled",
                    G_CALLBACK (on_optSize1_clicked),
                    NULL);
  g_signal_connect (G_OBJECT (optSize2), "toggled",
                    G_CALLBACK (on_optSize2_clicked),
                    NULL);
  g_signal_connect (G_OBJECT (optSize3), "toggled",
                    G_CALLBACK (on_optSize3_clicked),
                    NULL);

  g_signal_connect (G_OBJECT (optColor1), "toggled",
                    G_CALLBACK (on_optColor1_clicked),
                    NULL);
  g_signal_connect (G_OBJECT (optColor2), "toggled",
                    G_CALLBACK (on_optColor2_clicked),
                    NULL);

  g_signal_connect (G_OBJECT (btnSaveAs), "clicked",
                    G_CALLBACK (on_btnSaveAs_clicked),
                    NULL);
  g_signal_connect (G_OBJECT (btnOpen), "clicked",
                    G_CALLBACK (on_btnOpen_clicked),
                    NULL);
  g_signal_connect (G_OBJECT (btnProtoFile), "clicked",
                    G_CALLBACK (on_btnProtoFile_clicked),
                    NULL);

  g_signal_connect (G_OBJECT (chkStartProgram), "toggled",
                    G_CALLBACK (on_chkStartProgram_toggled),
                    NULL);

  /* Store pointers to all widgets, for use by lookup_widget(). */
  g_object_set_data (G_OBJECT (frmConnect), "frmConnect", frmConnect);

  HOOKUP_OBJECT (frmConnect, vbxComplete, "vbxComplete");
  HOOKUP_OBJECT (frmConnect, nbkComplete, "nbkComplete");

  HOOKUP_OBJECT (frmConnect, vbxMain, "vbxMain");
  HOOKUP_OBJECT (frmConnect, vbxBanner, "vbxBanner");
  HOOKUP_OBJECT (frmConnect, imgBanner, "imgBanner");

  // General Tab Widgets
  HOOKUP_OBJECT (frmConnect, lblGeneralTab1, "lblGeneralTab1");
  HOOKUP_OBJECT (frmConnect, vbxGeneralTab1, "vbxGeneralTab1");

  // Logon Frame Widgets
  HOOKUP_OBJECT (frmConnect, frameLogon, "frameLogon");
  HOOKUP_OBJECT (frmConnect, lblLogonFrame, "lblLogonFrame");
  HOOKUP_OBJECT (frmConnect, tblLogon0, "tblLogon0");
  HOOKUP_OBJECT (frmConnect, tblLogon1, "tblLogon1");
  HOOKUP_OBJECT (frmConnect, lblLogonNote, "lblLogonNote");
  HOOKUP_OBJECT (frmConnect, imgGeneralLogon, "imgGeneralLogon");
  HOOKUP_OBJECT (frmConnect, lblComputer, "lblComputer");
  HOOKUP_OBJECT (frmConnect, cboComputer, "cboComputer");
  HOOKUP_OBJECT (frmConnect, txtComputer, "txtComputer");
  HOOKUP_OBJECT (frmConnect, lblProtocol, "lblProtocol");
  HOOKUP_OBJECT (frmConnect, optProtocol, "optProtocol");
  HOOKUP_OBJECT (frmConnect, lblUsername, "lblUsername");
  HOOKUP_OBJECT (frmConnect, txtUsername, "txtUsername");
  HOOKUP_OBJECT (frmConnect, lblPassword, "lblPassword");
  HOOKUP_OBJECT (frmConnect, txtPassword, "txtPassword");
  HOOKUP_OBJECT (frmConnect, lblDomain, "lblDomain");
  HOOKUP_OBJECT (frmConnect, txtDomain, "txtDomain");
  HOOKUP_OBJECT (frmConnect, lblClientHostname, "lblClientHostname");
  HOOKUP_OBJECT (frmConnect, txtClientHostname, "txtClientHostname");
  HOOKUP_OBJECT (frmConnect, lblProtoFile, "lblProtoFile");
  HOOKUP_OBJECT (frmConnect, hbxProtoFile, "hbxProtoFile");
  HOOKUP_OBJECT (frmConnect, txtProtoFile, "txtProtoFile");
  HOOKUP_OBJECT (frmConnect, btnProtoFile, "btnProtoFile");
  HOOKUP_OBJECT (frmConnect, frameSecurity, "frameSecurity");
  HOOKUP_OBJECT (frmConnect, lblSecurityFrame, "lblSecurityFrame");
  HOOKUP_OBJECT (frmConnect, tblSecurity, "tblSecurity");
  HOOKUP_OBJECT (frmConnect, optTlsVersion, "optTlsVersion");
  HOOKUP_OBJECT (frmConnect, chkDisableEncryption, "chkDisableEncryption");
  HOOKUP_OBJECT (frmConnect, chkDisableClientEncryption, "chkDisableClientEncryption");

  // Profile Launcher Widgets
  HOOKUP_OBJECT (frmConnect, hbxProfileLauncher, "hbxProfileLauncher");
  HOOKUP_OBJECT (frmConnect, optProfileLauncher, "optProfileLauncher");
  // Profile File Ops Widgets
  HOOKUP_OBJECT (frmConnect, hbxFileOps, "hbxFileOps");
  HOOKUP_OBJECT (frmConnect, btnSaveAs, "btnSaveAs");
  HOOKUP_OBJECT (frmConnect, btnOpen, "btnOpen");

  // Display Tab Widgets
  HOOKUP_OBJECT (frmConnect, lblDisplayTab1, "lblDisplayTab1");
  HOOKUP_OBJECT (frmConnect, vbxDisplayTab1, "vbxDisplayTab1");

  // Size Frame Widgets
  HOOKUP_OBJECT (frmConnect, frameSize, "frameSize");
  HOOKUP_OBJECT (frmConnect, lblSizeFrame, "lblSizeFrame");
  HOOKUP_OBJECT (frmConnect, hbxSize, "hbxSize");
  HOOKUP_OBJECT (frmConnect, imgSize, "imgSize");
  HOOKUP_OBJECT (frmConnect, vbxSize, "vbxSize");
  HOOKUP_OBJECT (frmConnect, optSize1, "optSize1");
  HOOKUP_OBJECT (frmConnect, optSize2, "optSize2");
  HOOKUP_OBJECT (frmConnect, alnSize, "alnSize");
  HOOKUP_OBJECT (frmConnect, optSize, "optSize");
  HOOKUP_OBJECT (frmConnect, optSize3, "optSize3");

  // Color Frame Widgets
  HOOKUP_OBJECT (frmConnect, frameColor, "frameColor");
  HOOKUP_OBJECT (frmConnect, lblColorFrame, "lblColorFrame");
  HOOKUP_OBJECT (frmConnect, hbxColor, "hbxColor");
  HOOKUP_OBJECT (frmConnect, imgColor, "imgColor");
  HOOKUP_OBJECT (frmConnect, vbxColor, "vbxColor");
  HOOKUP_OBJECT (frmConnect, optColor1, "optColor1");
  HOOKUP_OBJECT (frmConnect, optColor2, "optColor2");
  HOOKUP_OBJECT (frmConnect, alnColor, "alnColor");
  HOOKUP_OBJECT (frmConnect, optColor, "optColor");
  HOOKUP_OBJECT (frmConnect, chkForceBitmap, "chkForceBitmap");
  HOOKUP_OBJECT (frmConnect, chkBackingStore, "chkBackingStore");

  // Local Resources Tab Widgets
  HOOKUP_OBJECT (frmConnect, lblLocalTab1, "lblLocalTab1");
  HOOKUP_OBJECT (frmConnect, vbxLocalTab1, "vbxLocalTab1");

  // Sound Frame Widgets
  HOOKUP_OBJECT (frmConnect, frameSound, "frameSound");
  HOOKUP_OBJECT (frmConnect, lblSoundFrame, "lblSoundFrame");
  HOOKUP_OBJECT (frmConnect, tblSound, "tblSound");
  HOOKUP_OBJECT (frmConnect, imgSound, "imgSound");
  HOOKUP_OBJECT (frmConnect, vbxSound, "vbxSound");
  HOOKUP_OBJECT (frmConnect, optSound1, "optSound1");
  HOOKUP_OBJECT (frmConnect, optSound2, "optSound2");
  HOOKUP_OBJECT (frmConnect, optSound3, "optSound3");

  // Keyboard Frame Widgets
  HOOKUP_OBJECT (frmConnect, frameKeyboard, "frameKeyboard");
  HOOKUP_OBJECT (frmConnect, lblKeyboardFrame, "lblKeyboardFrame");
  HOOKUP_OBJECT (frmConnect, tblKeyboard, "tblKeyboard");
  HOOKUP_OBJECT (frmConnect, lblKeyboard, "lblKeyboard");
  HOOKUP_OBJECT (frmConnect, optKeyboard, "optKeyboard");
  HOOKUP_OBJECT (frmConnect, lblKeyboardLang, "lblKeyboardLang");
  HOOKUP_OBJECT (frmConnect, txtKeyboardLang, "txtKeyboardLang");
  HOOKUP_OBJECT (frmConnect, imgKeyboard, "imgKeyboard");
  HOOKUP_OBJECT (frmConnect, chkDisableCtrl, "chkDisableCtrl");
  HOOKUP_OBJECT (frmConnect, chkSyncNumlock, "chkSyncNumlock");
  HOOKUP_OBJECT (frmConnect, lblCodepage, "lblCodepage");
  HOOKUP_OBJECT (frmConnect, txtCodepage, "txtCodepage");


  // Program Tab Widgets
  HOOKUP_OBJECT (frmConnect, lblProgramsTab1, "lblProgramsTab1");

  // Program Frame Widgets
  HOOKUP_OBJECT (frmConnect, frameProgram, "frameProgram");
  HOOKUP_OBJECT (frmConnect, lblProgramFrame, "lblProgramFrame");
  HOOKUP_OBJECT (frmConnect, tblProgram, "tblProgram");
  HOOKUP_OBJECT (frmConnect, chkStartProgram, "chkStartProgram");
  HOOKUP_OBJECT (frmConnect, imgProgram, "imgProgram");
  HOOKUP_OBJECT (frmConnect, lblProgramPath, "lblProgramPath");
  HOOKUP_OBJECT (frmConnect, txtProgramPath, "txtProgramPath");
  HOOKUP_OBJECT (frmConnect, lblStartFolder, "lblStartFolder");
  HOOKUP_OBJECT (frmConnect, txtStartFolder, "txtStartFolder");

  // Performance Tab Widgets
  HOOKUP_OBJECT (frmConnect, lblPerformanceTab1, "lblPerformanceTab1");

  // Performance Frame Widgets
  HOOKUP_OBJECT (frmConnect, framePerform, "framePerform");
  HOOKUP_OBJECT (frmConnect, lblPerformFrame, "lblPerformFrame");
  HOOKUP_OBJECT (frmConnect, tblPerform, "tblPerform");
  HOOKUP_OBJECT (frmConnect, imgPerform, "imgPerform");
  HOOKUP_OBJECT (frmConnect, lblPerformanceOptions, "lblPerformanceOptions");
  HOOKUP_OBJECT (frmConnect, vbxExpChecks, "vbxExpChecks");
  HOOKUP_OBJECT (frmConnect, chkDesktopBackground, "chkDesktopBackground");
  HOOKUP_OBJECT (frmConnect, chkWindowContent, "chkWindowContent");
  HOOKUP_OBJECT (frmConnect, chkAnimation, "chkAnimation");
  HOOKUP_OBJECT (frmConnect, chkThemes, "chkThemes");
  HOOKUP_OBJECT (frmConnect, chkBitmapCache, "chkBitmapCache");
  HOOKUP_OBJECT (frmConnect, chkNoMotionEvents, "chkNoMotionEvents");
  HOOKUP_OBJECT (frmConnect, chkEnableWMKeys, "chkEnableWMKeys");
  HOOKUP_OBJECT (frmConnect, chkHideWMDecorations, "chkHideWMDecorations");
  HOOKUP_OBJECT (frmConnect, chkAttachToConsole, "chkAttachToConsole");

  // Complete Button Box Widgets
  HOOKUP_OBJECT (frmConnect, hbbAppOps, "hbbAppOps");
  HOOKUP_OBJECT (frmConnect, btnConnect, "btnConnect");
  HOOKUP_OBJECT (frmConnect, alnConnect, "alnConnect");
  HOOKUP_OBJECT (frmConnect, hbxConnect, "hbxConnect");
  HOOKUP_OBJECT (frmConnect, imgConnect, "imgConnect");
  HOOKUP_OBJECT (frmConnect, lblConnect, "lblConnect");
  HOOKUP_OBJECT (frmConnect, btnQuit, "btnQuit");
  HOOKUP_OBJECT (frmConnect, btnHelp, "btnHelp");

  // disable the keyboard opts until they are supported
  gtk_widget_set_sensitive (optKeyboard, FALSE);

  // toggle the start program thingy to false.  will be set after this by loading.
  tsc_toggle_button_set_active ((GtkToggleButton*) chkStartProgram, FALSE);
  gtk_widget_set_sensitive (txtProgramPath, FALSE);
  gtk_widget_set_sensitive (txtStartFolder, FALSE);

  gtk_widget_set_sensitive (alnColor, FALSE);
  tsc_set_protocol_widgets (frmConnect, 0);

  gtk_widget_set_visible (frmConnect, TRUE);

  // load mru & profile launcher
  mru_to_screen (frmConnect);
  rdp_load_profile_launcher (frmConnect);

  gtk_widget_set_visible (chkDesktopBackground, FALSE);
  gtk_widget_set_visible (chkWindowContent, FALSE);
  gtk_widget_set_visible (chkAnimation, FALSE);
  gtk_widget_set_visible (chkThemes, FALSE);

  gConnect = frmConnect;
  tsc_update_size_controls ();
  tsc_update_color_controls ();

  //return frmConnect;
  return 0;
}


/***************************************
*                                      *
*   Event Handlers                     *
*                                      *
***************************************/


void
on_btnConnect_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget *main_window;
  rdp_file *rdp = NULL;
  gchar *home = tsc_home_path ();
  gchar *filename = g_build_path ("/", home, "last.tsc", NULL);

  #ifdef TSCLIENT_DEBUG
  printf ("on_btnConnect_clicked\n");
  #endif

  main_window = lookup_widget((GtkWidget*)button, "frmConnect");

  rdp = g_new0 (rdp_file, 1);
  rdp_file_init (rdp);
  rdp_file_get_screen (rdp, main_window);
  // save the damn thing as last
  rdp_file_save (rdp, filename);

  gtk_widget_set_visible (main_window, FALSE);
  gtk_window_destroy (GTK_WINDOW (main_window));

  gchar** error = g_malloc(sizeof(gchar*));

  if (tsc_launch_remote (rdp, 0, error) == 0) {
    mru_add_server (rdp->full_address);
  } else {
    tsc_connect_error (rdp,*error);
  }
  if (*error)
    g_free (*error);
  g_free (error);

  // recreate the main window
  create_frmConnect ();
  rdp_file_set_screen (rdp, gConnect);
  g_free (rdp);

  g_free (filename);
  g_free (home);
}


void
on_btnQuit_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget *main_window;
  rdp_file *rdp_last = NULL;
  gchar *home = tsc_home_path ();
  gchar *filename = g_build_path ("/", home, "last.tsc", NULL);

  #ifdef TSCLIENT_DEBUG
  printf ("on_btnQuit_clicked\n");
  #endif

  main_window = gConnect;
  gtk_widget_set_visible (main_window, FALSE);

  // save the damn thing
  rdp_last = g_new0 (rdp_file, 1);
  rdp_file_init (rdp_last);
  rdp_file_get_screen (rdp_last, main_window);
  rdp_file_save (rdp_last, filename);
  g_free (rdp_last);

  g_free (filename);
  g_free (home);

  if (tsc_app)
    g_application_quit (G_APPLICATION (tsc_app));
}


void
on_btnHelp_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
  tsc_about_dialog ();
}


void
on_optSize1_clicked                    (GtkToggleButton *button,
                                        gpointer         user_data)
{
  if (!tsc_toggle_button_get_active (button))
    return;

  tsc_update_size_controls ();
}


void
on_optSize2_clicked                    (GtkToggleButton *button,
                                        gpointer         user_data)
{
  if (!tsc_toggle_button_get_active (button))
    return;

  tsc_update_size_controls ();
}


void
on_optSize3_clicked                    (GtkToggleButton *button,
                                        gpointer         user_data)
{
  if (!tsc_toggle_button_get_active (button))
    return;

  tsc_update_size_controls ();
}


void
on_optColor1_clicked                   (GtkToggleButton *button,
                                        gpointer         user_data)
{
  if (!tsc_toggle_button_get_active (button))
    return;

  tsc_update_color_controls ();
}


void
on_optColor2_clicked                   (GtkToggleButton *button,
                                        gpointer         user_data)
{
  if (!tsc_toggle_button_get_active (button))
    return;

  tsc_update_color_controls ();
}

static void
on_recent_server_selected (GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
  GtkWidget *entry = GTK_WIDGET (user_data);
  (void) pspec;

  if (!entry)
    return;

  guint selected = gtk_drop_down_get_selected (dropdown);
  if (selected == 0)
    return;

  const gchar *server = tsc_dropdown_get_string (GTK_WIDGET (dropdown), selected);
  if (server)
    gtk_editable_set_text (GTK_EDITABLE (entry), server);

  gtk_drop_down_set_selected (dropdown, 0);
}

void
on_btnSaveAs_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget *main_window;
  rdp_file *rdp = NULL;
  GtkFileDialog *dialog;
  GFile *folder;
  GFile *file;
  gchar *home;

  main_window = lookup_widget((GtkWidget *)button, "frmConnect");

  dialog = tsc_create_rdp_dialog (_("Input a filename to save as..."), TRUE);
  gtk_file_dialog_set_accept_label (dialog, _("_Save"));
  home = tsc_home_path ();
  folder = g_file_new_for_path (home);
  gtk_file_dialog_set_initial_folder (dialog, folder);
  g_object_unref (folder);

  file = tsc_file_dialog_run_save (GTK_WINDOW (main_window), dialog);
  if (file) {
    gchar *filename = g_file_get_path (file);
    // Add our extension if needed
    char *lastdelim = g_strrstr (filename, "/");
    if (lastdelim && g_strrstr (lastdelim + 1, ".") == NULL) {
      char *tmp = g_strconcat (filename, ".rdp", NULL);
      g_free (filename);
      filename = tmp;
    }
    #ifdef TSCLIENT_DEBUG
    printf ("filename: %s\n", filename);
    #endif
    // save rdp file
    rdp = g_new0 (rdp_file, 1);
    rdp_file_init (rdp);
    rdp_file_get_screen (rdp, main_window);
    rdp_file_save (rdp, filename);
    g_free (rdp);
    // Add to quick pick list
    lastdelim = g_strrstr (filename, home);
    if (lastdelim) {
      GtkWidget *opt = lookup_widget (main_window, "optProfileLauncher");
      lastdelim += strlen (home) + 1;
      tsc_dropdown_append (opt, lastdelim);
      gtk_widget_set_sensitive (opt, TRUE);
    }
    // Try to avoid *some* memory leaks
    g_free (filename);
    g_object_unref (file);
  }
  g_object_unref (dialog);
  g_free (home);
  return;
}


void
on_btnOpen_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget *main_window;
  rdp_file *rdp = NULL;
  GtkFileDialog *dialog;
  GFile *folder;
  GFile *file;
  gchar *home;

  main_window = lookup_widget((GtkWidget *)button, "frmConnect");

  dialog = tsc_create_rdp_dialog (_("Choose a RDP file to open..."), FALSE);
  gtk_file_dialog_set_accept_label (dialog, _("_Open"));
  home = tsc_home_path ();
  folder = g_file_new_for_path (home);
  gtk_file_dialog_set_initial_folder (dialog, folder);
  g_object_unref (folder);

  file = tsc_file_dialog_run_open (GTK_WINDOW (main_window), dialog);
  if (file) {
    gchar *filename = g_file_get_path (file);
    #ifdef TSCLIENT_DEBUG
    printf ("filename: %s\n", filename);
    #endif
    // check if the file exists
    if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
      // load if exists
      rdp = g_new0 (rdp_file, 1);
      rdp_file_init (rdp);
      rdp_file_load (rdp, filename);
      rdp_file_set_screen (rdp, main_window);
      g_free (rdp);
    }
    g_free (filename);
    g_object_unref (file);
  }
  g_object_unref (dialog);
  g_free (home);
  return;
}


void
on_btnProtoFile_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget *main_window, *proto_file;
  GtkFileDialog *dialog;
  GFile *folder;
  GFile *file;
  gchar *home = tsc_home_path ();

  main_window = lookup_widget((GtkWidget*)button, "frmConnect");

  dialog = tsc_create_rdp_dialog (_("Choose an RDP file to open..."), FALSE);
  gtk_file_dialog_set_accept_label (dialog, _("_Open"));
  folder = g_file_new_for_path (home);
  gtk_file_dialog_set_initial_folder (dialog, folder);
  g_object_unref (folder);

  file = tsc_file_dialog_run_open (GTK_WINDOW (main_window), dialog);
  if (file) {
    gchar *filename = g_file_get_path (file);
    if (filename) {
      #ifdef TSCLIENT_DEBUG
      printf ("filename: %s\n", filename);
      #endif
      proto_file = lookup_widget((GtkWidget*)main_window, "txtProtoFile");
      gtk_editable_set_text (GTK_EDITABLE (proto_file), filename);
      g_free (filename);
    }
    g_object_unref (file);
  }
  g_object_unref (dialog);
  g_free (home);
  return;
}


void
on_chkStartProgram_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  GtkWidget *txtProgramPath, *txtStartFolder;
  txtProgramPath = lookup_widget (GTK_WIDGET (gConnect), "txtProgramPath");
  txtStartFolder = lookup_widget (GTK_WIDGET (gConnect), "txtStartFolder");

  if (tsc_toggle_button_get_active (togglebutton)) {
    gtk_widget_set_sensitive (txtProgramPath, TRUE);
    gtk_widget_set_sensitive (txtStartFolder, TRUE);
  } else {
    gtk_widget_set_sensitive (txtProgramPath, FALSE);
    gtk_widget_set_sensitive (txtStartFolder, FALSE);
  }
}


void
on_protocol_changed (GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
  (void) pspec;
  GtkWidget *main_win = user_data ? GTK_WIDGET (user_data) : gConnect;
  guint idx = gtk_drop_down_get_selected (dropdown);
  gint protocol = 0;

  switch (idx) {
  case 1:
    protocol = 4;
    break;
  case 2:
    protocol = 1;
    break;
  case 3:
    protocol = 2;
    break;
  case 4:
    protocol = 3;
    break;
  default:
    protocol = 0;
    break;
  }

  tsc_set_protocol_widgets (main_win, protocol);
}
