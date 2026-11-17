#include "launcher.h"
#include "freerdp.h"
#include <glib/gi18n.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <glib/gstdio.h>

/* A successful probe is advisory: only the X server can reserve a display.
 * Leave stale locks/sockets alone; skipping them is safer than removing them. */
static gboolean
path_absent (const gchar *path)
{
  struct stat st;
  int result;
  do { result = g_lstat (path, &st); } while (result < 0 && errno == EINTR);
  return result < 0 && errno == ENOENT;
}

static gboolean
port_available (int family, int display)
{
  int fd = socket (family, SOCK_STREAM, 0);
  int result;
  if (fd < 0)
    return family == AF_INET6 &&
           (errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT);
  if (family == AF_INET6) {
    struct sockaddr_in6 address = {0};
    address.sin6_family = AF_INET6;
    address.sin6_addr = in6addr_loopback;
    address.sin6_port = htons (6000 + display);
    result = bind (fd, (struct sockaddr *) &address, sizeof address);
    /* IPv6 can be supported by the kernel but disabled on loopback. */
    if (result < 0 && errno == EADDRNOTAVAIL) result = 0;
  } else {
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    address.sin_port = htons (6000 + display);
    result = bind (fd, (struct sockaddr *) &address, sizeof address);
  }
  close (fd);
  return result == 0;
}

static int
free_display (void)
{
  int display;
  for (display = 1; display < 3000; display++) {
    gchar *lock = g_strdup_printf ("/tmp/.X%d-lock", display);
    gchar *socket_path = g_strdup_printf ("/tmp/.X11-unix/X%d", display);
    gboolean available = path_absent (lock) && path_absent (socket_path);
    g_free (lock);
    g_free (socket_path);
    if (available && port_available (AF_INET, display) &&
        port_available (AF_INET6, display))
      return display;
  }
  return -1;
}

G_DEFINE_QUARK (tsc-launcher-error-quark, tsc_launcher_error)

static void
arg (GPtrArray *args, const gchar *value)
{
  g_ptr_array_add (args, g_strdup (value));
}

static void
pair (GPtrArray *args, const gchar *option, const gchar *value)
{
  if (value && *value) {
    arg (args, option);
    arg (args, value);
  }
}

static void
joined (GPtrArray *args, const gchar *option, const gchar *value)
{
  if (value && *value)
    g_ptr_array_add (args, g_strconcat (option, value, NULL));
}

static void
geometry (GPtrArray *args, const rdp_file *rdp, gboolean rdesktop)
{
  gchar *size;
  if (rdp->desktopwidth <= 0 || rdp->desktopheight <= 0) return;
  size = g_strdup_printf ("%dx%d", rdp->desktopwidth, rdp->desktopheight);
  if (rdesktop) joined (args, "-g", size);
  else pair (args, "-geometry", size);
  g_free (size);
}

static gchar *
find_executable (int protocol)
{
  const gchar * const vnc[] = {
    "vncviewer", "xvncviewer", "xtightvncviewer", "svncviewer", NULL
  };
  gchar *path;
  guint i;

  switch (protocol) {
    case 0:
    case 4: return g_find_program_in_path ("rdesktop");
    case 1:
      for (i = 0; vnc[i]; i++) {
        path = g_find_program_in_path (vnc[i]);
        if (path) return path;
      }
      return NULL;
    case 2: return g_find_program_in_path ("Xnest");
    case 3:
      path = g_find_program_in_path ("wfica");
      if (!path &&
          g_file_test ("/usr/lib/ICAClient/wfica", G_FILE_TEST_IS_EXECUTABLE) &&
          !g_file_test ("/usr/lib/ICAClient/wfica", G_FILE_TEST_IS_DIR))
        path = g_strdup ("/usr/lib/ICAClient/wfica");
      return path;
    case 5:
      path = g_find_program_in_path ("xfreerdp3");
      return path ? path : g_find_program_in_path ("xfreerdp");
    default: return NULL;
  }
}

static void
rdesktop_args (GPtrArray *args, const rdp_file *rdp)
{
  g_ptr_array_add (args, g_strdup_printf ("-T%s - %s", rdp->full_address,
                                         _("Terminal Server Client")));
  joined (args, "-u", rdp->username);
  joined (args, "-p", rdp->password);
  joined (args, "-d", rdp->domain);
  joined (args, "-n", rdp->client_hostname);
  if (rdp->screen_mode_id == 2)
    arg (args, "-f");
  else
    geometry (args, rdp, TRUE);
  switch (rdp->session_bpp) {
    case 8: case 15: case 16: case 24: case 32:
      g_ptr_array_add (args, g_strdup_printf ("-a%d", rdp->session_bpp));
      break;
  }
  arg (args, rdp->audiomode == 0 ? "-rsound:local" :
             rdp->audiomode == 1 ? "-rsound:remote" : "-rsound:off");
  arg (args, "-rclipboard:PRIMARYCLIPBOARD");
#define FLAG(field, option) if (rdp->field == 1) arg (args, option)
  FLAG (bitmapcachepersistenable, "-P");
  FLAG (disable_encryption, "-e");
  FLAG (disable_client_encryption, "-E");
  pair (args, "-V", rdp->tls_version);
  FLAG (no_motion_events, "-m");
  FLAG (enable_wm_keys, "-K");
  FLAG (hide_wm_decorations, "-D");
  FLAG (attach_to_console, "-0");
  FLAG (force_bitmap_updates, "-b");
  FLAG (use_backing_store, "-B");
  pair (args, "-k", rdp->keyboard_language);
  pair (args, "-L", rdp->local_codepage);
  FLAG (disable_remote_ctrl, "-t");
  FLAG (sync_numlock, "-N");
#undef FLAG
  if (rdp->enable_alternate_shell == 1) {
    pair (args, "-s", rdp->alternate_shell);
    pair (args, "-c", rdp->shell_working_directory);
  }
  if (rdp->protocol == 0) arg (args, "-4");
  arg (args, rdp->full_address);
}

