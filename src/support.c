#define TSCLIENT_DEFAULT_ICON "tsclient"
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h> 

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <glib/gprintf.h>

#include <glib/gi18n.h>

#include <sys/types.h> 
#include <sys/stat.h> 
#include <dirent.h> 
#include <fcntl.h> 

#include <arpa/inet.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <errno.h>
#include <signal.h>

#include "rdpfile.h"
#include "support.h"
#include "connect.h"

/* Stolen from jirka's vicious lib */
#define VE_IGNORE_EINTR(expr)                   \
        do {                                    \
                errno = 0;                      \
                expr;                           \
        } while G_UNLIKELY (errno == EINTR);


GtkWidget* lookup_widget (GtkWidget *widget, const gchar *widget_name) {
  GtkWidget *parent, *found_widget;

  for (;;) {

    parent = gtk_widget_get_parent (widget);
    if (!parent)
      parent = g_object_get_data (G_OBJECT (widget), "GladeParentKey");

    if (parent == NULL)
      break;

    widget = parent;
  }

  found_widget = (GtkWidget*) g_object_get_data (G_OBJECT (widget), widget_name);

  if (!found_widget)
    g_warning ("Widget not found: %s", widget_name);

  return found_widget;
}


static GList *pixmaps_directories = NULL;
static gboolean tsc_icon_paths_registered = FALSE;

static gboolean
tsc_pixmap_debug_enabled (void)
{
  static gint state = -1;
  if (state == -1)
    state = g_getenv ("TSCLIENT_PIXMAP_DEBUG") ? 1 : 0;
  return state == 1;
}

/* Use this function to set the directory containing installed pixmaps. */
void add_pixmap_directory (const gchar *directory) {
  pixmaps_directories = g_list_prepend (pixmaps_directories, g_strdup (directory));
  if (tsc_pixmap_debug_enabled ())
    g_message ("tsclient pixmap dir: %s", directory);
}


/* This is an internally used function to find pixmap files. */
gchar* find_pixmap_file (const gchar *filename) {
  GList *elem;
  gchar *pathname = NULL;
  static const gchar *fallback_dirs[] = {
    "icons",
    "./icons",
    "/usr/share/tsclient/icons",
    "/app/share/tsclient/icons",
    NULL
  };
  /* We step through each of the pixmaps directory to find it. */
  elem = pixmaps_directories;
  while (elem) {
    gchar *candidate = g_strdup_printf ("%s%s%s", (gchar*)elem->data,
                                        G_DIR_SEPARATOR_S, filename);
    if (g_file_test (candidate, G_FILE_TEST_EXISTS)) {
      pathname = candidate;
      break;
    }
    g_free (candidate);
    elem = elem->next;
  }

  if (!pathname) {
    for (const gchar **dir = fallback_dirs; *dir; dir++) {
      gchar *candidate = g_build_filename (*dir, filename, NULL);
      if (g_file_test (candidate, G_FILE_TEST_EXISTS)) {
        pathname = candidate;
        break;
      }
      g_free (candidate);
    }
  }
  if (tsc_pixmap_debug_enabled ()) {
    if (pathname)
      g_message ("tsclient pixmap found: %s -> %s", filename, pathname);
    else
      g_message ("tsclient pixmap missing: %s", filename);
  }
  return pathname;
}


/* This is an internally used function to create pixmaps. */
GtkWidget* create_pixmap (GtkWidget *widget, const gchar *filename) {
  gchar *pathname = NULL;
  GtkWidget *pixmap;

  if (!filename || !filename[0])
      return gtk_image_new ();

  pathname = find_pixmap_file (filename);
  if (!pathname) {
    g_warning ("Couldn't find pixmap file: %s", filename);
    return gtk_image_new ();
  }
  pixmap = gtk_image_new_from_file (pathname);
  g_free (pathname);
  return pixmap;
}


void
tsc_register_icon_theme_dirs (void)
{
  if (tsc_icon_paths_registered)
    return;

  GdkDisplay *display = gdk_display_get_default ();
  if (!display)
    return;

  GtkIconTheme *theme = gtk_icon_theme_get_for_display (display);
  if (!theme)
    return;

  for (GList *iter = pixmaps_directories; iter != NULL; iter = iter->next) {
    const gchar *dir = iter->data;
    if (!dir || !g_file_test (dir, G_FILE_TEST_IS_DIR))
      continue;
    gtk_icon_theme_add_search_path (theme, dir);
  }

  tsc_icon_paths_registered = TRUE;
}


/* This is an internally used function to create pixmaps. */
GdkPixbuf* create_pixbuf (const gchar *filename) {
  gchar *pathname = NULL;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  if (!filename || !filename[0])
      return NULL;

  pathname = find_pixmap_file (filename);

  if (!pathname)
    {
      g_warning ("Couldn't find pixmap file: %s", filename);
      return NULL;
    }

  pixbuf = gdk_pixbuf_new_from_file (pathname, &error);
  if (!pixbuf)
    {
      fprintf (stderr, "Failed to load pixbuf file: %s: %s\n",
               pathname, error->message);
      g_error_free (error);
    }
  g_free (pathname);
  return pixbuf;
}


