#include "connection.h"
#include "credentials.h"
#include "launcher.h"
#include "session.h"
#include "support.h"
#include "connect.h"
#include "mrulist.h"
#include "main.h"
#include <glib/gi18n.h>

typedef struct {
  GtkWidget *window;
  GApplication *application;
  rdp_file *profile;
  gchar *filename;
  GCancellable *cancel;
  gboolean launch;
  gboolean clear;
} Operation;

static gboolean closed (GtkWidget *window)
{
  return g_object_get_data (G_OBJECT (window), "tsc-closed") != NULL;
}

static void show_error (GtkWidget *window, const gchar *message)
{
  if (!closed (window)) {
    GtkAlertDialog *alert = gtk_alert_dialog_new ("%s", _("Unable to complete the operation"));
    gtk_alert_dialog_set_detail (alert, message);
    gtk_alert_dialog_show (alert, GTK_WINDOW (window));
    g_object_unref (alert);
  }
}

static gboolean same_identity (const rdp_file *a, const rdp_file *b)
{
  return a->protocol == b->protocol && !g_strcmp0 (a->full_address, b->full_address) &&
         !g_strcmp0 (a->username, b->username) && !g_strcmp0 (a->domain, b->domain);
}

static void set_busy (GtkWidget *window, gboolean busy)
{
  const gchar *names[] = { "nbkComplete", "btnOpen", "btnSaveAs", "optProfileLauncher", NULL };
  for (guint i = 0; names[i]; i++) {
    GtkWidget *widget = lookup_widget (window, names[i]);
    if (widget) gtk_widget_set_sensitive (widget, !busy);
  }
  GtkWidget *label = lookup_widget (window, "lblConnect");
  gtk_label_set_text_with_mnemonic (GTK_LABEL (label), busy ? _("_Cancel") : _("_Connect"));
}

static gboolean close_when_idle (gpointer data)
{
  GtkWidget *window = data;
  g_object_set_data (G_OBJECT (window), "tsc-close-scheduled", NULL);
  if (!closed (window)) gtk_window_close (GTK_WINDOW (window));
  return G_SOURCE_REMOVE;
}

static void operation_free (Operation *op)
{
  if (g_object_get_data (G_OBJECT (op->window), "tsc-operation") == op->cancel) {
    g_object_set_data (G_OBJECT (op->window), "tsc-operation", NULL);
    if (!closed (op->window)) set_busy (op->window, FALSE);
  }
  g_object_unref (op->cancel);
  rdp_file_free (op->profile);
  g_free (op->filename);
  gboolean finish_close = g_object_get_data (G_OBJECT (op->window), "tsc-close-requested") &&
                          !g_object_get_data (G_OBJECT (op->window), "tsc-operation");
  GtkWidget *window = g_object_ref (op->window);
  g_object_unref (op->window);
  if (op->application) {
    g_application_release (op->application);
    g_object_unref (op->application);
  }
  g_free (op);
  if (finish_close && !closed (window) && !g_object_get_data (G_OBJECT (window), "tsc-close-scheduled")) {
    g_object_set_data (G_OBJECT (window), "tsc-close-scheduled", GINT_TO_POINTER (1));
    g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, close_when_idle, g_object_ref (window), g_object_unref);
  }
  g_object_unref (window);
}

static void session_done (GObject *source, GAsyncResult *result, gpointer data)
{
  Operation *op = data;
  GError *error = NULL;
  (void) source;
  gboolean ok = tsc_session_finish (result, &error);
  if (!closed (op->window)) {
    if (ok) {
      mru_add_server (op->profile->full_address);
      mru_to_screen (op->window);
    } else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      show_error (op->window, error->message);
  }
  g_clear_error (&error);
  operation_free (op);
}

