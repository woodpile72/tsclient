/* Owned profile storage. init requires fresh/cleared storage; clear accepts
 * initialized or zeroed storage. load requires an initialized destination; copy returns a new allocation.
 * All int APIs return 0 on success, 1 on error. */
#include <glib.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
typedef struct _GtkWidget GtkWidget;
#include "rdpfile.h"

#define PROFILE_LIMIT (1024 * 1024)
typedef struct {
  const char *name, *legacy;
  gsize offset;
  gboolean string;
  int maximum;
} Field;
#define S(n,l) {#n,l,G_STRUCT_OFFSET(rdp_file,n),TRUE,0}
#define I(n,l,m) {#n,l,G_STRUCT_OFFSET(rdp_file,n),FALSE,m}
static const Field fields[] = {
  S(alternate_shell, "alternate shell"),
  I(attach_to_console, "attach to console", 1),
  I(audiomode, "audiomode", 2),
  I(auto_connect, "auto connect", 1),
  I(bitmapcachepersistenable, "bitmapcachepersistenable", 1),
  S(client_hostname, "client hostname"),
  I(compression, "compression", 1),
  S(description, "description"),
  I(desktop_size_id, "desktop size id", 2),
  I(desktopheight, "desktopheight", 65535),
  I(desktopwidth, "desktopwidth", 65535),
  I(disable_client_encryption, "disable client encryption", 1),
  I(disable_full_window_drag, "disable full window drag", 1),
  I(disable_remote_ctrl, "disable remote ctrl", 1),
  I(disable_menu_anims, "disable menu anims", 1),
  I(disable_themes, "disable themes", 1),
  I(disable_wallpaper, "disable wallpaper", 1),
  I(disable_encryption, "disable encryption", 1),
  I(displayconnectionbar, "displayconnectionbar", 1),
  S(domain, "domain"),
  I(enable_alternate_shell, "enable alternate shell", 1),
  I(enable_wm_keys, "enable wm keys", 1),
  I(force_bitmap_updates, "force bitmap updates", 1),
  S(full_address, "full address"),
  I(hide_wm_decorations, "hide wm decorations", 1),
  S(keyboard_language, "keyboard language"),
  I(keyboardhook, "keyboardhook", 2),
  S(local_codepage, "local codepage"),
  I(no_motion_events, "no motion events", 1),
  I(remember_password, "remember password", 1),
  S(password, "password"),
  S(win_password, "win password"),
  S(progman_group, "progman group"),
  I(protocol, "protocol", 5),
  S(proto_file, "protocol file"),
  I(redirectcomports, "redirectcomports", 1),
  I(redirectdrives, "redirectdrives", 1),
  I(redirectprinters, "redirectprinters", 1),
  I(redirectsmartcards, "redirectsmartcards", 1),
  I(screen_mode_id, "screen mode id", 2),
  I(session_bpp, "session bpp", 32),
  I(sync_numlock, "sync numlock", 1),
  S(shell_working_directory, "shell working directory"),
  S(tls_version, "tls version"),
  I(use_backing_store, "use backing store", 1),
  S(username, "username"),
  S(winposstr, "winposstr"),
};
#undef S
#undef I

static gpointer slot (rdp_file *rdp, const Field *f)
{ return (guint8 *) rdp + f->offset; }

int rdp_file_init (rdp_file *rdp)
{
  if (!rdp) return 1;
  memset (rdp, 0, sizeof *rdp);
  for (guint i = 0; i < G_N_ELEMENTS (fields); i++)
    if (fields[i].string) *(char **) slot (rdp, &fields[i]) = g_strdup ("");
  return 0;
}

void rdp_file_clear (rdp_file *rdp)
{
  if (!rdp) return;
  for (guint i = 0; i < G_N_ELEMENTS (fields); i++)
    if (fields[i].string) g_free (*(char **) slot (rdp, &fields[i]));
  memset (rdp, 0, sizeof *rdp);
}

void rdp_file_free (rdp_file *rdp)
{ if (rdp) { rdp_file_clear (rdp); g_free (rdp); } }

rdp_file *rdp_file_copy (const rdp_file *source)
{
  if (!source) return NULL;
  rdp_file *copy = g_new (rdp_file, 1);
  *copy = *source;
  for (guint i = 0; i < G_N_ELEMENTS (fields); i++)
    if (fields[i].string) {
      char **p = slot (copy, &fields[i]);
      *p = g_strdup (*p ? *p : "");
    }
  return copy;
}

