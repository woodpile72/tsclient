#include "launcher.h"
#include "freerdp.h"
#include <glib/gstdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

static gchar *directory;
static const gchar *clients[] = {
  "rdesktop", "vncviewer", "xvncviewer", "xtightvncviewer", "svncviewer",
  "Xnest", "wfica", "xfreerdp3", "xfreerdp", NULL
};

static void client (const gchar *name, gboolean present)
{
  gchar *path = g_build_filename (directory, name, NULL);
  if (present) {
    g_assert_true (g_file_set_contents (path, "#!/bin/sh\nexit 0\n", -1, NULL));
    g_assert_cmpint (g_chmod (path, 0700), ==, 0);
  } else g_remove (path);
  g_free (path);
}

static gchar **build (rdp_file *rdp)
{
  GError *error = NULL;
  gchar **args = tsc_remote_argv (rdp, &error);
  g_assert_no_error (error);
  g_assert_nonnull (args);
  return args;
}

static void equal_args (gchar **args, const gchar * const *expected)
{
  guint i;
  for (i = 0; expected[i]; i++) g_assert_cmpstr (args[i + 1], ==, expected[i]);
  g_assert_null (args[i + 1]);
}

static void all_options (void)
{
  rdp_file r = {0};
  gchar **a;
  const gchar *expected[] = {
    "-Tserver - Terminal Server Client", "-uuser", "-ppass", "-ddomain", "-nhost",
    "-g1400x1050", "-a24", "-rsound:remote", "-rclipboard:PRIMARYCLIPBOARD",
    "-P", "-e", "-E", "-V", "1.2", "-m", "-K", "-D", "-0", "-b", "-B",
    "-k", "en-us", "-L", "UTF-8", "-t", "-N", "-s", "program", "-c", "directory",
    "-4", "server", NULL
  };
  r.full_address = "server"; r.username = "user"; r.password = "pass";
  r.domain = "domain"; r.client_hostname = "host";
  r.desktopwidth = 1400; r.desktopheight = 1050; r.session_bpp = 24; r.audiomode = 1;
  r.bitmapcachepersistenable = r.disable_encryption = r.disable_client_encryption = 1;
  r.tls_version = "1.2";
  r.no_motion_events = r.enable_wm_keys = r.hide_wm_decorations = 1;
  r.attach_to_console = r.force_bitmap_updates = r.use_backing_store = 1;
  r.keyboard_language = "en-us"; r.local_codepage = "UTF-8";
  r.disable_remote_ctrl = r.sync_numlock = r.enable_alternate_shell = 1;
  r.alternate_shell = "program"; r.shell_working_directory = "directory";
  a = build (&r); equal_args (a, expected);
  g_assert_cmpuint (g_strv_length (a), >, 16); g_strfreev (a);
}

static void literal_values (void)
{
  rdp_file r = {0};
  gchar *long_value = g_strnfill (100000, 'a');
  gchar *value = g_strconcat (long_value, " '\"\\\n\t$(touch nope);%", NULL);
  int protocol;
  r.full_address = r.username = r.password = r.domain = r.client_hostname = value;
  r.keyboard_language = r.local_codepage = r.tls_version = value;
  r.alternate_shell = r.shell_working_directory = value;
  r.enable_alternate_shell = 1;
  for (protocol = 0; protocol <= 5; protocol++) {
    gchar **a;
    r.protocol = protocol; a = build (&r);
    if (protocol == 0 || protocol == 4) {
      const gchar *prefixes[] = {"-u", "-p", "-d", "-n", NULL};
      guint i;
      for (i = 0; prefixes[i]; i++) {
        gchar *joined = g_strconcat (prefixes[i], value, NULL);
        g_assert_cmpstr (a[i + 2], ==, joined); g_free (joined);
      }
      g_assert_cmpstr (a[g_strv_length (a) - 1], ==, value);
      g_assert_cmpstr (a[g_strv_length (a) - (protocol == 0 ? 3 : 2)], ==, value);
    } else if (protocol == 5) {
      gchar **expected = tsc_freerdp_argv (&r, a[0]);
      guint i;
      g_assert_cmpuint (g_strv_length (a), ==, g_strv_length (expected));
      for (i = 0; expected[i]; i++) g_assert_cmpstr (a[i], ==, expected[i]);
      g_strfreev (expected);
    } else {
      g_assert_cmpstr (a[g_strv_length (a) - 1], ==, value);
      if (protocol == 3) {
        g_assert_cmpstr (a[1], ==, "-username");
        g_assert_cmpstr (a[2], ==, value);
        g_assert_cmpstr (a[4], ==, value);
      }
    }
    g_strfreev (a);
  }
  g_free (value); g_free (long_value);
}

