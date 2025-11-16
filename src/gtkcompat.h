#pragma once

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#ifndef GTK_CONTAINER
#define GTK_CONTAINER(widget) (widget)
#endif
#ifndef GTK_BOX
#define GTK_BOX(widget) (widget)
#endif
#ifndef GTK_MISC
#define GTK_MISC(widget) (widget)
#endif
#ifndef GTK_RADIO_BUTTON
#define GTK_RADIO_BUTTON(widget) (widget)
#endif

#ifndef GTK_STOCK_OPEN
#define GTK_STOCK_OPEN "_Open"
#endif
#ifndef GTK_STOCK_SAVE_AS
#define GTK_STOCK_SAVE_AS "Save _As"
#endif
#ifndef GTK_STOCK_ABOUT
#define GTK_STOCK_ABOUT "_About"
#endif
#ifndef GTK_STOCK_CLOSE
#define GTK_STOCK_CLOSE "_Close"
#endif
#ifndef GTK_STOCK_CONNECT
#define GTK_STOCK_CONNECT "_Connect"
#endif
#ifndef GTK_STOCK_CANCEL
#define GTK_STOCK_CANCEL "_Cancel"
#endif
#ifndef GTK_STOCK_DIALOG_WARNING
#define GTK_STOCK_DIALOG_WARNING "dialog-warning"
#endif
#ifndef GTK_STOCK_OK
#define GTK_STOCK_OK "_OK"
#endif
#ifndef GTK_ICON_SIZE_BUTTON
#define GTK_ICON_SIZE_BUTTON GTK_ICON_SIZE_LARGE
#endif

typedef enum {
  GTK_EXPAND = (1 << 0),
  GTK_SHRINK = (1 << 1),
  GTK_FILL   = (1 << 2)
} GtkAttachOptions;

typedef struct _GtkTooltips {
  int unused;
} GtkTooltips;

static inline GtkTooltips *
gtk_tooltips_new (void)
{
  return g_new0 (GtkTooltips, 1);
}

static inline void
gtk_tooltips_set_tip (GtkTooltips *tooltips, GtkWidget *widget,
                      const gchar *tip, const gchar *priv)
{
  (void) tooltips;
  (void) priv;
  if (tip)
    gtk_widget_set_tooltip_text (widget, tip);
}

static inline GtkWidget *
gtk_vbox_new (gboolean homogeneous, gint spacing)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, spacing);
  gtk_box_set_homogeneous (GTK_BOX (box), homogeneous);
  return box;
}

static inline GtkWidget *
gtk_hbox_new (gboolean homogeneous, gint spacing)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, spacing);
  gtk_box_set_homogeneous (GTK_BOX (box), homogeneous);
  return box;
}

static inline void
gtk_box_pack_start (GtkBox *box, GtkWidget *child,
                    gboolean expand, gboolean fill, guint padding)
{
  gtk_box_append (box, child);
  if (padding) {
    gtk_widget_set_margin_start (child, padding);
    gtk_widget_set_margin_end (child, padding);
  }
  if (expand) {
    if (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)) == GTK_ORIENTATION_HORIZONTAL)
      gtk_widget_set_hexpand (child, TRUE);
    else
      gtk_widget_set_vexpand (child, TRUE);
  }
  if (!fill) {
    if (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)) == GTK_ORIENTATION_HORIZONTAL)
      gtk_widget_set_vexpand (child, FALSE);
    else
      gtk_widget_set_hexpand (child, FALSE);
  }
}

static inline void
gtk_box_pack_end (GtkBox *box, GtkWidget *child,
                  gboolean expand, gboolean fill, guint padding)
{
  gtk_box_prepend (box, child);
  if (padding) {
    gtk_widget_set_margin_start (child, padding);
    gtk_widget_set_margin_end (child, padding);
  }
  if (expand) {
    if (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)) == GTK_ORIENTATION_HORIZONTAL)
      gtk_widget_set_hexpand (child, TRUE);
    else
      gtk_widget_set_vexpand (child, TRUE);
  }
  if (!fill) {
    if (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)) == GTK_ORIENTATION_HORIZONTAL)
      gtk_widget_set_vexpand (child, FALSE);
    else
      gtk_widget_set_hexpand (child, FALSE);
  }
}

