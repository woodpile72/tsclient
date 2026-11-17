/* Standalone mock tests; run from the repository root:
 *
 * cc -std=c11 -Wall -Wextra -Werror -g -Isrc \
 *   $(pkg-config --cflags gio-2.0 libsecret-1) \
 *   src/credentials.c tests/test-credentials.c \
 *   $(pkg-config --libs gio-2.0) -o /tmp/test-credentials
 * /tmp/test-credentials
 *
 * Optional: add -fsanitize=address,undefined to the compiler command.
 * Do NOT link libsecret: all referenced secret_* symbols are mocks below.
 * This executable has no Secret Service implementation and cannot contact a
 * real keyring. New, unmocked libsecret calls fail at link time.
 */
#include "credentials.h"
#include <libsecret/secret.h>
#include <stdarg.h>
#include <string.h>

/* Local substitutes for libsecret's non-I/O helpers as well, so the test
 * binary never needs to load libsecret. Copy attributes just as libsecret's
 * string-attribute builder does; dispatch() verifies the resulting identity. */
GHashTable *
secret_attributes_build (const SecretSchema *schema, ...)
{
  GHashTable *attributes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, g_free);
  va_list ap;
  const gchar *name;
  g_assert_cmpstr (schema->name, ==, "org.tsclient.Password.v1");
  g_assert_cmpint (schema->flags, ==, SECRET_SCHEMA_NONE);
  va_start (ap, schema);
  while ((name = va_arg (ap, const gchar *)) != NULL) {
    gboolean found = FALSE;
    for (guint i = 0; schema->attributes[i].name != NULL; i++) {
      if (g_str_equal (name, schema->attributes[i].name)) {
        g_assert_cmpint (schema->attributes[i].type, ==,
                         SECRET_SCHEMA_ATTRIBUTE_STRING);
        found = TRUE;
        break;
      }
    }
    g_assert_true (found);
    g_hash_table_insert (attributes, g_strdup (name),
                         g_strdup (va_arg (ap, const gchar *)));
  }
  va_end (ap);
  return attributes;
}

void
secret_password_free (gchar *password)
{
  if (password != NULL) {
    volatile gchar *p = password;
    gsize size = strlen (password);
    while (size-- > 0) *p++ = 0;
    g_free (password);
  }
}