static void save_and_launch (Operation *op)
{
  if (closed (op->window) || g_cancellable_is_cancelled (op->cancel)) {
    operation_free (op);
    return;
  }
  if (rdp_file_save (op->profile, op->filename) != 0) {
    g_object_set_data (G_OBJECT (op->window), "tsc-close-requested", NULL);
    show_error (op->window, _("The profile could not be saved. Check the destination and permissions."));
    operation_free (op);
    return;
  }
  g_object_set_data_full (G_OBJECT (op->window), "tsc-remembered-profile",
                         op->profile->remember_password ? rdp_file_copy (op->profile) : NULL,
                         (GDestroyNotify) rdp_file_free);
  if (!op->launch) {
    if (g_object_get_data (G_OBJECT (op->window), "tsc-close-requested"))
      g_object_set_data (G_OBJECT (op->window), "tsc-close-ready", GINT_TO_POINTER (1));
    rdp_load_profile_launcher (op->window);
    operation_free (op);
    return;
  }
  GError *error = NULL;
  gchar **argv = tsc_remote_argv (op->profile, &error);
  if (!argv) {
    show_error (op->window, error->message);
    g_clear_error (&error);
    operation_free (op);
    return;
  }
  tsc_session_start ((const gchar *const *) argv, op->cancel, session_done, op);
  g_strfreev (argv);
}

static void keyring_saved (GObject *source, GAsyncResult *result, gpointer data)
{
  Operation *op = data;
  GError *error = NULL;
  (void) source;
  gboolean ok = op->clear ? tsc_credentials_clear_finish (result, &error)
                          : tsc_credentials_store_finish (result, &error);
  if (op->clear && !error) ok = TRUE;
  if (!ok) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
        g_object_get_data (G_OBJECT (op->window), "tsc-close-requested"))
      g_object_set_data (G_OBJECT (op->window), "tsc-skip-keyring-close", GINT_TO_POINTER (1));
    else
      g_object_set_data (G_OBJECT (op->window), "tsc-close-requested", NULL);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      show_error (op->window, _("The desktop keyring could not be updated. Unlock it and try again, or turn off Remember password. Passwords are never saved in profile files."));
    g_clear_error (&error);
    operation_free (op);
    return;
  }
  g_object_set_data (G_OBJECT (op->window), "tsc-keyring-confirmed",
                     op->clear ? NULL : GINT_TO_POINTER (1));
  save_and_launch (op);
}

static void start_operation (GtkWidget *window, const gchar *filename, gboolean launch)
{
  GCancellable *running = g_object_get_data (G_OBJECT (window), "tsc-operation");
  if (running) {
    if (launch) g_cancellable_cancel (running);
    return;
  }
  if (g_object_get_data (G_OBJECT (window), "tsc-lookup")) {
    show_error (window, _("The saved password is still loading. Please wait or change the connection details."));
    return;
  }
  Operation *op = g_new0 (Operation, 1);
  op->window = g_object_ref (window);
  if (tsc_app) {
    op->application = g_object_ref (G_APPLICATION (tsc_app));
    g_application_hold (op->application);
  }
  op->profile = g_new0 (rdp_file, 1);
  rdp_file_init (op->profile);
  rdp_file_get_screen (op->profile, window);
  op->filename = g_strdup (filename);
  op->launch = launch;
  op->cancel = g_cancellable_new ();
  if (launch) {
    GError *error = NULL;
    gchar **argv = tsc_remote_argv (op->profile, &error);
    if (!argv) {
      show_error (window, error->message);
      g_clear_error (&error);
      operation_free (op);
      return;
    }
    g_strfreev (argv);
  }
  g_object_set_data (G_OBJECT (window), "tsc-operation", op->cancel);
  set_busy (window, TRUE);
  rdp_file *remembered = g_object_get_data (G_OBJECT (window), "tsc-remembered-profile");
  if (g_object_get_data (G_OBJECT (window), "tsc-close-requested") &&
      g_object_get_data (G_OBJECT (window), "tsc-skip-keyring-close")) {
    g_object_set_data (G_OBJECT (window), "tsc-skip-keyring-close", NULL);
    save_and_launch (op);
  }
  else if (op->profile->remember_password && remembered &&
           g_object_get_data (G_OBJECT (window), "tsc-keyring-confirmed") && same_identity (remembered, op->profile) &&
           !g_strcmp0 (remembered->password, op->profile->password))
    save_and_launch (op);
  else if (op->profile->remember_password) {
    /* A cancelled store may already have reached the service. Keep its identity
     * so turning Remember off can still remove that credential. */
    g_object_set_data_full (G_OBJECT (window), "tsc-remembered-profile",
                           rdp_file_copy (op->profile), (GDestroyNotify) rdp_file_free);
    g_object_set_data (G_OBJECT (window), "tsc-keyring-confirmed", NULL);
    tsc_credentials_store_async (op->profile, op->cancel, keyring_saved, op);
  }
  else if (remembered && same_identity (remembered, op->profile)) {
    op->clear = TRUE;
    tsc_credentials_clear_async (op->profile, op->cancel, keyring_saved, op);
  } else
    save_and_launch (op);
}