GtkWidget *
tsc_dropdown_new (const gchar *const *items) {
  GtkStringList *model = gtk_string_list_new (NULL);
  guint i;

  if (items) {
    for (i = 0; items[i]; i++) {
      gtk_string_list_append (model, items[i]);
    }
  }

  GtkWidget *dropdown = gtk_drop_down_new (G_LIST_MODEL (model), NULL);
  g_object_set_data_full (G_OBJECT (dropdown), "tsc-dropdown-model",
                          model, g_object_unref);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), 0);
  return dropdown;
}

GtkStringList *
tsc_dropdown_get_model (GtkWidget *dropdown) {
  GtkStringList *model = GTK_STRING_LIST (
      g_object_get_data (G_OBJECT (dropdown), "tsc-dropdown-model"));
  return model;
}

guint
tsc_dropdown_get_selected (GtkWidget *dropdown) {
  return gtk_drop_down_get_selected (GTK_DROP_DOWN (dropdown));
}

void
tsc_dropdown_set_selected (GtkWidget *dropdown, guint idx) {
  gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), idx);
}

gchar *
tsc_dropdown_get_selected_text (GtkWidget *dropdown) {
  GObject *item = gtk_drop_down_get_selected_item (GTK_DROP_DOWN (dropdown));
  if (!item)
    return NULL;
  return g_strdup (gtk_string_object_get_string (GTK_STRING_OBJECT (item)));
}

const gchar *
tsc_dropdown_get_string (GtkWidget *dropdown, guint idx) {
  GtkStringList *model = tsc_dropdown_get_model (dropdown);
  if (!model)
    return NULL;
  if (idx >= g_list_model_get_n_items (G_LIST_MODEL (model)))
    return NULL;
  return gtk_string_list_get_string (model, idx);
}

void
tsc_dropdown_append (GtkWidget *dropdown, const gchar *label) {
  GtkStringList *model = tsc_dropdown_get_model (dropdown);
  if (!model)
    return;
  gtk_string_list_append (model, label);
}

void
tsc_dropdown_clear (GtkWidget *dropdown) {
  GtkStringList *model = tsc_dropdown_get_model (dropdown);
  if (!model)
    return;
  guint items = g_list_model_get_n_items (G_LIST_MODEL (model));
  while (items > 0) {
    gtk_string_list_remove (model, items - 1);
    items--;
  }
}


/***************************************
*                                      *
*   tsc_check_files                    *
*                                      *
***************************************/
 
