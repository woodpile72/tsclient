/* Real connection.c with mocked credentials, profile persistence and sessions.
 * No libsecret, profile files, subprocesses, or real keyring are used.
 * Build/run from repository root:
 * cc -std=c11 -Wall -Wextra -Werror -g -Isrc \
 *   $(pkg-config --cflags gtk4) tests/test-connection-credentials.c \
 *   $(pkg-config --libs gtk4) -o /tmp/test-connection-credentials
 * GTK_A11Y=none xvfb-run -a /tmp/test-connection-credentials
 * All regression cases run by default; no opt-in flags are needed.
 * Optional: -fsanitize=address,undefined (GTK may retain process-global caches).
 */
#include "../src/connection.c"

GtkWidget *gConnect;
GtkApplication *tsc_app;
static guint application_holds;
void g_application_hold (GApplication *app)
{
  g_assert_true (app == G_APPLICATION (tsc_app));
  application_holds++;
}
void g_application_release (GApplication *app)
{
  g_assert_true (app == G_APPLICATION (tsc_app));
  g_assert_cmpuint (application_holds, >, 0);
  application_holds--;
}
/* Exercise the close handler synchronously; actual widget destruction remains
 * under fixture control so late callbacks can be checked without a display race. */
void gtk_window_close (GtkWindow *window)
{
  window_closing (window, NULL);
}
static rdp_file screen;
static GPtrArray *pending;
static guint saves, launches, alerts;
static rdp_file *last_store;

typedef enum { LOOKUP, STORE, CLEAR, SESSION } Kind;
typedef struct {
  GTask *task;
  Kind kind;
  rdp_file *profile;
} Pending;

int rdp_file_init (rdp_file *r)
{
  memset (r, 0, sizeof *r);
  r->full_address = g_strdup ("server");
  r->username = g_strdup ("");
  r->domain = g_strdup ("");
  r->password = g_strdup ("");
  return 0;
}
void rdp_file_clear (rdp_file *r)
{
  g_free (r->full_address); g_free (r->username);
  g_free (r->domain); g_free (r->password);
  memset (r, 0, sizeof *r);
}
void rdp_file_free (rdp_file *r)
{
  if (r) { rdp_file_clear (r); g_free (r); }
}
rdp_file *rdp_file_copy (const rdp_file *r)
{
  rdp_file *copy = g_new0 (rdp_file, 1);
  copy->protocol = r->protocol;
  copy->remember_password = r->remember_password;
  copy->full_address = g_strdup (r->full_address);
  copy->username = g_strdup (r->username);
  copy->domain = g_strdup (r->domain);
  copy->password = g_strdup (r->password);
  return copy;
}
GtkWidget *lookup_widget (GtkWidget *w, const gchar *name)
{
  return g_object_get_data (G_OBJECT (w), name);
}
int rdp_file_get_screen (rdp_file *r, GtkWidget *w)
{
  rdp_file_clear (r);
  rdp_file *copy = rdp_file_copy (&screen);
  *r = *copy; g_free (copy);
  g_free (r->password);
  r->remember_password = gtk_check_button_get_active (GTK_CHECK_BUTTON (lookup_widget (w, "chkRememberPassword")));
  r->password = g_strdup (gtk_editable_get_text (
      GTK_EDITABLE (lookup_widget (w, "txtPassword"))));
  return 0;
}
int rdp_file_save (rdp_file *r, const char *path)
{
  (void) r; (void) path; saves++; return 0;
}
int rdp_load_profile_launcher (GtkWidget *w) { (void) w; return 0; }
int mru_add_server (const char *server) { (void) server; return 0; }
int mru_to_screen (GtkWidget *w) { (void) w; return 0; }
gchar *tsc_home_path (void) { return g_strdup ("/unused-mock-home"); }
gchar **tsc_remote_argv (const rdp_file *r, GError **error)
{
  (void) r; (void) error;
  gchar **argv = g_new0 (gchar *, 2);
  argv[0] = g_strdup ("never-executed"); return argv;
}
/* Prevent dialogs from creating extra toplevels during negative tests. */
void gtk_alert_dialog_show (GtkAlertDialog *dialog, GtkWindow *parent)
{
  (void) dialog; (void) parent; alerts++;
}