void tsc_connection_start (GtkWidget *window)
{
  gchar *home = tsc_home_path ();
  gchar *filename = g_build_filename (home, "last.tsc", NULL);
  start_operation (window, filename, TRUE);
  g_free (filename);
  g_free (home);
}

void tsc_profile_save_for_window (GtkWidget *window, const gchar *filename)
{
  start_operation (window, filename, FALSE);
}

static void password_loaded (GObject *source, GAsyncResult *result, gpointer data)
{
  Operation *op = data;
  GError *error = NULL;
  gchar *password = tsc_credentials_lookup_finish (result, &error);
  (void) source;
  if (!closed (op->window) && !g_cancellable_is_cancelled (op->cancel)) {
    rdp_file current = {0};
    rdp_file_init (&current);
    rdp_file_get_screen (&current, op->window);
    if (same_identity (&current, op->profile) && current.remember_password &&
        g_strcmp0 (current.password, op->profile->password) == 0) {
      if (password) {
        g_object_set_data (G_OBJECT (op->window), "tsc-keyring-confirmed", GINT_TO_POINTER (1));
        gtk_editable_set_text (GTK_EDITABLE (lookup_widget (op->window, "txtPassword")), password);
        g_free (op->profile->password);
        op->profile->password = g_strdup (password);
        g_object_set_data_full (G_OBJECT (op->window), "tsc-remembered-profile", rdp_file_copy (op->profile), (GDestroyNotify) rdp_file_free);
      }
      else if (error)
        show_error (op->window, _("The saved password could not be read from the desktop keyring. You can enter it manually."));
    }
    rdp_file_clear (&current);
  }
  if (g_object_get_data (G_OBJECT (op->window), "tsc-lookup") == op->cancel) {
    g_object_set_data (G_OBJECT (op->window), "tsc-lookup", NULL);
    if (!closed (op->window) && !g_object_get_data (G_OBJECT (op->window), "tsc-operation")) {
      gtk_widget_set_sensitive (lookup_widget (op->window, "btnConnect"), TRUE);
      gtk_widget_set_sensitive (lookup_widget (op->window, "btnSaveAs"), TRUE);
    }
  }
  tsc_credentials_password_free (password);
  g_clear_error (&error);
  operation_free (op);
}