int tsc_check_files () {
  gchar *home = tsc_home_path ();
  gchar *file_name = g_build_path ("/", home, "mru.tsc", NULL);
  int ret = 0;

  #ifdef TSCLIENT_DEBUG
  printf ("tsc_check_files\n");
  #endif

  // create .tsclient dir in ~/
  mkdir (home, 0700);

  // create mru.tsc dir in ~/.tsclient/
  if (!g_file_test (file_name, G_FILE_TEST_EXISTS)) {
    ret = open (file_name, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (ret > 0) close (ret);
  }

  // create last.tsc dir in ~/.tsclient/
  if (!g_file_test (file_name, G_FILE_TEST_EXISTS)) {
    ret = open (file_name, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (ret > 0) close (ret);
  }

  /* clean up */
  g_free (file_name);
  g_free (home);
  /* complete successfully */
  return 0;
}


/***************************************
*                                      *
*   tsc_home_path                      *
*                                      *
***************************************/
 
gchar *tsc_home_path () {

  #ifdef TSCLIENT_DEBUG
  printf ("tsc_home_path\n");
  #endif

  return g_build_path ("/", g_get_home_dir(), ".tsclient", NULL);

}


/**
 * tsc_get_free_display
 * @start: Start at this display, use 0 as safe value.
 * @server_uid: UID of X server
 *
 * Evil function to figure out which display number is free.
 *
 * Borrowed from gdm-2.6.0:daemon/misc.c
 *
 * Returns: A free X display number or -1 on failure.
 */

int tsc_get_free_display (int start, uid_t server_uid)
{
  int sock;
  int i;
  struct sockaddr_in serv_addr = {0};

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  /* Cap this at 3000, I'm not sure we can ever seriously
   * go that far */
  for (i = start; i < 3000; i ++) {
    struct stat s;
    char buf[256];
    FILE *fp;
    int r;

#ifdef ENABLE_IPV6
    if (have_ipv6 ()) {
      struct sockaddr_in6 serv6_addr= {0};

      sock = socket (AF_INET6, SOCK_STREAM,0);

      serv6_addr.sin6_family = AF_INET6;
      serv6_addr.sin6_addr = in6addr_loopback;
      serv6_addr.sin6_port = htons (6000 + i);
      errno = 0;
      VE_IGNORE_EINTR (connect (sock,
                                (struct sockaddr *)&serv6_addr,
                                sizeof (serv6_addr)));
    }
    else
#endif
      {
        sock = socket (AF_INET, SOCK_STREAM, 0);

        serv_addr.sin_port = htons (6000 + i);

        errno = 0;
        VE_IGNORE_EINTR (connect (sock,
                                  (struct sockaddr *)&serv_addr,
                                  sizeof (serv_addr)));
      }
    if (errno != 0 && errno != ECONNREFUSED) {
      VE_IGNORE_EINTR (close (sock));
      continue;
    }
    VE_IGNORE_EINTR (close (sock));

    /* if lock file exists and the process exists */
    g_snprintf (buf, sizeof (buf), "/tmp/.X%d-lock", i);
    VE_IGNORE_EINTR (r = stat (buf, &s));
    if (r == 0 &&
        ! S_ISREG (s.st_mode)) {
      /* Eeeek! not a regular file?  Perhaps someone
         is trying to play tricks on us */
      continue;
    }
    VE_IGNORE_EINTR (fp = fopen (buf, "r"));
    if (fp != NULL) {
      char buf2[100];
      char *getsret;
      VE_IGNORE_EINTR (getsret = fgets (buf2, sizeof (buf2), fp));
      if (getsret != NULL) {
        gulong pid;
        if (sscanf (buf2, "%lu", &pid) == 1 &&
            kill (pid, 0) == 0) {
          VE_IGNORE_EINTR (fclose (fp));
          continue;
        }

      }
      VE_IGNORE_EINTR (fclose (fp));

      /* whack the file, it's a stale lock file */
      VE_IGNORE_EINTR (unlink (buf));
    }

    /* if starting as root, we'll be able to overwrite any
     * stale sockets or lock files, but a user may not be
     * able to */
    if (server_uid > 0) {
      g_snprintf (buf, sizeof (buf),
                  "/tmp/.X11-unix/X%d", i);
      VE_IGNORE_EINTR (r = stat (buf, &s));
      if (r == 0 &&
          s.st_uid != server_uid) {
        continue;
      }

      g_snprintf (buf, sizeof (buf),
                  "/tmp/.X%d-lock", i);
      VE_IGNORE_EINTR (r = stat (buf, &s));
      if (r == 0 &&
          s.st_uid != server_uid) {
        continue;
      }
    }

    return i;
  }

  return -1;
}


/***************************************
*                                      *
*   tsc_launch_remote                  *
*                                      *
***************************************/

int tsc_launch_remote (rdp_file *rdp_in, int launch_async, gchar** error)
{
  rdp_file *rdp = NULL;
  gchar *c_argv[MAX_ARGVS];
  gchar buffer[MAX_ARGV_LEN];
  gint c_argc = 0;
  GError *err = NULL;
  GSpawnFlags sflags = 0;
  gint cnt = 0;
  gchar *cmd;
  gchar *std_out;
  gchar *std_err;
  gint exit_stat = 0;
  gint retval = 0;

  #ifdef TSCLIENT_DEBUG
  printf ("tsc_launch_remote\n");
  #endif

  if(error) {
    *error=NULL;
  }
  rdp = rdp_in;
  
  if (strlen(rdp->full_address)) {

    cmd = NULL;

    if (rdp->protocol == 0 || rdp->protocol == 4) {
      if (g_find_program_in_path ("rdesktop")) {
        sflags += G_SPAWN_SEARCH_PATH;
        cmd = "rdesktop";
      } else {
	if(error) {
	  *error = g_strdup(_("rdesktop was not found in your path.\nPlease verify your rdesktop installation."));
	}
        return 1;
      }
      g_strlcpy(buffer, cmd, sizeof(buffer));
      c_argv[c_argc++] = g_strdup (buffer);

      sprintf(buffer, "-T%s - %s", rdp->full_address, _("Terminal Server Client"));
      c_argv[c_argc++] = g_strdup (buffer);
      
      // full window mode - use all opts    
      if ( rdp->username && strlen (rdp->username) ) {
        sprintf(buffer, "-u%s", (char*)g_strescape(rdp->username, NULL));
        c_argv[c_argc++] = g_strdup (buffer);
      }
      if ( rdp->password && strlen (rdp->password) ) {
        sprintf(buffer, "-p%s", (char*)g_strescape(rdp->password, NULL));
        c_argv[c_argc++] = g_strdup (buffer);
      }
      if ( rdp->domain && strlen (rdp->domain) ) {
        sprintf(buffer, "-d%s", (char*)g_strescape(rdp->domain, NULL));
        c_argv[c_argc++] = g_strdup (buffer);
      }

      if ( rdp->client_hostname && strlen (rdp->client_hostname) ) {
        sprintf(buffer, "-n%s", (char*)g_strescape(rdp->client_hostname, NULL));
        c_argv[c_argc++] = g_strdup (buffer);
      }

      if (rdp->screen_mode_id == 2) {
        sprintf(buffer, "-f");
        c_argv[c_argc++] = g_strdup (buffer);
      } else {
        switch (rdp->desktopwidth) {
        case 640:
          sprintf(buffer, "-g640x480");
          c_argv[c_argc++] = g_strdup (buffer);
          break;
        case 800:
          sprintf(buffer, "-g800x600");
          c_argv[c_argc++] = g_strdup (buffer);
          break;
        case 1024:
          sprintf(buffer, "-g1024x768");
          c_argv[c_argc++] = g_strdup (buffer);
          break;
        case 1152:
          sprintf(buffer, "-g1152x864");
          c_argv[c_argc++] = g_strdup (buffer);
          break;
        case 1280:
          sprintf(buffer, "-g1280x960");
          c_argv[c_argc++] = g_strdup (buffer);
          break;
        case 1400:
          sprintf(buffer, "-g1400x1050");
          c_argv[c_argc++] = g_strdup (buffer);
          break;
        default:
          break;
        }
      }
    
      switch (rdp->session_bpp) {
      case 8:
        sprintf(buffer, "-a8");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      case 15:
        sprintf(buffer, "-a15");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      case 16:
        sprintf(buffer, "-a16");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      case 24:
        sprintf(buffer, "-a24");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      default:
        break;
      }
    
      // Extra Options
      if (rdp->audiomode == 0) {
        sprintf(buffer, "-rsound:local");
        c_argv[c_argc++] = g_strdup (buffer);
      } else if (rdp->audiomode == 1) {
        sprintf(buffer, "-rsound:remote");
        c_argv[c_argc++] = g_strdup (buffer);
      } else {
        sprintf(buffer, "-rsound:off");
        c_argv[c_argc++] = g_strdup (buffer);
      }

      /*  clipboard feature, rdesktop 1.5 */
      sprintf(buffer, "-rclipboard:PRIMARYCLIPBOARD");
      c_argv[c_argc++] = g_strdup (buffer);

      if (rdp->bitmapcachepersistenable == 1) {
        sprintf(buffer, "-P");
        c_argv[c_argc++] = g_strdup (buffer);
      }

      if (rdp->disable_encryption == 1) {
        c_argv[c_argc++] = g_strdup ("-e");
      }

      if (rdp->disable_client_encryption == 1) {
        c_argv[c_argc++] = g_strdup ("-E");
      }

      if (rdp->tls_version && rdp->tls_version[0]) {
        c_argv[c_argc++] = g_strdup ("-V");
        c_argv[c_argc++] = g_strdup (rdp->tls_version);
      }

      if (rdp->no_motion_events == 1) {
        sprintf(buffer, "-m");
        c_argv[c_argc++] = g_strdup (buffer);
      }

      if (rdp->enable_wm_keys == 1) {
        sprintf(buffer, "-K");
        c_argv[c_argc++] = g_strdup (buffer);
      }

      if (rdp->hide_wm_decorations == 1) {
        sprintf ( buffer, "-D");
        c_argv[c_argc++] = g_strdup (buffer);
      }

      if (rdp->attach_to_console == 1) {
        sprintf ( buffer, "-0");
        c_argv[c_argc++] = g_strdup (buffer);
      }

      if (rdp->force_bitmap_updates == 1) {
        c_argv[c_argc++] = g_strdup ("-b");
      }

      if (rdp->use_backing_store == 1) {
        c_argv[c_argc++] = g_strdup ("-B");
      }

      if ( rdp->keyboard_language && strlen (rdp->keyboard_language) ) {
        sprintf ( buffer, "-k");
        c_argv[c_argc++] = g_strdup (buffer);
        c_argv[c_argc++] = g_strdup (rdp->keyboard_language);
      }
      
      if ( rdp->local_codepage && strlen (rdp->local_codepage) ) {
        c_argv[c_argc++] = g_strdup ("-L");
        c_argv[c_argc++] = g_strdup (rdp->local_codepage);
      }

      if (rdp->disable_remote_ctrl == 1) {
        c_argv[c_argc++] = g_strdup ("-t");
      }

      if (rdp->sync_numlock == 1) {
        c_argv[c_argc++] = g_strdup ("-N");
      }

      if (rdp->enable_alternate_shell == 1) {
        if ( rdp->alternate_shell && strlen (rdp->alternate_shell) ) {
          sprintf ( buffer, "-s");
          c_argv[c_argc++] = g_strdup (buffer);
          c_argv[c_argc++] = g_strdup (rdp->alternate_shell);
        }

        if (rdp->shell_working_directory && strlen (rdp->shell_working_directory)) {
          sprintf ( buffer, "-c");
          c_argv[c_argc++] = g_strdup (buffer);
          c_argv[c_argc++] = g_strdup (rdp->shell_working_directory);
        }
      }
      
      if (rdp->protocol == 0) {
        sprintf (buffer, "-4");
        c_argv[c_argc++] = g_strdup (buffer);
      }

      // do this shit for all modes
      sprintf(buffer, "%s", (char*)g_strescape(rdp->full_address, NULL));
      c_argv[c_argc++] = g_strdup (buffer);

    } else if (rdp->protocol == 1) {

      // it's a vnc call

      if (g_find_program_in_path ("vncviewer")) {
        cmd = "vncviewer";
      } else if (g_find_program_in_path ("xvncviewer")) {
        cmd = "xvncviewer";
      } else if (g_find_program_in_path ("xtightvncviewer")) {
        cmd = "xtightvncviewer";
      } else if (g_find_program_in_path ("svncviewer")) {
        cmd = "svncviewer";
      } else {
	if(error) {
	  *error = g_strdup(_("vncviewer or xvncviewer were\n not found in your path.\nPlease verify your vnc installation."));
	}
        return 1;
      }
      sflags += G_SPAWN_SEARCH_PATH;
      
      g_strlcpy(buffer, cmd, sizeof(buffer));
      c_argv[c_argc++] = g_strdup (buffer);

      if (rdp->screen_mode_id == 2) {
        sprintf(buffer, "-fullscreen");
        c_argv[c_argc++] = g_strdup (buffer);
      } else {
        switch (rdp->desktopwidth) {
        case 640:
          sprintf(buffer, "-geometry");
          c_argv[c_argc++] = g_strdup (buffer);
          sprintf(buffer, "640x480");
          c_argv[c_argc++] = g_strdup (buffer);
          break;
        case 800:
          sprintf(buffer, "-geometry");
          c_argv[c_argc++] = g_strdup (buffer);
          sprintf(buffer, "800x600");
          c_argv[c_argc++] = g_strdup (buffer);
          break;
        case 1024:
          sprintf(buffer, "-geometry");
          c_argv[c_argc++] = g_strdup (buffer);
          sprintf(buffer, "1024x768");
          c_argv[c_argc++] = g_strdup (buffer);
          break;
        case 1152:
          sprintf(buffer, "-geometry");
          c_argv[c_argc++] = g_strdup (buffer);
          sprintf(buffer, "1152x864");
          c_argv[c_argc++] = g_strdup (buffer);
          break;
        case 1280:
          sprintf(buffer, "-geometry");
          c_argv[c_argc++] = g_strdup (buffer);
          sprintf(buffer, "1280x960");
          c_argv[c_argc++] = g_strdup (buffer);
          break;
        default:
          break;
        }
      }

      /*  this is diff in all vnc vers
      switch (rdp->session_bpp) {
      case 8:
        sprintf(buffer, "-bgr233");
        c_argv[c_argc++] = g_strdup (buffer);
        sprintf(buffer, "-depth");
        c_argv[c_argc++] = g_strdup (buffer);
        sprintf(buffer, "8");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      case 15:
        sprintf(buffer, "-depth");
        c_argv[c_argc++] = g_strdup (buffer);
        sprintf(buffer, "15");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      case 16:
        sprintf(buffer, "-depth");
        c_argv[c_argc++] = g_strdup (buffer);
        sprintf(buffer, "16");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      case 24:
        sprintf(buffer, "-depth");
        c_argv[c_argc++] = g_strdup (buffer);
        sprintf(buffer, "24");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      default:
        break;
      }
      */

      if (rdp->no_motion_events == 1) {
        sprintf(buffer, "-v");
        c_argv[c_argc++] = g_strdup (buffer);
      }

      if (strlen(rdp->proto_file) && g_file_test (rdp->proto_file, G_FILE_TEST_EXISTS)) {
        sprintf(buffer, "-passwd");
        c_argv[c_argc++] = g_strdup (buffer);
        sprintf(buffer, "%s", (char*)g_strescape(rdp->proto_file, NULL));
        c_argv[c_argc++] = g_strdup (buffer);
      }

      // do this shit for all modes
      sprintf(buffer, "%s", (char*)g_strescape(rdp->full_address, NULL));
      //buffer[strlen(buffer)-4] = '\0';
      c_argv[c_argc++] = g_strdup (buffer);

    } else if (rdp->protocol == 2) {
      int display;
    
      if (g_find_program_in_path ("Xnest")) {
        sflags += G_SPAWN_SEARCH_PATH;
        
        g_strlcpy(buffer, "Xnest", sizeof(buffer));
        c_argv[c_argc++] = strdup(buffer);
      } else {
	if(error) {
	  *error = g_strdup(_("Xnest was not found in your path.\nPlease verify your Xnest installation."));
	}
        return 1;
      }
        
      /* Starting search from :1 (assuming we run at :0) */
      display = tsc_get_free_display (1, getuid());
      if (-1 == display) {
	if(error) {
	  *error = g_strdup(_("Could not find a free X display."));
	}
        return 1;
      }
      sprintf(buffer, ":%d", display);
      c_argv[c_argc++] = strdup(buffer);

      sprintf(buffer, "-once");
      c_argv[c_argc++] = strdup(buffer);

      switch (rdp->desktopwidth) {
      case 640:
        sprintf(buffer, "-geometry");
        c_argv[c_argc++] = strdup(buffer);
        sprintf(buffer, "640x480");
        c_argv[c_argc++] = strdup(buffer);
        break;
      case 800:
        sprintf(buffer, "-geometry");
        c_argv[c_argc++] = strdup(buffer);
        sprintf(buffer, "800x600");
        c_argv[c_argc++] = strdup(buffer);
        break;
      case 1024:
        sprintf(buffer, "-geometry");
        c_argv[c_argc++] = strdup(buffer);
        sprintf(buffer, "1024x768");
        c_argv[c_argc++] = strdup(buffer);
        break;
      case 1152:
        sprintf(buffer, "-geometry");
        c_argv[c_argc++] = strdup(buffer);
        sprintf(buffer, "1152x864");
        c_argv[c_argc++] = strdup(buffer);
        break;
      case 1280:
        sprintf(buffer, "-geometry");
        c_argv[c_argc++] = strdup(buffer);
        sprintf(buffer, "1280x960");
        c_argv[c_argc++] = strdup(buffer);
        break;
      default:
        break;
      }

      sprintf(buffer, "-query");
      c_argv[c_argc++] = strdup(buffer);

      sprintf(buffer, "%s", (char*)g_strescape(rdp->full_address, NULL));
      //buffer[strlen(buffer)-2] = '\0';
      c_argv[c_argc++] = strdup(buffer);

    } else if (rdp->protocol == 3) {
      // ICA/Citrix Connection
      if (g_find_program_in_path ("wfica")) {
        cmd = "wfica";
      } else if (g_file_test ("/usr/lib/ICAClient/wfica", G_FILE_TEST_EXISTS)) {
        cmd = "wfica";
      } else {
	if(error) {
	  *error = g_strdup(_("wfica was not found in your path.\nPlease verify your ICAClient installation."));
	}
        return 1;
      }
      g_strlcpy(buffer, cmd, sizeof(buffer));
      c_argv[c_argc++] = g_strdup (buffer);

      if ( rdp->username && strlen (rdp->username) ) {
        sprintf(buffer, "-username %s", (char*)g_strescape(rdp->username, NULL));
        c_argv[c_argc++] = g_strdup (buffer);
      }
      if ( rdp->password && strlen (rdp->password) ) {
        sprintf(buffer, "-password %s", (char*)g_strescape(rdp->password, NULL));
        c_argv[c_argc++] = g_strdup (buffer);
      }
      if ( rdp->domain && strlen (rdp->domain) ) {
        sprintf(buffer, "-domain %s", (char*)g_strescape(rdp->domain, NULL));
        c_argv[c_argc++] = g_strdup (buffer);
      }

      if ( rdp->client_hostname && strlen (rdp->client_hostname) ) {
        sprintf(buffer, "-clientname %s", (char*)g_strescape(rdp->client_hostname, NULL));
        c_argv[c_argc++] = g_strdup (buffer);
      }

      switch (rdp->desktopwidth) {
      case 640:
        sprintf(buffer, "-geometry");
        c_argv[c_argc++] = strdup(buffer);
        sprintf(buffer, "640x480");
        c_argv[c_argc++] = strdup(buffer);
        break;
      case 800:
        sprintf(buffer, "-geometry");
        c_argv[c_argc++] = strdup(buffer);
        sprintf(buffer, "800x600");
        c_argv[c_argc++] = strdup(buffer);
        break;
      case 1024:
        sprintf(buffer, "-geometry");
        c_argv[c_argc++] = strdup(buffer);
        sprintf(buffer, "1024x768");
        c_argv[c_argc++] = strdup(buffer);
        break;
      case 1152:
        sprintf(buffer, "-geometry");
        c_argv[c_argc++] = strdup(buffer);
        sprintf(buffer, "1152x864");
        c_argv[c_argc++] = strdup(buffer);
        break;
      case 1280:
        sprintf(buffer, "-geometry");
        c_argv[c_argc++] = strdup(buffer);
        sprintf(buffer, "1280x960");
        c_argv[c_argc++] = strdup(buffer);
        break;
      default:
        break;
      }

      switch (rdp->session_bpp) {
      case 8:
        sprintf(buffer, "-depth");
        c_argv[c_argc++] = g_strdup (buffer);
        sprintf(buffer, "4");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      case 15:
        sprintf(buffer, "-depth");
        c_argv[c_argc++] = g_strdup (buffer);
        sprintf(buffer, "8");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      case 16:
        sprintf(buffer, "-depth");
        c_argv[c_argc++] = g_strdup (buffer);
        sprintf(buffer, "16");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      case 24:
        sprintf(buffer, "-depth");
        c_argv[c_argc++] = g_strdup (buffer);
        sprintf(buffer, "24");
        c_argv[c_argc++] = g_strdup (buffer);
        break;
      default:
        break;
      }

      if (rdp->enable_alternate_shell == 1) {
        if ( rdp->alternate_shell && strlen (rdp->alternate_shell) ) {
          sprintf(buffer, "-program %s", (char*)g_strescape(rdp->alternate_shell, NULL));
          c_argv[c_argc++] = g_strdup (buffer);
        }

        if ( rdp->shell_working_directory && strlen (rdp->shell_working_directory) ) {
          sprintf(buffer, "-directory %s", (char*)g_strescape(rdp->shell_working_directory, NULL));
          c_argv[c_argc++] = g_strdup (buffer);
        }
      }

      if (strlen(rdp->proto_file) && g_file_test (rdp->proto_file, G_FILE_TEST_EXISTS)) {
        sprintf(buffer, "-passwd");
        c_argv[c_argc++] = g_strdup (buffer);
        sprintf(buffer, "%s", (char*)g_strescape(rdp->proto_file, NULL));
        c_argv[c_argc++] = g_strdup (buffer);
      }

      if ( rdp->full_address && strlen (rdp->full_address) ) {
        sprintf(buffer, "-description %s", (char*)g_strescape(rdp->full_address, NULL));
        c_argv[c_argc++] = g_strdup (buffer);
      }
		
    }

    c_argv[c_argc++] = NULL;
    
    // complete events in gtk queue
    while (g_main_context_iteration (NULL, FALSE));

    if (launch_async == 0) {
      if (!g_spawn_sync (NULL, (gchar**)c_argv, NULL, G_SPAWN_SEARCH_PATH,
			 NULL, NULL, &std_out, &std_err, &exit_stat, &err)) {
	g_warning ("failed: spawn_sync of %s\n", cmd);
	if(error) {
	  *error = g_strdup(_("Failed to spawn.\nPlease verify your installation."));
	}
      }
      if (exit_stat && std_err && strlen(std_err)) {
	g_warning ("\n%s\n", std_err);
	if(error) {
	  *error = g_strdup((gchar*)std_err);
	}
	retval = 1;
      }
    } else {
      if (!g_spawn_async (NULL, (gchar**)c_argv, NULL, G_SPAWN_SEARCH_PATH,
			  NULL, NULL, NULL, &err)) 
	{
	  g_warning ("failed: spawn_async of %s\n", cmd);
	}
    }
    if (err) {
      g_warning ("message %s\n", err->message);
      if(error) {
	*error = g_strdup( err->message);
      }
      retval = 1;
    }

    for (cnt = 0; cnt < c_argc; cnt++) {
#ifdef TSCLIENT_DEBUG
      printf ("arg %d:  %s\n", cnt, c_argv[cnt]);
#endif
      free (c_argv[cnt]);
    }
    return retval;
  } else {
    // clean up and exit
    return 1;  
  }
  // clean up and exit
  return retval;
}


typedef struct {
  GtkWindow *window;
  GtkLabel *countdown;
  GMainLoop *loop;
  gint response;
  gint timeout_left;
  guint timeout_id;
} TscConnectDialogState;

static void
tsc_connect_error_finish (TscConnectDialogState *state, gint response)
{
  state->response = response;
  if (state->timeout_id) {
    g_source_remove (state->timeout_id);
    state->timeout_id = 0;
  }
  if (gtk_widget_get_visible (GTK_WIDGET (state->window)))
    gtk_window_destroy (state->window);
  if (state->loop && g_main_loop_is_running (state->loop))
    g_main_loop_quit (state->loop);
}

static gboolean
tsc_connect_error_close_request (GtkWindow *window, gpointer user_data)
{
  TscConnectDialogState *state = user_data;
  tsc_connect_error_finish (state, GTK_RESPONSE_CANCEL);
  return FALSE;
}

static void
tsc_connect_error_button_clicked (GtkButton *button, gpointer user_data)
{
  TscConnectDialogState *state = user_data;
  gint response = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "response-id"));
  tsc_connect_error_finish (state, response);
}

