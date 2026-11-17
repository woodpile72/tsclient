/* Real libsecret lifecycle. Run ONLY via tests/run-credentials-keyring.sh,
 * which creates a private bus, daemon, HOME and XDG directories. Synthetic
 * secrets only; assertions deliberately do not print secret values. */
#include "credentials.h"
#include <string.h>

static GAsyncResult *result;
static void ready (GObject *source, GAsyncResult *r, gpointer data)
{
  (void) data;
  g_assert_null (source);
  g_assert_null (result);
  result = g_object_ref (r);
}
static void await_result (void)
{
  while (!result) g_main_context_iteration (NULL, TRUE);
}
static void store (rdp_file *r)
{
  GError *error = NULL;
  tsc_credentials_store_async (r, NULL, ready, NULL);
  await_result ();
  g_assert_true (tsc_credentials_store_finish (result, &error));
  g_assert_no_error (error);
  g_clear_object (&result);
}
static void lookup (rdp_file *r, const gchar *expected)
{
  GError *error = NULL;
  tsc_credentials_lookup_async (r, NULL, ready, NULL);
  await_result ();
  gchar *password = tsc_credentials_lookup_finish (result, &error);
  g_assert_no_error (error);
  g_assert_true (g_strcmp0 (password, expected) == 0);
  tsc_credentials_password_free (password);
  g_clear_object (&result);
}
static void clear (rdp_file *r, gboolean expected)
{
  GError *error = NULL;
  tsc_credentials_clear_async (r, NULL, ready, NULL);
  await_result ();
  gboolean removed = tsc_credentials_clear_finish (result, &error);
  g_assert_no_error (error);
  g_assert_true (removed == expected);
  g_clear_object (&result);
}
static gboolean deadline (gpointer data)
{
  (void) data;
  g_error ("Isolated keyring test timed out");
  return G_SOURCE_REMOVE;
}
int main (void)
{
  /* This guard prevents accidental direct execution under the user's session. */
  if (g_strcmp0 (g_getenv ("TSC_TEST_PRIVATE_KEYRING"), "1") != 0 ||
      !g_getenv ("DBUS_SESSION_BUS_ADDRESS") ||
      !g_getenv ("HOME") || !strstr (g_getenv ("HOME"), "/tsc-private-keyring."))
    g_error ("Run via tests/run-credentials-keyring.sh only");
  guint watchdog = g_timeout_add_seconds (20, deadline, NULL);
  rdp_file r = {0};
  r.protocol = 2;
  r.full_address = "synthetic.invalid:3389";
  r.username = "synthetic-user";
  r.domain = "SYNTHETIC";
  r.password = "synthetic-credential-for-isolated-tests";
  lookup (&r, NULL);
  clear (&r, FALSE);
  store (&r);
  lookup (&r, r.password);
  /* Each of the four attributes must independently distinguish identities. */
  rdp_file other = r;
  other.protocol++; lookup (&other, NULL); other = r;
  other.full_address = "other.invalid"; lookup (&other, NULL); other = r;
  other.username = "other-user"; lookup (&other, NULL); other = r;
  other.domain = "OTHER"; lookup (&other, NULL);
  r.password = "synthetic-replacement";
  store (&r); lookup (&r, r.password);
  r.password = "";
  store (&r); lookup (&r, "");
  clear (&r, TRUE); lookup (&r, NULL); clear (&r, FALSE);
  r.username = NULL; r.domain = NULL;
  store (&r);
  r.username = ""; r.domain = "";
  lookup (&r, ""); clear (&r, TRUE);
  GCancellable *cancel = g_cancellable_new ();
  g_cancellable_cancel (cancel);
  tsc_credentials_store_async (&r, cancel, ready, NULL);
  await_result ();
  GError *error = NULL;
  g_assert_false (tsc_credentials_store_finish (result, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error); g_clear_object (&result); g_object_unref (cancel);
  lookup (&r, NULL);
  g_source_remove (watchdog);
  g_print ("PASS: isolated real libsecret store/update/lookup/clear, all key attributes, empty secrets, NULL normalization, cancellation.\n");
  return 0;
}