static void backends (void)
{
  rdp_file r = {0};
  gchar *password_path = g_build_filename (directory, "pass '\"\\ file", NULL);
  gchar **a;
  const gchar *vnc[] = {"-fullscreen", "-v", "-passwd", password_path, "server", NULL};
  const gchar *ica[] = {"-username", "u", "-password", "p", "-domain", "d",
    "-clientname", "c", "-geometry", "800x600", "-depth", "8", "-program", "s",
    "-directory", "w", "-passwd", password_path, "-description", "server", NULL};
  g_assert_true (g_file_set_contents (password_path, "secret", -1, NULL));
  r.full_address = "server"; r.proto_file = password_path; r.protocol = 1;
  r.screen_mode_id = 2; r.no_motion_events = 1;
  a = build (&r); equal_args (a, vnc); g_strfreev (a);
  r.protocol = 3; r.username = "u"; r.password = "p"; r.domain = "d";
  r.client_hostname = "c"; r.desktopwidth = 800; r.desktopheight = 600; r.session_bpp = 15;
  r.enable_alternate_shell = 1; r.alternate_shell = "s"; r.shell_working_directory = "w";
  a = build (&r); equal_args (a, ica); g_assert_cmpuint (g_strv_length (a), >, 16);
  g_strfreev (a);
  r.protocol = 2;
  a = build (&r);
  g_assert_cmpint (a[1][0], ==, ':');
  g_assert_cmpint (g_ascii_strtoll (a[1] + 1, NULL, 10), >=, 1);
  { const gchar *tail[] = {"-once", "-geometry", "800x600", "-query", "server", NULL};
    equal_args (a + 1, tail); }
  g_strfreev (a); g_remove (password_path); g_free (password_path);
}

static void mappings (void)
{
  const int depths[] = {8, 15, 16, 24, 32, 48};
  const gchar *ica_depths[] = {"4", "8", "16", "24", NULL, NULL};
  rdp_file r = {0};
  guint i, j;
  r.full_address = "server";
  for (r.protocol = 0; r.protocol < 5; r.protocol++) {
    for (j = 0; j < G_N_ELEMENTS (depths); j++) {
      gchar **a;
      r.session_bpp = depths[j]; a = build (&r);
      if (r.protocol == 0 || r.protocol == 4) {
        gchar *option = g_strdup_printf ("-a%d", depths[j]);
        g_assert_cmpint (g_strv_contains ((const gchar * const *) a, option), ==, j < 5);
        g_free (option);
      } else if (r.protocol == 1) {
        g_assert_false (g_strv_contains ((const gchar * const *) a, "-depth"));
        g_assert_false (g_strv_contains ((const gchar * const *) a, "-bgr233"));
      } else if (r.protocol == 3) {
        g_assert_cmpint (g_strv_contains ((const gchar * const *) a, "-depth"), ==, j < 4);
        if (j < 4) g_assert_true (g_strv_contains ((const gchar * const *) a, ica_depths[j]));
      }
      g_strfreev (a);
    }
  }
  r.protocol = 4; r.screen_mode_id = 2;
  for (i = 0; i < 3; i++) {
    const gchar *sounds[] = {"-rsound:local", "-rsound:remote", "-rsound:off"};
    gchar **a;
    r.audiomode = i; a = build (&r);
    g_assert_true (g_strv_contains ((const gchar * const *) a, "-f"));
    g_assert_true (g_strv_contains ((const gchar * const *) a, sounds[i]));
    g_assert_false (g_strv_contains ((const gchar * const *) a, "-4"));
    g_strfreev (a);
  }
}