/* One operation at a time. Modes: success, no match, service failure. */
static int calls, mode, op;
static const gchar *test_password = "";
typedef struct {
  GHashTable *attributes;
  const gchar *password;
  int operation, response;
} MockRequest;
static void mock_request_free (gpointer data)
{
  MockRequest *request = data;
  g_hash_table_unref (request->attributes);
  g_free (request);
}
static const char *borrowed_password;
static GAsyncResult *completed;
static gboolean dispatch (gpointer data)
{
  GTask *task = data;
  MockRequest *request = g_task_get_task_data (task);
  GHashTable *attrs = request->attributes;
  g_assert_cmpstr (g_hash_table_lookup(attrs, "protocol"), ==, "2");
  g_assert_cmpstr (g_hash_table_lookup(attrs, "server"), ==, "Server:3389");
  g_assert_cmpstr (g_hash_table_lookup(attrs, "username"), ==, "");
  g_assert_cmpstr (g_hash_table_lookup(attrs, "domain"), ==, "DOMAIN");
  if (request->operation == 0) g_assert_cmpstr(request->password, ==, test_password);
  if (request->response == 2)
    g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED, "denied");
  else if (request->operation == 1)
    g_task_return_pointer(task, request->response == 1 ? NULL : g_strdup(test_password), (GDestroyNotify) secret_password_free);
  else
    g_task_return_boolean(task, request->response != 1);
  g_object_unref(task);
  return G_SOURCE_REMOVE;
}
static void start(const SecretSchema *schema, GHashTable *a, GCancellable *c, GAsyncReadyCallback cb, gpointer data)
{
  g_assert_cmpstr(schema->name, ==, "org.tsclient.Password.v1");
  g_assert_cmpint(g_hash_table_size(a), ==, 4);
  g_atomic_int_inc (&calls);
  GTask *task = g_task_new (NULL, c, cb, data);
  MockRequest *request = g_new0 (MockRequest, 1);
  request->attributes = g_hash_table_ref (a);
  request->password = borrowed_password;
  request->operation = op;
  request->response = mode;
  g_task_set_task_data (task, request, mock_request_free);
  GSource *idle = g_idle_source_new ();
  g_source_set_callback (idle, dispatch, task, NULL);
  g_source_attach (idle, g_task_get_context (task));
  g_source_unref (idle);
}
void secret_password_storev(const SecretSchema *s, GHashTable *a, const gchar *collection, const gchar *label, const gchar *pw, GCancellable *c, GAsyncReadyCallback cb, gpointer data)
{
  g_assert_cmpstr(collection, ==, SECRET_COLLECTION_DEFAULT);
  g_assert_nonnull(label);
  borrowed_password = pw;
  start(s,a,c,cb,data);
}
void secret_password_lookupv(const SecretSchema *s,GHashTable *a,GCancellable *c,GAsyncReadyCallback cb,gpointer d) {start(s,a,c,cb,d);}
void secret_password_clearv(const SecretSchema *s,GHashTable *a,GCancellable *c,GAsyncReadyCallback cb,gpointer d) {start(s,a,c,cb,d);}
gboolean secret_password_store_finish(GAsyncResult *r,GError **e) {return g_task_propagate_boolean(G_TASK(r),e);}
gboolean secret_password_clear_finish(GAsyncResult *r,GError **e) {return g_task_propagate_boolean(G_TASK(r),e);}
gchar *secret_password_lookup_finish(GAsyncResult *r,GError **e) {return g_task_propagate_pointer(G_TASK(r),e);}
static void done(GObject *source,GAsyncResult *r,gpointer data)
{
  g_assert_null(source); g_assert_true(data == &calls);
  completed = g_object_ref(r);
}
static void invoke(rdp_file *rdp,GCancellable *c)
{
  if(op == 0) tsc_credentials_store_async(rdp,c,done,&calls);
  if(op == 1) tsc_credentials_lookup_async(rdp,c,done,&calls);
  if(op == 2) tsc_credentials_clear_async(rdp,c,done,&calls);
  g_assert_null(completed);
}
static void finish(int expected_error)
{
  GError *error = NULL;
  while(!completed) g_main_context_iteration(NULL,TRUE);
  if(op == 1) {
    gchar *pw = tsc_credentials_lookup_finish(completed,&error);
    if(expected_error >= 0 || mode == 1) g_assert_null(pw);
    else g_assert_cmpstr(pw, ==, test_password);
    tsc_credentials_password_free(pw);
  } else {
    gboolean ok = op == 0 ? tsc_credentials_store_finish(completed,&error) : tsc_credentials_clear_finish(completed,&error);
    g_assert_cmpint(ok, ==, expected_error < 0 && mode != 1);
  }
  if(expected_error >= 0) {g_assert_error(error,G_IO_ERROR,expected_error);}
  else g_assert_no_error(error);
  g_clear_error(&error); g_clear_object(&completed);
}
static void test_operation (gconstpointer data)
{
  op = GPOINTER_TO_INT (data);
  test_password = "";
  for(mode=0;mode<3;mode++) {
    rdp_file r = {0}; r.protocol=2;
    r.full_address=g_strdup("Server:3389"); r.domain=g_strdup("DOMAIN"); r.password=g_strdup("");
    invoke(&r,NULL);
    g_free(r.full_address);g_free(r.domain);g_free(r.password);
    memset(&r,0,sizeof r);
    finish(mode == 2 ? G_IO_ERROR_PERMISSION_DENIED : -1);
  }
  /* Nonempty secrets also survive caller snapshot destruction. */
  mode=0;
  test_password = "synthetic test secret";
  rdp_file nonempty = {0};
  nonempty.protocol = 2;
  nonempty.full_address = g_strdup("Server:3389");
  nonempty.domain = g_strdup("DOMAIN");
  nonempty.password = g_strdup(test_password);
  invoke(&nonempty, NULL);
  g_free(nonempty.full_address);
  g_free(nonempty.domain);
  g_free(nonempty.password);
  memset(&nonempty, 0, sizeof nonempty);
  finish(-1);
  test_password = "";
  int before=calls;
  invoke(NULL,NULL); finish(G_IO_ERROR_INVALID_ARGUMENT);
  rdp_file r={0};r.protocol=2;r.full_address="Server:3389";r.domain="DOMAIN";r.password="";
  GCancellable *c=g_cancellable_new();g_cancellable_cancel(c);
  invoke(&r,c);finish(G_IO_ERROR_CANCELLED);g_assert_cmpint(calls,==,before);
  g_cancellable_reset(c);invoke(&r,c);g_cancellable_cancel(c);finish(G_IO_ERROR_CANCELLED);
  g_object_unref(c);
  r.full_address="";invoke(&r,NULL);finish(G_IO_ERROR_INVALID_ARGUMENT);
  r.full_address="Server:3389";r.domain="\xff";invoke(&r,NULL);finish(G_IO_ERROR_INVALID_ARGUMENT);
  if(op==0) {r.domain="DOMAIN";r.password=NULL;invoke(&r,NULL);finish(G_IO_ERROR_INVALID_ARGUMENT);}
}

