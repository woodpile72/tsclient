#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include "main.h"
#include "connect.h"
#include "connection.h"
#include "support.h"
#include "rdpfile.h"
#include "tsc-presets.h"

GtkApplication *tsc_app;

static void test_profile_screen (void)
{
  rdp_file before = {0}, after = {0};
  rdp_file_init (&before);
  rdp_file_init (&after);
  /* Fields without controls must survive an update from the form. */
  g_free (after.description); after.description = g_strdup ("keep description");
  g_free (after.progman_group); after.progman_group = g_strdup ("keep group");
  g_free (after.winposstr); after.winposstr = g_strdup ("keep position");
  after.auto_connect = after.compression = after.displayconnectionbar = 1;
  after.redirectcomports = after.redirectdrives = after.redirectprinters = after.redirectsmartcards = 1;
  const int protocol_positions[] = {0, 2, 3, 4, 1, 5};
  const int depths[] = {0, 8, 15, 16, 24, 32};
  const char *tls[] = {"", "1.0", "1.1", "1.2"};
  for (guint i = 0; i < 24; i++) {
    /* Alternate populated/empty values to catch stale text and stale flags. */
#define TEXT(field, value) do { g_free (before.field); before.field = g_strdup (i % 2 ? "" : value); } while (0)
    TEXT (full_address, "test.example:3389");
    TEXT (username, "José😀");
    TEXT (password, "secret%\\\"");
    TEXT (domain, "EXAMPLE");
    TEXT (client_hostname, "local-host");
    TEXT (proto_file, "/tmp/例.ica");
    TEXT (keyboard_language, "en-us");
    TEXT (local_codepage, "UTF-8");
    TEXT (alternate_shell, "C:\\Program Files\\app.exe");
    TEXT (shell_working_directory, "C:\\例");
    TEXT (win_password, "owned legacy value");
#undef TEXT
    before.protocol = i % 6;
    before.desktopwidth = 1280; before.desktopheight = 720;
    before.session_bpp = depths[i % G_N_ELEMENTS (depths)];
    before.audiomode = i % 3;
    before.keyboardhook = i % 3;
    g_free (before.tls_version); before.tls_version = g_strdup (tls[i % G_N_ELEMENTS (tls)]);
    before.enable_alternate_shell = !(i % 2);
#define FLAG(field) before.field = i % 2
    FLAG (attach_to_console); FLAG (bitmapcachepersistenable);
    FLAG (disable_encryption); FLAG (disable_client_encryption);
    FLAG (force_bitmap_updates); FLAG (use_backing_store);
    FLAG (disable_remote_ctrl); FLAG (sync_numlock);
    FLAG (disable_wallpaper); FLAG (disable_full_window_drag);
    FLAG (disable_menu_anims); FLAG (disable_themes);
    FLAG (no_motion_events); FLAG (enable_wm_keys); FLAG (hide_wm_decorations);
#undef FLAG
    /* Empty identity exercises remember state without contacting a keyring. */
    before.remember_password = i % 2;
    rdp_file_set_screen (&before, gConnect);
    g_assert_null (g_object_get_data (G_OBJECT (gConnect), "tsc-setting-profile"));
    g_assert_cmpuint (gtk_drop_down_get_selected (GTK_DROP_DOWN (lookup_widget (gConnect, "optProtocol"))),
                      ==, protocol_positions[before.protocol]);
    g_assert_cmpstr (gtk_editable_get_text (GTK_EDITABLE (lookup_widget (gConnect, "txtDomain"))), ==, before.domain);
    g_assert_cmpint (gtk_check_button_get_active (GTK_CHECK_BUTTON (lookup_widget (gConnect, "chkDesktopBackground"))),
                     ==, !before.disable_wallpaper);
    rdp_file_get_screen (&after, gConnect);
#define SAME_TEXT(field) do { \
    g_assert_cmpstr (after.field, ==, before.field); \
    g_assert_true (after.field != before.field); \
  } while (0)
    SAME_TEXT (full_address); SAME_TEXT (username); SAME_TEXT (password);
    SAME_TEXT (domain); SAME_TEXT (client_hostname); SAME_TEXT (proto_file);
    SAME_TEXT (keyboard_language); SAME_TEXT (local_codepage);
    SAME_TEXT (alternate_shell); SAME_TEXT (shell_working_directory);
    SAME_TEXT (win_password); SAME_TEXT (tls_version);
#undef SAME_TEXT
#define SAME_INT(field) g_assert_cmpint (after.field, ==, before.field)
    SAME_INT (protocol); SAME_INT (desktopwidth); SAME_INT (desktopheight);
    SAME_INT (session_bpp); SAME_INT (audiomode); SAME_INT (keyboardhook);
    SAME_INT (remember_password); SAME_INT (enable_alternate_shell);
    SAME_INT (attach_to_console); SAME_INT (bitmapcachepersistenable);
    SAME_INT (disable_encryption); SAME_INT (disable_client_encryption);
    SAME_INT (force_bitmap_updates); SAME_INT (use_backing_store);
    SAME_INT (disable_remote_ctrl); SAME_INT (sync_numlock);
    SAME_INT (disable_wallpaper); SAME_INT (disable_full_window_drag);
    SAME_INT (disable_menu_anims); SAME_INT (disable_themes);
    SAME_INT (no_motion_events); SAME_INT (enable_wm_keys); SAME_INT (hide_wm_decorations);
#undef SAME_INT
    g_assert_cmpint (after.desktop_size_id, ==, 1);
    g_assert_cmpint (after.screen_mode_id, ==, 1);
    g_assert_cmpstr (after.description, ==, "keep description");
    g_assert_cmpstr (after.progman_group, ==, "keep group");
    g_assert_cmpstr (after.winposstr, ==, "keep position");
    g_assert_cmpint (after.auto_connect, ==, 1);
    g_assert_cmpint (after.compression, ==, 1);
    g_assert_cmpint (after.displayconnectionbar, ==, 1);
    g_assert_cmpint (after.redirectcomports, ==, 1);
    g_assert_cmpint (after.redirectdrives, ==, 1);
    g_assert_cmpint (after.redirectprinters, ==, 1);
    g_assert_cmpint (after.redirectsmartcards, ==, 1);
  }
  /* The returned profile owns its strings even after the form changes. */
  gtk_editable_set_text (GTK_EDITABLE (lookup_widget (gConnect, "txtUsername")), "new user");
  g_assert_cmpstr (after.username, ==, "");
  rdp_file_get_screen (&after, gConnect);
  g_assert_cmpstr (after.username, ==, "new user");
  g_assert_cmpstr (before.username, ==, "");
  rdp_file_clear (&before); rdp_file_clear (&after);
}

