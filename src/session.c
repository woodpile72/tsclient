#include "session.h"

#include <string.h>

#define STDERR_LIMIT (64 * 1024)

/* This state belongs to a private worker context. Only cancel_child runs on
 * another thread, and it touches only immutable, thread-safe object refs. */
typedef struct
{
  GSubprocess *child;
  GCancellable *io_cancel;
  GMainLoop *loop;
  GByteArray *prefix;
  GSource *grace;
  gchar chunk[4096];
  gboolean read_done, wait_done, waited, read_failed, truncated;
} Worker;

static void
maybe_done (Worker *w)
{
  if (w->read_done && w->wait_done)
    g_main_loop_quit (w->loop);
}

static void
cancel_child (GCancellable *cancellable, gpointer data)
{
  Worker *w = data;
  (void) cancellable;
  g_subprocess_force_exit (w->child);
  g_cancellable_cancel (w->io_cancel);
}

static gboolean
stop_drain (gpointer data)
{
  Worker *w = data;
  g_cancellable_cancel (w->io_cancel);
  return G_SOURCE_REMOVE;
}

static void read_next (Worker *w);

static void
read_ready (GObject *source, GAsyncResult *result, gpointer data)
{
  Worker *w = data;
  GError *error = NULL;
  gssize count = g_input_stream_read_finish (G_INPUT_STREAM (source), result, &error);
  if (count > 0)
    {
      gsize keep = MIN ((gsize) count, STDERR_LIMIT - w->prefix->len);
      g_byte_array_append (w->prefix, (const guint8 *) w->chunk, keep);
      w->truncated |= keep < (gsize) count;
      read_next (w);
      return;
    }
  if (count < 0)
    {
      w->truncated = TRUE;
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          w->read_failed = TRUE;
          g_subprocess_force_exit (w->child);
        }
      g_clear_error (&error);
    }
  w->read_done = TRUE;
  maybe_done (w);
}

static void
read_next (Worker *w)
{
  g_input_stream_read_async (g_subprocess_get_stderr_pipe (w->child),
                             w->chunk, sizeof w->chunk, G_PRIORITY_DEFAULT,
                             w->io_cancel, read_ready, w);
}

static void
wait_ready (GObject *source, GAsyncResult *result, gpointer data)
{
  Worker *w = data;
  w->waited = g_subprocess_wait_finish (G_SUBPROCESS (source), result, NULL);
  w->wait_done = TRUE;
  /* Drain buffered diagnostics, but never wait indefinitely for descendants
   * which inherited stderr. The wait itself is uncancellable and reaps child. */
  if (!w->read_done)
    {
      w->grace = g_timeout_source_new (50);
      g_source_set_callback (w->grace, stop_drain, w, NULL);
      g_source_attach (w->grace, g_main_loop_get_context (w->loop));
    }
  maybe_done (w);
}

static gchar *
diagnostics (Worker *w, const gchar * const *argv)
{
  gsize len = w->prefix->len, i, j;
  gboolean *masked = g_new0 (gboolean, len);
  GString *redacted = g_string_new (NULL);
  gchar *valid;

  /* Mask raw bytes before UTF-8 repair, including overlapping passwords and
   * a password prefix cut off by the capture limit or interrupted drainage. */
  for (i = 1; argv[i] != NULL; i++)
    {
      const gchar *password = NULL;
      if (strcmp (argv[i], "-p") == 0 || strcmp (argv[i], "-password") == 0)
        {
          if (argv[i + 1] != NULL)
            password = argv[++i];
        }
      else if (g_str_has_prefix (argv[i], "-p"))
        password = argv[i] + 2;
      else if (g_str_has_prefix (argv[i], "/p:"))
        password = argv[i] + 3;
      if (password == NULL || *password == '\0')
        continue;
      gsize plen = strlen (password);
      for (j = 0; j < len; j++)
        {
          gsize n = MIN (plen, len - j);
          if ((n == plen || w->truncated) &&
              memcmp (w->prefix->data + j, password, n) == 0)
            {
              gsize k;
              for (k = j; k < j + n; k++)
                masked[k] = TRUE;
            }
        }
    }
  for (j = 0; j < len; j++)
    {
      if (masked[j])
        {
          if (j == 0 || !masked[j - 1])
            g_string_append (redacted, "[REDACTED]");
        }
      else
        g_string_append_c (redacted, w->prefix->data[j] == 0
                                      ? ' ' : w->prefix->data[j]);
    }
  g_free (masked);
  valid = g_utf8_make_valid (redacted->str, redacted->len);
  g_string_free (redacted, TRUE);
  if (strlen (valid) > STDERR_LIMIT)
    {
      gchar *end = valid + STDERR_LIMIT;
      while ((*end & 0xc0) == 0x80)
        end--;
      *end = '\0';
      w->truncated = TRUE;
    }
  return valid;
}

