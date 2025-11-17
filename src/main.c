
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
#include "rdpfile.h"
#include "connect.h"
#include "support.h"

static void tsc_print_help (void);
static void tsc_print_version (void);
static void tsc_activate (GtkApplication *app, gpointer user_data);
static void tsc_add_dev_pixmap_dirs (const char *argv0);

typedef struct {
  gchar *rdp_file_name;
} TscStartupContext;

GtkApplication *tsc_app = NULL;

int
main (int argc, char *argv[])
{
  gint i;
  TscStartupContext ctx = {0};
  gchar *home, *tsc_default, *tsc_last;
  rdp_file *rdp = NULL;

  #ifdef TSCLIENT_DEBUG
  printf ("debugging messages are on\n");
  #endif

  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (PACKAGE, "UTF-8");
  textdomain (PACKAGE);

  /* Default to the software renderer so we don't require /dev/dri access. */
  if (g_getenv ("GSK_RENDERER") == NULL)
    g_setenv ("GSK_RENDERER", "cairo", TRUE);

  home        = tsc_home_path ();
  tsc_default = g_build_path ("/", home, "default.tsc", NULL);
  tsc_last    = g_build_path ("/", home, "last.tsc", NULL);

  g_free (home);

  add_pixmap_directory (PACKAGE_DATA_DIR "/tsclient/icons/");
  tsc_add_dev_pixmap_dirs (argc > 0 ? argv[0] : NULL);

  tsc_check_files ();

  if (g_file_test (tsc_default, G_FILE_TEST_EXISTS)) {
    ctx.rdp_file_name = g_strdup (tsc_default);
  } else if (g_file_test (tsc_last, G_FILE_TEST_EXISTS)) {
    ctx.rdp_file_name = g_strdup (tsc_last);
  }

  for (i = 1; i < argc; i++) {
    if (strcmp("--help", argv[i]) == 0 || strcmp("-h", argv[i]) == 0) {
      tsc_print_help ();
      g_free (ctx.rdp_file_name);
      g_free (tsc_last);
      g_free (tsc_default);
      return 0;
    }
    if (strcmp("--version", argv[i]) == 0 || strcmp("-v", argv[i]) == 0) {
      tsc_print_version ();
      g_free (ctx.rdp_file_name);
      g_free (tsc_last);
      g_free (tsc_default);
      return 0;
    }
    if (strcmp("-x", argv[i]) == 0 && (i + 1) < argc) {
      if (g_file_test (argv[i+1], G_FILE_TEST_EXISTS)) {
        rdp = g_new0 (rdp_file, 1);
        rdp_file_init (rdp);
        rdp_file_load (rdp, argv[i+1]);
        gchar **error = g_malloc (sizeof (gchar*));
        if (tsc_launch_remote (rdp, 1, error) == 0) {
          g_free (rdp);
          g_free (error);
          g_free (ctx.rdp_file_name);
          g_free (tsc_last);
          g_free (tsc_default);
          return 0;
        }
        if (*error)
          g_free (*error);
        g_free (error);
        g_free (rdp);
      }
    } else if (g_file_test (argv[i], G_FILE_TEST_EXISTS)) {
      g_free (ctx.rdp_file_name);
      ctx.rdp_file_name = g_strdup (argv[i]);
    }
  }

  tsc_app = gtk_application_new ("com.tsclient.app", G_APPLICATION_DEFAULT_FLAGS);
  gtk_window_set_default_icon_name ("tsclient");
  g_signal_connect (tsc_app, "activate", G_CALLBACK (tsc_activate), &ctx);
  g_application_run (G_APPLICATION (tsc_app), argc, argv);
  g_object_unref (tsc_app);

  g_free (ctx.rdp_file_name);
  g_free (tsc_last);
  g_free (tsc_default);
  return 0;
}

static void
tsc_apply_rdp_defaults (TscStartupContext *ctx)
{
  rdp_file *rdp;

  if (!ctx->rdp_file_name || !gConnect)
    return;

  rdp = g_new0 (rdp_file, 1);
  rdp_file_init (rdp);
  rdp_file_load (rdp, ctx->rdp_file_name);
  rdp_file_set_screen (rdp, gConnect);
  g_free (rdp);

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
  if (!exe_dir)
    return;

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

void tsc_print_help () {

  printf ("\n");
  printf ("  Usage: tsclient [OPTION]... [FILE]...\n\n");
  printf ("  FILE           an rdp format file containing options\n");
  printf ("  -h, --help     display this help and exit\n");
  printf ("  -v, --version  output version information and exit\n");
  printf ("  -x FILE        launch rdesktop with options specified in FILE\n");
  printf ("\n");
  return;

}


void tsc_print_version () {

  printf ("\n");
  printf ("  Terminal Server Client for Linux\n");
  printf ("  a frontend for rdesktop\n");
  printf ("  tsclient version "VERSION"\n");
  printf ("\n");
  return;

}