static void actual_geometry (void)
{
  /* Repeated widths deliberately exercise distinct profile heights. */
  const int dimensions[][2] = {
    {640, 480}, {800, 600}, {1024, 768}, {1152, 864},
    {1280, 720}, {1280, 800}, {1280, 960}, {1366, 768}, {1400, 1050},
    {1920, 1080}, {3840, 2160}, {1234, 567},
    {1280, 0}, {0, 720}, {-1, 720}, {1280, -1}
  };
  rdp_file r = {0};
  guint i, fullscreen;
  r.full_address = "server";
  for (r.protocol = 0; r.protocol <= 5; r.protocol++) {
    for (i = 0; i < G_N_ELEMENTS (dimensions); i++) {
      for (fullscreen = 0; fullscreen <= 1; fullscreen++) {
        gchar **a;
        guint j;
        gboolean seen = FALSE;
        gboolean supports_fullscreen = r.protocol == 0 || r.protocol == 1 ||
                                       r.protocol == 4 || r.protocol == 5;
        gboolean valid = dimensions[i][0] > 0 && dimensions[i][1] > 0;
        gchar *size = g_strdup_printf ("%dx%d", dimensions[i][0], dimensions[i][1]);
        r.desktopwidth = dimensions[i][0]; r.desktopheight = dimensions[i][1];
        r.screen_mode_id = fullscreen ? 2 : 1;
        a = build (&r);
        for (j = 1; a[j]; j++) {
          const gchar *value = NULL;
          if ((r.protocol == 0 || r.protocol == 4) && g_str_has_prefix (a[j], "-g"))
            value = a[j] + 2;
          else if (r.protocol == 5 && g_str_has_prefix (a[j], "/size:"))
            value = a[j] + 6;
          else if (g_str_equal (a[j], "-geometry")) value = a[j + 1];
          if (value) { g_assert_cmpstr (value, ==, size); seen = TRUE; }
        }
        g_assert_cmpint (seen, ==, valid && !(fullscreen && supports_fullscreen));
        if (fullscreen && supports_fullscreen) {
          const gchar *flag = r.protocol == 5 ? "/f" :
                              r.protocol == 1 ? "-fullscreen" : "-f";
          g_assert_true (g_strv_contains ((const gchar * const *) a, flag));
        }
        g_strfreev (a); g_free (size);
      }
    }
  }
}

/* Occupy the port selected by the first probe, then ensure it is skipped. */
static void occupied_display (void)
{
  rdp_file r = {0};
  struct sockaddr_in address = {0};
  gchar **a;
  int fd, display;
  r.protocol = 2; r.full_address = "server";
  a = build (&r);
  display = (int) g_ascii_strtoll (a[1] + 1, NULL, 10);
  g_strfreev (a);
  fd = socket (AF_INET, SOCK_STREAM, 0);
  g_assert_cmpint (fd, >=, 0);
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  address.sin_port = htons (6000 + display);
  g_assert_cmpint (bind (fd, (struct sockaddr *) &address, sizeof address), ==, 0);
  a = build (&r);
  g_assert_cmpint (g_ascii_strtoll (a[1] + 1, NULL, 10), !=, display);
  g_strfreev (a);
  close (fd);
}

static void expect_error (rdp_file *r, TscLauncherError code)
{
  GError *error = NULL;
  g_assert_null (tsc_remote_argv (r, &error));
  g_assert_error (error, TSC_LAUNCHER_ERROR, (gint) code);
  g_clear_error (&error);
  g_assert_null (tsc_remote_argv (r, NULL));
}

