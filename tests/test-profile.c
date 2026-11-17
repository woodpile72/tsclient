#include <glib.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <sys/stat.h>
typedef struct _GtkWidget GtkWidget;
#include "rdpfile.h"
static gchar *directory, *path;
/* Optional standalone MRU coverage: compile with -DTEST_MRU,
 * -ffunction-sections -fdata-sections and -Wl,--gc-sections to omit UI code. */
#ifdef TEST_MRU
#include "../src/mrulist.c"
static char *test_home;
gchar *tsc_home_path (void) { return g_strdup (test_home); }
static void mru_migration (void) {
  test_home = g_dir_make_tmp ("tsclient-mru-XXXXXX", NULL);
  gchar *path = mru_path ();
  const char *values[] = {"例😀", "a\"quote", "a\\path", "comma,\nnewline", "a\\\",\\n"};
  GSList *list = NULL, *loaded = NULL;
  GString *broken = g_string_new ("[\\n");
  for (guint i=0; i<G_N_ELEMENTS(values); i++) {
    list = g_slist_append (list, g_strdup (values[i]));
    gchar *escaped = g_strescape (values[i], NULL);
    g_string_append_printf (broken, "%s  \\\"%s\\\"", i ? ",\\n" : "", escaped);
    g_free (escaped);
  }
  g_string_append (broken, "\\n]\\n");
  g_assert_true (g_file_set_contents (path, broken->str, -1, NULL));
  g_assert_cmpint (mru_file_to_list (&loaded), ==, 0);
  guint i = 0;
  for (GSList *p = loaded; p; p=p->next) g_assert_cmpstr (p->data, ==, values[i++]);
  g_assert_cmpuint (i, ==, G_N_ELEMENTS(values));
  g_assert_cmpint (mru_list_to_file (&loaded), ==, 0);
  g_assert_cmpint (mru_file_to_list (&loaded), ==, 0);
  i=0;
  for (GSList *p = loaded; p; p=p->next) g_assert_cmpstr (p->data, ==, values[i++]);
  g_assert_cmpint (mru_add_server ("new😀"), ==, 0);
  g_assert_cmpint (mru_file_to_list (&loaded), ==, 0);
  g_assert_cmpstr (loaded->data, ==, "new😀");
  GSList *same = loaded;
  const char *bad[] = {"[\"ok\",42]", "[\"unterminated", "[\\n  \\\"bad\\q\\\"\\n]\\n", "[\"\\u0000\"]"};
  for (i=0; i<G_N_ELEMENTS(bad); i++) {
    g_file_set_contents (path,bad[i],-1,NULL);
    g_assert_cmpint (mru_file_to_list (&loaded), ==, 1);
    g_assert_true (same == loaded);
  }
  g_file_set_contents (path,"one\r\n\r\ntwo\n",-1,NULL);
  g_assert_cmpint (mru_file_to_list (&loaded), ==, 0);
  g_assert_cmpuint (g_slist_length (loaded), ==, 2);
  g_assert_cmpstr (loaded->next->data, ==, "two");
  g_file_set_contents (path,"[\\n\\n]\\n",-1,NULL);
  g_assert_cmpint (mru_file_to_list (&loaded), ==, 0);
  g_assert_null (loaded);
  g_string_free (broken, TRUE); g_slist_free_full (list, g_free);
  g_slist_free_full (loaded,g_free); g_remove(path); g_free(path);
  g_rmdir (test_home); g_free (test_home);
}

