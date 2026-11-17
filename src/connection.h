#pragma once
#include "rdpfile.h"

void tsc_connection_start (GtkWidget *window);
void tsc_profile_save_for_window (GtkWidget *window, const gchar *filename);
void tsc_credentials_load_for_window (GtkWidget *window, const rdp_file *profile);
void tsc_connection_bind_window (GtkWidget *window);