static void enqueue (Kind kind, const rdp_file *r, GCancellable *c,
                     GAsyncReadyCallback callback, gpointer data)
{
  Pending *p = g_new0 (Pending, 1);
  p->kind = kind;
  p->profile = r ? rdp_file_copy (r) : NULL;
  p->task = g_task_new (NULL, c, callback, data);
  g_ptr_array_add (pending, p);
}
void tsc_credentials_lookup_async (const rdp_file *r, GCancellable *c,
                                   GAsyncReadyCallback cb, gpointer d)
{ enqueue (LOOKUP, r, c, cb, d); }
void tsc_credentials_store_async (const rdp_file *r, GCancellable *c,
                                  GAsyncReadyCallback cb, gpointer d)
{
  rdp_file_free (last_store); last_store = rdp_file_copy (r);
  enqueue (STORE, r, c, cb, d);
}
void tsc_credentials_clear_async (const rdp_file *r, GCancellable *c,
                                  GAsyncReadyCallback cb, gpointer d)
{ enqueue (CLEAR, r, c, cb, d); }
gchar *tsc_credentials_lookup_finish (GAsyncResult *r, GError **e)
{ return g_task_propagate_pointer (G_TASK (r), e); }
gboolean tsc_credentials_store_finish (GAsyncResult *r, GError **e)
{ return g_task_propagate_boolean (G_TASK (r), e); }
gboolean tsc_credentials_clear_finish (GAsyncResult *r, GError **e)
{ return g_task_propagate_boolean (G_TASK (r), e); }
void tsc_credentials_password_free (gchar *password) { g_free (password); }
void tsc_session_start (const gchar *const *argv, GCancellable *c,
                        GAsyncReadyCallback cb, gpointer d)
{ (void) argv; launches++; enqueue (SESSION, NULL, c, cb, d); }
gboolean tsc_session_finish (GAsyncResult *r, GError **e)
{ return g_task_propagate_boolean (G_TASK (r), e); }

static void drain (void)
{ while (g_main_context_iteration (NULL, FALSE)); }
static void complete (guint index, gboolean success, const gchar *password)
{
  Pending *p = g_ptr_array_steal_index (pending, index);
  if (p->kind == LOOKUP)
    g_task_return_pointer (p->task, g_strdup (password), g_free);
  else
    g_task_return_boolean (p->task, success);
  g_object_unref (p->task);
  rdp_file_free (p->profile); g_free (p);
  drain ();
}
static GtkWidget *setup (void)
{
  g_assert_null (pending);
  pending = g_ptr_array_new ();
  tsc_app = gtk_application_new (NULL, G_APPLICATION_NON_UNIQUE);
  g_assert_cmpuint (application_holds, ==, 0);
  saves = launches = alerts = 0;
  rdp_file_init (&screen); screen.remember_password = 1;
  GtkWidget *w = gtk_window_new ();
  g_object_ref (w);
  const gchar *names[] = {"txtPassword", "lblConnect", "nbkComplete",
                         "btnOpen", "btnSaveAs", "optProfileLauncher", "btnConnect",
                         "txtComputer", "txtUsername", "txtDomain"};
  for (guint i = 0; i < G_N_ELEMENTS (names); i++) {
    GtkWidget *child = i == 0 || i >= 7 ? gtk_entry_new () : gtk_label_new ("");
    g_object_set_data_full (G_OBJECT (w), names[i], g_object_ref_sink (child),
                            g_object_unref);
  }
  GtkWidget *remember = gtk_check_button_new ();
  gtk_check_button_set_active (GTK_CHECK_BUTTON (remember), TRUE);
  g_object_set_data_full (G_OBJECT (w), "chkRememberPassword",
                          g_object_ref_sink (remember), g_object_unref);
  return w;
}
static void teardown (GtkWidget *w)
{
  g_assert_cmpuint (pending->len, ==, 0);
  g_assert_null (g_object_get_data (G_OBJECT (w), "tsc-operation"));
  g_assert_null (g_object_get_data (G_OBJECT (w), "tsc-lookup"));
  gtk_window_destroy (GTK_WINDOW (w)); g_object_unref (w);
  g_clear_pointer (&pending, g_ptr_array_unref);
  g_assert_cmpuint (application_holds, ==, 0);
  g_clear_object (&tsc_app);
  rdp_file_clear (&screen);
  g_clear_pointer (&last_store, rdp_file_free);
  drain ();
}
static const gchar *password_text (GtkWidget *w)
{ return gtk_editable_get_text (GTK_EDITABLE (lookup_widget (w, "txtPassword"))); }

