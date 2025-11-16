#pragma once

#include <glib.h>
#include <glib/gi18n.h>

typedef struct {
  gint width;
  gint height;
  const char *label;
} TscScreenPreset;

static const TscScreenPreset tsc_screen_presets[] = {
  { 640, 480,   N_("640 x 480 pixels") },
  { 800, 600,   N_("800 x 600 pixels") },
  { 1024, 768,  N_("1024 x 768 pixels") },
  { 1152, 864,  N_("1152 x 864 pixels") },
  { 1280, 720,  N_("1280 x 720 pixels") },
  { 1280, 800,  N_("1280 x 800 pixels") },
  { 1280, 960,  N_("1280 x 960 pixels") },
  { 1366, 768,  N_("1366 x 768 pixels") },
  { 1440, 900,  N_("1440 x 900 pixels") },
  { 1600, 900,  N_("1600 x 900 pixels") },
  { 1680, 1050, N_("1680 x 1050 pixels") },
  { 1920, 1080, N_("1920 x 1080 pixels") },
  { 1920, 1200, N_("1920 x 1200 pixels") },
  { 2560, 1080, N_("2560 x 1080 pixels") },
  { 2560, 1440, N_("2560 x 1440 pixels") },
  { 2560, 1600, N_("2560 x 1600 pixels") },
  { 3440, 1440, N_("3440 x 1440 pixels") },
  { 3840, 2160, N_("3840 x 2160 pixels") },
};

#define TSC_SCREEN_PRESET_COUNT (G_N_ELEMENTS (tsc_screen_presets))