static void errors_and_discovery (void)
{
  rdp_file r = {0};
  guint i;
  expect_error (NULL, TSC_LAUNCHER_ERROR_INVALID_SERVER);
  expect_error (&r, TSC_LAUNCHER_ERROR_INVALID_SERVER);
  r.full_address = ""; expect_error (&r, TSC_LAUNCHER_ERROR_INVALID_SERVER);
  for (r.protocol = 0; r.protocol <= 5; r.protocol++) {
    const gchar *invalid[] = {"-", "--", "-passwd", "--help", "-exec=command", NULL};
    for (i = 0; invalid[i]; i++) {
      r.full_address = (gchar *) invalid[i];
      expect_error (&r, TSC_LAUNCHER_ERROR_INVALID_SERVER);
    }
  }
  r.full_address = "server";
  r.protocol = -1; expect_error (&r, TSC_LAUNCHER_ERROR_UNSUPPORTED_PROTOCOL);
  r.protocol = 6; expect_error (&r, TSC_LAUNCHER_ERROR_UNSUPPORTED_PROTOCOL);
  r.protocol = 1;
  for (i = 1; i <= 4; i++) {
    gchar **a = build (&r);
    gchar *expected = g_build_filename (directory, clients[i], NULL);
    g_assert_cmpstr (a[0], ==, expected);
    g_free (expected); g_strfreev (a); client (clients[i], FALSE);
  }
  expect_error (&r, TSC_LAUNCHER_ERROR_EXECUTABLE_NOT_FOUND);
  r.protocol = 5;
  for (i = 7; i <= 8; i++) {
    gchar **a = build (&r);
    gchar *expected = g_build_filename (directory, clients[i], NULL);
    g_assert_cmpstr (a[0], ==, expected);
    g_free (expected); g_strfreev (a); client (clients[i], FALSE);
  }
  expect_error (&r, TSC_LAUNCHER_ERROR_EXECUTABLE_NOT_FOUND);
  client ("rdesktop", FALSE); client ("Xnest", FALSE);
  r.protocol = 0; expect_error (&r, TSC_LAUNCHER_ERROR_EXECUTABLE_NOT_FOUND);
  r.protocol = 4; expect_error (&r, TSC_LAUNCHER_ERROR_EXECUTABLE_NOT_FOUND);
  r.protocol = 2; expect_error (&r, TSC_LAUNCHER_ERROR_EXECUTABLE_NOT_FOUND);
  for (i = 0; clients[i]; i++) client (clients[i], TRUE);
}

int main (int argc, char **argv)
{
  int result;
  guint i;
  gchar *saved_path = g_strdup (g_getenv ("PATH"));
  g_test_init (&argc, &argv, NULL);
  directory = g_dir_make_tmp ("tsclient-launcher-XXXXXX", NULL);
  g_assert_nonnull (directory);
  g_setenv ("PATH", directory, TRUE);
  for (i = 0; clients[i]; i++) client (clients[i], TRUE);
  g_test_add_func ("/launcher/all-options", all_options);
  g_test_add_func ("/launcher/literal-values", literal_values);
  g_test_add_func ("/launcher/backends", backends);
  g_test_add_func ("/launcher/mappings", mappings);
  g_test_add_func ("/launcher/actual-geometry", actual_geometry);
  g_test_add_func ("/launcher/occupied-display", occupied_display);
  g_test_add_func ("/launcher/errors-discovery", errors_and_discovery);
  result = g_test_run ();
  for (i = 0; clients[i]; i++) client (clients[i], FALSE);
  g_rmdir (directory); g_free (directory);
  if (saved_path) g_setenv ("PATH", saved_path, TRUE); else g_unsetenv ("PATH");
  g_free (saved_path);
  return result;
}
