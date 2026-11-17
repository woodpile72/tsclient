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
#include "connection.h"

static void on_btnConnect_clicked (GtkButton *button, gpointer user_data);
static void on_btnQuit_clicked (GtkButton *button, gpointer user_data);
static void on_btnHelp_clicked (GtkButton *button, gpointer user_data);
static void on_size_changed (GtkCheckButton *button, gpointer user_data);
static void on_color_changed (GtkCheckButton *button, gpointer user_data);
static void on_btnSaveAs_clicked (GtkButton *button, gpointer user_data);
static void on_btnOpen_clicked (GtkButton *button, gpointer user_data);
static void on_btnProtoFile_clicked (GtkButton *button, gpointer user_data);
static void on_chkStartProgram_toggled (GtkCheckButton *togglebutton, gpointer user_data);
static void on_protocol_changed (GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data);

static void
tsc_widget_pad (GtkWidget *widget, gint horizontal, gint vertical)
{
  gtk_widget_set_margin_start (widget, horizontal);
  gtk_widget_set_margin_end (widget, horizontal);
  gtk_widget_set_margin_top (widget, vertical);
  gtk_widget_set_margin_bottom (widget, vertical);
}

static void on_recent_server_selected (GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data);
static GtkFileDialog *tsc_create_rdp_dialog (const gchar *title, gboolean include_all_filter);
static GtkWidget *tsc_create_banner (void);
static GdkTexture *tsc_load_icon_texture (const gchar *const *candidates, gint size);
static void tsc_ensure_form_css (void);
static void tsc_align_panel_icon (GtkWidget *widget);
static void tsc_update_size_controls (GtkWidget *window);
static void tsc_update_color_controls (GtkWidget *window);

GtkWidget *gConnect = NULL;

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
  gtk_css_provider_load_from_string (form_provider,
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
    "}");

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
tsc_update_size_controls (GtkWidget *window)
{
  if (!window)
    return;

  GtkWidget *aln_size = lookup_widget (window, "alnSize");
  GtkWidget *opt_custom = lookup_widget (window, "optSize2");
  GtkWidget *opt_full = lookup_widget (window, "optSize3");
  gboolean use_custom = FALSE;
  gboolean fullscreen = FALSE;

  if (opt_custom)
    use_custom = gtk_check_button_get_active (GTK_CHECK_BUTTON (opt_custom));
  if (opt_full)
    fullscreen = gtk_check_button_get_active (GTK_CHECK_BUTTON (opt_full));

  if (aln_size)
    gtk_widget_set_sensitive (aln_size, use_custom && !fullscreen);
}

