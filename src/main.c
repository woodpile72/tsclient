
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h> 
#include <sys/stat.h> 
#include <stdio.h>

#include "main.h"
#include "rdpfile.h"
#include "connect.h"
#include "support.h"
#include "launcher.h"
#include "credentials.h"
#include <locale.h>

static void tsc_print_version (void);
static void tsc_activate (GtkApplication *app, gpointer user_data);
static void tsc_add_dev_pixmap_dirs (const char *argv0);

typedef struct {
  gchar *rdp_file_name;
} TscStartupContext;

GtkApplication *tsc_app = NULL;

typedef struct {
  GMainLoop *loop;
  gchar *password;
  GError *error;
} CliPassword;

static void cli_password_ready (GObject *source, GAsyncResult *result, gpointer data)
{
  CliPassword *request = data;
  (void) source;
  request->password = tsc_credentials_lookup_finish (result, &request->error);
  g_main_loop_quit (request->loop);
}

int
main (int argc, char *argv[])
{
  TscStartupContext ctx = {0};
  gboolean version = FALSE;
  gchar *execute = NULL;
  gchar **files = NULL;
  GError *error = NULL;
  GOptionEntry entries[] = {
    {"version", 'v', 0, G_OPTION_ARG_NONE, &version, "Show version", NULL},
    {"execute", 'x', 0, G_OPTION_ARG_FILENAME, &execute, "Launch a saved profile", "FILE"},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, "Profile to open", "FILE"},
    {NULL}
  };
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (PACKAGE, "UTF-8");
  textdomain (PACKAGE);
  GOptionContext *options = g_option_context_new ("— remote desktop client");
  g_option_context_add_main_entries (options, entries, PACKAGE);
  if (!g_option_context_parse (options, &argc, &argv, &error)) {
    g_printerr ("%s\n", error->message);
    g_clear_error (&error);
    g_option_context_free (options);
    g_strfreev (files);
    g_free (execute);
    return 1;
  }
  g_option_context_free (options);
  if (version) {
    tsc_print_version ();
    g_free (execute); g_strfreev (files);
    return 0;
  }
  if (files && files[1]) {
    g_printerr ("Open one profile at a time.\n");
    g_free (execute); g_strfreev (files);
    return 1;
  }
  if (execute) {
    rdp_file profile = {0};
    rdp_file_init (&profile);
    gboolean ok = rdp_file_load (&profile, execute) == 0;
    if (!ok) g_printerr ("Unable to read the profile.\n");
    if (ok && profile.remember_password) {
      CliPassword request = {g_main_loop_new (NULL, FALSE), NULL, NULL};
      tsc_credentials_lookup_async (&profile, NULL, cli_password_ready, &request);
      g_main_loop_run (request.loop);
      g_main_loop_unref (request.loop);
      if (request.error) {
        g_printerr ("Unable to read the saved password from the desktop keyring.\n");
        g_clear_error (&request.error);
        ok = FALSE;
      } else if (request.password) {
        g_free (profile.password);
        profile.password = g_strdup (request.password);
      }
      tsc_credentials_password_free (request.password);
    }
    gchar **args = ok ? tsc_remote_argv (&profile, &error) : NULL;
    if (ok) ok = args && g_spawn_async (NULL, args, NULL, 0, NULL, NULL, NULL, &error);
    if (error) { g_printerr ("%s\n", error->message); g_clear_error (&error); }
    g_strfreev (args);
    rdp_file_clear (&profile);
    g_free (execute); g_strfreev (files);
    return ok ? 0 : 1;
  }
  if (!g_getenv ("GSK_RENDERER")) g_setenv ("GSK_RENDERER", "cairo", TRUE);
  add_pixmap_directory (PACKAGE_DATA_DIR "/tsclient/icons/");
  tsc_add_dev_pixmap_dirs (argv[0]);
  tsc_check_files ();
  gchar *home = tsc_home_path ();
  gchar *def = g_build_filename (home, "default.tsc", NULL);
  gchar *last = g_build_filename (home, "last.tsc", NULL);
  if (files && files[0]) ctx.rdp_file_name = g_strdup (files[0]);
  else if (g_file_test (def, G_FILE_TEST_IS_REGULAR)) ctx.rdp_file_name = g_strdup (def);
  else if (g_file_test (last, G_FILE_TEST_IS_REGULAR)) ctx.rdp_file_name = g_strdup (last);
  g_free (home); g_free (def); g_free (last); g_strfreev (files);

  /* A separately opened profile gets its own window and startup context. */
  tsc_app = gtk_application_new ("com.tsclient.app", G_APPLICATION_NON_UNIQUE);
  gtk_window_set_default_icon_name ("tsclient");
  g_signal_connect (tsc_app, "activate", G_CALLBACK (tsc_activate), &ctx);
  int status = g_application_run (G_APPLICATION (tsc_app), 1, argv);
  g_object_unref (tsc_app);
  g_free (ctx.rdp_file_name);
  return status;
}

