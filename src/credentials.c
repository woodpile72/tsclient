#include "credentials.h"

#include <libsecret/secret.h>

static const SecretSchema credentials_schema = {
  .name = "org.tsclient.Password.v1",
  .flags = SECRET_SCHEMA_NONE,
  .attributes = {
    { "protocol", SECRET_SCHEMA_ATTRIBUTE_STRING },
    { "server", SECRET_SCHEMA_ATTRIBUTE_STRING },
    { "username", SECRET_SCHEMA_ATTRIBUTE_STRING },
    { "domain", SECRET_SCHEMA_ATTRIBUTE_STRING },
    { NULL, 0 }
  }
};

typedef struct
{
  GHashTable *attributes;
  gchar *password;
} CredentialsRequest;

void
tsc_credentials_password_free (gchar *password)
{
  if (password != NULL)
    secret_password_free (password);
}

static void
credentials_request_free (gpointer data)
{
  CredentialsRequest *request = data;
  g_hash_table_unref (request->attributes);
  tsc_credentials_password_free (request->password);
  g_free (request);
}

static gboolean
valid_optional_string (const gchar *value)
{
  return value == NULL || g_utf8_validate (value, -1, NULL);
}

/* Return an owned task even on validation failure, so callers always unref it.
 * A NULL task-data pointer indicates that completion has already been queued. */
static GTask *
credentials_task_new (const rdp_file *snapshot, gboolean store,
                      GCancellable *cancellable, GAsyncReadyCallback callback,
                      gpointer user_data, gpointer source_tag)
{
  GTask *task = g_task_new (NULL, cancellable, callback, user_data);
  CredentialsRequest *request;
  gchar *protocol;

  g_task_set_source_tag (task, source_tag);
  if (g_task_return_error_if_cancelled (task))
    return task;

  if (snapshot == NULL || snapshot->protocol < 0 ||
      snapshot->full_address == NULL || snapshot->full_address[0] == '\0' ||
      !valid_optional_string (snapshot->full_address) ||
      !valid_optional_string (snapshot->username) ||
      !valid_optional_string (snapshot->domain) ||
      (store && (snapshot->password == NULL ||
                 !valid_optional_string (snapshot->password))))
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                               "Invalid credential identity or password");
      return task;
    }

  request = g_new0 (CredentialsRequest, 1);
  protocol = g_strdup_printf ("%d", snapshot->protocol);
  request->attributes = secret_attributes_build (
      &credentials_schema,
      "protocol", protocol,
      "server", snapshot->full_address,
      "username", snapshot->username != NULL ? snapshot->username : "",
      "domain", snapshot->domain != NULL ? snapshot->domain : "",
      NULL);
  g_free (protocol);
  if (store)
    request->password = g_strdup (snapshot->password);
  g_task_set_task_data (task, request, credentials_request_free);
  return task;
}

static void
store_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  GError *error = NULL;
  gboolean stored = secret_password_store_finish (result, &error);
  (void) source;

  if (error != NULL)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, stored);
  g_object_unref (task);
}

void
tsc_credentials_store_async (const rdp_file *snapshot,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback, gpointer user_data)
{
  GTask *task = credentials_task_new (snapshot, TRUE, cancellable, callback,
                                     user_data, tsc_credentials_store_async);
  CredentialsRequest *request = g_task_get_task_data (task);
  if (request != NULL)
    secret_password_storev (&credentials_schema, request->attributes,
                            SECRET_COLLECTION_DEFAULT, "tsclient password",
                            request->password, cancellable, store_done,
                            g_object_ref (task));
  g_object_unref (task);
}

gboolean
tsc_credentials_store_finish (GAsyncResult *result, GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (
      result, tsc_credentials_store_async), FALSE);
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
lookup_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  GError *error = NULL;
  gchar *password = secret_password_lookup_finish (result, &error);
  (void) source;

  if (error != NULL)
    {
      tsc_credentials_password_free (password);
      g_task_return_error (task, error);
    }
  else
    g_task_return_pointer (task, password,
                           (GDestroyNotify) tsc_credentials_password_free);
  g_object_unref (task);
}

void
tsc_credentials_lookup_async (const rdp_file *snapshot,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback, gpointer user_data)
{
  GTask *task = credentials_task_new (snapshot, FALSE, cancellable, callback,
                                     user_data, tsc_credentials_lookup_async);
  CredentialsRequest *request = g_task_get_task_data (task);
  if (request != NULL)
    secret_password_lookupv (&credentials_schema, request->attributes,
                             cancellable, lookup_done, g_object_ref (task));
  g_object_unref (task);
}

gchar *
tsc_credentials_lookup_finish (GAsyncResult *result, GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);
  g_return_val_if_fail (g_async_result_is_tagged (
      result, tsc_credentials_lookup_async), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
clear_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  GError *error = NULL;
  gboolean cleared = secret_password_clear_finish (result, &error);
  (void) source;

  if (error != NULL)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, cleared);
  g_object_unref (task);
}

void
tsc_credentials_clear_async (const rdp_file *snapshot,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback, gpointer user_data)
{
  GTask *task = credentials_task_new (snapshot, FALSE, cancellable, callback,
                                     user_data, tsc_credentials_clear_async);
  CredentialsRequest *request = g_task_get_task_data (task);
  if (request != NULL)
    secret_password_clearv (&credentials_schema, request->attributes,
                            cancellable, clear_done, g_object_ref (task));
  g_object_unref (task);
}

gboolean
tsc_credentials_clear_finish (GAsyncResult *result, GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (
      result, tsc_credentials_clear_async), FALSE);
  return g_task_propagate_boolean (G_TASK (result), error);
}