static void
tsc_update_color_controls (GtkWidget *window)
{
  if (!window)
    return;

  GtkWidget *aln_color = lookup_widget (window, "alnColor");
  GtkWidget *opt_color_specific = lookup_widget (window, "optColor2");
  gboolean use_specific = FALSE;

  if (opt_color_specific)
    use_specific = gtk_check_button_get_active (GTK_CHECK_BUTTON (opt_color_specific));

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
  gtk_css_provider_load_from_string (css,
                                   "#tsc-banner {"
                                   "  background: #5A6EBE;"
                                   "  min-height: 68px;"
                                   "  padding: 20px;"
                                   "}"
                                   ".tsc-banner-text {"
                                   "  color: #FFFFFF;"
                                   "  font-weight: 700;"
                                   "}");
  gtk_style_context_add_provider_for_display (gtk_widget_get_display (overlay),
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

static GtkFileDialog *
tsc_create_rdp_dialog (const gchar *title, gboolean save)
{
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  GtkFileFilter *filter = gtk_file_filter_new ();
  gtk_file_dialog_set_title (dialog, title);
  gtk_file_filter_set_name (filter, save ? _("TSClient profiles") : _("Remote desktop profiles"));
  gtk_file_filter_add_pattern (filter, "*.tsc");
  if (!save) gtk_file_filter_add_pattern (filter, "*.rdp");
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  gtk_file_dialog_set_default_filter (dialog, filter);
  if (save) gtk_file_dialog_set_initial_name (dialog, "connection.tsc");
  g_object_unref (filter);
  g_object_unref (filters);
  return dialog;
}

/***************************************
*                                      *
*   frmConnect Create                  *
*                                      *
***************************************/

static GtkWidget *
tsc_section_new (const gchar *title)
{
  GtkWidget *frame = gtk_frame_new (NULL);
  GtkWidget *label = gtk_label_new (title);
  PangoAttrList *attributes = pango_attr_list_new ();
  pango_attr_list_insert (attributes, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attributes);
  pango_attr_list_unref (attributes);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  tsc_widget_pad (frame, 3, 3);
  return frame;
}

static GtkWidget *
tsc_field_label (GtkWidget *grid, const gchar *text, gint row)
{
  GtkWidget *label = gtk_label_new_with_mnemonic (text);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  tsc_widget_pad (label, 6, 6);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  return label;
}

static void
tsc_build_general_tab (GtkWidget *frmConnect, GtkWidget *nbkComplete)
{
  const gchar *tls_version_labels[] = {
    _("Negotiate TLS (default)"),
    _("TLS 1.0"),
    _("TLS 1.1"),
    _("TLS 1.2"),
    NULL
  };
  GtkWidget *vbxGeneralTab1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK (nbkComplete), vbxGeneralTab1, gtk_label_new_with_mnemonic (_("_General")));

  GtkWidget *frameLogon = tsc_section_new (_("Logon Settings"));
  gtk_box_append (GTK_BOX (vbxGeneralTab1), frameLogon);
  gtk_widget_set_vexpand (frameLogon, TRUE);

  GtkWidget *tblLogon0 = gtk_grid_new ();
  gtk_frame_set_child (GTK_FRAME (frameLogon), tblLogon0);

  GtkWidget *lblLogonNote = gtk_label_new (_("Type the name of the computer or choose a computer from the drop-down list."));
  gtk_grid_attach (GTK_GRID (tblLogon0), lblLogonNote, 1, 0, 1, 1);
  gtk_widget_set_hexpand (lblLogonNote, TRUE);
  gtk_label_set_justify (GTK_LABEL (lblLogonNote), GTK_JUSTIFY_LEFT);
  gtk_label_set_wrap (GTK_LABEL (lblLogonNote), TRUE);
  gtk_widget_set_halign (lblLogonNote, GTK_ALIGN_START);
  gtk_widget_set_valign (lblLogonNote, GTK_ALIGN_CENTER);

  GtkWidget *imgGeneralLogon = create_pixmap (frmConnect, "icon_laptop.png");
  gtk_grid_attach (GTK_GRID (tblLogon0), imgGeneralLogon, 0, 0, 1, 1);
  tsc_widget_pad (imgGeneralLogon, 3, 3);
  tsc_align_panel_icon (imgGeneralLogon);

  GtkWidget *tblLogon1 = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (tblLogon0), tblLogon1, 1, 1, 1, 1);
  gtk_grid_set_row_spacing (GTK_GRID (tblLogon1), 4);
  gtk_grid_set_column_spacing (GTK_GRID (tblLogon1), 4);

  GtkWidget *lblComputer = tsc_field_label (tblLogon1, _("Compu_ter:"), 0);

  GtkWidget *lblProtocol = tsc_field_label (tblLogon1, _("Pro_tocol:"), 1);

  GtkWidget *lblUsername = tsc_field_label (tblLogon1, _("_User Name:"), 2);

  GtkWidget *lblPassword = tsc_field_label (tblLogon1, _("Pass_word:"), 3);

  GtkWidget *lblDomain = tsc_field_label (tblLogon1, _("Do_main:"), 4);

  GtkWidget *lblClientHostname = tsc_field_label (tblLogon1, _("C_lient Hostname:"), 5);

  GtkWidget *lblProtoFile = tsc_field_label (tblLogon1, _("Prot_ocol File:"), 6);

  GtkWidget *hbxComputer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_grid_attach (GTK_GRID (tblLogon1), hbxComputer, 1, 0, 1, 1);
  gtk_widget_set_hexpand (hbxComputer, TRUE);

  GtkWidget *txtComputer = gtk_entry_new ();
  g_object_set_data (G_OBJECT (frmConnect), "txtComputer", txtComputer);
  gtk_box_append (GTK_BOX (hbxComputer), txtComputer);
  gtk_widget_set_hexpand (txtComputer, TRUE);
  gtk_entry_set_activates_default (GTK_ENTRY (txtComputer), TRUE);
  gtk_widget_set_tooltip_text (txtComputer, _("Enter the name or address of the remote system."));

  GtkWidget *cboComputer = tsc_dropdown_new ((const gchar * const []) { _("Recent"), NULL });
  g_object_set_data (G_OBJECT (frmConnect), "cboComputer", cboComputer);
  gtk_box_append (GTK_BOX (hbxComputer), cboComputer);
  gtk_widget_set_vexpand (cboComputer, FALSE);
  gtk_widget_set_sensitive (cboComputer, FALSE);
  g_signal_connect (cboComputer, "notify::selected",
                    G_CALLBACK (on_recent_server_selected), txtComputer);

  const gchar *protocol_items[] = {
    _("RDPv4 (legacy)"),
    _("RDPv5 (rdesktop)"),
    _("VNC"),
    _("XDMCP"),
    _("ICA"),
    _("RDP (FreeRDP)"),
    NULL
  };
  GtkWidget *optProtocol = tsc_dropdown_new (protocol_items);
  g_object_set_data (G_OBJECT (frmConnect), "optProtocol", optProtocol);
  gtk_grid_attach (GTK_GRID (tblLogon1), optProtocol, 1, 1, 1, 1);
  gtk_widget_set_hexpand (optProtocol, TRUE);
  gtk_widget_set_tooltip_text (optProtocol, _("Select the protocol to use for this connection."));
  tsc_dropdown_set_selected (optProtocol, 1);
  g_signal_connect (optProtocol, "notify::selected",
                    G_CALLBACK (on_protocol_changed), frmConnect);

  GtkWidget *txtUsername = gtk_entry_new ();
  g_object_set_data (G_OBJECT (frmConnect), "txtUsername", txtUsername);
  gtk_entry_set_activates_default (GTK_ENTRY (txtUsername), TRUE);
  gtk_grid_attach (GTK_GRID (tblLogon1), txtUsername, 1, 2, 1, 1);
  gtk_widget_set_hexpand (txtUsername, TRUE);
  gtk_widget_set_tooltip_text (txtUsername, _("Enter the username for the remote system.\nFor VNC, enter the path to your saved vnc password file."));

  GtkWidget *password_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_grid_attach (GTK_GRID (tblLogon1), password_box, 1, 3, 1, 1);
  GtkWidget *txtPassword = gtk_entry_new ();
  g_object_set_data (G_OBJECT (frmConnect), "txtPassword", txtPassword);
  gtk_entry_set_activates_default (GTK_ENTRY (txtPassword), TRUE);
  gtk_entry_set_visibility (GTK_ENTRY (txtPassword), FALSE);
  gtk_box_append (GTK_BOX (password_box), txtPassword);
  GtkWidget *remember = gtk_check_button_new_with_mnemonic (_("_Remember password in keyring"));
  gtk_box_append (GTK_BOX (password_box), remember);
  gtk_widget_set_tooltip_text (remember, _("Store this password in your desktop keyring. Profile files never contain passwords."));
  g_object_set_data (G_OBJECT (frmConnect), "chkRememberPassword", remember);

  GtkWidget *txtDomain = gtk_entry_new ();
  g_object_set_data (G_OBJECT (frmConnect), "txtDomain", txtDomain);
  gtk_entry_set_activates_default (GTK_ENTRY (txtDomain), TRUE);
  gtk_grid_attach (GTK_GRID (tblLogon1), txtDomain, 1, 4, 1, 1);
  gtk_widget_set_hexpand (txtDomain, TRUE);
  gtk_widget_set_tooltip_text (txtDomain, _("Enter the domain for the remote system."));

  GtkWidget *txtClientHostname = gtk_entry_new ();
  g_object_set_data (G_OBJECT (frmConnect), "txtClientHostname", txtClientHostname);
  gtk_entry_set_activates_default (GTK_ENTRY (txtClientHostname), TRUE);
  gtk_grid_attach (GTK_GRID (tblLogon1), txtClientHostname, 1, 5, 1, 1);
  gtk_widget_set_hexpand (txtClientHostname, TRUE);
  gtk_widget_set_tooltip_text (txtClientHostname, _("Enter the local hostname for this system."));

  GtkWidget *hbxProtoFile = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_grid_attach (GTK_GRID (tblLogon1), hbxProtoFile, 1, 6, 1, 1);
  gtk_widget_set_hexpand (hbxProtoFile, TRUE);

  GtkWidget *txtProtoFile = gtk_entry_new ();
  g_object_set_data (G_OBJECT (frmConnect), "txtProtoFile", txtProtoFile);
  gtk_entry_set_activates_default (GTK_ENTRY (txtProtoFile), TRUE);
  gtk_box_append (GTK_BOX (hbxProtoFile), txtProtoFile);
  gtk_widget_set_hexpand (txtProtoFile, TRUE);
  gtk_widget_set_tooltip_text (txtProtoFile, _("Some protocols require a file containing settings. If required, enter the path to the file here."));

  GtkWidget *btnProtoFile = gtk_button_new_with_label ("...");
  g_object_set_data (G_OBJECT (frmConnect), "btnProtoFile", btnProtoFile);
  gtk_widget_add_css_class (btnProtoFile, "tsc-action");
  gtk_box_append (GTK_BOX (hbxProtoFile), btnProtoFile);

  GtkWidget *frameSecurity = tsc_section_new (_("Security"));
  gtk_box_append (GTK_BOX (vbxGeneralTab1), frameSecurity);

  GtkWidget *tblSecurity = gtk_grid_new ();
  gtk_frame_set_child (GTK_FRAME (frameSecurity), tblSecurity);
  gtk_grid_set_column_spacing (GTK_GRID (tblSecurity), 6);

  GtkWidget *imgSecurity = create_pixmap (frmConnect, "icon_security.svg");
  gtk_grid_attach (GTK_GRID (tblSecurity), imgSecurity, 0, 0, 1, 3);
  tsc_widget_pad (imgSecurity, 3, 3);
  tsc_align_panel_icon (imgSecurity);

  GtkWidget *optTlsVersion = tsc_dropdown_new (tls_version_labels);
  g_object_set_data (G_OBJECT (frmConnect), "optTlsVersion", optTlsVersion);
  gtk_grid_attach (GTK_GRID (tblSecurity), optTlsVersion, 1, 0, 1, 1);
  gtk_widget_set_hexpand (optTlsVersion, TRUE);
  gtk_widget_set_tooltip_text (optTlsVersion, _("Choose which TLS version to advertise to the server."));
  tsc_dropdown_set_selected (optTlsVersion, 0);

  GtkWidget *chkDisableEncryption = gtk_check_button_new_with_mnemonic (_("Disable all encryption (-e)"));
  g_object_set_data (G_OBJECT (frmConnect), "chkDisableEncryption", chkDisableEncryption);
  gtk_grid_attach (GTK_GRID (tblSecurity), chkDisableEncryption, 1, 1, 1, 1);

  GtkWidget *chkDisableClientEncryption = gtk_check_button_new_with_mnemonic (_("Disable client-to-server encryption (-E)"));
  g_object_set_data (G_OBJECT (frmConnect), "chkDisableClientEncryption", chkDisableClientEncryption);
  gtk_grid_attach (GTK_GRID (tblSecurity), chkDisableClientEncryption, 1, 2, 1, 1);

  gtk_label_set_mnemonic_widget (GTK_LABEL (lblComputer), txtComputer);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblProtocol), optProtocol);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblUsername), txtUsername);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblPassword), txtPassword);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblDomain), txtDomain);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblClientHostname), txtClientHostname);
  gtk_label_set_mnemonic_widget (GTK_LABEL (lblProtoFile), txtProtoFile);
  g_signal_connect (btnProtoFile, "clicked", G_CALLBACK (on_btnProtoFile_clicked), NULL);
}

