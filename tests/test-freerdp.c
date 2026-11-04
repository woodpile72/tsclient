#include "freerdp.h"
#include <glib/gstdio.h>

static gboolean has (gchar **args, const gchar *value)
{
  return g_strv_contains ((const gchar * const *)args, value);
}

int main (void)
{
  rdp_file rdp = {0};
  rdp.full_address = "server.example:3390";
  rdp.username = "user name";
  rdp.password = "p%ss ' \" $(touch nope)";
  rdp.domain = "EXAMPLE";
  rdp.desktopwidth = 1280;
  rdp.desktopheight = 720;
  rdp.session_bpp = 24;
  gchar **args = tsc_freerdp_argv (&rdp, "/test/xfreerdp3");
  g_assert_cmpstr (args[0], ==, "/test/xfreerdp3");
  g_assert_true (has (args, "/v:server.example:3390"));
  g_assert_true (has (args, "/u:user name"));
  g_assert_true (has (args, "/p:p%ss ' \" $(touch nope)"));
  g_assert_true (has (args, "/size:1280x720"));
  g_assert_true (has (args, "/sound"));
  g_assert_false (has (args, "/cert:ignore"));
  g_strfreev (args);
  rdp.screen_mode_id = 2;
  rdp.audiomode = 2;
  rdp.password = NULL;
  args = tsc_freerdp_argv (&rdp, "xfreerdp");
  g_assert_true (has (args, "/f"));
  g_assert_true (has (args, "/audio-mode:2"));
  g_assert_false (has (args, "/sound"));
  g_assert_false (has (args, "/size:1280x720"));
  g_strfreev (args);

  gchar *dir = g_dir_make_tmp ("tsclient-freerdp-test-XXXXXX", NULL);
  gchar *client = g_build_filename (dir, "xfreerdp", NULL);
  gchar *error = NULL;
  g_setenv ("PATH", dir, TRUE);
  g_assert_cmpint (tsc_launch_freerdp (&rdp, 0, &error), ==, 1);
  g_assert_nonnull (error);
  g_clear_pointer (&error, g_free);
  g_assert_true (g_file_set_contents (client, "#!/bin/sh\nexit 0\n", -1, NULL));
  g_chmod (client, 0700);
  g_assert_cmpint (tsc_launch_freerdp (&rdp, 0, &error), ==, 0);
  g_assert_null (error);
  g_assert_true (g_file_set_contents (client, "#!/bin/sh\necho connection-failed >&2\nexit 7\n", -1, NULL));
  g_chmod (client, 0700);
  g_assert_cmpint (tsc_launch_freerdp (&rdp, 0, &error), ==, 1);
  g_assert_nonnull (strstr (error, "connection-failed"));
  g_free (error);
  g_remove (client);
  g_rmdir (dir);
  g_free (client);
  g_free (dir);
  return 0;
}