static void test_stale_lookup (void)
{
  GtkWidget *w = setup ();
  tsc_credentials_load_for_window (w, &screen);
  tsc_credentials_load_for_window (w, &screen);
  complete (1, TRUE, "new-secret");
  complete (0, TRUE, "old-secret");
  g_assert_cmpstr (password_text (w), ==, "new-secret");
  teardown (w);
}
static void test_edited_password (void)
{
  GtkWidget *w = setup ();
  tsc_credentials_load_for_window (w, &screen);
  gtk_editable_set_text (GTK_EDITABLE (lookup_widget (w, "txtPassword")), "typed");
  complete (0, TRUE, "saved");
  g_assert_cmpstr (password_text (w), ==, "typed");
  teardown (w);
}
static void test_cancel_store (void)
{
  GtkWidget *w = setup ();
  tsc_connection_start (w);
  tsc_connection_start (w); /* Connect becomes Cancel while busy. */
  complete (0, TRUE, NULL); /* Simulate service committing before cancellation. */
  g_assert_cmpuint (saves, ==, 0);
  g_assert_cmpuint (launches, ==, 0);
  g_assert_cmpuint (alerts, ==, 0);
  teardown (w);
}
static void test_close_lookup (void)
{
  GtkWidget *w = setup ();
  tsc_credentials_load_for_window (w, &screen);
  g_assert_true (window_closing (GTK_WINDOW (w), NULL));
  complete (0, TRUE, "late");
  g_assert_cmpstr (password_text (w), ==, "");
  g_assert_cmpuint (alerts, ==, 0);
  teardown (w);
}
static void test_close_store (void)
{
  GtkWidget *w = setup ();
  tsc_connection_start (w);
  g_assert_true (window_closing (GTK_WINDOW (w), NULL));
  g_assert_cmpuint (application_holds, >, 0);
  complete (0, TRUE, NULL);
  g_assert_true (closed (w));
  g_assert_cmpuint (saves, ==, 1);
  g_assert_cmpuint (launches, ==, 0);
  teardown (w);
}
static void test_close_session (void)
{
  GtkWidget *w = setup ();
  screen.remember_password = 0;
  gtk_check_button_set_active (GTK_CHECK_BUTTON (lookup_widget (w, "chkRememberPassword")), FALSE);
  tsc_connection_start (w);
  g_assert_cmpuint (launches, ==, 1);
  Pending *p = g_ptr_array_index (pending, 0);
  g_assert_true (window_closing (GTK_WINDOW (w), NULL));
  g_assert_true (g_cancellable_is_cancelled (g_task_get_cancellable (p->task)));
  g_assert_cmpuint (application_holds, >, 0);
  complete (0, TRUE, NULL);
  g_assert_cmpuint (alerts, ==, 0);
  teardown (w);
}
static void test_empty_overwrite (void)
{
  GtkWidget *w = setup ();
  tsc_credentials_load_for_window (w, &screen);
  tsc_profile_save_for_window (w, "/unused");
  /* Save must not overwrite a remembered secret while lookup is unresolved. */
  g_assert_null (last_store);
  g_assert_cmpuint (saves, ==, 0);
  while (pending->len) complete (0, TRUE, "saved");
  teardown (w);
}
static void test_absent_clear (void)
{
  GtkWidget *w = setup ();
  tsc_credentials_load_for_window (w, &screen);
  complete (0, TRUE, NULL);
  screen.remember_password = 0;
  gtk_check_button_set_active (GTK_CHECK_BUTTON (lookup_widget (w, "chkRememberPassword")), FALSE);
  tsc_profile_save_for_window (w, "/unused");
  complete (0, FALSE, NULL); /* No matching key, no error. */
  g_assert_cmpuint (saves, ==, 1);
  g_assert_cmpuint (alerts, ==, 0);
  teardown (w);
}
static void test_changed_identity (void)
{
  GtkWidget *w = setup ();
  tsc_credentials_load_for_window (w, &screen);
  complete (0, TRUE, "saved");
  g_free (screen.full_address); screen.full_address = g_strdup ("other-server");
  identity_changed (G_OBJECT (lookup_widget (w, "txtComputer")), NULL, w);
  g_assert_cmpstr (password_text (w), ==, "");
  g_assert_false (gtk_check_button_get_active (GTK_CHECK_BUTTON (
      lookup_widget (w, "chkRememberPassword"))));
  tsc_profile_save_for_window (w, "/unused");
  /* A new identity must not clear the unrelated previous connection's key. */
  g_assert_cmpuint (pending->len, ==, 0);
  g_assert_null (last_store);
  g_assert_cmpuint (saves, ==, 1);
  teardown (w);
}
static void test_identity_reset (void)
{
  GtkWidget *w = setup ();
  tsc_credentials_load_for_window (w, &screen);
  identity_changed (G_OBJECT (lookup_widget (w, "txtComputer")), NULL, w);
  g_assert_null (g_object_get_data (G_OBJECT (w), "tsc-remembered-profile"));
  g_assert_false (gtk_check_button_get_active (GTK_CHECK_BUTTON (
      lookup_widget (w, "chkRememberPassword"))));
  complete (0, TRUE, "stale-secret");
  g_assert_cmpstr (password_text (w), ==, "");
  teardown (w);
}
static void test_loading_guard (void)
{
  GtkWidget *w = setup ();
  tsc_credentials_load_for_window (w, &screen);
  g_object_set_data (G_OBJECT (w), "tsc-setting-profile", GINT_TO_POINTER (1));
  identity_changed (G_OBJECT (lookup_widget (w, "txtComputer")), NULL, w);
  g_object_set_data (G_OBJECT (w), "tsc-setting-profile", NULL);
  complete (0, TRUE, "saved");
  g_assert_cmpstr (password_text (w), ==, "saved");
  teardown (w);
}
static void test_cancel_then_uncheck_quit (void)
{
  GtkWidget *w = setup ();
  tsc_credentials_load_for_window (w, &screen);
  complete (0, TRUE, "saved");
  gtk_editable_set_text (GTK_EDITABLE (lookup_widget (w, "txtPassword")), "edited");
  tsc_profile_save_for_window (w, "/unused");
  tsc_connection_start (w); /* Cancel, without requesting Quit. */
  complete (0, TRUE, NULL);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (lookup_widget (w, "chkRememberPassword")), FALSE);
  window_closing (GTK_WINDOW (w), NULL);
  g_assert_cmpuint (pending->len, ==, 1);
  Pending *p = g_ptr_array_index (pending, 0);
  g_assert_cmpint (p->kind, ==, CLEAR);
  complete (0, TRUE, NULL);
  teardown (w);
}
static void test_empty_password (void)
{
  GtkWidget *w = setup ();
  tsc_profile_save_for_window (w, "/unused");
  /* Explicit Remember opt-in includes an empty secret. */
  g_assert_nonnull (last_store);
  g_assert_cmpstr (last_store->password, ==, "");
  g_assert_cmpuint (pending->len, ==, 1);
  Pending *p = g_ptr_array_index (pending, 0);
  g_assert_cmpint (p->kind, ==, STORE);
  complete (0, TRUE, NULL);
  g_assert_cmpuint (saves, ==, 1);
  g_assert_cmpuint (alerts, ==, 0);
  teardown (w);
}