static void
tsc_build_display_tab (GtkWidget *frmConnect, GtkWidget *nbkComplete)
{
  gsize preset_count;
  const gchar **size_items;
  const gchar *color_items[] = {
    _("256 Colors (8 bit)"),
    _("High Color (15 bit)"),
    _("High Color (16 bit)"),
    _("True Color (24 bit)"),
    _("True Color (32 bit)"),
	  NULL
  };
  GtkWidget *vbxDisplayTab1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK (nbkComplete), vbxDisplayTab1, gtk_label_new_with_mnemonic (_("_Display")));

  GtkWidget *frameSize = tsc_section_new (_("Remote Desktop Size"));
  gtk_box_append (GTK_BOX (vbxDisplayTab1), frameSize);
  gtk_widget_set_vexpand (frameSize, TRUE);

  GtkWidget *hbxSize = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_frame_set_child (GTK_FRAME (frameSize), hbxSize);

  GtkWidget *imgSize = create_pixmap (frmConnect, "icon_size.png");
  gtk_box_append (GTK_BOX (hbxSize), imgSize);
  gtk_widget_set_halign (imgSize, GTK_ALIGN_START);
  gtk_widget_set_valign (imgSize, GTK_ALIGN_START);
  tsc_widget_pad (imgSize, 3, 3);
  tsc_align_panel_icon (imgSize);

  GtkWidget *vbxSize = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (hbxSize), vbxSize);
  gtk_widget_set_hexpand (vbxSize, TRUE);

  GtkWidget *optSize1 = gtk_check_button_new_with_mnemonic (_("Use default screen size"));
  g_object_set_data (G_OBJECT (frmConnect), "optSize1", optSize1);
  gtk_box_append (GTK_BOX (vbxSize), optSize1);
  gtk_widget_set_hexpand (optSize1, FALSE);
  tsc_widget_pad (optSize1, 3, 3);
  gtk_widget_set_tooltip_text (optSize1, _("Use the default screen size."));
  gtk_check_button_set_active (GTK_CHECK_BUTTON (optSize1), TRUE);

  GtkWidget *optSize2 = gtk_check_button_new_with_mnemonic (_("Use specified screen size"));
  g_object_set_data (G_OBJECT (frmConnect), "optSize2", optSize2);
  gtk_box_append (GTK_BOX (vbxSize), optSize2);
  gtk_widget_set_hexpand (optSize2, FALSE);
  tsc_widget_pad (optSize2, 3, 3);
  gtk_widget_set_tooltip_text (optSize2, _("Use the list to choose the screen size to use."));
  gtk_check_button_set_group (GTK_CHECK_BUTTON (optSize2), GTK_CHECK_BUTTON (optSize1));

  GtkWidget *alnSize = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  g_object_set_data (G_OBJECT (frmConnect), "alnSize", alnSize);
  gtk_widget_set_halign (alnSize, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (alnSize, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbxSize), alnSize);

  preset_count = TSC_SCREEN_PRESET_COUNT;
  size_items = g_new0 (const gchar *, preset_count + 1);
  for (gsize idx = 0; idx < preset_count; idx++) {
    size_items[idx] = _(tsc_screen_presets[idx].label);
  }
  GtkWidget *optSize = tsc_dropdown_new (size_items);
  g_object_set_data (G_OBJECT (frmConnect), "optSize", optSize);
  g_free ((gpointer) size_items);
  gtk_box_append (GTK_BOX (alnSize), optSize);

  GtkWidget *optSize3 = gtk_check_button_new_with_mnemonic (_("Operate in full screen mode"));
  g_object_set_data (G_OBJECT (frmConnect), "optSize3", optSize3);
  gtk_box_append (GTK_BOX (vbxSize), optSize3);
  gtk_widget_set_hexpand (optSize3, FALSE);
  tsc_widget_pad (optSize3, 3, 3);
  gtk_widget_set_tooltip_text (optSize3, _("Work in full screen mode."));
  gtk_check_button_set_group (GTK_CHECK_BUTTON (optSize3), GTK_CHECK_BUTTON (optSize1));

  GtkWidget *frameColor = tsc_section_new (_("Colors"));
  gtk_box_append (GTK_BOX (vbxDisplayTab1), frameColor);
  gtk_widget_set_vexpand (frameColor, TRUE);

  GtkWidget *hbxColor = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_frame_set_child (GTK_FRAME (frameColor), hbxColor);

  GtkWidget *imgColor = create_pixmap (frmConnect, "icon_colors.png");
  gtk_box_append (GTK_BOX (hbxColor), imgColor);
  gtk_widget_set_halign (imgColor, GTK_ALIGN_START);
  gtk_widget_set_valign (imgColor, GTK_ALIGN_START);
  tsc_widget_pad (imgColor, 3, 3);
  tsc_align_panel_icon (imgColor);

  GtkWidget *vbxColor = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (hbxColor), vbxColor);
  gtk_widget_set_hexpand (vbxColor, TRUE);

  GtkWidget *optColor1 = gtk_check_button_new_with_mnemonic (_("Use default color depth"));
  g_object_set_data (G_OBJECT (frmConnect), "optColor1", optColor1);
  gtk_box_append (GTK_BOX (vbxColor), optColor1);
  gtk_widget_set_hexpand (optColor1, FALSE);
  tsc_widget_pad (optColor1, 3, 3);
  gtk_widget_set_tooltip_text (optColor1, _("Use the default color depth."));
  gtk_check_button_set_active (GTK_CHECK_BUTTON (optColor1), TRUE);

  GtkWidget *optColor2 = gtk_check_button_new_with_mnemonic (_("Use specified color depth"));
  g_object_set_data (G_OBJECT (frmConnect), "optColor2", optColor2);
  gtk_box_append (GTK_BOX (vbxColor), optColor2);
  gtk_widget_set_hexpand (optColor2, FALSE);
  tsc_widget_pad (optColor2, 3, 3);
  gtk_widget_set_tooltip_text (optColor2, _("Use the list to choose the color depth to use."));
  gtk_check_button_set_group (GTK_CHECK_BUTTON (optColor2), GTK_CHECK_BUTTON (optColor1));

  GtkWidget *alnColor = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  g_object_set_data (G_OBJECT (frmConnect), "alnColor", alnColor);
  gtk_widget_set_halign (alnColor, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (alnColor, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbxColor), alnColor);

  GtkWidget *optColor = tsc_dropdown_new (color_items);
  g_object_set_data (G_OBJECT (frmConnect), "optColor", optColor);
  gtk_box_append (GTK_BOX (alnColor), optColor);
  gtk_widget_set_tooltip_text (optColor, _("Pick a color depth to request from the server."));

  GtkWidget *chkForceBitmap = gtk_check_button_new_with_mnemonic (_("Force bitmap updates (-b)"));
  g_object_set_data (G_OBJECT (frmConnect), "chkForceBitmap", chkForceBitmap);
  gtk_box_append (GTK_BOX (vbxColor), chkForceBitmap);
  gtk_widget_set_hexpand (chkForceBitmap, FALSE);

  GtkWidget *chkBackingStore = gtk_check_button_new_with_mnemonic (_("Use X server backing store (-B)"));
  g_object_set_data (G_OBJECT (frmConnect), "chkBackingStore", chkBackingStore);
  gtk_box_append (GTK_BOX (vbxColor), chkBackingStore);
  gtk_widget_set_hexpand (chkBackingStore, FALSE);
  g_signal_connect (optSize1, "toggled", G_CALLBACK (on_size_changed), frmConnect);
  g_signal_connect (optSize2, "toggled", G_CALLBACK (on_size_changed), frmConnect);
  g_signal_connect (optSize3, "toggled", G_CALLBACK (on_size_changed), frmConnect);
  g_signal_connect (optColor1, "toggled", G_CALLBACK (on_color_changed), frmConnect);
  g_signal_connect (optColor2, "toggled", G_CALLBACK (on_color_changed), frmConnect);
}

