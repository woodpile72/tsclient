/* MRU entries are owned UTF-8 strings. Files are JSON arrays; legacy line
 * lists and the former writer's escaped framing are read for migration. */
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <string.h>
#include "mrulist.h"
#include "support.h"

#define MRU_LIMIT 11
#define MRU_FILE_LIMIT (1024 * 1024)

static gchar *mru_path (void)
{
  gchar *home = tsc_home_path ();
  gchar *path = g_build_filename (home, "mru.tsc", NULL);
  g_free (home);
  return path;
}

static gboolean parse_json (GSList **list, const gchar *data)
{
  JsonParser *parser = json_parser_new ();
  GSList *temp = NULL;
  gboolean ok = FALSE;
  /* Do not silently truncate strings containing JSON NUL escapes. */
  for (const char *p = data; *p; p++) {
    if (*p == '\\' && p[1]) {
      if (g_str_has_prefix (p, "\\u0000")) goto done;
      p++;
    }
  }
  if (!json_parser_load_from_data (parser, data, -1, NULL)) goto done;
  JsonNode *root = json_parser_get_root (parser);
  if (!root || !JSON_NODE_HOLDS_ARRAY (root)) goto done;
  JsonArray *array = json_node_get_array (root);
  for (guint i = 0; i < json_array_get_length (array); i++) {
    JsonNode *node = json_array_get_element (array, i);
    if (!JSON_NODE_HOLDS_VALUE (node) || json_node_get_value_type (node) != G_TYPE_STRING)
      goto done;
    const char *value = json_node_get_string (node);
    if (!g_utf8_validate (value, -1, NULL)) goto done;
    if (i < MRU_LIMIT) temp = g_slist_prepend (temp, g_strdup (value));
  }
  *list = g_slist_reverse (temp);
  temp = NULL;
  ok = TRUE;
done:
  g_slist_free_full (temp, g_free);
  g_object_unref (parser);
  return ok;
}

/* Exact framing emitted by the broken writer. Decode only individual values,
 * never the whole document; normal JSON is always tried first. */
static gboolean parse_broken_writer (GSList **list, const gchar *data)
{
  if (!g_str_has_prefix (data, "[\\n")) return FALSE;
  const char *p = data + 3;
  GSList *temp = NULL;
  guint count = 0;
  if (!strcmp (p, "\\n]\\n")) { *list = NULL; return TRUE; }
  while (*p) {
    if (!g_str_has_prefix (p, "  \\\"")) goto bad;
    p += 4;
    const char *start = p;
    while (*p && !g_str_has_prefix (p, "\\\",\\n") &&
                 !g_str_has_prefix (p, "\\\"\\n]\\n")) {
      if (*p == '\\') {
        p++;
        if (!*p) goto bad;
        if (*p >= '0' && *p <= '3') {
          if (!(p[1] >= '0' && p[1] <= '7' && p[2] >= '0' && p[2] <= '7')) goto bad;
          if (p[0] == '0' && p[1] == '0' && p[2] == '0') goto bad;
          p += 2;
        } else if (!strchr ("\\\"bfnrtv", *p)) goto bad;
      } else if ((guchar)*p < 32 || *p == '"') goto bad;
      p++;
    }
    if (!*p) goto bad;
    gchar *escaped = g_strndup (start, p - start);
    gchar *value = g_strcompress (escaped);
    g_free (escaped);
    if (!g_utf8_validate (value, -1, NULL)) { g_free (value); goto bad; }
    if (count++ < MRU_LIMIT) temp = g_slist_prepend (temp, value);
    else g_free (value);
    if (g_str_has_prefix (p, "\\\"\\n]\\n")) {
      if (p[7]) goto bad;
      *list = g_slist_reverse (temp);
      return TRUE;
    }
    p += 5;
  }
bad:
  g_slist_free_full (temp, g_free);
  return FALSE;
}