static inline void
gtk_container_set_border_width (GtkWidget *widget, guint border_width)
{
  gtk_widget_set_margin_top (widget, border_width);
  gtk_widget_set_margin_bottom (widget, border_width);
  gtk_widget_set_margin_start (widget, border_width);
  gtk_widget_set_margin_end (widget, border_width);
}

static inline GtkWidget *
gtk_alignment_new (gfloat xalign, gfloat yalign, gfloat xscale, gfloat yscale)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (box, xalign <= 0.0 ? GTK_ALIGN_START :
                              xalign >= 1.0 ? GTK_ALIGN_END : GTK_ALIGN_CENTER);
  gtk_widget_set_valign (box, yalign <= 0.0 ? GTK_ALIGN_START :
                              yalign >= 1.0 ? GTK_ALIGN_END : GTK_ALIGN_CENTER);
  (void) xscale;
  (void) yscale;
  return box;
}

static inline void
gtk_misc_set_alignment (GtkWidget *widget, gfloat xalign, gfloat yalign)
{
  gtk_widget_set_halign (widget,
                         xalign <= 0.0 ? GTK_ALIGN_START :
                         xalign >= 1.0 ? GTK_ALIGN_END : GTK_ALIGN_CENTER);
  gtk_widget_set_valign (widget,
                         yalign <= 0.0 ? GTK_ALIGN_START :
                         yalign >= 1.0 ? GTK_ALIGN_END : GTK_ALIGN_CENTER);
}

static inline void
gtk_misc_set_padding (GtkWidget *widget, gint xpad, gint ypad)
{
  gtk_widget_set_margin_start (widget, xpad);
  gtk_widget_set_margin_end (widget, xpad);
  gtk_widget_set_margin_top (widget, ypad);
  gtk_widget_set_margin_bottom (widget, ypad);
}

static inline GtkWidget *
gtk_table_new (guint rows, guint columns, gboolean homogeneous)
{
  GtkWidget *grid = gtk_grid_new ();
  gtk_grid_set_row_homogeneous (GTK_GRID (grid), homogeneous);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), homogeneous);
  return grid;
}

static inline void
gtk_table_attach (GtkGrid *grid, GtkWidget *child,
                  guint left_attach, guint right_attach,
                  guint top_attach, guint bottom_attach,
                  GtkAttachOptions xoptions,
                  GtkAttachOptions yoptions,
                  guint xpadding, guint ypadding)
{
  guint width = right_attach - left_attach;
  guint height = bottom_attach - top_attach;
  gtk_grid_attach (grid, child, left_attach, top_attach, width, height);
  if (xoptions & GTK_EXPAND)
    gtk_widget_set_hexpand (child, TRUE);
  if (yoptions & GTK_EXPAND)
    gtk_widget_set_vexpand (child, TRUE);
  if (!(xoptions & GTK_FILL))
    gtk_widget_set_hexpand (child, FALSE);
  if (!(yoptions & GTK_FILL))
    gtk_widget_set_vexpand (child, FALSE);
  gtk_misc_set_padding (child, xpadding, ypadding);
}

static inline void
gtk_table_set_row_spacings (GtkGrid *grid, guint spacing)
{
  gtk_grid_set_row_spacing (grid, spacing);
}

static inline void
gtk_table_set_col_spacings (GtkGrid *grid, guint spacing)
{
  gtk_grid_set_column_spacing (grid, spacing);
}

#define GTK_TABLE(widget) GTK_GRID(widget)