#endif
static void put (const char *data, gssize length)
{ g_assert_true (g_file_set_contents (path, data, length, NULL)); }
static void roundtrip (void)
{
  rdp_file a, b;
  g_assert_cmpint (rdp_file_init (&a), ==, 0);
  rdp_file_init (&b);
  g_assert_cmpstr (a.password, ==, "");
  g_assert_cmpint (a.remember_password, ==, 0);
  g_assert_cmpint (rdp_file_set_from_line (&a, "description:s:日本語 😀 \"quoted\" \\path\t"), ==, 0);
  rdp_file_set_from_line (&a, "password:s:secret-unique");
  rdp_file_set_from_line (&a, "remember password:i:1");
  a.protocol = 5;
  g_assert_cmpint (rdp_file_save (&a, path), ==, 0);
  struct stat st;
  g_assert_cmpint (g_stat (path, &st), ==, 0);
  g_assert_cmpint (st.st_mode & 0777, ==, 0600);
  g_assert_cmpint (g_chmod (path, 0644), ==, 0);
  g_assert_cmpint (rdp_file_save (&a, path), ==, 0);
  g_assert_cmpint (g_stat (path, &st), ==, 0);
  g_assert_cmpint (st.st_mode & 0777, ==, 0600);
  gchar *data;
  g_file_get_contents (path, &data, NULL, NULL);
  g_assert_null (strstr (data, "secret-unique"));
  JsonParser *parser = json_parser_new ();
  g_assert_true (json_parser_load_from_data (parser, data, -1, NULL));
  JsonObject *obj = json_node_get_object (json_parser_get_root (parser));
  g_assert_false (json_object_has_member (obj, "password"));
  g_assert_false (json_object_has_member (obj, "win_password"));
  g_free (data); g_object_unref (parser);
  g_assert_cmpint (rdp_file_load (&b, path), ==, 0);
  g_assert_cmpstr (a.description, ==, b.description);
  g_assert_cmpint (b.remember_password, ==, 1);
  g_assert_cmpint (b.protocol, ==, 5);
  g_assert_cmpstr (b.password, ==, "");
  rdp_file_clear (&b);
  rdp_file *duplicate = rdp_file_copy (&a);
  b = *duplicate; g_free (duplicate);
  g_assert_true (a.description != b.description);
  g_assert_true (a.password != b.password);
  g_assert_cmpstr (b.password, ==, "secret-unique");
  g_assert_null (rdp_file_copy (NULL));
  rdp_file_clear (&a); rdp_file_clear (&a); rdp_file_clear (&b);
  rdp_file *heap = g_new0 (rdp_file, 1);
  rdp_file_init (heap); rdp_file_free (heap); rdp_file_free (NULL);
}
static void encodings (void)
{
  const char *rdp = "full address:s:例.example:3389\r\nusername:s:Zoë😀\r\naudiomode:i:2\r\npassword 51:b:0100deadbeef\r\n";
  const char *enc[] = {"UTF-8", "UTF-16LE", "UTF-16BE"};
  for (guint i = 0; i < G_N_ELEMENTS (enc); i++) {
    gsize n;
    gchar *converted = g_convert (rdp, -1, enc[i], "UTF-8", NULL, &n, NULL);
    GString *bytes = g_string_new (i == 0 ? "\357\273\277" : i == 1 ? "\377\376" : "\376\377");
    g_string_append_len (bytes, converted, n); put (bytes->str, bytes->len);
    rdp_file a; rdp_file_init (&a);
    g_assert_cmpint (rdp_file_load (&a, path), ==, 0);
    g_assert_cmpstr (a.full_address, ==, "例.example:3389");
    g_assert_cmpstr (a.username, ==, "Zoë😀");
    g_assert_cmpstr (a.win_password, ==, "");
    g_assert_cmpint (a.audiomode, ==, 2);
    rdp_file_clear (&a); g_string_free (bytes, TRUE); g_free (converted);
  }
  put (rdp, -1); rdp_file a; rdp_file_init (&a);
  g_assert_cmpint (rdp_file_load (&a, path), ==, 0);
  put ("{\"username\":\"\\u65e5\\ud83d\\ude00\",\"description\":\"\\303\\251\\\\123\",\"password\":\"migrate\",\"unknown\":{\"nested\":[true,null]}}", -1);
  g_assert_cmpint (rdp_file_load (&a, path), ==, 0);
  g_assert_cmpstr (a.username, ==, "日😀");
  g_assert_cmpstr (a.description, ==, "é\\123");
  g_assert_cmpstr (a.password, ==, "migrate");
  rdp_file_clear (&a);
}
static void malformed (void)
{
  const char *bad[] = {
    "{\"username\":\"changed\",\"protocol\":6}", "{\"protocol\":-1}",
    "{\"protocol\":1.0}", "{\"protocol\":true}", "{\"protocol\":\"1\"}",
    "{\"protocol\":9223372036854775808}", "{\"username\":null}",
    "{\"audiomode\":-1}", "{\"audiomode\":3}", "{\"session_bpp\":17}",
    "{\"desktopwidth\":65536}", "{\"remember_password\":2}",
    "{\"username\":\"a\\u0000b\"}", "{\"username\":\"\\000\"}",
    "{\"username\":\"\\377\"}", "{\"username\":\"\\x41\"}",
    "{\"username\":\"x\"", "{} garbage", "[]", "not a profile",
    "username:s:changed\nprotocol:i:99", "protocol:i:1junk", "protocol:s:1",
    "protocol:i:999999999999999999999999999", "username:i:2"
  };
  rdp_file a; rdp_file_init (&a);
  rdp_file_set_from_line (&a, "username:s:original");
  char *original = a.username;
  for (guint i = 0; i < G_N_ELEMENTS (bad); i++) {
    put (bad[i], -1);
    g_test_message ("reject %s", bad[i]);
    g_assert_cmpint (rdp_file_load (&a, path), ==, 1);
    g_assert_true (a.username == original);
    g_assert_cmpstr (a.username, ==, "original");
  }
  const char invalid[][6] = {{'x',0,'y'}, {'\377','\376',0,'\330'}, {'\376','\377',0}, {'\377'}};
  const gsize lengths[] = {3,4,3,1};
  for (guint i = 0; i < G_N_ELEMENTS (invalid); i++) {
    put (invalid[i], lengths[i]);
    g_assert_cmpint (rdp_file_load (&a, path), ==, 1);
    g_assert_true (a.username == original);
  }
  rdp_file_clear (&a);
}
static void long_inputs (void)
{
  gchar *value = g_strnfill (100000, 'a');
  gchar *line = g_strconcat ("description:s:", value, NULL);
  put (line, -1);
  rdp_file a; rdp_file_init (&a);
  g_assert_cmpint (rdp_file_load (&a, path), ==, 0);
  g_assert_cmpstr (a.description, ==, value);
  g_assert_cmpint (rdp_file_save (&a, path), ==, 0);
  g_assert_cmpint (rdp_file_load (&a, path), ==, 0);
  g_assert_cmpstr (a.description, ==, value);
  gchar *oversize = g_strnfill (1024*1024+1, 'x'); put (oversize, -1);
  g_assert_cmpint (rdp_file_load (&a, path), ==, 1);
  g_assert_cmpstr (a.description, ==, value);
  g_free (oversize); g_free (line); g_free (value); rdp_file_clear (&a);
}
static void nonregular_input (void)
{
  rdp_file profile;
  rdp_file_init (&profile);
  gchar *fifo = g_build_filename (directory, "pipe.tsc", NULL);
  g_assert_cmpint (mkfifo (fifo, 0600), ==, 0);
  /* With no writer, a blocking open would hang this test. */
  g_assert_cmpint (rdp_file_load (&profile, fifo), ==, 1);
  g_assert_cmpint (rdp_file_load (&profile, directory), ==, 1);
  g_remove (fifo);
  g_free (fifo);
  rdp_file_clear (&profile);
}
int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  directory = g_dir_make_tmp ("tsclient-profile-XXXXXX", NULL);
  path = g_build_filename (directory, "profile.tsc", NULL);
  g_test_add_func ("/profile/roundtrip", roundtrip);
  g_test_add_func ("/profile/encodings-migration", encodings);
  g_test_add_func ("/profile/malformed-atomic", malformed);
  g_test_add_func ("/profile/long", long_inputs);
  g_test_add_func ("/profile/nonregular", nonregular_input);
#ifdef TEST_MRU
  g_test_add_func ("/mru/migration-roundtrip", mru_migration);
#endif
  int result = g_test_run ();
  g_remove (path); g_rmdir (directory); g_free (path); g_free (directory);
  return result;
}
