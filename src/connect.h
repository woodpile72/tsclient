#pragma once

#include <gtk/gtk.h>

extern GtkWidget *gConnect;  // used for global accessibility of main form!  :-P

int create_frmConnect (void);

void on_btnConnect_clicked (GtkButton *button, gpointer user_data);

void on_btnQuit_clicked (GtkButton *button, gpointer user_data);

void on_btnHelp_clicked (GtkButton *button, gpointer user_data);

void on_optSize1_clicked (GtkToggleButton *button, gpointer user_data);

void on_optSize2_clicked (GtkToggleButton *button, gpointer user_data);

void on_optSize3_clicked (GtkToggleButton *button, gpointer user_data);

void on_optColor1_clicked (GtkToggleButton *button, gpointer user_data);

void on_optColor2_clicked (GtkToggleButton *button, gpointer user_data);

void on_btnSaveAs_clicked (GtkButton *button, gpointer user_data);

void on_btnOpen_clicked (GtkButton *button, gpointer user_data);

void on_btnProtoFile_clicked (GtkButton *button, gpointer user_data);

void on_txtPerform_changed (GtkEditable *editable, gpointer user_data);

void on_chkStartProgram_toggled (GtkToggleButton *togglebutton, gpointer user_data);

void on_protocol_changed (GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data);