static void
session_worker (GTask *task, gpointer source_object, gpointer task_data,
                GCancellable *cancellable)
{
  const gchar * const *argv = task_data;
  Worker w = {0};
  GError *error = NULL;
  GMainContext *context;
  gulong cancel_id = 0;
  gchar *detail, *suffix;

  (void) source_object;
  if (g_task_return_error_if_cancelled (task))
    return;
  w.child = g_subprocess_newv (argv, G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                    G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
  if (w.child == NULL)
    {
      /* GLib's spawn message may contain argv[0]; preserve only its code. */
      gint code = error != NULL && error->domain == G_IO_ERROR
                    ? error->code : G_IO_ERROR_FAILED;
      g_clear_error (&error);
      g_task_return_new_error (task, G_IO_ERROR, code,
                               "Unable to start session process.");
      return;
    }

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);
  w.loop = g_main_loop_new (context, FALSE);
  w.io_cancel = g_cancellable_new ();
  w.prefix = g_byte_array_sized_new (STDERR_LIMIT);
  if (cancellable != NULL)
    cancel_id = g_cancellable_connect (cancellable, G_CALLBACK (cancel_child),
                                       &w, NULL);
  read_next (&w);
  g_subprocess_wait_async (w.child, NULL, wait_ready, &w);
  g_main_loop_run (w.loop);
  if (cancel_id != 0)
    g_cancellable_disconnect (cancellable, cancel_id);
  if (w.grace != NULL)
    {
      g_source_destroy (w.grace);
      g_source_unref (w.grace);
    }

  detail = diagnostics (&w, argv);
  suffix = g_strdup_printf ("%s%s%s", *detail ? "\n" : "", detail,
                             w.truncated ? "\n[stderr truncated]" : "");
  if (g_task_return_error_if_cancelled (task))
    ;
  else if (!w.waited)
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Unable to wait for session process.%s", suffix);
  else if (g_subprocess_get_if_exited (w.child))
    {
      gint status = g_subprocess_get_exit_status (w.child);
      if (status != 0)
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                                 "Session process exited with status %d.%s", status, suffix);
      else if (w.read_failed)
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                                 "Unable to read session diagnostics.%s", suffix);
      else
        g_task_return_boolean (task, TRUE);
    }
  else
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Session process terminated by signal %d.%s",
                             g_subprocess_get_term_sig (w.child), suffix);
  g_free (suffix);
  g_free (detail);
  g_byte_array_unref (w.prefix);
  g_object_unref (w.io_cancel);
  g_object_unref (w.child);
  g_main_loop_unref (w.loop);
  g_main_context_pop_thread_default (context);
  g_main_context_unref (context);
}

void
tsc_session_start (const gchar * const *argv, GCancellable *cancellable,
                   GAsyncReadyCallback callback, gpointer data)
{
  GTask *task;

  g_return_if_fail (argv != NULL && argv[0] != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (NULL, cancellable, callback, data);
  g_task_set_source_tag (task, tsc_session_start);
  g_task_set_task_data (task, g_strdupv ((gchar **) argv),
                        (GDestroyNotify) g_strfreev);
  /* Keep the default return-on-cancel FALSE so cleanup precedes callback. */
  g_task_run_in_thread (task, session_worker);
  g_object_unref (task);
}

gboolean
tsc_session_finish (GAsyncResult *result, GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result, tsc_session_start), FALSE);
  return g_task_propagate_boolean (G_TASK (result), error);
}