static void test_display_and_program (void)
{
  rdp_file before, after;
  rdp_file_init (&before); rdp_file_init (&after);
  for (guint i = 0; i < TSC_SCREEN_PRESET_COUNT; i++) {
    before.desktopwidth = tsc_screen_presets[i].width;
    before.desktopheight = tsc_screen_presets[i].height;
    rdp_file_set_screen (&before, gConnect);
    g_assert_true (gtk_widget_get_sensitive (lookup_widget (gConnect, "alnSize")));
    rdp_file_get_screen (&after, gConnect);
    g_assert_cmpint (after.desktopwidth, ==, before.desktopwidth);
    g_assert_cmpint (after.desktopheight, ==, before.desktopheight);
  }
  /* Fullscreen wins over a matching size; unsupported sizes use defaults. */
  before.screen_mode_id = 2;
  rdp_file_set_screen (&before, gConnect);
  rdp_file_get_screen (&after, gConnect);
  g_assert_cmpint (after.screen_mode_id, ==, 2);
  g_assert_cmpint (after.desktopwidth, ==, 0);
  g_assert_cmpint (after.desktopheight, ==, 0);
  g_assert_false (gtk_widget_get_sensitive (lookup_widget (gConnect, "alnSize")));
  before.screen_mode_id = 1;
  before.desktopwidth = 123; before.desktopheight = 456;
  before.session_bpp = 17;
  g_free (before.tls_version); before.tls_version = g_strdup ("unknown");
  before.protocol = 99; before.audiomode = -1;
  rdp_file_set_screen (&before, gConnect);
  rdp_file_get_screen (&after, gConnect);
  g_assert_cmpint (after.screen_mode_id, ==, 1);
  g_assert_cmpint (after.desktopwidth, ==, 0);
  g_assert_cmpint (after.desktopheight, ==, 0);
  g_assert_cmpint (after.session_bpp, ==, 0);
  g_assert_cmpint (after.protocol, ==, 0);
  g_assert_cmpint (after.audiomode, ==, 0);
  g_assert_cmpstr (after.tls_version, ==, "");
  g_assert_false (gtk_widget_get_sensitive (lookup_widget (gConnect, "alnSize")));
  g_assert_false (gtk_widget_get_sensitive (lookup_widget (gConnect, "alnColor")));
  /* Either nonempty program field enables the legacy profile's program. */
  before.protocol = 4;
  for (guint i = 0; i < 2; i++) {
    g_free (before.alternate_shell); before.alternate_shell = g_strdup (i ? "" : "app");
    g_free (before.shell_working_directory); before.shell_working_directory = g_strdup (i ? "/tmp" : "");
    before.enable_alternate_shell = 0;
    rdp_file_set_screen (&before, gConnect);
    rdp_file_get_screen (&after, gConnect);
    g_assert_cmpint (after.enable_alternate_shell, ==, 1);
    g_assert_true (gtk_widget_get_sensitive (lookup_widget (gConnect, "txtProgramPath")));
    g_assert_true (gtk_widget_get_sensitive (lookup_widget (gConnect, "txtStartFolder")));
  }
  before.protocol = 1; /* VNC disables program controls, but keeps their values. */
  rdp_file_set_screen (&before, gConnect);
  g_assert_false (gtk_widget_get_sensitive (lookup_widget (gConnect, "txtProgramPath")));
  rdp_file_get_screen (&after, gConnect);
  g_assert_cmpstr (after.shell_working_directory, ==, "/tmp");
  rdp_file_clear (&before); rdp_file_clear (&after);
}