static void
tsc_build_resources_tab (GtkWidget *frmConnect, GtkWidget *nbkComplete)
{
  GtkWidget *vbxLocalTab1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK (nbkComplete), vbxLocalTab1, gtk_label_new_with_mnemonic (_("Local _Resources")));

  GtkWidget *frameSound = tsc_section_new (_("Remote Computer Sound"));
  gtk_box_append (GTK_BOX (vbxLocalTab1), frameSound);
  gtk_widget_set_vexpand (frameSound, TRUE);

  GtkWidget *tblSound = gtk_grid_new ();
  gtk_frame_set_child (GTK_FRAME (frameSound), tblSound);
  gtk_grid_set_column_spacing (GTK_GRID (tblSound), 6);

  GtkWidget *imgSound = create_pixmap (frmConnect, "icon_sound.png");
  tsc_widget_pad (imgSound, 3, 3);
  gtk_grid_attach (GTK_GRID (tblSound), imgSound, 0, 0, 1, 1);
  tsc_align_panel_icon (imgSound);

  GtkWidget *vbxSound = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  g_object_set_data (G_OBJECT (frmConnect), "vbxSound", vbxSound);
  gtk_grid_attach (GTK_GRID (tblSound), vbxSound, 1, 0, 1, 2);
  gtk_widget_set_hexpand (vbxSound, TRUE);

  GtkWidget *optSound1 = gtk_check_button_new_with_mnemonic (_("On the local computer"));
  g_object_set_data (G_OBJECT (frmConnect), "optSound1", optSound1);
  gtk_box_append (GTK_BOX (vbxSound), optSound1);
  gtk_widget_set_hexpand (optSound1, FALSE);
  tsc_widget_pad (optSound1, 3, 3);
  gtk_widget_set_tooltip_text (optSound1, _("On the local computer"));
  gtk_check_button_set_active (GTK_CHECK_BUTTON (optSound1), TRUE);

  GtkWidget *optSound2 = gtk_check_button_new_with_mnemonic (_("On the remote computer"));
  g_object_set_data (G_OBJECT (frmConnect), "optSound2", optSound2);
  gtk_box_append (GTK_BOX (vbxSound), optSound2);
  gtk_widget_set_hexpand (optSound2, FALSE);
  tsc_widget_pad (optSound2, 3, 3);
  gtk_widget_set_tooltip_text (optSound2, _("On the remote computer"));
  gtk_check_button_set_group (GTK_CHECK_BUTTON (optSound2), GTK_CHECK_BUTTON (optSound1));

  GtkWidget *optSound3 = gtk_check_button_new_with_mnemonic (_("Do not play"));
  g_object_set_data (G_OBJECT (frmConnect), "optSound3", optSound3);
  gtk_box_append (GTK_BOX (vbxSound), optSound3);
  gtk_widget_set_hexpand (optSound3, FALSE);
  tsc_widget_pad (optSound3, 3, 3);
  gtk_widget_set_tooltip_text (optSound3, _("Do not play"));
  gtk_check_button_set_group (GTK_CHECK_BUTTON (optSound3), GTK_CHECK_BUTTON (optSound1));

  GtkWidget *frameKeyboard = tsc_section_new (_("Keyboard"));
  gtk_box_append (GTK_BOX (vbxLocalTab1), frameKeyboard);
  gtk_widget_set_vexpand (frameKeyboard, TRUE);

  GtkWidget *tblKeyboard = gtk_grid_new ();
  gtk_frame_set_child (GTK_FRAME (frameKeyboard), tblKeyboard);
  gtk_grid_set_column_spacing (GTK_GRID (tblKeyboard), 6);

  const gchar *keyboard_items[] = {
    _("On the local computer"),
    _("On the remote computer"),
    _("In full screen mode only"),
    NULL
  };
  GtkWidget *optKeyboard = tsc_dropdown_new (keyboard_items);
  g_object_set_data (G_OBJECT (frmConnect), "optKeyboard", optKeyboard);
  gtk_grid_attach (GTK_GRID (tblKeyboard), optKeyboard, 1, 1, 1, 1);
  gtk_widget_set_hexpand (optKeyboard, TRUE);

  GtkWidget *lblKeyboardLang = gtk_label_new (_("Use the following keyboard language\n(2 char keycode)"));
  gtk_grid_attach (GTK_GRID (tblKeyboard), lblKeyboardLang, 1, 2, 1, 1);
  gtk_label_set_justify (GTK_LABEL (lblKeyboardLang), GTK_JUSTIFY_LEFT);
  gtk_widget_set_halign (lblKeyboardLang, GTK_ALIGN_START);
  gtk_widget_set_valign (lblKeyboardLang, GTK_ALIGN_CENTER);

  GtkWidget *txtKeyboardLang = gtk_entry_new ();
  g_object_set_data (G_OBJECT (frmConnect), "txtKeyboardLang", txtKeyboardLang);
  gtk_grid_attach (GTK_GRID (tblKeyboard), txtKeyboardLang, 1, 3, 1, 1);
  gtk_widget_set_hexpand (txtKeyboardLang, TRUE);

  GtkWidget *chkDisableCtrl = gtk_check_button_new_with_mnemonic (_("Disable remote Ctrl key combinations (-t)"));
  g_object_set_data (G_OBJECT (frmConnect), "chkDisableCtrl", chkDisableCtrl);
  gtk_grid_attach (GTK_GRID (tblKeyboard), chkDisableCtrl, 1, 4, 1, 1);

  GtkWidget *chkSyncNumlock = gtk_check_button_new_with_mnemonic (_("Synchronize Num Lock state (-N)"));
  g_object_set_data (G_OBJECT (frmConnect), "chkSyncNumlock", chkSyncNumlock);
  gtk_grid_attach (GTK_GRID (tblKeyboard), chkSyncNumlock, 1, 5, 1, 1);

  GtkWidget *lblCodepage = gtk_label_new (_("Local codepage (for -L)"));
  gtk_grid_attach (GTK_GRID (tblKeyboard), lblCodepage, 1, 6, 1, 1);
  gtk_label_set_justify (GTK_LABEL (lblCodepage), GTK_JUSTIFY_LEFT);
  gtk_widget_set_halign (lblCodepage, GTK_ALIGN_START);
  gtk_widget_set_valign (lblCodepage, GTK_ALIGN_CENTER);

  GtkWidget *txtCodepage = gtk_entry_new ();
  g_object_set_data (G_OBJECT (frmConnect), "txtCodepage", txtCodepage);
  gtk_grid_attach (GTK_GRID (tblKeyboard), txtCodepage, 1, 7, 1, 1);
  gtk_widget_set_hexpand (txtCodepage, TRUE);

  GtkWidget *lblKeyboard = gtk_label_new (_("Apply Windows key combinations\n(for example ALT+TAB) (unsupported)"));
  gtk_grid_attach (GTK_GRID (tblKeyboard), lblKeyboard, 1, 0, 1, 1);
  gtk_label_set_justify (GTK_LABEL (lblKeyboard), GTK_JUSTIFY_LEFT);
  gtk_widget_set_halign (lblKeyboard, GTK_ALIGN_START);
  gtk_widget_set_valign (lblKeyboard, GTK_ALIGN_CENTER);

  GtkWidget *imgKeyboard = create_pixmap (frmConnect, "icon_keyboard.png");
  gtk_grid_attach (GTK_GRID (tblKeyboard), imgKeyboard, 0, 0, 1, 1);
  tsc_widget_pad (imgKeyboard, 3, 3);
  tsc_align_panel_icon (imgKeyboard);
}