#ifndef GTK_SHADOW_TYPE_DEFINED
typedef enum {
  GTK_SHADOW_NONE,
  GTK_SHADOW_IN,
  GTK_SHADOW_OUT,
  GTK_SHADOW_ETCHED_IN,
  GTK_SHADOW_ETCHED_OUT
} GtkShadowType;
#define GTK_SHADOW_TYPE_DEFINED 1
#endif

static inline void
gtk_frame_set_shadow_type (GtkFrame *frame, GtkShadowType type)
{
  (void) frame;
  (void) type;
}

#ifndef GTK_NOTEBOOK
#define GTK_NOTEBOOK(widget) (widget)
#endif

static inline void
gtk_notebook_set_tab_label_packing (GtkNotebook *notebook,
                                    GtkWidget   *child,
                                    gboolean     expand,
                                    gboolean     fill,
                                    GtkPackType  pack_type)
{
  (void) notebook;
  (void) child;
  (void) expand;
  (void) fill;
  (void) pack_type;
}

static inline void
gtk_container_add (GtkWidget *container, GtkWidget *child)
{
  if (GTK_IS_WINDOW (container))
    gtk_window_set_child (GTK_WINDOW (container), child);
  else if (GTK_IS_FRAME (container))
    gtk_frame_set_child (GTK_FRAME (container), child);
  else if (GTK_IS_BOX (container))
    gtk_box_append (GTK_BOX (container), child);
  else if (GTK_IS_BUTTON (container))
    gtk_button_set_child (GTK_BUTTON (container), child);
  else if (GTK_IS_NOTEBOOK (container))
    gtk_notebook_append_page (GTK_NOTEBOOK (container), child, NULL);
  else
    gtk_widget_set_parent (child, container);
}

static inline void
tsc_radio_assign_group (GSList *group)
{
  for (GSList *iter = group; iter; iter = iter->next)
    g_object_set_data (G_OBJECT (iter->data), "tsc-radio-group", group);
}

static void
tsc_radio_button_handle_toggle (GtkCheckButton *button, gpointer user_data)
{
  (void) user_data;
  if (!gtk_check_button_get_active (button))
    return;

  GSList *group = g_object_get_data (G_OBJECT (button), "tsc-radio-group");
  for (GSList *iter = group; iter; iter = iter->next) {
    GtkWidget *other = iter->data;
    if (other != GTK_WIDGET (button))
      gtk_check_button_set_active (GTK_CHECK_BUTTON (other), FALSE);
  }
}

static inline void
gtk_radio_button_set_group (GtkWidget *button, GSList *group)
{
  GSList *new_group = group;
  if (new_group) {
    if (!g_slist_find (new_group, button))
      new_group = g_slist_prepend (new_group, button);
  } else {
    new_group = g_slist_prepend (NULL, button);
  }
  tsc_radio_assign_group (new_group);
}

static inline GSList *
gtk_radio_button_get_group (GtkWidget *button)
{
  return g_object_get_data (G_OBJECT (button), "tsc-radio-group");
}

static inline GtkWidget *
gtk_radio_button_new_with_mnemonic (GSList *group, const gchar *label)
{
  GtkWidget *button = gtk_check_button_new_with_mnemonic (label);
  gtk_widget_add_css_class (button, "radio");
  g_signal_connect (button, "toggled",
                    G_CALLBACK (tsc_radio_button_handle_toggle), NULL);
  gtk_radio_button_set_group (button, group);
  if (!group)
    gtk_check_button_set_active (GTK_CHECK_BUTTON (button), TRUE);
  else
    gtk_check_button_set_active (GTK_CHECK_BUTTON (button), FALSE);
  return button;
}


static inline void
tsc_toggle_button_set_active (gpointer object, gboolean active)
{
  if (!object)
    return;
#if GTK_CHECK_VERSION(4, 0, 0)
  GtkWidget *widget = GTK_WIDGET (object);
  if (GTK_IS_CHECK_BUTTON (widget))
    gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), active);
  else if (GTK_IS_SWITCH (widget))
    gtk_switch_set_active (GTK_SWITCH (widget), active);
  else if (GTK_IS_TOGGLE_BUTTON (widget))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);