/* Each worker owns its context, snapshot and completion; a second thread
 * cancels concurrently with backend completion. Both outcomes are legal, but
 * the callback must run exactly once in the initiating context. */
typedef struct {
  GMainContext *context;
  guint callbacks;
} RaceResult;

static void race_done (GObject *source, GAsyncResult *result, gpointer data)
{
  RaceResult *r = data;
  GError *error = NULL;
  gchar *password = tsc_credentials_lookup_finish (result, &error);
  g_assert_null (source);
  g_assert_true (g_main_context_is_owner (r->context));
  if (error != NULL) {
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_assert_null (password);
  } else {
    g_assert_cmpstr (password, ==, "");
  }
  tsc_credentials_password_free (password);
  g_clear_error (&error);
  r->callbacks++;
}

static gpointer cancel_thread (gpointer data)
{
  g_cancellable_cancel (data);
  return NULL;
}

static gpointer lookup_thread (gpointer unused)
{
  (void) unused;
  GMainContext *context = g_main_context_new ();
  g_main_context_push_thread_default (context);
  for (guint i = 0; i < 50; i++) {
    RaceResult result = {context, 0};
    rdp_file snapshot = {0};
    snapshot.protocol = 2;
    snapshot.full_address = g_strdup ("Server:3389");
    snapshot.domain = g_strdup ("DOMAIN");
    GCancellable *cancel = g_cancellable_new ();
    tsc_credentials_lookup_async (&snapshot, cancel, race_done, &result);
    g_free (snapshot.full_address);
    g_free (snapshot.domain);
    memset (&snapshot, 0, sizeof snapshot);
    GThread *thread = g_thread_new ("cancel", cancel_thread, cancel);
    while (!result.callbacks) g_main_context_iteration (context, TRUE);
    g_thread_join (thread);
    while (g_main_context_iteration (context, FALSE));
    g_assert_cmpuint (result.callbacks, ==, 1);
    g_object_unref (cancel);
  }
  g_main_context_pop_thread_default (context);
  g_main_context_unref (context);
  return NULL;
}

static void test_cancellation_races (void)
{
  /* Immutable mock configuration throughout the threaded test. */
  op = 1; mode = 0; test_password = ""; borrowed_password = NULL;
  GThread *a = g_thread_new ("lookup-a", lookup_thread, NULL);
  GThread *b = g_thread_new ("lookup-b", lookup_thread, NULL);
  g_thread_join (a);
  g_thread_join (b);
}

static gboolean timeout_failed (gpointer data)
{
  (void) data;
  g_error ("Timed out waiting for a credentials callback");
  return G_SOURCE_REMOVE;
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  guint timeout = g_timeout_add_seconds (10, timeout_failed, NULL);
  g_test_add_data_func ("/credentials/store", GINT_TO_POINTER(0), test_operation);
  g_test_add_data_func ("/credentials/lookup", GINT_TO_POINTER(1), test_operation);
  g_test_add_data_func ("/credentials/clear", GINT_TO_POINTER(2), test_operation);
  g_test_add_func ("/credentials/cancellation-races", test_cancellation_races);
  int result = g_test_run ();
  g_source_remove (timeout);
  return result;
}
