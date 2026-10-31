#ifndef AM_ARTWORK_H
#define AM_ARTWORK_H
/* Refresh this title's cached source artwork and LiveArea only.
 * Does not rebuild the application database or alter bubble positions.
 * Shell's rendered bubble cache may still require a system restart. */
int am_artwork_refresh(void);
#endif
