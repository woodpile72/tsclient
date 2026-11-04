#ifndef TSC_FREERDP_H
#define TSC_FREERDP_H
#include <gtk/gtk.h>
#include "rdpfile.h"
gchar **tsc_freerdp_argv (const rdp_file *rdp, const gchar *executable);
int tsc_launch_freerdp (const rdp_file *rdp, int launch_async, gchar **error);
#endif
