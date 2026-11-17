#ifndef TSC_LAUNCHER_H
#define TSC_LAUNCHER_H

#include <gtk/gtk.h>
#include "rdpfile.h"

G_BEGIN_DECLS

#define TSC_LAUNCHER_ERROR (tsc_launcher_error_quark ())
typedef enum {
  TSC_LAUNCHER_ERROR_INVALID_SERVER,
  TSC_LAUNCHER_ERROR_UNSUPPORTED_PROTOCOL,
  TSC_LAUNCHER_ERROR_EXECUTABLE_NOT_FOUND,
  TSC_LAUNCHER_ERROR_NO_DISPLAY
} TscLauncherError;

GQuark tsc_launcher_error_quark (void);

/* Returns an owned, NULL-terminated vector of literal arguments, or NULL
 * with error set. Free with g_strfreev(). argv[0] is the discovered path;
 * pass directly to g_spawn_*(), without a shell or further escaping.
 * Xnest selects a display internally, skipping existing locks/sockets.
 * Display selection does not reserve the display until Xnest starts.
 * Protocol 5 delegates argument construction to tsc_freerdp_argv(). */
gchar **tsc_remote_argv (const rdp_file *rdp, GError **error);

G_END_DECLS
#endif