static void
password_file (GPtrArray *args, const rdp_file *rdp)
{
  if (rdp->proto_file && *rdp->proto_file &&
      g_file_test (rdp->proto_file, G_FILE_TEST_EXISTS))
    pair (args, "-passwd", rdp->proto_file);
}

gchar **
tsc_remote_argv (const rdp_file *rdp, GError **error)
{
  static const gchar * const names[] = {
    "rdesktop", "vncviewer, xvncviewer, xtightvncviewer or svncviewer",
    "Xnest", "wfica", "rdesktop", "xfreerdp3 or xfreerdp"
  };
  GPtrArray *args;
  gchar *executable;
  int display;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  if (!rdp || !rdp->full_address || !*rdp->full_address) {
    g_set_error_literal (error, TSC_LAUNCHER_ERROR,
                         TSC_LAUNCHER_ERROR_INVALID_SERVER,
                         _("A server address is required."));
    return NULL;
  }
  if (rdp->full_address[0] == '-') {
    g_set_error_literal (error, TSC_LAUNCHER_ERROR,
                         TSC_LAUNCHER_ERROR_INVALID_SERVER,
                         _("A server address must not begin with '-'."));
    return NULL;
  }
  if (rdp->protocol < 0 || rdp->protocol > 5) {
    g_set_error (error, TSC_LAUNCHER_ERROR,
                 TSC_LAUNCHER_ERROR_UNSUPPORTED_PROTOCOL,
                 _("Unsupported remote protocol: %d"), rdp->protocol);
    return NULL;
  }
  executable = find_executable (rdp->protocol);
  if (!executable) {
    g_set_error (error, TSC_LAUNCHER_ERROR,
                 TSC_LAUNCHER_ERROR_EXECUTABLE_NOT_FOUND,
                 _("Remote client executable not found: %s"), names[rdp->protocol]);
    return NULL;
  }
  if (rdp->protocol == 5) {
    gchar **result = tsc_freerdp_argv (rdp, executable);
    g_free (executable);
    return result;
  }
  args = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (args, executable);
  switch (rdp->protocol) {
    case 0: case 4:
      rdesktop_args (args, rdp);
      break;
    case 1:
      /* Legacy VNC depth switches were commented out: viewer variants use
       * incompatible color options, so retain each viewer's default. */
      if (rdp->screen_mode_id == 2) arg (args, "-fullscreen");
      else geometry (args, rdp, FALSE);
      if (rdp->no_motion_events == 1) arg (args, "-v");
      password_file (args, rdp);
      arg (args, rdp->full_address);
      break;
    case 2:
      display = free_display ();
      if (display < 0) {
        g_set_error_literal (error, TSC_LAUNCHER_ERROR,
                             TSC_LAUNCHER_ERROR_NO_DISPLAY,
                             _("Could not find a free X display."));
        g_ptr_array_free (args, TRUE);
        return NULL;
      }
      g_ptr_array_add (args, g_strdup_printf (":%d", display));
      arg (args, "-once");
      geometry (args, rdp, FALSE);
      pair (args, "-query", rdp->full_address);
      break;
    case 3:
      pair (args, "-username", rdp->username);
      pair (args, "-password", rdp->password);
      pair (args, "-domain", rdp->domain);
      pair (args, "-clientname", rdp->client_hostname);
      geometry (args, rdp, FALSE);
      switch (rdp->session_bpp) {
        case 8: pair (args, "-depth", "4"); break;
        case 15: pair (args, "-depth", "8"); break;
        case 16: pair (args, "-depth", "16"); break;
        case 24: pair (args, "-depth", "24"); break;
      }
      if (rdp->enable_alternate_shell == 1) {
        pair (args, "-program", rdp->alternate_shell);
        pair (args, "-directory", rdp->shell_working_directory);
      }
      password_file (args, rdp);
      pair (args, "-description", rdp->full_address);
      break;
  }
  g_ptr_array_add (args, NULL);
  return (gchar **) g_ptr_array_free (args, FALSE);
}
