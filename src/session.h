#ifndef TSC_SESSION_H
#define TSC_SESSION_H

#include <gio/gio.h>

G_BEGIN_DECLS

/* Run a NULL-terminated argv directly, without a shell. argv is copied before
 * returning. The callback runs in the calling thread's thread-default context;
 * its source_object is NULL. stdout is discarded. stderr is drained, retaining
 * at most 64 KiB. Failure diagnostics include bounded, valid UTF-8 stderr with
 * exact password values from -pVALUE, -p VALUE, -password VALUE and /p:value replaced by
 * [REDACTED]. argv itself is never formatted into errors. Other secrets or
 * transformed password values cannot be inferred and are not redacted.
 * After direct-child exit, stderr drainage has a 50 ms grace period so an
 * inherited pipe held open by descendants cannot delay completion indefinitely.
 *
 * Cancellation force-terminates the direct child, not its descendants, and
 * reaps it before completing with G_IO_ERROR_CANCELLED. All process creation,
 * I/O and waiting happen off the calling thread. A cancelled operation can
 * therefore take a short time to complete while the child is reaped.
 */
void tsc_session_start (const gchar * const *argv,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer data);

/* TRUE only for exit status zero. Failures use G_IO_ERROR; nonzero exit and
 * signal termination use G_IO_ERROR_FAILED with status and redacted diagnostics. */
gboolean tsc_session_finish (GAsyncResult *result, GError **error);

G_END_DECLS

#endif
