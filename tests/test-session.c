#include "session.h"

#include <errno.h>
#include <signal.h>
#include <glib/gstdio.h>

typedef struct
{
  GMainLoop *loop;
  GError *error;
  gboolean success;
  guint ticks;
} Result;

static void
completed (GObject *source, GAsyncResult *result, gpointer data)
{
  Result *r = data;
  g_assert_null (source);
  r->success = tsc_session_finish (result, &r->error);
  g_main_loop_quit (r->loop);
}

static gboolean
tick (gpointer data)
{
  Result *r = data;
  r->ticks++;
  return G_SOURCE_CONTINUE;
}

static gboolean
deadline (gpointer data)
{
  (void) data;
  g_error ("Session test timed out");
  return G_SOURCE_REMOVE;
}

static void
run (Result *r, const gchar * const *argv, GCancellable *cancel)
{
  guint timer, watchdog;
  r->loop = g_main_loop_new (NULL, FALSE);
  timer = g_timeout_add (10, tick, r);
  watchdog = g_timeout_add_seconds (10, deadline, NULL);
  tsc_session_start (argv, cancel, completed, r);
  g_main_loop_run (r->loop);
  g_source_remove (timer);
  g_source_remove (watchdog);
  g_main_loop_unref (r->loop);
}

static void
test_responsive (void)
{
  Result r = {0};
  const gchar *argv[] = {"/bin/sh", "-c", "sleep 0.2; printf secret", NULL};
  run (&r, argv, NULL);
  g_assert_true (r.success);
  g_assert_no_error (r.error);
  g_assert_cmpuint (r.ticks, >=, 5);
}

static void
test_failures (void)
{
  const gchar *scripts[] = {"exit 7", "printf password-secret >&2; exit 7",
                             "kill -TERM $$"};
  guint i;
  for (i = 0; i < G_N_ELEMENTS (scripts); i++)
    {
      Result r = {0};
      const gchar *argv[] = {"/bin/sh", "-c", scripts[i], "session-test", "/p:password-secret", NULL};
      run (&r, argv, NULL);
      g_assert_false (r.success);
      g_assert_error (r.error, G_IO_ERROR, G_IO_ERROR_FAILED);
      g_assert_null (strstr (r.error->message, "password-secret"));
      if (i < 2)
        g_assert_nonnull (strstr (r.error->message, "7"));
      g_clear_error (&r.error);
    }
}

static void
test_spawn_failure (void)
{
  Result r = {0};
  const gchar *argv[] = {"/nonexistent/password-secret/client", NULL};
  run (&r, argv, NULL);
  g_assert_false (r.success);
  g_assert_nonnull (r.error);
  g_assert_null (strstr (r.error->message, "password-secret"));
  g_clear_error (&r.error);
}

