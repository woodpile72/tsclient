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