static void
tsc_build_program_tab (GtkWidget *frmConnect, GtkWidget *nbkComplete)
{
  GtkWidget *frameProgram = tsc_section_new (_("Start a Program"));
  gtk_notebook_append_page (GTK_NOTEBOOK (nbkComplete), frameProgram, gtk_label_new_with_mnemonic (_("_Programs")));

  GtkWidget *tblProgram = gtk_grid_new ();
  gtk_frame_set_child (GTK_FRAME (frameProgram), tblProgram);

  GtkWidget *imgProgram = create_pixmap (frmConnect, "icon_program.png");
  gtk_grid_attach (GTK_GRID (tblProgram), imgProgram, 0, 0, 1, 1);
  tsc_widget_pad (imgProgram, 3, 3);
  tsc_align_panel_icon (imgProgram);

  GtkWidget *chkStartProgram = gtk_check_button_new_with_mnemonic (_("Start the following program on connection"));
  g_object_set_data (G_OBJECT (frmConnect), "chkStartProgram", chkStartProgram);
  gtk_grid_attach (GTK_GRID (tblProgram), chkStartProgram, 1, 0, 1, 1);
  gtk_widget_set_hexpand (chkStartProgram, TRUE);
  tsc_widget_pad (chkStartProgram, 3, 3);

  GtkWidget *lblProgramPath = gtk_label_new (_("Program path and filename"));
  gtk_grid_attach (GTK_GRID (tblProgram), lblProgramPath, 1, 1, 1, 1);
  gtk_widget_set_hexpand (lblProgramPath, TRUE);
  gtk_label_set_justify (GTK_LABEL (lblProgramPath), GTK_JUSTIFY_LEFT);

  GtkWidget *txtProgramPath = gtk_entry_new ();
  g_object_set_data (G_OBJECT (frmConnect), "txtProgramPath", txtProgramPath);
  gtk_grid_attach (GTK_GRID (tblProgram), txtProgramPath, 1, 2, 1, 1);
  gtk_widget_set_hexpand (txtProgramPath, TRUE);

  GtkWidget *lblStartFolder = gtk_label_new (_("Start in the following folder"));
  gtk_grid_attach (GTK_GRID (tblProgram), lblStartFolder, 1, 3, 1, 1);
  gtk_widget_set_hexpand (lblStartFolder, TRUE);
  gtk_label_set_justify (GTK_LABEL (lblStartFolder), GTK_JUSTIFY_LEFT);

  GtkWidget *txtStartFolder = gtk_entry_new ();
  g_object_set_data (G_OBJECT (frmConnect), "txtStartFolder", txtStartFolder);
  gtk_grid_attach (GTK_GRID (tblProgram), txtStartFolder, 1, 4, 1, 1);
  gtk_widget_set_hexpand (txtStartFolder, TRUE);
  g_signal_connect (chkStartProgram, "toggled", G_CALLBACK (on_chkStartProgram_toggled), frmConnect);
}