static gboolean valid_int (const Field *f, gint64 value)
{
  if (value < 0 || value > f->maximum) return FALSE;
  if (!strcmp (f->name, "session_bpp"))
    return value == 0 || value == 8 || value == 15 || value == 16 ||
           value == 24 || value == 32;
  return TRUE;
}

/* Only historical g_strescape octal bytes are accepted as an extension.
 * Never unescape a whole JSON document (which would alter its structure). */
static gchar *normalize_json (const char *data)
{
  GString *out = g_string_new (NULL);
  gboolean quoted = FALSE;
  for (const char *p = data; *p; p++) {
    if (*p == '"') quoted = !quoted;
    if (quoted && *p == '\\') {
      if (p[1] >= '0' && p[1] <= '3' && p[2] >= '0' && p[2] <= '7' &&
          p[3] >= '0' && p[3] <= '7') {
        guint byte = (p[1]-'0')*64 + (p[2]-'0')*8 + p[3]-'0';
        if (!byte) goto bad;
        if (byte < 32 || byte == '"' || byte == '\\')
          g_string_append_printf (out, "\\u%04x", byte);
        else g_string_append_c (out, byte);
        p += 3;
        continue;
      }
      if (!p[1] || !strchr ("\\\"/bfnrtu", p[1])) goto bad;
      /* C strings cannot represent embedded NUL. */
      if (g_str_has_prefix (p, "\\u0000")) goto bad;
      g_string_append_c (out, *p++);
    }
    g_string_append_c (out, *p);
  }
  if (quoted || !g_utf8_validate (out->str, out->len, NULL)) goto bad;
  return g_string_free (out, FALSE);
bad:
  g_string_free (out, TRUE);
  return NULL;
}

static gboolean parse_json (rdp_file *rdp, const char *data)
{
  gchar *normalized = normalize_json (data);
  JsonParser *parser = json_parser_new ();
  gboolean ok = FALSE;
  if (!normalized || !json_parser_load_from_data (parser, normalized, -1, NULL))
    goto done;
  JsonNode *root = json_parser_get_root (parser);
  if (!root || !JSON_NODE_HOLDS_OBJECT (root)) goto done;
  JsonObject *object = json_node_get_object (root);
  for (guint i = 0; i < G_N_ELEMENTS (fields); i++) {
    const Field *f = &fields[i];
    if (!json_object_has_member (object, f->name)) continue;
    JsonNode *node = json_object_get_member (object, f->name);
    if (!JSON_NODE_HOLDS_VALUE (node)) goto done;
    if (f->string) {
      if (json_node_get_value_type (node) != G_TYPE_STRING) goto done;
      char **p = slot (rdp, f);
      g_free (*p);
      *p = g_strdup (json_node_get_string (node));
    } else {
      if (json_node_get_value_type (node) != G_TYPE_INT64) goto done;
      gint64 value = json_node_get_int (node);
      if (!valid_int (f, value)) goto done;
      *(int *) slot (rdp, f) = value;
    }
  }
  ok = TRUE;
done:
  g_object_unref (parser);
  g_free (normalized);
  return ok;
}

int rdp_file_set_from_line (rdp_file *rdp, const char *line)
{
  int result = 1;
  if (!rdp || !line || strlen (line) > PROFILE_LIMIT ||
      !g_utf8_validate (line, -1, NULL)) return 1;
  gchar *copy = g_strdup (line);
  gsize length = strlen (copy);
  while (length && (copy[length-1] == '\r' || copy[length-1] == '\n')) copy[--length] = 0;
  if (!length) { result = 0; goto done; }
  char *type = strchr (copy, ':');
  if (!type) goto done;
  *type++ = 0;
  char *value = strchr (type, ':');
  if (!value) goto done;
  *value++ = 0;
  if (strlen (type) != 1 || !strchr ("isb", *type)) goto done;
  /* password 51 is a Windows encrypted blob, never a plaintext credential. */
  if (!strcmp (copy, "password 51")) { result = 0; goto done; }
  for (guint i = 0; i < G_N_ELEMENTS (fields); i++) {
    const Field *f = &fields[i];
    if (strcmp (copy, f->legacy)) continue;
    if (f->string) {
      if (*type != 's') goto done;
      char **p = slot (rdp, f);
      gchar *owned = g_strdup (value);
      g_free (*p);
      *p = owned;
    } else {
      if (*type != 'i' || !*value) goto done;
      const char *digit = value;
      if (*digit == '-') digit++;
      if (!*digit) goto done;
      for (; *digit; digit++) if (!g_ascii_isdigit (*digit)) goto done;
      char *end;
      errno = 0;
      gint64 number = g_ascii_strtoll (value, &end, 10);
      if (errno || *end || !valid_int (f, number)) goto done;
      *(int *) slot (rdp, f) = number;
    }
    break;
  }
  result = 0;
done:
  g_free (copy);
  return result;
}