static void test_legacy_password_store (gconstpointer data)
{
  GtkWidget *w = setup ();
  g_free (screen.password);
  screen.password = g_strdup ("synthetic-legacy-password");
  gtk_editable_set_text (GTK_EDITABLE (lookup_widget (w, "txtPassword")), screen.password);
  tsc_credentials_load_for_window (w, &screen);
  if (GPOINTER_TO_INT (data)) {
    Pending *p = g_ptr_array_steal_index (pending, 0);
    g_task_return_new_error (p->task, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Synthetic lookup failure");
    g_object_unref (p->task);
    rdp_file_free (p->profile); g_free (p);
    drain ();
    g_assert_cmpuint (alerts, ==, 1);
  } else {
    complete (0, TRUE, NULL);
    g_assert_cmpuint (alerts, ==, 0);
  }
  g_assert_null (g_object_get_data (G_OBJECT (w), "tsc-keyring-confirmed"));
  g_assert_cmpstr (password_text (w), ==, screen.password);
  tsc_profile_save_for_window (w, "/unused");
  g_assert_cmpuint (pending->len, ==, 1);
  Pending *p = g_ptr_array_index (pending, 0);
  g_assert_cmpint (p->kind, ==, STORE);
  g_assert_true (g_strcmp0 (p->profile->password, screen.password) == 0);
  g_assert_cmpuint (saves, ==, 0);
  complete (0, TRUE, NULL);
  g_assert_cmpuint (saves, ==, 1);
  g_assert_nonnull (g_object_get_data (G_OBJECT (w), "tsc-keyring-confirmed"));
  teardown (w);
}

static void test_cancel_edit_close (void)
{
  GtkWidget *w = setup ();
  gtk_editable_set_text (GTK_EDITABLE (lookup_widget (w, "txtPassword")), "first");
  tsc_profile_save_for_window (w, "/unused");
  tsc_connection_start (w); /* Cancel store without closing. */
  complete (0, TRUE, NULL);
  g_assert_cmpuint (saves, ==, 0);
  g_assert_null (g_object_get_data (G_OBJECT (w), "tsc-skip-keyring-close"));
  g_assert_nonnull (g_object_get_data (G_OBJECT (w), "tsc-remembered-profile"));
  gtk_editable_set_text (GTK_EDITABLE (lookup_widget (w, "txtPassword")), "replacement");
  g_assert_true (window_closing (GTK_WINDOW (w), NULL));
  g_assert_cmpuint (pending->len, ==, 1);
  Pending *p = g_ptr_array_index (pending, 0);
  g_assert_cmpint (p->kind, ==, STORE);
  g_assert_true (g_strcmp0 (p->profile->password, "replacement") == 0);
  g_assert_false (closed (w));
  complete (0, TRUE, NULL);
  g_assert_cmpuint (saves, ==, 1);
  g_assert_true (closed (w));
  g_assert_cmpuint (alerts, ==, 0);
  teardown (w);
}

static void test_manual_password_identity_clear (void)
{
  GtkWidget *w = setup ();
  g_assert_null (g_object_get_data (G_OBJECT (w), "tsc-remembered-profile"));
  gtk_check_button_set_active (GTK_CHECK_BUTTON (lookup_widget (w, "chkRememberPassword")), FALSE);
  gtk_editable_set_text (GTK_EDITABLE (lookup_widget (w, "txtPassword")), "manual-secret");
  identity_changed (G_OBJECT (lookup_widget (w, "txtComputer")), NULL, w);
  g_assert_cmpstr (password_text (w), ==, "");
  g_assert_cmpuint (pending->len, ==, 0);
  g_assert_null (last_store);
  teardown (w);
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ()) return 77;
  g_test_add_func ("/connection/stale-lookup", test_stale_lookup);
  g_test_add_func ("/connection/edited-password", test_edited_password);
  g_test_add_func ("/connection/cancel-store", test_cancel_store);
  g_test_add_func ("/connection/close-lookup", test_close_lookup);
  g_test_add_func ("/connection/close-store", test_close_store);
  g_test_add_func ("/connection/close-session", test_close_session);
  g_test_add_func ("/connection/pending-lookup-save", test_empty_overwrite);
  g_test_add_func ("/connection/absent-clear", test_absent_clear);
  g_test_add_func ("/connection/changed-identity", test_changed_identity);
  g_test_add_func ("/connection/identity-reset", test_identity_reset);
  g_test_add_func ("/connection/loading-guard", test_loading_guard);
  g_test_add_func ("/connection/cancel-then-uncheck-quit", test_cancel_then_uncheck_quit);
  g_test_add_func ("/connection/empty-password", test_empty_password);
  g_test_add_data_func ("/connection/missing-lookup-legacy-store", GINT_TO_POINTER (0), test_legacy_password_store);
  g_test_add_data_func ("/connection/failed-lookup-legacy-store", GINT_TO_POINTER (1), test_legacy_password_store);
  g_test_add_func ("/connection/cancel-edit-close", test_cancel_edit_close);
  g_test_add_func ("/connection/manual-password-identity-clear", test_manual_password_identity_clear);
  return g_test_run ();
}