static void
tsc_build_performance_tab (GtkWidget *frmConnect, GtkWidget *nbkComplete)
{
  GtkWidget *framePerform = tsc_section_new (_("Optimize Performance"));
  gtk_notebook_append_page (GTK_NOTEBOOK (nbkComplete), framePerform, gtk_label_new_with_mnemonic (_("Perf_ormance")));

  GtkWidget *tblPerform = gtk_grid_new ();
  gtk_frame_set_child (GTK_FRAME (framePerform), tblPerform);

  GtkWidget *imgPerform = create_pixmap (frmConnect, "icon_perform.png");
  gtk_grid_attach (GTK_GRID (tblPerform), imgPerform, 0, 0, 1, 1);
  tsc_widget_pad (imgPerform, 3, 3);
  tsc_align_panel_icon (imgPerform);

  GtkWidget *vbxExpChecks = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_grid_attach (GTK_GRID (tblPerform), vbxExpChecks, 1, 1, 1, 1);
  gtk_widget_set_vexpand (vbxExpChecks, TRUE);

  GtkWidget *chkDesktopBackground = gtk_check_button_new_with_mnemonic (_("Desktop background (u)"));
  g_object_set_data (G_OBJECT (frmConnect), "chkDesktopBackground", chkDesktopBackground);
  gtk_box_append (GTK_BOX (vbxExpChecks), chkDesktopBackground);
  gtk_widget_set_hexpand (chkDesktopBackground, FALSE);

  GtkWidget *chkWindowContent = gtk_check_button_new_with_mnemonic (_("Show content of window while dragging (u)"));
  g_object_set_data (G_OBJECT (frmConnect), "chkWindowContent", chkWindowContent);
  gtk_box_append (GTK_BOX (vbxExpChecks), chkWindowContent);
  gtk_widget_set_hexpand (chkWindowContent, FALSE);

  GtkWidget *chkAnimation = gtk_check_button_new_with_mnemonic (_("Menu and window animation (u)"));
  g_object_set_data (G_OBJECT (frmConnect), "chkAnimation", chkAnimation);
  gtk_box_append (GTK_BOX (vbxExpChecks), chkAnimation);
  gtk_widget_set_hexpand (chkAnimation, FALSE);

  GtkWidget *chkThemes = gtk_check_button_new_with_mnemonic (_("Themes (u)"));
  g_object_set_data (G_OBJECT (frmConnect), "chkThemes", chkThemes);
  gtk_box_append (GTK_BOX (vbxExpChecks), chkThemes);
  gtk_widget_set_hexpand (chkThemes, FALSE);

  GtkWidget *chkBitmapCache = gtk_check_button_new_with_mnemonic (_("Enable bitmap caching"));
  g_object_set_data (G_OBJECT (frmConnect), "chkBitmapCache", chkBitmapCache);
  gtk_box_append (GTK_BOX (vbxExpChecks), chkBitmapCache);
  gtk_widget_set_hexpand (chkBitmapCache, FALSE);

  GtkWidget *chkNoMotionEvents = gtk_check_button_new_with_mnemonic (_("Do not send motion events"));
  g_object_set_data (G_OBJECT (frmConnect), "chkNoMotionEvents", chkNoMotionEvents);
  gtk_box_append (GTK_BOX (vbxExpChecks), chkNoMotionEvents);
  gtk_widget_set_hexpand (chkNoMotionEvents, FALSE);

  GtkWidget *chkEnableWMKeys = gtk_check_button_new_with_mnemonic (_("Enable window manager's key bindings"));
  g_object_set_data (G_OBJECT (frmConnect), "chkEnableWMKeys", chkEnableWMKeys);
  gtk_box_append (GTK_BOX (vbxExpChecks), chkEnableWMKeys);
  gtk_widget_set_hexpand (chkEnableWMKeys, FALSE);

  GtkWidget *chkHideWMDecorations = gtk_check_button_new_with_mnemonic (_("Hide window manager's decorations"));
  g_object_set_data (G_OBJECT (frmConnect), "chkHideWMDecorations", chkHideWMDecorations);
  gtk_box_append (GTK_BOX (vbxExpChecks), chkHideWMDecorations);
  gtk_widget_set_hexpand (chkHideWMDecorations, FALSE);

  GtkWidget *chkAttachToConsole = gtk_check_button_new_with_mnemonic (_("Attach to console"));
  g_object_set_data (G_OBJECT (frmConnect), "chkAttachToConsole", chkAttachToConsole);
  gtk_box_append (GTK_BOX (vbxExpChecks), chkAttachToConsole);
  gtk_widget_set_hexpand (chkAttachToConsole, FALSE);

  GtkWidget *lblPerformanceOptions = gtk_label_new (_("Options available for optimizing performance"));
  gtk_grid_attach (GTK_GRID (tblPerform), lblPerformanceOptions, 1, 0, 1, 1);
  gtk_widget_set_hexpand (lblPerformanceOptions, TRUE);
  gtk_label_set_justify (GTK_LABEL (lblPerformanceOptions), GTK_JUSTIFY_LEFT);
  gtk_label_set_wrap (GTK_LABEL (lblPerformanceOptions), TRUE);
  gtk_widget_set_halign (lblPerformanceOptions, GTK_ALIGN_START);
  gtk_widget_set_valign (lblPerformanceOptions, GTK_ALIGN_CENTER);
}

