#ifndef AM_MIGRATION_H
#define AM_MIGRATION_H
/* Retain legacy files; never replace an existing current destination. */
int am_migrate_data(void);
/* Bridge the first installer started by a pre-1.1.8 app. */
int am_migrate_update(void);
#endif
