#include "freerdp.h"

/* Each setting is one argv element: no shell quoting or shell evaluation. */
gchar **
tsc_freerdp_argv (const rdp_file *rdp, const gchar *executable)
{
  GPtrArray *args = g_ptr_array_new ();
#define ARG(value) g_ptr_array_add (args, g_strdup (value))
#define VALUE(option, value) do { if ((value) && *(value)) \
  g_ptr_array_add (args, g_strdup_printf ("/%s:%s", option, value)); } while (0)
  ARG (executable);
  VALUE ("v", rdp->full_address);
  VALUE ("u", rdp->username);
  VALUE ("p", rdp->password);
  VALUE ("d", rdp->domain);
  VALUE ("client-hostname", rdp->client_hostname);
  if (rdp->screen_mode_id == 2)
    ARG ("/f");
  else if (rdp->desktopwidth > 0 && rdp->desktopheight > 0)
    g_ptr_array_add (args, g_strdup_printf ("/size:%dx%d", rdp->desktopwidth, rdp->desktopheight));
  if (rdp->session_bpp > 0)
    g_ptr_array_add (args, g_strdup_printf ("/bpp:%d", rdp->session_bpp));
  g_ptr_array_add (args, g_strdup_printf ("/audio-mode:%d", CLAMP (rdp->audiomode, 0, 2)));
  if (rdp->audiomode == 0) ARG ("/sound");
  ARG ("+clipboard");
  if (rdp->attach_to_console) ARG ("/admin");
  if (rdp->hide_wm_decorations) ARG ("-decorations");
  if (rdp->enable_wm_keys) ARG ("-grab-keyboard");
  if (rdp->enable_alternate_shell) {
    VALUE ("shell", rdp->alternate_shell);
    VALUE ("shell-dir", rdp->shell_working_directory);
  }
  ARG (rdp->disable_wallpaper ? "-wallpaper" : "+wallpaper");
  ARG (rdp->disable_full_window_drag ? "-window-drag" : "+window-drag");
  ARG (rdp->disable_menu_anims ? "-menu-anims" : "+menu-anims");
  ARG (rdp->disable_themes ? "-themes" : "+themes");
  /* Keep FreeRDP's authentication, encryption and certificate defaults. */
  g_ptr_array_add (args, NULL);
#undef ARG
#undef VALUE
  return (gchar **) g_ptr_array_free (args, FALSE);
}

int
tsc_launch_freerdp (const rdp_file *rdp, int launch_async, gchar **error)
{
  gchar *executable = g_find_program_in_path ("xfreerdp3");
  gchar **argv;
  gchar *stderr_text = NULL;
  GError *err = NULL;
  gint status = 0;
  gboolean ok;
  if (!executable) executable = g_find_program_in_path ("xfreerdp");
  if (!executable) {
    if (error) *error = g_strdup ("FreeRDP was not found. Install xfreerdp3 or xfreerdp in the environment running tsclient.");
    return 1;
  }
  argv = tsc_freerdp_argv (rdp, executable);
  if (launch_async)
    ok = g_spawn_async (NULL, argv, NULL, 0, NULL, NULL, NULL, &err);
  else {
    ok = g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, NULL, &stderr_text, &status, &err);
    if (ok) ok = g_spawn_check_wait_status (status, &err);
  }
  if (!ok && error)
    *error = g_strdup (stderr_text && *stderr_text ? stderr_text : err->message);
  g_clear_error (&err);
  g_free (stderr_text);
  g_strfreev (argv);
  g_free (executable);
  return ok ? 0 : 1;
}