static void test_radio_groups (void)
{
  const char *groups[][4] = {
    {"optSize1", "optSize2", "optSize3", NULL},
    {"optColor1", "optColor2", NULL},
    {"optSound1", "optSound2", "optSound3", NULL}
  };
  for (guint g=0; g<G_N_ELEMENTS(groups); g++) {
    guint active = 0;
    for (guint i=0; groups[g][i]; i++)
      active += gtk_check_button_get_active (GTK_CHECK_BUTTON (lookup_widget (gConnect, groups[g][i])));
    g_assert_cmpuint (active, ==, 1);
    for (guint selected=0; groups[g][selected]; selected++) {
      gtk_check_button_set_active (GTK_CHECK_BUTTON (lookup_widget (gConnect, groups[g][selected])), TRUE);
      for (guint i=0; groups[g][i]; i++)
        g_assert_cmpint (gtk_check_button_get_active (GTK_CHECK_BUTTON (lookup_widget (gConnect, groups[g][i]))), ==, i == selected);
    }
  }
}

static void test_connection_async (void)
{
  gchar *bin = g_dir_make_tmp ("tsclient-ui-client-XXXXXX", NULL);
  gchar *exe = g_build_filename (bin, "rdesktop", NULL);
  g_assert_true (g_file_set_contents (exe, "#!/bin/sh\n/bin/sleep 0.2\nexit 0\n", -1, NULL));
  g_chmod (exe, 0700);
  gchar *old_path = g_strdup (g_getenv ("PATH"));
  g_setenv ("PATH", bin, TRUE);
  rdp_file profile = {0};
  rdp_file_init (&profile);
  g_free (profile.full_address); profile.full_address = g_strdup ("test.example");
  profile.protocol = 4;
  rdp_file_set_screen (&profile, gConnect);
  rdp_file_clear (&profile);
  tsc_connection_start (gConnect);
  g_assert_nonnull (g_object_get_data (G_OBJECT (gConnect), "tsc-operation"));
  g_assert_true (gtk_widget_get_visible (gConnect));
  gint64 deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  guint ticks = 0;
  while (g_object_get_data (G_OBJECT (gConnect), "tsc-operation") && g_get_monotonic_time () < deadline) {
    g_main_context_iteration (NULL, FALSE);
    g_usleep (1000);
    ticks++;
  }
  g_assert_cmpuint (ticks, >, 10);
  g_assert_null (g_object_get_data (G_OBJECT (gConnect), "tsc-operation"));
  g_assert_true (gtk_widget_get_sensitive (lookup_widget (gConnect, "nbkComplete")));
  g_setenv ("PATH", old_path, TRUE);
  g_free (old_path); g_unlink (exe); g_rmdir (bin); g_free (exe); g_free (bin);
}

int main (int argc, char **argv)
{
  gchar *home = g_dir_make_tmp ("tsclient-ui-home-XXXXXX", NULL);
  g_setenv ("HOME", home, TRUE);
  g_setenv ("GTK_IM_MODULE", "simple", TRUE);
  g_setenv ("GTK_A11Y", "none", TRUE);
  g_setenv ("GSK_RENDERER", "cairo", TRUE);
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ()) { g_free(home); return 77; }
  tsc_app = gtk_application_new ("com.tsclient.Test", G_APPLICATION_NON_UNIQUE);
  g_assert_true (g_application_register (G_APPLICATION (tsc_app), NULL, NULL));
  gchar *profile_dir = tsc_home_path ();
  g_assert_true (g_str_has_prefix (profile_dir, home));
  g_free (profile_dir);
  tsc_check_files ();
  /* Use checkout assets, including in out-of-tree builds and fresh CI runners. */
  g_assert_true (g_file_test (TEST_ICON_DIR "/icon_laptop.png", G_FILE_TEST_IS_REGULAR));
  add_pixmap_directory (TEST_ICON_DIR);
  create_frmConnect ();
  g_test_add_func ("/ui/radio-groups", test_radio_groups);
  g_test_add_func ("/ui/profile-roundtrip", test_profile_screen);
  g_test_add_func ("/ui/display-and-program", test_display_and_program);
  g_test_add_func ("/ui/async-connection", test_connection_async);
  int result = g_test_run ();
  gtk_window_close (GTK_WINDOW (gConnect));
  gint64 end = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  while (gConnect && g_get_monotonic_time () < end) {
    g_main_context_iteration (NULL, FALSE);
    g_usleep (1000);
  }
  g_assert_null (gConnect);
  g_object_unref (tsc_app);
  gchar *dir = g_build_filename (home, ".tsclient", NULL);
  GDir *entries = g_dir_open (dir, 0, NULL);
  if (entries) {
    const char *name;
    while ((name = g_dir_read_name (entries))) { gchar *path = g_build_filename (dir, name, NULL); g_unlink (path); g_free (path); }
    g_dir_close (entries);
  }
  g_rmdir (dir); g_rmdir (home); g_free (dir); g_free (home);
  return result;
}