static void
tsc_connect_error_update_label (TscConnectDialogState *state)
{
  if (!state->countdown)
    return;

  gchar *txt = g_strdup_printf (_("Reconnect in %d sec"), state->timeout_left);
  gtk_label_set_text (state->countdown, txt);
  g_free (txt);
}

static gboolean
tsc_connect_error_timeout_cb (gpointer user_data)
{
  TscConnectDialogState *state = user_data;
  if (!gtk_widget_get_visible (GTK_WIDGET (state->window))) {
    state->timeout_id = 0;
    return G_SOURCE_REMOVE;
  }

  if (state->timeout_left <= 0) {
    state->timeout_id = 0;
    tsc_connect_error_finish (state, GTK_RESPONSE_OK);
    return G_SOURCE_REMOVE;
  }

  tsc_connect_error_update_label (state);
  state->timeout_left--;
  return G_SOURCE_CONTINUE;
}

void tsc_connect_error (rdp_file * rdp, const gchar* error)
{
  GtkWidget *dialog, *err_label, *label, *label_detail, *image, *hbox, *vbox, *expander;
  GtkWidget *content, *button_box, *btn_reconnect, *btn_cancel;
  TscConnectDialogState state = {0};
  
#ifdef TSCLIENT_DEBUG
  printf ("tsc_connect_error\n");
#endif
  
  if (!error)
    error = "";

  /* create the widgets */
  dialog = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), _("Terminal Server Client Error"));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  if (gConnect)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gConnect));

  content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_window_set_child (GTK_WINDOW (dialog), content);

  err_label = gtk_label_new ( _("An error has occurred."));
  label     = gtk_label_new ("");
  image     = gtk_image_new_from_icon_name ("dialog-warning");
  hbox      = gtk_hbox_new (FALSE, 12);
  vbox      = gtk_vbox_new (FALSE, 0);

  gtk_label_set_markup (GTK_LABEL (err_label), g_strconcat ("<span weight=\"bold\">", _("An error has occurred."), "</span>", NULL));
  gtk_label_set_justify (GTK_LABEL (err_label), GTK_JUSTIFY_LEFT);

  /* add the label, and show everything we've added to the dialog. */
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 12);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 12);
  gtk_box_pack_start (GTK_BOX (vbox), err_label, TRUE, TRUE, 12);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 12);
  gtk_box_append (GTK_BOX (content), hbox);
  
  /* Create the expander */
  expander = gtk_expander_new (_("Details"));
  gtk_box_append (GTK_BOX (content), expander);
  label_detail = gtk_label_new (error);
  gtk_expander_set_child (GTK_EXPANDER (expander), label_detail);

  button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign (button_box, GTK_ALIGN_END);
  gtk_box_append (GTK_BOX (content), button_box);

  btn_reconnect = gtk_button_new_with_mnemonic (_("_Reconnect"));
  g_object_set_data (G_OBJECT (btn_reconnect), "response-id", GINT_TO_POINTER (GTK_RESPONSE_OK));
  gtk_box_append (GTK_BOX (button_box), btn_reconnect);
  btn_cancel = gtk_button_new_with_mnemonic (_("_Cancel"));
  g_object_set_data (G_OBJECT (btn_cancel), "response-id", GINT_TO_POINTER (GTK_RESPONSE_CANCEL));
  gtk_box_append (GTK_BOX (button_box), btn_cancel);

  state.window = GTK_WINDOW (dialog);
  state.countdown = GTK_LABEL (label);
  state.loop = g_main_loop_new (NULL, FALSE);
  state.response = GTK_RESPONSE_CANCEL;
  state.timeout_left = 30;

  g_signal_connect (dialog, "close-request",
                    G_CALLBACK (tsc_connect_error_close_request), &state);
  g_signal_connect (btn_reconnect, "clicked",
                    G_CALLBACK (tsc_connect_error_button_clicked), &state);
  g_signal_connect (btn_cancel, "clicked",
                    G_CALLBACK (tsc_connect_error_button_clicked), &state);

  tsc_connect_error_update_label (&state);
  state.timeout_id = g_timeout_add_seconds (1, tsc_connect_error_timeout_cb, &state);

  gtk_widget_set_visible (dialog, TRUE);
  g_main_loop_run (state.loop);

  if (state.timeout_id) {
    g_source_remove (state.timeout_id);
    state.timeout_id = 0;
  }

  if (state.loop)
    g_main_loop_unref (state.loop);

  if (state.response == GTK_RESPONSE_OK) {
    gchar **err2 = g_malloc (sizeof (gchar*));
    if (tsc_launch_remote (rdp, 0, err2) != 0 && err2 && *err2) {
      tsc_connect_error (rdp, *err2);
    }
    if (err2) {
      if (*err2)
        g_free (*err2);
      g_free (err2);
    }
  }
}

