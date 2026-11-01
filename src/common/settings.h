/* PSVSD/AM persistent configuration. GPL-3.0-or-later. */
#ifndef PSVSD_AM_SETTINGS_H
#define PSVSD_AM_SETTINGS_H
#include <stddef.h>
enum { AM_UMA0=0, AM_UX0=1 };
typedef struct { int enabled, target; } AmSettings;
int am_settings_parse(const char *text, AmSettings *out);
int am_settings_format(const AmSettings *settings, char *out, size_t capacity);
const char *am_target_name(int target);
#endif
