#ifndef AM_ARTWORK_H
#define AM_ARTWORK_H
/* Refresh this title's cached source artwork and LiveArea only.
 * Does not rebuild the application database or alter bubble positions.
 * Shell's rendered bubble cache may still require a system restart. */
/* Read-only startup check; no system refresh, module load or cache writes. */
int am_artwork_needs_refresh(void);
/* Call only when the user has chosen Exit or Reboot. */
int am_artwork_refresh(void);
#endif