static void
test_large_stderr (void)
{
  Result r = {0};
  const gchar *argv[] = {"/bin/sh", "-c",
    "i=0; while [ $i -lt 20000 ]; do "
    "printf 'password-secret-012345678901234567890123456789\\n' >&2; "
    "i=$((i+1)); done; exit 9", "session-test", "-ppassword-secret", NULL};
  run (&r, argv, NULL);
  g_assert_false (r.success);
  g_assert_error (r.error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpuint (strlen (r.error->message), <, 64 * 1024 + 128);
  g_assert_null (strstr (r.error->message, "password-secret"));
  g_clear_error (&r.error);
}

typedef struct
{
  GCancellable *cancel;
  const gchar *pidfile;
  gint pid;
} Cancellation;

static gboolean
cancel_when_started (gpointer data)
{
  Cancellation *c = data;
  gchar *contents = NULL;
  if (!g_file_get_contents (c->pidfile, &contents, NULL, NULL))
    return G_SOURCE_CONTINUE;
  c->pid = (gint) g_ascii_strtoll (contents, NULL, 10);
  g_free (contents);
  if (c->pid <= 0)
    return G_SOURCE_CONTINUE;
  g_cancellable_cancel (c->cancel);
  return G_SOURCE_REMOVE;
}

static void
test_cancel (gconstpointer data)
{
  Result r = {0};
  GError *error = NULL;
  gchar *dir = g_dir_make_tmp ("tsc-session-XXXXXX", &error);
  gchar *pidfile;
  GCancellable *cancel = g_cancellable_new ();
  Cancellation c;
  const gchar *argv[] = {"/bin/sh", "-c", data, "session-test", NULL, NULL};
  g_assert_no_error (error);
  pidfile = g_build_filename (dir, "pid", NULL);
  argv[4] = pidfile;
  c = (Cancellation) {cancel, pidfile, 0};
  g_timeout_add (10, cancel_when_started, &c);
  run (&r, argv, cancel);
  g_assert_false (r.success);
  g_assert_error (r.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_assert_cmpint (c.pid, >, 0);
  errno = 0;
  g_assert_cmpint (kill (c.pid, 0), ==, -1);
  g_assert_cmpint (errno, ==, ESRCH);
  g_clear_error (&r.error);
  g_object_unref (cancel);
  g_remove (pidfile);
  g_rmdir (dir);
  g_free (pidfile);
  g_free (dir);
}

static void
test_precancelled (void)
{
  Result r = {0};
  GCancellable *cancel = g_cancellable_new ();
  const gchar *argv[] = {"/bin/sh", "-c", "exit 0", NULL};
  g_cancellable_cancel (cancel);
  run (&r, argv, cancel);
  g_assert_false (r.success);
  g_assert_error (r.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&r.error);
  g_object_unref (cancel);
}

static void
test_redaction (void)
{
  const gchar *options[] = {"-psecret with spaces!", "-p", "/p:secret with spaces!",
                            "-password"};
  guint i;
  for (i = 0; i < G_N_ELEMENTS (options); i++)
    {
      Result r = {0};
      const gchar *argv[] = {"/bin/sh", "-c",
        "case \"$1\" in -p|-password) pw=$2;; -p*) pw=${1#-p};; /p:*) pw=${1#/p:};; esac; "
        "printf 'connection refused: %s / %s \\377\\000tail\\n' \"$pw\" \"$pw\" >&2; exit 23",
        "argv-marker-not-for-errors", options[i], "secret with spaces!", NULL};
      run (&r, argv, NULL);
      g_assert_error (r.error, G_IO_ERROR, G_IO_ERROR_FAILED);
      g_assert_nonnull (strstr (r.error->message, "status 23"));
      g_assert_nonnull (strstr (r.error->message,
                               "connection refused: [REDACTED] / [REDACTED]"));
      g_assert_nonnull (strstr (r.error->message, "tail"));
      g_assert_null (strstr (r.error->message, "secret with spaces!"));
      g_assert_null (strstr (r.error->message, "argv-marker-not-for-errors"));
      g_assert_true (g_utf8_validate (r.error->message, -1, NULL));
      g_clear_error (&r.error);
    }
}

static void
test_invalid_password (void)
{
  Result r = {0};
  const gchar *argv[] = {"/bin/sh", "-c",
    "printf 'auth failed: %s' \"${1#/p:}\" >&2; exit 4",
    "session-test", "/p:raw\xffsecret", NULL};
  run (&r, argv, NULL);
  g_assert_error (r.error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_nonnull (strstr (r.error->message, "auth failed: [REDACTED]"));
  g_assert_null (strstr (r.error->message, "raw"));
  g_assert_true (g_utf8_validate (r.error->message, -1, NULL));
  g_clear_error (&r.error);
}

static void
test_truncated_password (void)
{
  Result r = {0};
  const gchar *argv[] = {"/bin/sh", "-c",
    "printf '%65530s' '' >&2; printf '%s' \"${1#/p:}\" >&2; exit 8",
    "session-test", "/p:secret-at-boundary", NULL};
  run (&r, argv, NULL);
  g_assert_error (r.error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_nonnull (strstr (r.error->message, "status 8"));
  g_assert_nonnull (strstr (r.error->message, "[stderr truncated]"));
  g_assert_null (strstr (r.error->message, "secret"));
  g_assert_true (g_utf8_validate (r.error->message, -1, NULL));
  g_assert_cmpuint (strlen (r.error->message), <, 64 * 1024 + 128);
  g_clear_error (&r.error);
}

static void
test_inherited_stderr (void)
{
  Result r = {0};
  GError *error = NULL;
  gchar *dir = g_dir_make_tmp ("tsc-descendant-XXXXXX", &error);
  gchar *pidfile, *contents = NULL;
  gint64 started;
  const gchar *argv[] = {"/bin/sh", "-c",
    "sleep 5 & echo $! > \"$1\"; printf 'connection refused' >&2; exit 6",
    "session-test", NULL, NULL};
  g_assert_no_error (error);
  pidfile = g_build_filename (dir, "pid", NULL);
  argv[4] = pidfile;
  started = g_get_monotonic_time ();
  run (&r, argv, NULL);
  g_assert_cmpint (g_get_monotonic_time () - started, <, 2 * G_TIME_SPAN_SECOND);
  g_assert_error (r.error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_nonnull (strstr (r.error->message, "status 6"));
  g_assert_nonnull (strstr (r.error->message, "connection refused"));
  g_assert_true (g_file_get_contents (pidfile, &contents, NULL, &error));
  g_assert_no_error (error);
  gint pid = (gint) g_ascii_strtoll (contents, NULL, 10);
  g_assert_cmpint (pid, >, 0);
  kill (pid, SIGTERM);
  g_free (contents);
  g_clear_error (&r.error);
  g_remove (pidfile);
  g_rmdir (dir);
  g_free (pidfile);
  g_free (dir);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/session/responsive", test_responsive);
  g_test_add_func ("/session/failures", test_failures);
  g_test_add_func ("/session/spawn-failure", test_spawn_failure);
  g_test_add_func ("/session/large-stderr", test_large_stderr);
  g_test_add_data_func ("/session/cancel-read",
                        "echo $$ > \"$1\"; exec sleep 30", test_cancel);
  g_test_add_data_func ("/session/cancel-wait",
                        "exec 2>&-; echo $$ > \"$1\"; exec sleep 30", test_cancel);
  g_test_add_func ("/session/precancelled", test_precancelled);
  g_test_add_func ("/session/redaction", test_redaction);
  g_test_add_func ("/session/invalid-password", test_invalid_password);
  g_test_add_func ("/session/truncated-password", test_truncated_password);
  g_test_add_func ("/session/inherited-stderr", test_inherited_stderr);
  return g_test_run ();
}