static int mru_file_to_list (GSList **list)
{
  if (!list) return 1;
  gchar *path = mru_path ();
  FILE *file = g_fopen (path, "rb");
  g_free (path);
  if (!file) return 1;
  gchar *data = g_malloc (MRU_FILE_LIMIT + 2);
  gsize length = fread (data, 1, MRU_FILE_LIMIT + 1, file);
  gboolean ok = !ferror (file) && length <= MRU_FILE_LIMIT;
  fclose (file);
  data[length] = 0;
  GSList *temp = NULL;
  if (!ok || memchr (data, 0, length) || !g_utf8_validate (data, length, NULL)) {
    ok = FALSE; goto done;
  }
  const char *start = data;
  if (g_str_has_prefix (start, "\357\273\277")) start += 3;
  while (g_ascii_isspace (*start)) start++;
  if (*start == '[') {
    ok = parse_json (&temp, start) || parse_broken_writer (&temp, start);
  } else {
    gchar **lines = g_strsplit_set (start, "\r\n", -1);
    guint count = 0;
    for (guint i = 0; lines[i] && count < MRU_LIMIT; i++) {
      if (*lines[i]) { temp = g_slist_prepend (temp, g_strdup (lines[i])); count++; }
    }
    temp = g_slist_reverse (temp);
    g_strfreev (lines);
  }
done:
  g_free (data);
  if (ok) { g_slist_free_full (*list, g_free); *list = temp; }
  else g_slist_free_full (temp, g_free);
  return ok ? 0 : 1;
}

static int mru_list_to_file (GSList **list)
{
  if (!list) return 1;
  JsonBuilder *builder = json_builder_new ();
  gboolean ok = FALSE;
  json_builder_begin_array (builder);
  guint count = 0;
  for (GSList *p = *list; p && count < MRU_LIMIT; p = p->next, count++) {
    if (!p->data || !g_utf8_validate (p->data, -1, NULL)) goto done;
    json_builder_add_string_value (builder, p->data);
  }
  json_builder_end_array (builder);
  JsonNode *root = json_builder_get_root (builder);
  JsonGenerator *generator = json_generator_new ();
  json_generator_set_root (generator, root);
  json_generator_set_pretty (generator, TRUE);
  gsize length;
  gchar *data = json_generator_to_data (generator, &length);
  gchar *path = mru_path ();
  if (length <= MRU_FILE_LIMIT)
    ok = g_file_set_contents_full (path, data, length,
           G_FILE_SET_CONTENTS_CONSISTENT | G_FILE_SET_CONTENTS_DURABLE, 0600, NULL);
  g_free (path); g_free (data);
  g_object_unref (generator); json_node_free (root);
done:
  g_object_unref (builder);
  return ok ? 0 : 1;
}

int mru_add_server (const char *server_name)
{
  if (!server_name || !g_utf8_validate (server_name, -1, NULL) ||
      strlen (server_name) > MRU_FILE_LIMIT) return 1;
  GSList *list = NULL, *updated = NULL;
  mru_file_to_list (&list);
  guint count = 1;
  for (GSList *p = list; p && count < MRU_LIMIT; p = p->next) {
    if (strcmp (p->data, server_name)) {
      updated = g_slist_prepend (updated, g_strdup (p->data));
      count++;
    }
  }
  updated = g_slist_prepend (g_slist_reverse (updated), g_strdup (server_name));
  int result = mru_list_to_file (&updated);
  g_slist_free_full (list, g_free);
  g_slist_free_full (updated, g_free);
  return result;
}

int mru_to_screen (GtkWidget *main_window)
{
  GSList *list = NULL;
  mru_file_to_list (&list);
  GtkWidget *widget = lookup_widget (main_window, "cboComputer");
  if (widget) {
    tsc_dropdown_clear (widget);
    tsc_dropdown_append (widget, _("Recent"));
    for (GSList *p = list; p; p = p->next)
      tsc_dropdown_append (widget, p->data);
    tsc_dropdown_set_selected (widget, 0);
    gtk_widget_set_sensitive (widget, list != NULL);
  }
  g_slist_free_full (list, g_free);
  return 0;
}