void tsc_error_message (gchar *message)
{
  GtkAlertDialog *alert;

  #ifdef TSCLIENT_DEBUG
  printf ("tsc_error_message\n");
  #endif

  alert = gtk_alert_dialog_new (_("An error has occurred."));
  gtk_alert_dialog_set_detail (alert, message ? message : "");
  gtk_alert_dialog_show (alert,
                         gConnect ? GTK_WINDOW (gConnect) : NULL);
  g_object_unref (alert);
}


void tsc_about_dialog ()
{
  const gchar *name      = _("Terminal Server Client");
  const gchar *copyright = "(C) 2002-2025 Erick Woods\n"
			   "(C) 2007 Jonh Wendell";
  const gchar *website = "http://www.erick.com/";

  const gchar *authors[] = {
			   "Erick Woods <erick@gnomepro.com>",
			   "Kyle Davis <kyle@gnomeuser.org>",
			   "Jonh Wendell <wendell@bani.com.br>",
			   NULL };

  const gchar *artists[] = {
			   "Erick Woods <erick@gnomepro.com>",
			   "Jakub Steiner",
			   NULL };

  const gchar *license = _(\
    "Terminal Server Client is licensed under the \n" \
    "GNU General Public License (GPL)\n<http://www.gnu.org/licenses/gpl.html>");

  const gchar *comments = \
    _("Terminal Server Client is a frontend for \nrdesktop, vncviewer, wfica and xnest.");
  
  gchar *translators;
  /* Translators comment: put your own name here to appear in the about dialog. */
  translators = _("translator-credits");

  if (!strcmp (translators, "translator-credits"))
    translators = NULL;

  gtk_show_about_dialog (NULL,
			 "name",               name,
			 "comments",           comments,
			 "version",            VERSION,
			 "license",            license,
			 "authors",            authors,
			 "translator-credits", translators,
			 "logo-icon-name",     "tsclient",
			 "copyright",		copyright,
			 "website",		website,
			 "artists",		artists,
			 NULL);
}