#else
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), active);
#endif
}

static inline gboolean
tsc_toggle_button_get_active (gpointer object)
{
  if (!object)
    return FALSE;
#if GTK_CHECK_VERSION(4, 0, 0)
  GtkWidget *widget = GTK_WIDGET (object);
  if (GTK_IS_CHECK_BUTTON (widget))
    return gtk_check_button_get_active (GTK_CHECK_BUTTON (widget));
  if (GTK_IS_SWITCH (widget))
    return gtk_switch_get_active (GTK_SWITCH (widget));
  if (GTK_IS_TOGGLE_BUTTON (widget))
    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  return FALSE;
#else
  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object));
#endif
}

static inline GtkWidget *
gtk_hbutton_box_new (void)
{
  return gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
}

static inline GtkWidget *
gtk_button_new_from_stock (const gchar *stock_id)
{
  const gchar *label = stock_id;
  const gchar *icon = NULL;

  if (g_strcmp0 (stock_id, GTK_STOCK_OPEN) == 0) {
    label = GTK_STOCK_OPEN;
    icon = "document-open";
  } else if (g_strcmp0 (stock_id, GTK_STOCK_SAVE_AS) == 0) {
    label = GTK_STOCK_SAVE_AS;
    icon = "document-save-as";
  } else if (g_strcmp0 (stock_id, GTK_STOCK_ABOUT) == 0) {
    label = GTK_STOCK_ABOUT;
    icon = "help-about";
  } else if (g_strcmp0 (stock_id, GTK_STOCK_CLOSE) == 0) {
    label = GTK_STOCK_CLOSE;
    icon = "window-close";
  } else if (g_strcmp0 (stock_id, GTK_STOCK_CONNECT) == 0) {
    label = GTK_STOCK_CONNECT;
    icon = "network-connect";
  } else if (g_strcmp0 (stock_id, GTK_STOCK_CANCEL) == 0) {
    label = GTK_STOCK_CANCEL;
    icon = "process-stop";
  } else if (g_strcmp0 (stock_id, GTK_STOCK_OK) == 0) {
    label = GTK_STOCK_OK;
    icon = "emblem-ok";
  }

  GtkWidget *button = gtk_button_new_with_mnemonic (_(label));
  if (icon)
    gtk_button_set_icon_name (GTK_BUTTON (button), icon);
  return button;
}

static inline GtkWidget *
gtk_image_new_from_stock (const gchar *stock_id, GtkIconSize size)
{
  (void) size;
  const gchar *icon = stock_id;
  if (g_strcmp0 (stock_id, GTK_STOCK_DIALOG_WARNING) == 0)
    icon = "dialog-warning";
  else if (g_strcmp0 (stock_id, GTK_STOCK_CONNECT) == 0)
    icon = "network-connect";
  else if (g_strcmp0 (stock_id, GTK_STOCK_OPEN) == 0)
    icon = "document-open";
  else if (g_strcmp0 (stock_id, GTK_STOCK_SAVE_AS) == 0)
    icon = "document-save-as";
  else if (g_strcmp0 (stock_id, GTK_STOCK_ABOUT) == 0)
    icon = "help-about";
  else if (g_strcmp0 (stock_id, GTK_STOCK_CLOSE) == 0)
    icon = "window-close";
  else if (g_strcmp0 (stock_id, GTK_STOCK_OK) == 0)
    icon = "emblem-ok";
  else if (g_strcmp0 (stock_id, GTK_STOCK_CANCEL) == 0)
    icon = "process-stop";
  return gtk_image_new_from_icon_name (icon);
}

#define GTK_WIDGET_SET_FLAGS(widget, flag) \
  G_STMT_START { gtk_widget_set_can_default (widget, TRUE); } G_STMT_END