static void
tsc_build_actions (GtkWidget *frmConnect, GtkWidget *vbxComplete)
{
  GtkWidget *hbxProfileLauncher = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_append (GTK_BOX (vbxComplete), hbxProfileLauncher);
  gtk_widget_set_vexpand (hbxProfileLauncher, TRUE);
  tsc_widget_pad (hbxProfileLauncher, 6, 6);
  gtk_box_set_spacing (GTK_BOX (hbxProfileLauncher), 6);

  GtkWidget *optProfileLauncher = tsc_dropdown_new ((const gchar * const []) { _("Quick Connect"), NULL });
  g_object_set_data (G_OBJECT (frmConnect), "optProfileLauncher", optProfileLauncher);
  gtk_widget_set_tooltip_text (optProfileLauncher, _("Select a saved profile to launch it immediately."));
  gtk_box_append (GTK_BOX (hbxProfileLauncher), optProfileLauncher);
  gtk_widget_set_hexpand (optProfileLauncher, TRUE);
  g_signal_connect (optProfileLauncher, "notify::selected",
                    G_CALLBACK (tsc_quick_pick_activate), NULL);

  GtkWidget *hbxFileOps = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_homogeneous (GTK_BOX (hbxFileOps), TRUE);
  gtk_box_append (GTK_BOX (hbxProfileLauncher), hbxFileOps);
  tsc_widget_pad (hbxFileOps, 0, 0);
  gtk_box_set_spacing (GTK_BOX (hbxFileOps), 6);

  GtkWidget *btnOpen = gtk_button_new_with_mnemonic (_("_Open…"));
  g_object_set_data (G_OBJECT (frmConnect), "btnOpen", btnOpen);
  gtk_widget_add_css_class (btnOpen, "tsc-action");
  gtk_widget_set_tooltip_text (btnOpen, _("Open an existing profile file."));
  gtk_box_append (GTK_BOX (hbxFileOps), btnOpen);
  gtk_widget_set_hexpand (btnOpen, TRUE);

  GtkWidget *btnSaveAs = gtk_button_new_with_mnemonic (_("Save _As…"));
  g_object_set_data (G_OBJECT (frmConnect), "btnSaveAs", btnSaveAs);
  gtk_widget_add_css_class (btnSaveAs, "tsc-action");
  gtk_widget_set_tooltip_text (btnSaveAs, _("Save the current settings to a profile file."));
  gtk_box_append (GTK_BOX (hbxFileOps), btnSaveAs);
  gtk_widget_set_hexpand (btnSaveAs, TRUE);

  GtkWidget *hbbAppOps = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append (GTK_BOX (vbxComplete), hbbAppOps);
  gtk_widget_set_hexpand (hbbAppOps, TRUE);
  gtk_widget_set_halign (hbbAppOps, GTK_ALIGN_END);
  tsc_widget_pad (hbbAppOps, 6, 6);

  GtkWidget *btnHelp = gtk_button_new_with_mnemonic (_("_About"));
  g_object_set_data (G_OBJECT (frmConnect), "btnHelp", btnHelp);
  gtk_widget_add_css_class (btnHelp, "tsc-action");
  gtk_box_append (GTK_BOX (hbbAppOps), btnHelp);
  gtk_widget_set_vexpand (btnHelp, FALSE);

  GtkWidget *btnQuit = gtk_button_new_with_mnemonic (_("_Exit"));
  g_object_set_data (G_OBJECT (frmConnect), "btnQuit", btnQuit);
  gtk_widget_add_css_class (btnQuit, "tsc-action");
  gtk_box_append (GTK_BOX (hbbAppOps), btnQuit);
  gtk_widget_set_vexpand (btnQuit, FALSE);

  GtkWidget *btnConnect = gtk_button_new_with_mnemonic (_("Co_nnect"));
  g_object_set_data (G_OBJECT (frmConnect), "btnConnect", btnConnect);
  gtk_widget_add_css_class (btnConnect, "tsc-action");
  gtk_box_append (GTK_BOX (hbbAppOps), btnConnect);
  gtk_widget_set_vexpand (btnConnect, FALSE);
  gtk_window_set_default_widget (GTK_WINDOW (frmConnect), btnConnect);

  GtkWidget *lblConnect = gtk_button_get_child (GTK_BUTTON (btnConnect));
  g_object_set_data (G_OBJECT (frmConnect), "lblConnect", lblConnect);
  g_signal_connect (btnConnect, "clicked", G_CALLBACK (on_btnConnect_clicked), NULL);
  g_signal_connect (btnQuit, "clicked", G_CALLBACK (on_btnQuit_clicked), NULL);
  g_signal_connect (btnHelp, "clicked", G_CALLBACK (on_btnHelp_clicked), NULL);
  g_signal_connect (btnSaveAs, "clicked", G_CALLBACK (on_btnSaveAs_clicked), NULL);
  g_signal_connect (btnOpen, "clicked", G_CALLBACK (on_btnOpen_clicked), NULL);
}

