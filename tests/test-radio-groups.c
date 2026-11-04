#include "gtkcompat.h"

static void check_group (guint count)
{
  GtkWidget *buttons[3];
  GSList *group = NULL;
  for (guint i = 0; i < count; i++) {
    buttons[i] = gtk_radio_button_new_with_mnemonic (NULL, "Option");
    g_object_ref_sink (buttons[i]);
    gtk_radio_button_set_group (buttons[i], group);
    group = gtk_radio_button_get_group (buttons[i]);
  }
  for (guint i = 0; i < count; i++)
    g_assert_cmpint (gtk_check_button_get_active (GTK_CHECK_BUTTON (buttons[i])), ==, i == 0);
  for (guint selected = 0; selected < count; selected++) {
    /* Profile loading and user activation must both be exclusive. */
    tsc_toggle_button_set_active (buttons[selected], TRUE);
    for (guint i = 0; i < count; i++)
      g_assert_cmpint (gtk_check_button_get_active (GTK_CHECK_BUTTON (buttons[i])), ==, i == selected);
    gtk_widget_activate (buttons[selected]);
    g_assert_true (gtk_check_button_get_active (GTK_CHECK_BUTTON (buttons[selected])));
  }
  for (guint i = 0; i < count; i++) g_object_unref (buttons[i]);
}

int main (void)
{
  gtk_init ();
  check_group (3); /* Desktop size */
  check_group (2); /* Color depth */
  check_group (3); /* Sound */
  return 0;
}
