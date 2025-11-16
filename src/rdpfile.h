#ifndef RDPFILE_H
#define RDPFILE_H

/***************************************
*                                      *
*   Definitions                        *
*                                      *
***************************************/

#define MAX_KEY_SIZE 128
#define MAX_TYPE_SIZE 16
#define MAX_VALUE_SIZE 2048
#define MAX_BUFFER_SIZE 4096

typedef struct
{
  char key[MAX_KEY_SIZE];
  char type[MAX_TYPE_SIZE];
  char value[MAX_VALUE_SIZE];
} rdp_rec;

typedef struct
{
  char *alternate_shell;
  int attach_to_console;
  int audiomode;
  int auto_connect;
  int bitmapcachepersistenable;
  char *client_hostname;
  int compression;
  char *description;
  int desktop_size_id;
  int desktopheight;
  int desktopwidth;
  int disable_client_encryption;
  int disable_full_window_drag;
  int disable_remote_ctrl;
  int disable_menu_anims;
  int disable_themes;
  int disable_wallpaper;
  int disable_encryption;
  int displayconnectionbar;
  char *domain;
  int enable_alternate_shell;
  int enable_wm_keys;
  int force_bitmap_updates;
  char *full_address;
  int hide_wm_decorations;
  char *keyboard_language;
  int keyboardhook;
  char *local_codepage;
  int no_motion_events;
  char *password;
  char *win_password;
  char *progman_group;
  int protocol;
  char *proto_file;
  int redirectcomports;
  int redirectdrives;
  int redirectprinters;
  int redirectsmartcards;
  int screen_mode_id;
  int session_bpp;
  int sync_numlock;
  char *shell_working_directory;
  char *tls_version;
  int use_backing_store;
  char *username;
  char *winposstr;
} rdp_file;


int rdp_file_init (rdp_file *rdp_in);
int rdp_file_load (rdp_file *rdp_in, const char *fqpath);
int rdp_file_save (rdp_file *rdp_in, const char *fqpath);
int rdp_file_set_screen (rdp_file *rdp_in, GtkWidget *main_window);
int rdp_file_get_screen (rdp_file *rdp_in, GtkWidget *main_window);
int rdp_file_set_from_line (rdp_file *rdp_in, const char *str_in);
int rdp_load_profile_launcher (GtkWidget *main_window);
int rdp_files_to_list (GSList** list);
GHashTable* rdp_files_to_hash (void);

#endif /* RDPFILE_H */