void tsc_credentials_load_for_window (GtkWidget *window, const rdp_file *profile)
{
  GCancellable *previous = g_object_get_data (G_OBJECT (window), "tsc-lookup");
  if (previous) g_cancellable_cancel (previous);
  g_object_set_data (G_OBJECT (window), "tsc-lookup", NULL);
  gtk_widget_set_sensitive (lookup_widget (window, "btnConnect"), TRUE);
  gtk_widget_set_sensitive (lookup_widget (window, "btnSaveAs"), TRUE);
  g_object_set_data_full (G_OBJECT (window), "tsc-remembered-profile",
                         profile->remember_password ? rdp_file_copy (profile) : NULL,
                         (GDestroyNotify) rdp_file_free);
  g_object_set_data (G_OBJECT (window), "tsc-keyring-confirmed", NULL);
  if (!profile->remember_password || !*profile->full_address) return;
  Operation *op = g_new0 (Operation, 1);
  op->window = g_object_ref (window);
  if (tsc_app) {
    op->application = g_object_ref (G_APPLICATION (tsc_app));
    g_application_hold (op->application);
  }
  op->profile = rdp_file_copy (profile);
  op->cancel = g_cancellable_new ();
  g_object_set_data (G_OBJECT (window), "tsc-lookup", op->cancel);
  gtk_widget_set_sensitive (lookup_widget (window, "btnConnect"), FALSE);
  gtk_widget_set_sensitive (lookup_widget (window, "btnSaveAs"), FALSE);
  tsc_credentials_lookup_async (profile, op->cancel, password_loaded, op);
}

static void identity_changed (GObject *entry, gpointer unused, gpointer data)
{
  GtkWidget *window = data;
  (void) entry; (void) unused;
  if (closed (window) || g_object_get_data (G_OBJECT (window), "tsc-setting-profile")) return;
  GCancellable *lookup = g_object_get_data (G_OBJECT (window), "tsc-lookup");
  if (lookup) g_cancellable_cancel (lookup);
  g_object_set_data (G_OBJECT (window), "tsc-keyring-confirmed", NULL);
  gtk_editable_set_text (GTK_EDITABLE (lookup_widget (window, "txtPassword")), "");
  g_object_set_data (G_OBJECT (window), "tsc-lookup", NULL);
  g_object_set_data_full (G_OBJECT (window), "tsc-remembered-profile", NULL, NULL);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (lookup_widget (window, "chkRememberPassword")), FALSE);
  gtk_widget_set_sensitive (lookup_widget (window, "btnConnect"), TRUE);
  gtk_widget_set_sensitive (lookup_widget (window, "btnSaveAs"), TRUE);
}

static void entry_identity_changed (GtkEditable *entry, gpointer data)
{
  identity_changed (G_OBJECT (entry), NULL, data);
}

static gboolean window_closing (GtkWindow *window, gpointer unused)
{
  (void) unused;
  GCancellable *operation = g_object_get_data (G_OBJECT (window), "tsc-operation");
  GCancellable *lookup = g_object_get_data (G_OBJECT (window), "tsc-lookup");
  if (g_object_get_data (G_OBJECT (window), "tsc-close-ready")) {
    g_object_set_data (G_OBJECT (window), "tsc-closed", GINT_TO_POINTER (1));
    if (lookup) g_cancellable_cancel (lookup);
    gConnect = NULL;
    return FALSE;
  }
  g_object_set_data (G_OBJECT (window), "tsc-close-requested", GINT_TO_POINTER (1));
  if (operation) {
    g_cancellable_cancel (operation);
    return TRUE; /* Close after the child has been reaped. */
  }
  if (lookup) {
    g_object_set_data (G_OBJECT (window), "tsc-skip-keyring-close", GINT_TO_POINTER (1));
    g_cancellable_cancel (lookup);
    return TRUE; /* Wait for lookup cleanup before saving current settings. */
  }
  gchar *home = tsc_home_path ();
  gchar *path = g_build_filename (home, "last.tsc", NULL);
  start_operation (GTK_WIDGET (window), path, FALSE);
  g_free (path); g_free (home);
  return TRUE;
}

void tsc_connection_bind_window (GtkWidget *window)
{
  g_signal_connect (window, "close-request", G_CALLBACK (window_closing), NULL);
  const gchar *fields[] = {"txtComputer", "txtUsername", "txtDomain", NULL};
  for (guint i=0; fields[i]; i++)
    g_signal_connect (lookup_widget (window, fields[i]), "changed", G_CALLBACK (entry_identity_changed), window);
  g_signal_connect (lookup_widget (window, "optProtocol"), "notify::selected", G_CALLBACK (identity_changed), window);
}