void tsc_quick_pick_activate (GtkDropDown *widget, GParamSpec *pspec, gpointer user_data)
{
  gchar *file_name, *rdp_name;
  gchar *home = tsc_home_path ();
  rdp_file *rdp = NULL;
  (void) pspec;
  (void) user_data;

  #ifdef TSCLIENT_DEBUG
  printf ("tsc_quick_pick_activate\n");
  #endif

  /* Do nothing if it's the first element (just a instruction)*/
  if (gtk_drop_down_get_selected (widget) == 0)
    return;

  rdp_name = tsc_dropdown_get_selected_text (GTK_WIDGET (widget));

  // build path to file
  file_name = g_build_path ("/", home, (const gchar *)rdp_name, NULL);
  // check for file in ~/
  if (g_file_test (file_name, G_FILE_TEST_EXISTS)) {
    rdp = g_new0 (rdp_file, 1);
    rdp_file_init (rdp);
    if (rdp_file_load (rdp, file_name) == 0) {
      if (gConnect)
        rdp_file_set_screen (rdp, gConnect);
    } else {
      g_warning ("Failed to load profile %s", file_name);
    }
    g_free (rdp);
  } else {
    g_warning ("Quick connect profile missing: %s", file_name);
  }
  g_free (file_name);
  g_free (home);
  g_free (rdp_name);

  gtk_drop_down_set_selected (widget, 0);

  return;
}

