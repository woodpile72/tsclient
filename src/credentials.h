#ifndef TSC_CREDENTIALS_H
#define TSC_CREDENTIALS_H

#include <gio/gio.h>

/* rdpfile.h mentions GtkWidget, but this API does not depend on GTK. */
typedef struct _GtkWidget GtkWidget;
#include "rdpfile.h"

G_BEGIN_DECLS

/* All inputs are copied before the async call returns; the caller may then
 * modify/free the snapshot. Callbacks run in the calling thread's thread-default
 * main context, with a NULL source object. The matching finish function must be
 * called once. A running main context is required to dispatch callbacks.
 *
 * Identity schema: org.tsclient.Password.v1, with four string attributes:
 * protocol (decimal rdp_file.protocol), server (full_address), username, domain.
 * Strings are exact, case-sensitive UTF-8: no trimming or host/port rewriting.
 * NULL username/domain mean "". Server must be nonempty; protocol nonnegative.
 * Lookup/clear ignore password. Store rejects NULL passwords but preserves "".
 * The UI decides whether to save; these functions never persist a profile or
 * use a plaintext fallback. A Secret Service may prompt to unlock a collection.
 *
 * Invalid inputs report G_IO_ERROR_INVALID_ARGUMENT asynchronously. Service
 * errors propagate unchanged; cancellation reports G_IO_ERROR_CANCELLED.
 * Cancellation cannot undo a store/clear already committed by the service.
 */
void tsc_credentials_store_async (const rdp_file *snapshot,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);
gboolean tsc_credentials_store_finish (GAsyncResult *result, GError **error);

void tsc_credentials_lookup_async (const rdp_file *snapshot,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data);
/* NULL without error means no match; "" is a stored empty password.
 * Free the returned secret with tsc_credentials_password_free(). */
gchar *tsc_credentials_lookup_finish (GAsyncResult *result, GError **error);
void tsc_credentials_password_free (gchar *password);

void tsc_credentials_clear_async (const rdp_file *snapshot,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);
/* TRUE means at least one matching unlocked item was removed; FALSE without
 * error means none was removed (libsecret does not clear locked items).
 * All matching unlocked items are removed, including duplicate identities. */
gboolean tsc_credentials_clear_finish (GAsyncResult *result, GError **error);

G_END_DECLS

#endif /* TSC_CREDENTIALS_H */
