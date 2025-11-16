/***************************************
*                                      *
*   MRU File Handlers                  *
*                                      *
***************************************/


#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <dirent.h> 
#include <glib/gi18n.h>

#include "mrulist.h"
#include "rdpfile.h"
#include "support.h"

static int mru_file_to_list (GSList** list);
static int mru_list_to_file (GSList** list);
static gboolean mru_file_to_list_json (GSList **list, const gchar *data, gsize length);
static int mru_file_to_list_legacy (GSList **list, const gchar *mru_filename);

/***************************************
*                                      *
*   mru_file_to_list                   *
*                                      *
***************************************/

int mru_file_to_list (GSList** list)
{
  gchar *mru_filename;
  gchar *contents = NULL;
  gsize length = 0;
  int ret;

  #ifdef TSCLIENT_DEBUG
  printf ("mru_file_to_list\n");
  #endif

  mru_filename = g_build_path ("/", tsc_home_path(), "mru.tsc", NULL);

  if (g_file_get_contents (mru_filename, &contents, &length, NULL)) {
    const gchar *ptr = contents;
    while (ptr && g_ascii_isspace (*ptr))
      ptr++;
    if (ptr && *ptr == '[') {
      gboolean ok = mru_file_to_list_json (list, contents, length);
      g_free (contents);
      g_free (mru_filename);
      if (ok)
        return 0;
    } else {
      g_free (contents);
    }
  }

  ret = mru_file_to_list_legacy (list, mru_filename);
  g_free (mru_filename);
  return ret;
}


/***************************************
*                                      *
*   mru_list_to_file                   *
*                                      *
***************************************/
 
int mru_list_to_file (GSList** list)
{
  GSList* lptr = NULL;
  gchar *mru_filename;
  GString *json;
  gboolean ok;
  int i;
  gboolean first = TRUE;

  #ifdef TSCLIENT_DEBUG
  printf ("mru_list_to_file\n");
  #endif

  mru_filename = g_build_path ("/", tsc_home_path(), "mru.tsc", NULL);

  json = g_string_new ("[\\n");
  lptr = *list;
  i = 0;
  while ((lptr != NULL && i <= 10)) {
    gchar *escaped = g_strescape ((gchar*)lptr->data, NULL);
    g_string_append_printf (json, "%s  \\\"%s\\\"",
                            first ? "" : ",\\n",
                            escaped);
    g_free (escaped);
    lptr = lptr->next;
    i++;
    first = FALSE;
  }
  g_string_append (json, "\\n]\\n");

  ok = g_file_set_contents (mru_filename, json->str, json->len, NULL);

  g_string_free (json, TRUE);
  g_free (mru_filename);

  return ok ? 0 : 1;
}

static gboolean
mru_file_to_list_json (GSList **list, const gchar *data, gsize length)
{
  GScanner *scanner = g_scanner_new (NULL);
  gboolean success = FALSE;

  g_scanner_input_text (scanner, data, length);
  if (g_scanner_get_next_token (scanner) != '[') {
    g_scanner_destroy (scanner);
    return FALSE;
  }

  while (TRUE) {
    GTokenType token = g_scanner_get_next_token (scanner);
    if (token == G_TOKEN_EOF)
      break;
    if (token == ']') {
      success = TRUE;
      break;
    }
    if (token != G_TOKEN_STRING)
      break;
    *list = g_slist_append (*list, g_strdup (scanner->value.v_string));
    GTokenType next = g_scanner_peek_next_token (scanner);
    if (next == ',')
      g_scanner_get_next_token (scanner);
  }

  g_scanner_destroy (scanner);
  return success;
}

static int
mru_file_to_list_legacy (GSList **list, const gchar *mru_filename)
{
  FILE* fptr;
  gchar buffer[MAX_SERVER_SIZE];
  int i;

  if((fptr = fopen(mru_filename, "r")) == NULL) {
    return 1;
  }

  buffer[0] = '\0';
  if(fgets(buffer, MAX_SERVER_SIZE, fptr) == NULL) {
    fclose (fptr);
    return 0;
  }

  i = 0;
  do {
    if (buffer[0] != '\0') {
      if(buffer[strlen(buffer) - 1] == '\n') buffer[strlen(buffer) - 1] = '\0';
      if(buffer[strlen(buffer) - 1] == '\r') buffer[strlen(buffer) - 1] = '\0';
      buffer[MAX_SERVER_SIZE - 1] = '\0';
      *list = g_slist_append(*list, g_strdup(buffer));
    }
    buffer[0] = '\n';
    i++;
  }
  while ((fgets(buffer, MAX_SERVER_SIZE, fptr) != NULL && i <= 10));

  fclose (fptr);
  return 0;
}


/***************************************
*                                      *
*   mru_add_server                     *
*                                      *
***************************************/
 
int mru_add_server (const char *server_name)
{
  GSList* list = NULL;
  GSList* list_new = NULL;

  #ifdef TSCLIENT_DEBUG
  printf ("mru_add_server\n");
  #endif

  mru_file_to_list (&list);

  while (list) {
    if (strcmp(server_name, (char*)list->data) != 0) {
      list_new = g_slist_append (list_new, (gpointer)g_strdup ((gchar*)list->data));
    }
    g_free (list->data);
    list = list->next;
  }
  g_slist_free (list);
  
  list_new = g_slist_prepend (list_new, (gpointer)g_strdup (server_name));

  mru_list_to_file (&list_new);

  while (list_new) {
    g_free (list_new->data);
    list_new = list_new->next;
  }
  g_slist_free (list_new);

  /* complete successfully */
  return 0;
}


/***************************************
*                                      *
*   mru_list_to_screen                 *
*                                      *
***************************************/
 
int mru_to_screen (GtkWidget *main_window)
{
  GList *server_items = NULL;
  GSList *server_list = NULL;
  GtkWidget *widget;
  int i;

  #ifdef TSCLIENT_DEBUG
  printf ("mru_to_screen\n");
  #endif

  // Get the mru.tsc
  mru_file_to_list (&server_list);

  // load mru combos
  i = 0;
  while ((server_list != NULL && i <= 10)) {
    server_items = g_list_append (server_items, g_strdup (server_list->data));
    server_list = server_list->next;
    i++;
  }
  widget = lookup_widget (main_window, "cboComputer");
  if (widget) {
    tsc_dropdown_clear (widget);
    tsc_dropdown_append (widget, _("Recent"));
    for (GList *iter = server_items; iter != NULL; iter = iter->next) {
      tsc_dropdown_append (widget, (const gchar *) iter->data);
    }
    tsc_dropdown_set_selected (widget, 0);
    GtkStringList *model = tsc_dropdown_get_model (widget);
    gtk_widget_set_sensitive (widget,
      model && g_list_model_get_n_items (G_LIST_MODEL (model)) > 1);
  }
  g_list_free_full (server_items, g_free);

  return 0;
}