int
create_frmConnect (void)
{
  tsc_ensure_form_css ();

  GtkWidget *frmConnect = gtk_application_window_new (tsc_app);
  g_object_set_data (G_OBJECT (frmConnect), "frmConnect", frmConnect);
  gtk_window_set_title (GTK_WINDOW (frmConnect), _("Terminal Server Client"));
  gtk_window_set_default_size (GTK_WINDOW (frmConnect), 676, 560);
  gtk_window_set_resizable (GTK_WINDOW (frmConnect), TRUE);
  gtk_widget_add_css_class (frmConnect, "tsc-root");
  gtk_window_set_icon_name (GTK_WINDOW (frmConnect), "tsclient");

  GtkWidget *vbxMain = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (frmConnect), vbxMain);

  GtkWidget *vbxBanner = tsc_create_banner ();
  gtk_box_append (GTK_BOX (vbxMain), vbxBanner);

  GtkWidget *vbxComplete = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (vbxMain), vbxComplete);
  gtk_widget_set_vexpand (vbxComplete, TRUE);

  GtkWidget *nbkComplete = gtk_notebook_new ();
  g_object_set_data (G_OBJECT (frmConnect), "nbkComplete", nbkComplete);
  gtk_box_append (GTK_BOX (vbxComplete), nbkComplete);
  gtk_widget_set_vexpand (nbkComplete, TRUE);
  tsc_widget_pad (nbkComplete, 6, 6);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (nbkComplete), FALSE);
  tsc_build_general_tab (frmConnect, nbkComplete);
  tsc_build_display_tab (frmConnect, nbkComplete);
  tsc_build_resources_tab (frmConnect, nbkComplete);
  tsc_build_program_tab (frmConnect, nbkComplete);
  tsc_build_performance_tab (frmConnect, nbkComplete);
  tsc_build_actions (frmConnect, vbxComplete);

  // disable the keyboard opts until they are supported
  gtk_widget_set_sensitive (lookup_widget (frmConnect, "optKeyboard"), FALSE);

  // toggle the start program thingy to false.  will be set after this by loading.
  gtk_check_button_set_active ((GtkCheckButton*) lookup_widget (frmConnect, "chkStartProgram"), FALSE);
  gtk_widget_set_sensitive (lookup_widget (frmConnect, "txtProgramPath"), FALSE);
  gtk_widget_set_sensitive (lookup_widget (frmConnect, "txtStartFolder"), FALSE);

  gtk_widget_set_sensitive (lookup_widget (frmConnect, "alnColor"), FALSE);
  tsc_set_protocol_widgets (frmConnect, 0);

  gtk_widget_set_visible (frmConnect, TRUE);

  // load mru & profile launcher
  mru_to_screen (frmConnect);
  rdp_load_profile_launcher (frmConnect);

  gtk_widget_set_visible (lookup_widget (frmConnect, "chkDesktopBackground"), FALSE);
  gtk_widget_set_visible (lookup_widget (frmConnect, "chkWindowContent"), FALSE);
  gtk_widget_set_visible (lookup_widget (frmConnect, "chkAnimation"), FALSE);
  gtk_widget_set_visible (lookup_widget (frmConnect, "chkThemes"), FALSE);

  gConnect = frmConnect;
  tsc_connection_bind_window (frmConnect);
  tsc_update_size_controls (frmConnect);
  tsc_update_color_controls (frmConnect);

  return 0;
}

/***************************************
*                                      *
*   Event Handlers                     *
*                                      *
***************************************/

static void
on_btnConnect_clicked (GtkButton *button, gpointer user_data)
{
  (void) user_data;
  tsc_connection_start (lookup_widget (GTK_WIDGET (button), "frmConnect"));
}

static void
on_btnQuit_clicked (GtkButton *button, gpointer user_data)
{
  (void) button; (void) user_data;
  if (gConnect) gtk_window_close (GTK_WINDOW (gConnect));
}

static void
on_btnHelp_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
  tsc_about_dialog ();
}

static void
on_size_changed                    (GtkCheckButton *button,
                                        gpointer         user_data)
{
  if (!gtk_check_button_get_active (button))
    return;

  tsc_update_size_controls (GTK_WIDGET (user_data));
}

static void
on_color_changed                   (GtkCheckButton *button,
                                        gpointer         user_data)
{
  if (!gtk_check_button_get_active (button))
    return;

  tsc_update_color_controls (GTK_WIDGET (user_data));
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

typedef struct {
  GtkWidget *window;
  gint action; /* 0: open profile, 1: save profile, 2: select backend file */
} FileRequest;

static void
profile_dialog_done (GObject *source, GAsyncResult *result, gpointer data)
{
  FileRequest *request = data;
  GError *error = NULL;
  GFile *file = request->action == 1
    ? gtk_file_dialog_save_finish (GTK_FILE_DIALOG (source), result, &error)
    : gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, &error);
  if (!g_object_get_data (G_OBJECT (request->window), "tsc-closed")) {
    if (file) {
      gchar *path = g_file_get_path (file);
      if (!path)
        tsc_error_message (_("Please choose a local file."));
      else if (request->action == 1) {
        if (!g_str_has_suffix (path, ".tsc"))
          tsc_error_message (_("Save profiles with a .tsc filename. Windows .rdp files can be opened, but are not overwritten."));
        else
          tsc_profile_save_for_window (request->window, path);
      } else if (request->action == 2) {
        gtk_editable_set_text (GTK_EDITABLE (lookup_widget (request->window, "txtProtoFile")), path);
      } else {
        rdp_file profile = {0};
        rdp_file_init (&profile);
        if (rdp_file_load (&profile, path) == 0)
          rdp_file_set_screen (&profile, request->window);
        else
          tsc_error_message (_("This profile could not be read. It may be invalid or use an unsupported encoding."));
        rdp_file_clear (&profile);
      }
      g_free (path);
    } else if (error && !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) &&
               !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED))
      tsc_error_message (error->message);
  }
  g_clear_object (&file);
  g_clear_error (&error);
  g_object_unref (request->window);
  g_free (request);
}

static void
open_profile_dialog (GtkButton *button, gint action)
{
  GtkWidget *window = lookup_widget (GTK_WIDGET (button), "frmConnect");
  FileRequest *request = g_new0 (FileRequest, 1);
  request->window = g_object_ref (window);
  request->action = action;
  GtkFileDialog *dialog = tsc_create_rdp_dialog (action == 1 ? _("Save profile") : _("Open profile"), action == 1);
  gchar *home = tsc_home_path ();
  GFile *folder = g_file_new_for_path (home);
  gtk_file_dialog_set_initial_folder (dialog, folder);
  if (action == 1)
    gtk_file_dialog_save (dialog, GTK_WINDOW (window), NULL, profile_dialog_done, request);
  else
    gtk_file_dialog_open (dialog, GTK_WINDOW (window), NULL, profile_dialog_done, request);
  g_object_unref (dialog);
  g_object_unref (folder);
  g_free (home);
}

static void on_btnSaveAs_clicked (GtkButton *button, gpointer data)
{
  (void) data;
  open_profile_dialog (button, 1);
}

static void on_btnOpen_clicked (GtkButton *button, gpointer data)
{
  (void) data;
  open_profile_dialog (button, 0);
}

static void on_btnProtoFile_clicked (GtkButton *button, gpointer data)
{
  (void) data;
  open_profile_dialog (button, 2);
}

static void
on_chkStartProgram_toggled (GtkCheckButton *button, gpointer user_data)
{
  GtkWidget *window = GTK_WIDGET (user_data);
  gboolean active = gtk_check_button_get_active (button);
  gtk_widget_set_sensitive (lookup_widget (window, "txtProgramPath"), active);
  gtk_widget_set_sensitive (lookup_widget (window, "txtStartFolder"), active);
}


static void
on_protocol_changed (GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
  (void) pspec;
  GtkWidget *main_win = user_data ? GTK_WIDGET (user_data) : gConnect;
  const gint protocols[] = { 0, 4, 1, 2, 3, 5 };
  guint selected = gtk_drop_down_get_selected (dropdown);
  gint protocol = selected < G_N_ELEMENTS (protocols) ? protocols[selected] : 0;
  tsc_set_protocol_widgets (main_win, protocol);
}