void
tsc_set_protocol_widgets (GtkWidget *main_win, gint protocol)
{
  #ifdef TSCLIENT_DEBUG
  printf ("tsc_set_protocol_widgets: %d\n", protocol);
  #endif

  gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "btnProtoFile"), TRUE);
  gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProtoFile"), TRUE);
  
  gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "vbxSound"), TRUE);
  
  gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "chkStartProgram"), TRUE);
  if (tsc_toggle_button_get_active ((GtkToggleButton*)g_object_get_data (G_OBJECT (main_win), "chkStartProgram"))) {
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProgramPath"), TRUE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtStartFolder"), TRUE);
  } else {
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProgramPath"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtStartFolder"), FALSE);
  }
  gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "chkHideWMDecorations"), FALSE);

  switch (protocol) {
  case 0:  // rdp v4
  case 4:  // rdp v5
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtPassword"), TRUE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "btnProtoFile"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProtoFile"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "chkHideWMDecorations"), TRUE);
    break;
  case 1:  // vnc
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtPassword"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "vbxSound"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "chkStartProgram"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProgramPath"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtStartFolder"), FALSE);
    break;
  case 2:  // xdmcp
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtPassword"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "btnProtoFile"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProtoFile"), FALSE);

    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "vbxSound"), FALSE);

    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "chkStartProgram"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtProgramPath"), FALSE);
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtStartFolder"), FALSE);
    break;
  case 3:  // ica
    gtk_widget_set_sensitive ((GtkWidget*) g_object_get_data (G_OBJECT (main_win), "txtPassword"), TRUE);
    break;
  default:
    break;
  }
  
}