static void
tsc_apply_rdp_defaults (TscStartupContext *ctx)
{
  rdp_file *rdp;

  if (!ctx->rdp_file_name || !gConnect)
    return;

  rdp = g_new0 (rdp_file, 1);
  rdp_file_init (rdp);
  if (rdp_file_load (rdp, ctx->rdp_file_name) == 0)
    rdp_file_set_screen (rdp, gConnect);
  else
    tsc_error_message (_("The saved profile could not be loaded."));
  rdp_file_free (rdp);

  g_clear_pointer (&ctx->rdp_file_name, g_free);
}

static void
tsc_activate (GtkApplication *app, gpointer user_data)
{
  TscStartupContext *ctx = user_data;

  tsc_register_icon_theme_dirs ();

  if (!gConnect)
    create_frmConnect ();

  gtk_window_present (GTK_WINDOW (gConnect));
  gtk_window_set_icon_name (GTK_WINDOW (gConnect), "tsclient");
  tsc_apply_rdp_defaults (ctx);
}

static void
tsc_add_dev_pixmap_dirs (const char *argv0)
{
  const gchar *relative[] = {
    "icons",
    "../icons",
    "../../icons",
    NULL
  };
  gchar *cwd = g_get_current_dir ();

  for (gint i = 0; relative[i]; i++) {
    gchar *path = g_build_filename (cwd, relative[i], NULL);
    if (g_file_test (path, G_FILE_TEST_IS_DIR))
      add_pixmap_directory (path);
    g_free (path);
  }

  gchar *exe_path = argv0 ? g_strdup (argv0) : NULL;
  if (exe_path && !g_path_is_absolute (exe_path)) {
    gchar *tmp = g_canonicalize_filename (exe_path, cwd);
    g_free (exe_path);
    exe_path = tmp;
  }
  if (!exe_path)
    exe_path = g_file_read_link ("/proc/self/exe", NULL);
  gchar *exe_dir = exe_path ? g_path_get_dirname (exe_path) : NULL;
  g_free (exe_path);
  if (!exe_dir) {
    g_free (cwd);
    return;
  }

  const gchar *exe_rel[] = {
    "../icons",
    "../../icons",
    NULL
  };
  for (gint i = 0; exe_rel[i]; i++) {
    gchar *path = g_build_filename (exe_dir, exe_rel[i], NULL);
    if (g_file_test (path, G_FILE_TEST_IS_DIR))
      add_pixmap_directory (path);
    g_free (path);
  }
  g_free (exe_dir);
  g_free (cwd);
}

void tsc_print_version () {

  printf ("\n");
  printf ("  Terminal Server Client for Linux\n");
  printf ("  a GTK4 frontend for remote desktop clients\n");
  printf ("  tsclient version "VERSION"\n");
  printf ("\n");
  return;

}