/* Bounded reads also protect against growing files; no unbounded allocation. */
static gchar *read_profile (const char *path, gsize *length)
{
  int fd = g_open (path, O_RDONLY | O_NONBLOCK | O_CLOEXEC, 0);
  if (fd < 0) return NULL;
  struct stat st;
  if (fstat (fd, &st) || !S_ISREG (st.st_mode)) { close (fd); return NULL; }
  FILE *file = fdopen (fd, "rb");
  if (!file) { close (fd); return NULL; }
  gchar *data = g_malloc (PROFILE_LIMIT + 2);
  *length = fread (data, 1, PROFILE_LIMIT + 1, file);
  gboolean ok = !ferror (file) && *length <= PROFILE_LIMIT;
  fclose (file);
  if (!ok) { g_free (data); return NULL; }
  data[*length] = 0;
  return data;
}

int rdp_file_load (rdp_file *rdp, const char *path)
{
  if (!rdp || !path) return 1;
  gsize length;
  gchar *raw = read_profile (path, &length), *text = NULL;
  if (!raw) return 1;
  if (length >= 2 && (((guchar)raw[0] == 255 && (guchar)raw[1] == 254) ||
                      ((guchar)raw[0] == 254 && (guchar)raw[1] == 255))) {
    gsize read = 0, written = 0;
    text = g_convert (raw + 2, length - 2, "UTF-8",
                      (guchar)raw[0] == 255 ? "UTF-16LE" : "UTF-16BE",
                      &read, &written, NULL);
    if (read != length - 2 || written > PROFILE_LIMIT ||
        (text && memchr (text, 0, written))) g_clear_pointer (&text, g_free);
  } else if (!memchr (raw, 0, length) && g_utf8_validate (raw, length, NULL)) {
    text = g_strdup (g_str_has_prefix (raw, "\357\273\277") ? raw + 3 : raw);
  }
  g_free (raw);
  if (!text) return 1;
  rdp_file temp;
  rdp_file_init (&temp);
  const char *start = text;
  while (g_ascii_isspace (*start)) start++;
  gboolean ok = TRUE;
  if (*start == '{' || *start == '[') ok = parse_json (&temp, start);
  else {
    gchar **lines = g_strsplit_set (text, "\r\n", -1);
    for (guint i = 0; lines[i]; i++)
      if (rdp_file_set_from_line (&temp, lines[i])) { ok = FALSE; break; }
    g_strfreev (lines);
  }
  g_free (text);
  if (ok) { rdp_file_clear (rdp); *rdp = temp; }
  else rdp_file_clear (&temp);
  return ok ? 0 : 1;
}

int rdp_file_save (rdp_file *rdp, const char *path)
{
  if (!rdp || !path) return 1;
  JsonBuilder *builder = json_builder_new ();
  gboolean ok = FALSE;
  json_builder_begin_object (builder);
  for (guint i = 0; i < G_N_ELEMENTS (fields); i++) {
    const Field *f = &fields[i];
    if (!strcmp (f->name, "password") || !strcmp (f->name, "win_password")) continue;
    json_builder_set_member_name (builder, f->name);
    if (f->string) {
      const char *value = *(char **) slot (rdp, f);
      if (!value) value = "";
      if (!g_utf8_validate (value, -1, NULL)) goto done;
      json_builder_add_string_value (builder, value);
    } else {
      int value = *(int *) slot (rdp, f);
      if (!valid_int (f, value)) goto done;
      json_builder_add_int_value (builder, value);
    }
  }
  json_builder_end_object (builder);
  JsonNode *root = json_builder_get_root (builder);
  JsonGenerator *generator = json_generator_new ();
  json_generator_set_root (generator, root);
  json_generator_set_pretty (generator, TRUE);
  gsize length;
  gchar *data = json_generator_to_data (generator, &length);
  if (length <= PROFILE_LIMIT)
    ok = g_file_set_contents_full (path, data, length,
           G_FILE_SET_CONTENTS_CONSISTENT | G_FILE_SET_CONTENTS_DURABLE, 0600, NULL);
  g_free (data);
  json_node_free (root);
  g_object_unref (generator);
done:
  g_object_unref (builder);
  return ok ? 0 : 1;
}
